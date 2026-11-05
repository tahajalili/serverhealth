// health.c - tiny server health reader (Linux: /proc, /sys, statvfs)
// Build: make    Usage: ./health [path ...]   (paths default to "/")
// Exit code: 0 ok, 1 warning (thresholds below), 2 read error

#define _GNU_SOURCE
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/statvfs.h>
#include <time.h>
#include <unistd.h>

#define WARN_CPU   90.0
#define WARN_MEM   90.0
#define WARN_DISK  90.0
#define WARN_TEMP  80.0

static int status = 0;

static void warn(int level)
{
	if (level > status)
		status = level;
}

static const char *flag(double v, double limit)
{
	if (v >= limit) {
		warn(1);
		return "  !!";
	}
	return "";
}

struct cpu_times {
	unsigned long long idle, total;
};

static int read_cpu(struct cpu_times *t)
{
	FILE *f = fopen("/proc/stat", "r");
	unsigned long long v[8] = {0};
	int i;

	if (!f)
		return -1;
	if (fscanf(f, "cpu %llu %llu %llu %llu %llu %llu %llu %llu",
		   &v[0], &v[1], &v[2], &v[3], &v[4], &v[5], &v[6], &v[7]) < 4) {
		fclose(f);
		return -1;
	}
	fclose(f);
	t->idle = v[3] + v[4]; // idle + iowait
	t->total = 0;
	for (i = 0; i < 8; i++)
		t->total += v[i];
	return 0;
}

static void show_cpu(void)
{
	struct cpu_times a, b;
	struct timespec ts = {0, 500 * 1000 * 1000};
	double pct;

	if (read_cpu(&a) < 0) {
		warn(2);
		puts("cpu      unavailable");
		return;
	}
	nanosleep(&ts, NULL);
	if (read_cpu(&b) < 0 || b.total == a.total) {
		warn(2);
		puts("cpu      unavailable");
		return;
	}
	pct = 100.0 * (1.0 - (double)(b.idle - a.idle) / (double)(b.total - a.total));
	printf("cpu      %5.1f%% busy (%ld cores)%s\n", pct,
	       sysconf(_SC_NPROCESSORS_ONLN), flag(pct, WARN_CPU));
}

static void show_load_uptime(void)
{
	FILE *f;
	double l1, l5, l15, up;

	f = fopen("/proc/loadavg", "r");
	if (f && fscanf(f, "%lf %lf %lf", &l1, &l5, &l15) == 3)
		printf("load     %.2f %.2f %.2f\n", l1, l5, l15);
	else {
		warn(2);
		puts("load     unavailable");
	}
	if (f)
		fclose(f);

	f = fopen("/proc/uptime", "r");
	if (f && fscanf(f, "%lf", &up) == 1) {
		long s = (long)up;
		printf("uptime   %ldd %ldh %ldm\n", s / 86400, s % 86400 / 3600, s % 3600 / 60);
	} else {
		warn(2);
		puts("uptime   unavailable");
	}
	if (f)
		fclose(f);
}

static void show_mem(void)
{
	FILE *f = fopen("/proc/meminfo", "r");
	char key[64];
	unsigned long long kb, total = 0, avail = 0, swap_t = 0, swap_f = 0;

	if (!f) {
		warn(2);
		puts("mem      unavailable");
		return;
	}
	while (fscanf(f, "%63s %llu kB\n", key, &kb) == 2) {
		if (!strcmp(key, "MemTotal:"))
			total = kb;
		else if (!strcmp(key, "MemAvailable:"))
			avail = kb;
		else if (!strcmp(key, "SwapTotal:"))
			swap_t = kb;
		else if (!strcmp(key, "SwapFree:"))
			swap_f = kb;
	}
	fclose(f);
	if (!total) {
		warn(2);
		puts("mem      unavailable");
		return;
	}
	double used = 100.0 * (double)(total - avail) / (double)total;
	printf("mem      %5.1f%% used (%.0f / %.0f MiB)%s\n", used,
	       (total - avail) / 1024.0, total / 1024.0, flag(used, WARN_MEM));
	if (swap_t)
		printf("swap     %5.1f%% used (%.0f / %.0f MiB)\n",
		       100.0 * (double)(swap_t - swap_f) / (double)swap_t,
		       (swap_t - swap_f) / 1024.0, swap_t / 1024.0);
}

static void show_disk(const char *path)
{
	struct statvfs s;

	if (statvfs(path, &s) < 0) {
		warn(2);
		printf("disk     %s: unavailable\n", path);
		return;
	}
	double total = (double)s.f_blocks * s.f_frsize;
	double avail = (double)s.f_bavail * s.f_frsize;
	double used = total ? 100.0 * (1.0 - (double)s.f_bfree / s.f_blocks) : 0;
	printf("disk     %s: %5.1f%% used (%.1f GiB free of %.1f)%s\n", path, used,
	       avail / (1 << 30), total / (1 << 30), flag(used, WARN_DISK));
}

#define MAX_ZONES 32

struct zone {
	char name[64];
	int idx;
	long mc;
};

static int cmp_zone(const void *a, const void *b)
{
	return ((const struct zone *)a)->idx - ((const struct zone *)b)->idx;
}

static void show_temps(void)
{
	DIR *d = opendir("/sys/class/thermal");
	struct dirent *e;
	struct zone zones[MAX_ZONES];
	char p[300];
	int n = 0, i;

	if (!d)
		return;
	while ((e = readdir(d))) {
		FILE *f;
		long mc;
		char type[64] = "";

		if (strncmp(e->d_name, "thermal_zone", 12))
			continue;
		snprintf(p, sizeof p, "/sys/class/thermal/%s/type", e->d_name);
		f = fopen(p, "r");
		if (f) {
			if (fscanf(f, "%63s", type) != 1)
				type[0] = '\0';
			fclose(f);
		}
		snprintf(p, sizeof p, "/sys/class/thermal/%s/temp", e->d_name);
		f = fopen(p, "r");
		if (f && fscanf(f, "%ld", &mc) == 1 && n < MAX_ZONES) {
			snprintf(zones[n].name, sizeof zones[n].name, "%s",
				 type[0] ? type : "thermal_zone");
			zones[n].idx = atoi(e->d_name + 12);
			zones[n].mc = mc;
			n++;
		}
		if (f)
			fclose(f);
	}
	closedir(d);

	qsort(zones, n, sizeof zones[0], cmp_zone);
	for (i = 0; i < n; i++)
		printf("temp     %s (zone%d): %.1f C%s\n", zones[i].name, zones[i].idx,
		       zones[i].mc / 1000.0, flag(zones[i].mc / 1000.0, WARN_TEMP));
}

int main(int argc, char **argv)
{
	char host[256] = "?";
	int i;

	gethostname(host, sizeof host);
	printf("== %s ==\n", host);
	show_load_uptime();
	show_cpu();
	show_mem();
	if (argc < 2)
		show_disk("/");
	for (i = 1; i < argc; i++)
		show_disk(argv[i]);
	show_temps();
	return status;
}
