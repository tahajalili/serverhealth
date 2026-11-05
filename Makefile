CC     ?= cc
CFLAGS ?= -Os -s -Wall -Wextra -std=c11

health: health.c
	$(CC) $(CFLAGS) -o $@ $<

# self-contained x86-64 Linux binary for releases
static: health.c
	$(CC) $(CFLAGS) -static -o health-linux-x86_64 $<
	sha256sum health-linux-x86_64 > health-linux-x86_64.sha256
