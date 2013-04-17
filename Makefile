CC = gcc
CFLAGS = -Wall -O3
#DFLAGS = -DFREEBSD
DFLAGS = -DUSESYSLOG
OBJS = misc.o

all:
	$(CC) $(CFLAGS) $(DFLAGS) -c misc.c
	$(CC) $(CFLAGS) $(DFLAGS) -o udpbalancer udpbalancer.c $(OBJS)

clean:
	rm -f *.o udpbalancer
