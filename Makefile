INCLUDE_DIRS =
LIB_DIRS =
CC=g++

CDEFS= -DDEBUG_MODE_ON
CFLAGS= -O0 -pg -g $(INCLUDE_DIRS) $(CDEFS)
LIBS= -lpthread -lrt
CPPLIBS= -L/usr/lib -lopencv_core -lopencv_flann -lopencv_video

HFILES= utilities.h capture.hpp v4l2_capture.h
CFILES= main.c utilities.c v4l2_capture.c
CPPFILES= capture.cpp

SRCS= ${HFILES} ${CFILES}
CPPOBJS=

all:	main

clean:
	-rm -f *.o *.d
	-rm -f main

distclean:
	-rm -f *.o *.d

main: main.o utilities.o capture.o v4l2_capture.o
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $@.o utilities.o capture.o v4l2_capture.o `pkg-config --libs opencv` $(CPPLIBS) $(LIBS)

depend:

.c.o:
	$(CC) $(CFLAGS) -c $<

.cpp.o:
	$(CC) $(CFLAGS) -c $<
