CC            = gcc
CFLAGS        = -O4 -Wall -I/usr/local/include
DEST          = /usr/bin
LDFLAGS       = -L/usr/local/lib
LIBS          = -pthread -lwiringPi -lm
OBJS          = rasdiation.c
PROGRAM       = rasdiation

all:            $(PROGRAM)

$(PROGRAM):     $(OBJS)
		$(CC) $(OBJS) $(LDFLAGS) $(LIBS) -o $(PROGRAM)

clean:
		rm -f *.o *~ $(PROGRAM)

install:        $(PROGRAM)
		install -s $(PROGRAM) $(DEST)
