CFLAG = -g -std=gnu99 -Wall -Wno-unused

CC= gcc
INCLUDE = cpu9816.h hp9816emu.h common.h kml.h mops.h ops.h
SRC     = hp9816emu.c cpu9816.c display.c display16.c\
	  fetch.c files.c hp-2225.c hp-7908.c hp-9121.c hp-9122.c \
          hp-98620.c hp-98626.c hp-98635.c hp-ib.c keyboard.c kml.c mops.c \
          opcodes.c
OBJECTS = hp9816emu.o cpu9816.o display.o display16.o\
	  fetch.o files.o hp-2225.o hp-7908.o hp-9121.o hp-9122.o \
          hp-98620.o hp-98626.o hp-98635.o hp-ib.o keyboard.o kml.o mops.o \
          opcodes.o

LIBS = -L/usr/X11R6/lib -lEZ -lXext -LX11 -lSM -lICE -lX11 \
	-ljpeg -lpng -ltiff -lz -lm -lpthread

PROGRAMS = hp9816emu
SHELL   = /bin/bash

all: $(PROGRAMS) TAGS

strip:
	strip $(PROGRAMS)

clean:
	$(RM) $(PROGRAMS) $(OBJECTS) core *.gcda *.gcno *~

TAGS:	$(SRC) $(INCLUDE)
	etags -I $(SRC) $(INCLUDE)

hp9816emu: $(OBJECTS) Makefile
	   $(CC) $(CFLAG) -o hp9816emu $(OBJECTS)  $(LIBS)

$(OBJECTS):	$(INC)   #force recompile of all .o's

.c.o:
	$(CC) $(CFLAG) $(OPTS) -c $< 

.o:
	$(CC) $< $(LIBS) -o $@

.c:
	$(CC) $(CFLAG)  $< -o $@ $(LIBS)

