CC = gcc
CFLAGS = -Wall -Wextra -std=c99

all: poker

poker: poker.c
	$(CC) $(CFLAGS) -o poker poker.c

clean:
	rm -f poker
