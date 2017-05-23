CC=gcc
CFLAGS=-I. -I/usr/local/include

default: build kk_ihex_read.o

ihex:
	git clone https://github.com/scr34m/ihex

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

kk_ihex_read.o: ihex/kk_ihex_read.c
	$(CC) -c -o $@ $< $(CFLAGS)

build: ihex prog.o kk_ihex_read.o
	gcc -o prog prog.o kk_ihex_read.o -I. -L/usr/local/lib -l usb-1.0

.PHONY: build kk_ihex_read.o