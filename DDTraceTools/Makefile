TOP := $(shell echo $${PWD-`pwd`})
PERFGRAPH := $(TOP)/..
LINK_MAGIC:=-Wl,-rpath,$(PERFGRAPH)

ifeq "$(shell ../VersionCheck.py)" ""
CFLAG += -std=c++0x
else
CFLAG += -std=c++11
endif

CPP=g++
#CPP=clang++ -ferror-limit=2

EventParser: EventParser.cc ../libddtrace.so Makefile
	$(CPP) $(CFLAG) -g -o $@ -L.. -I..  $< -lddtrace ${LINK_MAGIC}

clean:
	rm -f EventParser Build.err
