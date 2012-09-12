PREFIX ?= /usr/local
bindir = $(PREFIX)/bin
mandir = $(PREFIX)/man

VERSION=0.1.0
OPTIMIZATIONS ?= -msse -msse2 -mfpmath=sse -ffast-math -fomit-frame-pointer -O3 -fno-finite-math-only

ifeq ($(shell pkg-config --exists opus sndfile || echo no), no)
  $(error "libopus and libsndfile are required")
endif

CFLAGS+=$(OPTIMIZATIONS) -Wall
CFLAGS+= -DVERSION="\"$(VERSION)\""
CFLAGS+=`pkg-config --cflags opus sndfile`
LOADLIBES=`pkg-config --libs opus sndfile` -lm

ifeq ($(shell test -f /usr/include/opus/opus_custom.h || echo no), no)
	CFLAGS+=-DHAVE_OPUS_CUSTOM
endif

all: opus-torture

man: opus-torture.1

opus-torture: opus-torture.c

clean:
	rm -f opus-torture

opus-torture.1: opus-torture
	help2man -N -n 'Opus Torture Test' -o opus-torture.1 ./opus-torture

install: opus-torture opus-torture.1
	install -d $(DESTDIR)$(bindir)
	install -d $(DESTDIR)$(mandir)/man1
	install -m755 opus-torture $(DESTDIR)$(bindir)
	install -m644 opus-torture.1 $(DESTDIR)$(mandir)/man1

uninstall:
	rm -f $(DESTDIR)$(bindir)/opus-torture
	rm -f $(DESTDIR)$(mandir)/man1/opus-torture
	-rmdir $(DESTDIR)$(bindir)
	-rmdir $(DESTDIR)$(mandir)


.PHONY: clean all install uninstall
