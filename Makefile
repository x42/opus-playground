PREFIX ?= /usr/local
bindir = $(PREFIX)/bin
mandir = $(PREFIX)/man

VERSION=0.2.1
OPTIMIZATIONS ?= -msse -msse2 -mfpmath=sse -ffast-math -fomit-frame-pointer -O3 -fno-finite-math-only

ifeq ($(shell pkg-config --exists opus sndfile || echo no), no)
  $(error "libopus and libsndfile are required")
endif

CFLAGS+=$(OPTIMIZATIONS) -Wall
CFLAGS+= -DVERSION="\"$(VERSION)\""
CFLAGS+=`pkg-config --cflags opus sndfile`
LOADLIBES=`pkg-config --libs opus sndfile` -lm

ifeq ($(shell test -f `pkg-config --variable=includedir opus`/opus/opus_custom.h && echo yes), yes)
  CFLAGS+=-DHAVE_OPUS_CUSTOM
else
  $(warning "building w/o opus-custom support")
endif

all: opus-playground

man: opus-playground.1

opus-playground: opus-playground.c

clean:
	rm -f opus-playground

opus-playground.1: opus-playground
	help2man -N -n 'Opus Torture Test' -o opus-playground.1 ./opus-playground

install: opus-playground opus-playground.1
	install -d $(DESTDIR)$(bindir)
	install -d $(DESTDIR)$(mandir)/man1
	install -m755 opus-playground $(DESTDIR)$(bindir)
	install -m644 opus-playground.1 $(DESTDIR)$(mandir)/man1

uninstall:
	rm -f $(DESTDIR)$(bindir)/opus-playground
	rm -f $(DESTDIR)$(mandir)/man1/opus-playground
	-rmdir $(DESTDIR)$(bindir)
	-rmdir $(DESTDIR)$(mandir)


.PHONY: clean all install uninstall
