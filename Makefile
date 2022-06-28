CC ?= gcc
AR ?= ar
RANLIB ?= ranlib
CFLAGS ?= -g -Wall -O3 -Wsign-compare
LIBS = -lm -lz
EXTRA_CFLAGS_PIC = -fpic
LDFLAGS =
LDLIBS =
INCLUDES = 

# Create a simple test-program to check if gcc can compile with curl
tmpfile:=$(shell mktemp --suffix=.c)
$(file >$(tmpfile),#include <curl/curl.h>)
$(file >>$(tmpfile),int main() { return 0; })
HAVE_CURL:=$(shell $(CC) $(CFLAGS) $(EXTRA_CFLAGS_PIC) $(LIBS) -lcurl $(tmpfile) -o /dev/null >/dev/null 2>&1 && echo "YES")
$(shell rm $(tmpfile))

ifeq ($(HAVE_CURL),YES)
	# If yes, add the library
	LIBS += -lcurl
else
	# and if not, disable CURL specific code compilation
	CFLAGS += -DNOCURL
endif


prefix = /usr/local
includedir = $(prefix)/include
libdir = $(exec_prefix)/lib

.PHONY: all clean lib test doc

.SUFFIXES: .c .o .pico

all: lib

lib: lib-static lib-shared

lib-static: libBigWig.a

lib-shared: libBigWig.so

doc:
	doxygen

OBJS = io.o bwValues.o bwRead.o bwStats.o bwWrite.o

.c.o:
	$(CC) -I. $(CFLAGS) $(INCLUDES) -c -o $@ $<

.c.pico:
	$(CC) -I. $(CFLAGS) $(INCLUDES) $(EXTRA_CFLAGS_PIC) -c -o $@ $<

libBigWig.a: $(OBJS)
	-@rm -f $@
	$(AR) -rcs $@ $(OBJS)
	$(RANLIB) $@

libBigWig.so: $(OBJS:.o=.pico)
	$(CC) -shared $(LDFLAGS) -o $@ $(OBJS:.o=.pico) $(LDLIBS) $(LIBS)

test/testLocal: libBigWig.a
	$(CC) -o $@ -I. $(CFLAGS) test/testLocal.c libBigWig.a $(LIBS)

test/testRemoteManyContigs: libBigWig.a
	$(CC) -o $@ -I. $(CFLAGS) test/testRemoteManyContigs.c libBigWig.a $(LIBS)

test/testRemote: libBigWig.a
	$(CC) -o $@ -I. $(CFLAGS) test/testRemote.c libBigWig.a $(LIBS)

test/testWrite: libBigWig.a
	$(CC) -o $@ -I. $(CFLAGS) test/testWrite.c libBigWig.a $(LIBS)

test/exampleWrite: libBigWig.so
	$(CC) -o $@ -I. -L. $(CFLAGS) test/exampleWrite.c libBigWig.a $(LIBS)

test/testBigBed: libBigWig.a
	$(CC) -o $@ -I. $(CFLAGS) test/testBigBed.c libBigWig.a $(LIBS)

test/testIterator: libBigWig.a
	$(CC) -o $@ -I. $(CFLAGS) test/testIterator.c libBigWig.a $(LIBS)

test: test/testLocal test/testRemote test/testWrite test/testLocal test/exampleWrite test/testRemoteManyContigs test/testBigBed test/testIterator
	./test/test.py test test/test.bw

clean:
	rm -f *.o libBigWig.a libBigWig.so *.pico test/testLocal test/testRemote test/testWrite test/exampleWrite test/testRemoteManyContigs test/testBigBed test/testIterator example_output.bw

install: libBigWig.a libBigWig.so
	install -d $(prefix)/lib $(prefix)/include
	install libBigWig.a $(prefix)/lib
	install libBigWig.so $(prefix)/lib
	install *.h $(prefix)/include
