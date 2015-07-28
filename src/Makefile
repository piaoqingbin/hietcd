
CC:=$(shell sh -c 'type $(CC) >/dev/null 2>/dev/null && echo $(CC) || echo gcc')
STD=-std=gnu99 -pedantic
WARN=-Wall -W
OPT=-O2
DEBUG=-g
INSTALL?= cp -a

PROJECT=hietcd
PREFIX?=/usr/local
INCLUDE_PATH?=include/$(PROJECT)
LIBRARY_PATH?=lib
INSTALL_INCLUDE_PATH=$(PREFIX)/$(INCLUDE_PATH)
INSTALL_LIBRARY_PATH=$(PREFIX)/$(LIBRARY_PATH)

LIBNAME=lib$(PROJECT)
DLIBNAME=$(LIBNAME).so
MAJOR_VERSION=$(shell grep HIETCD_MAJOR_VERSION hietcd.h | awk '{print $$3}')
DLIB_MAJOR_NAME=$(DLIBNAME).$(MAJOR_VERSION)
VERSION=$(shell grep HIETCD_VERSION hietcd.h | awk '{print $$3}')
DLIB_FULL_NAME=$(DLIBNAME).$(VERSION)
SLIBNAME=$(LIBNAME).a

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
sev.o: sev.c sev.h sev_impl.c

.c.o:
	$(CC) $(STD) $(OPT) $(WARN) $(DEBUG) $(HIETCD_DEF) -fPIC -c $<

clean:
	rm -rf $(DLIBNAME) $(SLIBNAME) $(OBJECTS)

install: $(DLIBNAME) $(SLIBNAME)
	mkdir -p $(INSTALL_INCLUDE_PATH) $(INSTALL_LIBRARY_PATH)
	$(INSTALL) *.h $(INSTALL_INCLUDE_PATH)
	$(INSTALL) $(DLIBNAME) $(INSTALL_LIBRARY_PATH)/$(DLIB_FULL_NAME)
	cd $(INSTALL_LIBRARY_PATH) && ln -sf $(DLIB_FULL_NAME) $(DLIB_MAJOR_NAME)
	cd $(INSTALL_LIBRARY_PATH) && ln -sf $(DLIB_MAJOR_NAME) $(DLIBNAME)
	$(INSTALL) $(SLIBNAME) $(INSTALL_LIBRARY_PATH)

dep:
	$(CC) -MM *.c

.PHONY: all clean install dep
