SHELL = /bin/sh

CC 	= gcc

LD 	= gcc

CFLAGS 	= -g -Wall -I$(LIBDIR)

LDFLAGS = -g -Wall -L$(LIBDIR) -llwp

PROG = 27_tid2thread_bogus

OBJS 	= numbersmain.o Random.o

REFFILE	= $(PROG).expected

LIBDIR 	= ../../lib

all:	$(REFFILE)

prog: $(PROG)

$(PROG): $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $^

$(REFFILE): $(PROG) 
	@echo -n Regenerating $@...
	@LD_LIBRARY_PATH=$(LIBDIR):$$LD_LIBRARY_PATH;\
	export LD_LIBRARY_PATH;\
	$(PROG) > $(REFFILE)
	@echo done.

test:	$(PROG)
	@LD_LIBRARY_PATH=$(LIBDIR):$$LD_LIBRARY_PATH;\
	export LD_LIBRARY_PATH;\
	$(PROG) 

clean:
	-rm -f $(OBJS) *~

allclean: clean
	-rm -f $(PROG) $(REFFILE)
