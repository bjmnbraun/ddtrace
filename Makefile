#
# This makefile system follows the structuring conventions
# recommended sby Peter Miller in his excellent paper:
#
#	Recursive Make Considered Harmful
#	http://aegis.sourceforge.net/auug97.pdf
#
TOP = .

CC	:= gcc -pipe
CPP	:= g++ -pipe

#CC	:= clang -pipe
#CPP	:= clang++ -pipe

PERL	:= perl

# Compiler flags
CFLAG := $(CFLAG) -I$(TOP) -MD
CFLAG += -g -Wall -Wno-unused -Wpointer-arith
CFLAG += -O3 -msse4.1 
CFLAG += -pthread
#CFLAG += -ferror-limit=2

#Set to 0 to disable verbose apps
#CFLAG += -DVERBOSE=1
CFLAG += -DVERBOSE=0

ifeq "$(shell ./VersionCheck.py)" ""
	CFLAG += -std=c++0x 
else
	CFLAG += -std=c++11
endif

LDFLAGS := -L/usr/local/lib 
LDFLAGS += -lstdc++ 
LDFLAGS += -pthread

# Make sure that 'all' is the first target
all:

# Eliminate default suffix rules
.SUFFIXES:

# Delete target files if there is an error (or make is interrupted)
.DELETE_ON_ERROR:

# Set to nothing (i.e., V = ) to enable verbose outputs.
V = @

# How to build libddtrace.so 
libddtrace.so: DDTrace/Cycles.o DDTrace.o DDTrace/Util.o
	$(CPP) $(CFLAG) $(LDFLAG) -shared  -o $@ $+ 

%.o : %.cc %.h
	$(CPP) $(CFLAG) -fPIC -shared -o $@ -c $<

clean:
	rm -f *.o *.so  *.gch *.err *.d .deps
	cd DDTrace; rm -f *.o *.so  *.gch *.err *.d .deps

prefix=/usr/local
install: libddtrace.so
	rsync -rI --chmod=D755,F755 libddtrace.so $(prefix)/lib/
	rsync -rI --chmod=D755,F644 DDTrace.h $(prefix)/include/
	rsync -rI --chmod=D755,F644 DDTrace $(prefix)/include/
	rsync -rI --chmod=D755,F644 DDTraceConfig $(prefix)/include/
	ldconfig

clean-install:
	rm -f $(prefix)/lib/libddtrace.so
	rm -f $(prefix)/include/DDTrace.h
	rm -rf $(prefix)/include/DDTrace
	rm -rf $(prefix)/include/DDTraceConfig

all: libddtrace.so

# This magic automatically generates makefile dependencies
# for header files included from C source files we compile,
# and keeps those dependencies up-to-date every time we recompile.
# See 'mergedep.pl' for more information.
.deps: $(wildcard *.d)
	@mkdir -p $(@D)
	@$(PERL) mergedep.pl $@ $^

-include .deps

always: 
	@:

.PHONY: always
