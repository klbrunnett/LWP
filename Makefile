CC 	= gcc

CFLAGS  = -Wall -g -I .

LD 	= gcc

LDFLAGS  = -Wall -g -L/home/pn-cs453/Given/Asgn2

PUBFILES =  README  hungrymain.c  libPLN.a  libsnakes.a  lwp.h\
	    numbersmain.c  snakemain.c  snakes.h

TARGET =  pn-cs453@hornet:Given/asgn2

PROGS	= snakes nums hungry

SNAKEOBJS  = snakemain.o 

HUNGRYOBJS = hungrymain.o 

NUMOBJS    = numbersmain.o

OBJS	= $(SNAKEOBJS) $(HUNGRYOBJS) $(NUMOBJS) 

SRCS	= snakemain.c numbersmain.c

HDRS	= 

EXTRACLEAN = core $(PROGS)

all: 	$(PROGS)

allclean: clean
	@rm -f $(EXTRACLEAN)

clean:	
	rm -f $(OBJS) *~ TAGS liblwp.a liblwp.so

snakes: snakemain.o liblwp.a libsnakes.a
	$(LD) $(LDFLAGS) -fPIC -o snakes snakemain.o -L. -lncurses -lsnakes -llwp magic64.S

hungry: hungrymain.o liblwp.a libsnakes.a
	$(LD) $(LDFLAGS) -fPIC -o hungry hungrymain.o -L. -lncurses -lsnakes -llwp magic64.S

nums: numbersmain.o liblwp.a 
	$(LD) $(LDFLAGS) -fPIC -o nums numbersmain.o -L. -llwp magic64.S

hungrymain.o: lwp.h snakes.h

snakemain.o: lwp.h snakes.h

numbermain.o: lwp.h

liblwp.a: lwp.c
	gcc -fPIC -c lwp.c
	ar r liblwp.a lwp.o
	rm lwp.o

pub:
	scp $(PUBFILES) $(TARGET)

liblwp.so:	lwp.o
	$(CC) $(CFLAGS) -fPIC -shared -o $@ lwp.o

lwp.o: lwp.c lwp.h
	$(CC) $(CFLAGS) -fPIC -c -o lwp.o lwp.c
