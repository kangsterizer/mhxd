CC = gcc
CFLAGS = -g -O2 -Wall -W
DEFS = -DHAVE_CONFIG_H
INCLUDES = -I../src/common -I../src/conf -I..
LD = gcc
LDFLAGS =

TARGETS = stress tdb tlist treg tspoof unprox

all: $(TARGETS)

stress_OBJS = stress.o
stress: $(stress_OBJS)
	$(LD) $(LDFLAGS) -o $@ $(stress_OBJS)

tdb_OBJS = tdb.o
tdb: $(tdb_OBJS)
	$(LD) $(LDFLAGS) -o $@ $(tdb_OBJS)

tlist_OBJS = tlist.o
tlist: $(tlist_OBJS)
	$(LD) $(LDFLAGS) -o $@ $(tlist_OBJS)

treg_OBJS = treg.o
treg: $(treg_OBJS)
	$(LD) $(LDFLAGS) -o $@ $(treg_OBJS)

tspoof_OBJS = tspoof.o libtreg.o
tspoof: $(tspoof_OBJS)
	$(LD) $(LDFLAGS) -o $@ $(tspoof_OBJS)

unprox_OBJS = unprox.o libtreg.o
unprox: $(unprox_OBJS)
	$(LD) $(LDFLAGS) -o $@ $(unprox_OBJS)

clean:
	rm -f *.o $(TARGETS)

.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) $(DEFS) -c $<
