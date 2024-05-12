CC := gcc
CFLAGS := -g -Wall

TARGETS := fatmod

# Make sure that 'all' is the first target
all: $(TARGETS)

fatmod: fatmod.c
	$(CC) $(CFLAGS) -o fatmod fatmod.c -lrt -lpthread

clean:
	rm -f *~ fatmod
