
CC:=$(shell sh -c 'type $(CC) >/dev/null 2>/dev/null && echo $(CC) || echo gcc')
STD= -std=c99 -pedantic
WARN= -Wall -W
OPT= -O2
DEBUG= -g

PROJECT=hietcd
PREFIX?=/usr/local
INCLUDE_PATH?=include/$(PROJECT)
LIBRARY_PATH?=lib
INSTALL_INCLUDE_PATH=$(DESTDIR)$(PREFIX)/$(INCLUDE_PATH)
INSTALL_LIBRARY_PATH=$(DESTDIR)$(PREFIX)/$(LIBRARY_PATH)

LIBNAME=lib$(PROJECT)
VERSION=$(shell grep HIETCD_VERSION hietcd.h | awk '{print $$3}')
DLIBNAME=$(LIBNAME).so.$(VERSION)
SLIBNAME=$(LIBNAME).a.$(VERSION)

HIETCD_DEF=
uname_s=$(shell sh -c 'uname -s 2>/dev/null || echo not')
ifeq ($(uname_s),Linux) 	
	HIETCD_DEF+=-DHAVE_EPOLL
endif
HIETCD_DCFLGS=$(STD) $(OPT) $(WARN) $(DEBUG) -fPIC -shared $(CFLAGS)
HIETCD_LDFLGS=-lpthread -lcurl -lyajl

OBJECTS=log.o sev.o request.o response.o io.o hietcd.o

all: $(DLIBNAME) $(SLIBNAME)

$(DLIBNAME): $(OBJECTS)
	$(CC) -o $@ $(HIETCD_DCFLGS) $(HIETCD_LDFLGS) $^

$(SLIBNAME): $(OBJECTS)
	ar rcs $@ $^

hietcd.o: hietcd.c hietcd.h io.h sev.h request.h response.h log.h
io.o: io.c sev.h log.h io.h request.h hietcd.h response.h
log.o: log.c log.h
request.o: request.c request.h
response.o: response.c hietcd.h io.h sev.h request.h response.h
sev.o: $(HIETCD_DEF) sev.c sev.h sev_impl.c

.c.o:
	$(CC) $(STD) $(OPT) $(WARN) $(DEBUG) -c -fPIC $(CFLAGS) $<

clean:
	rm -rf $(DLIBNAME) $(SLIBNAME) $(OBJECTS)

install:

dep:
	$(CC) -MM *.c

.PHONY: all clean install dep
