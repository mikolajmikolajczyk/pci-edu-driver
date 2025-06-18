# user/Makefile
CC=gcc
CFLAGS=-Wall -O2

all: edu_cli

edu_cli: edu_cli.c
	$(CC) $(CFLAGS) -static -o edu_cli edu_cli.c

clean:
	rm -f edu_cli
