#
# This makefile system follows the structuring conventions
# recommended sby Peter Miller in his excellent paper:
#
#	Recursive Make Considered Harmful
#	http://aegis.sourceforge.net/auug97.pdf
#
BINDIR := bin

#Lists all the makefrags add to
SRCDIRS :=

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

ifeq "$(shell ../VersionCheck.py)" ""
CFLAG += -std=c++0x
else
CFLAG += -std=c++11
endif

LDFLAGS := -L/usr/local/lib 
LDFLAGS += -lstdc++ 
LDFLAGS += -pthread

#DDTrace library
TOP := $(shell echo $${PWD-`pwd`})
PERFGRAPH := $(TOP)/..
CFLAG += -I$(PERFGRAPH)
LDFLAGS += -L$(PERFGRAPH)
LINK_MAGIC=-Wl,-rpath,$(PERFGRAPH)
LDFLAGS +=  $(LINK_MAGIC)

LDFLAGS += -lddtrace

# Make sure that 'all' is the first target
all:

# Eliminate default suffix rules
.SUFFIXES:

# Delete target files if there is an error (or make is interrupted)
.DELETE_ON_ERROR:

# Set to nothing (i.e., V = ) to enable verbose outputs.
#V = @
V = 

# Include Makefrags for subdirectories

include src/Makefrag

# How to build C++ files
APPS_OBJFILES_CPP := $(patsubst %.cc, $(BINDIR)/%.o, $(APPS_CPPFILES))
$(APPS_OBJFILES_CPP) $(COMMON_OBJFILES) : $(BINDIR)/%.o : %.cc 
	@echo + cpp $<
	@mkdir -p $(@D)
	$(V)$(CPP) $(CFLAG) -c -o $@ $<

#Linker has the same args regardless of language of app
APPS_BINS += $(patsubst $(BINDIR)/%.o, $(BINDIR)/%, $(APPS_OBJFILES_CPP))
$(APPS_BINS) : $(BINDIR)/% : $(BINDIR)/%.o $(COMMON_OBJFILES)
	@mkdir -p $(@D)
	@echo + mk $@
	$(V)$(CPP) -o $@ $^ $(LDFLAGS)

all: $(APPS_BINS)

# For deleting the build
clean:
	rm -rf $(BINDIR) 0x*

# This magic automatically generates makefile dependencies
# for header files included from C source files we compile,
# and keeps those dependencies up-to-date every time we recompile.
# See 'mergedep.pl' for more information.
$(BINDIR)/.deps: $(foreach dir, $(SRCDIRS), $(wildcard $(BINDIR)/$(dir)/*.d))
	@mkdir -p $(@D)
	@$(PERL) ../mergedep.pl $@ $^

-include $(BINDIR)/.deps

always: 
	@:

.PHONY: always
