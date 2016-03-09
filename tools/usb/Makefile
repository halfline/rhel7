# Makefile for USB tools

CC = $(CROSS_COMPILE)gcc
PTHREAD_LIBS = -lpthread
WARNINGS = -Wall -Wextra
CFLAGS = $(WARNINGS) -g $(PTHREAD_LIBS) -I../include

all: testusb ffs-test ffs-test-legacy

ffs-test-legacy: ffs-test.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) -DUSE_LEGACY_DESC_HEAD

%: %.c
	$(CC) $(CFLAGS) -o $@ $^

clean:
	$(RM) testusb ffs-test
