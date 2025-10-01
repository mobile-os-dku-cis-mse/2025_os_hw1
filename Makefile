# Makefile for lsh (patch_5.c single-file build)

# Toolchain
CC      ?= gcc
TARGET  ?= lsh
SRC     := my_shell.c

# Default flags (tweak as you like)
CSTD    ?= -std=gnu11
WARN    ?= -Wall -Wextra -Wformat=2 -Wshadow -Wpointer-arith -Wcast-qual -Wvla -Wstrict-prototypes
OPT     ?= -O2
DBG     ?= -g
DEFS    ?= -D_GNU_SOURCE -D_XOPEN_SOURCE=700

# Linux-only headers/APIs used: epoll, pidfd, termios, pthread
CFLAGS  ?= $(CSTD) $(WARN) $(OPT) $(DBG) $(DEFS)
LDFLAGS ?=
LDLIBS  ?= -pthread

# If you compile on older libcs/kernels, pidfd_open may be unavailable at runtime.
# The code already falls back when SYS_pidfd_open is missing, so no extra define needed.

.PHONY: all debug release asan ubsan clean run

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LDLIBS)

# Debug build (no optimizations, more symbols)
debug: CFLAGS := $(CSTD) $(WARN) -O0 -g3 $(DEFS)
debug: LDFLAGS :=
debug: $(TARGET)

# Release build (smaller, faster)
release: CFLAGS := $(CSTD) $(WARN) -O3 -DNDEBUG $(DEFS)
release: LDFLAGS :=
release: $(TARGET)

# AddressSanitizer (good for catching use-after-free, OOB, etc.)
asan: CFLAGS := $(CSTD) $(WARN) -O1 -g3 -fsanitize=address -fno-omit-frame-pointer $(DEFS)
asan: LDFLAGS := -fsanitize=address
asan: $(TARGET)

# UndefinedBehaviorSanitizer
ubsan: CFLAGS := $(CSTD) $(WARN) -O1 -g3 -fsanitize=undefined -fno-omit-frame-pointer $(DEFS)
ubsan: LDFLAGS := -fsanitize=undefined
ubsan: $(TARGET)

run: $(TARGET)
	./$(TARGET)

clean:
	$(RM) $(TARGET)
