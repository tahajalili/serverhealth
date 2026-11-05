# serverhealth

A tiny, dependency-free C tool that prints a health snapshot of a Linux server:
load, uptime, CPU, memory, swap, disk and temperatures. One source file, no
heap allocations, no libraries beyond libc.

```
== myserver ==
load     0.02 0.12 0.15
uptime   7d 3h 16m
cpu        0.0% busy (4 cores)
mem        7.5% used (847 / 11352 MiB)
swap       0.0% used (0 / 4096 MiB)
disk     /:   2.1% used (423.4 GiB free of 456.3)
temp     acpitz (zone0): 27.8 C
temp     acpitz (zone1): 29.8 C
temp     x86_pkg_temp (zone2): 48.0 C
```

## Install

### Prebuilt binary (x86-64 Linux, no compiler needed)

Download `health-linux-x86_64` and its `.sha256` from the
[latest release](../../releases/latest), then:

```
sha256sum -c health-linux-x86_64.sha256
chmod +x health-linux-x86_64
./health-linux-x86_64
```

### Build from source

```
gcc -Os -s -o health health.c
```

or `make`. Any C11 compiler works (`tcc -o health health.c` too).

## Usage

```
./health [path ...]
```

Paths are mounts to check disk usage on (default `/`).

Exit code: `0` ok, `1` a threshold was crossed (line marked `!!`), `2` a read
failed. That makes it easy to use from cron or a systemd timer.

Thresholds (90% CPU/memory/disk, 80 C) are `#define`s at the top of `health.c`.

## Platform

Linux only (reads `/proc`, `/sys/class/thermal`). It compiles on other Unixes
but reports most metrics as unavailable.

## License

MIT
