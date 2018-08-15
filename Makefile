INCLUDE_DIRS =
LIB_DIRS =
CC=g++

CDEFS= -DTIME_ANALYSIS -DDEBUG_MODE_ON
CFLAGS= -O0 -pg -g $(INCLUDE_DIRS) $(CDEFS)
LIBS= -lpthread -lrt
CPPLIBS= -L/usr/lib -lopencv_core -lopencv_flann -lopencv_video

HFILES= capture.hpp posix_timer.h utilities.h
CFILES= main.c posix_timer.c utilities.c
CPPFILES= capture.cpp

SRCS= ${HFILES} ${CFILES}
CPPOBJS=

all:	main

clean:
	-rm -f *.o *.d
	-rm -f main

distclean:
	-rm -f *.o *.d

main: main.o capture.o posix_timer.o utilities.o
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $@.o capture.o posix_timer.o utilities.o `pkg-config --libs opencv` $(CPPLIBS) $(LIBS)

depend:

.c.o:
	$(CC) $(CFLAGS) -c $<

.cpp.o:
	$(CC) $(CFLAGS) -c $<
