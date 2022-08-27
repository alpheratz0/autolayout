.POSIX:
.PHONY: all clean install uninstall dist

VERSION = 0.1.0

CC      = cc
CFLAGS  = -std=c99 -pedantic -Wall -Wextra -Os -DVERSION=\"$(VERSION)\"
LDLIBS  = -ljson-c
LDFLAGS = -s

PREFIX    = /usr/local
MANPREFIX = $(PREFIX)/share/man

SRC = src/autolayout.c src/i3.c src/debug.c
OBJ = src/autolayout.o src/i3.o src/debug.o

all: autolayout

autolayout: $(OBJ)
	$(CC) $(LDFLAGS) -o autolayout $(OBJ) $(LDLIBS)

clean:
	rm -f autolayout autolayout.o autolayout-$(VERSION).tar.gz $(OBJ)

install: all
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp -f autolayout $(DESTDIR)$(PREFIX)/bin
	chmod 755 $(DESTDIR)$(PREFIX)/bin/autolayout
	mkdir -p $(DESTDIR)$(MANPREFIX)/man1
	cp -f man/autolayout.1 $(DESTDIR)$(MANPREFIX)/man1
	chmod 644 $(DESTDIR)$(MANPREFIX)/man1/autolayout.1

dist: clean
	mkdir -p autolayout-$(VERSION)
	cp -R COPYING Makefile README src man autolayout-$(VERSION)
	tar -cf autolayout-$(VERSION).tar autolayout-$(VERSION)
	gzip autolayout-$(VERSION).tar
	rm -rf autolayout-$(VERSION)

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/autolayout
	rm -f $(DESTDIR)$(MANPREFIX)/man1/autolayout.1
