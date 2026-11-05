# serverhealth

A small C tool that prints a quick health check of a Linux box: load, uptime,
CPU, memory, disk and temperatures. One file, no dependencies.

```
== myserver ==
load     0.02 0.12 0.15
uptime   7d 3h 16m
cpu        0.0% busy (4 cores)
mem        7.5% used (847 / 11352 MiB)
swap       0.0% used (0 / 4096 MiB)
disk     /:   2.1% used (423.4 GiB free of 456.3)
temp     x86_pkg_temp (zone2): 48.0 C
```

## Get it

Grab the x86-64 binary from the [releases](../../releases/latest), or build it:

```
gcc -Os -s -o health health.c
```

## Use

```
./health [--json] [path ...]
```

Pass mount points to check disks (default `/`). `--json` prints one line of
JSON instead. Exit code is `0` fine, `1` something crossed a threshold
(90% CPU/mem/disk, 80 C, editable at the top of `health.c`), `2` a read failed.

Linux only (yet)

MIT license.
