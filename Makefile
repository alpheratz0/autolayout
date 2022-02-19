PREFIX = /usr/local
MANPREFIX = ${PREFIX}/share/man
INCS = -I. -I/usr/include
CFLAGS = -pedantic -Wall -Os ${INCS}
CC = cc

SRC = autolayout.c
OBJ = ${SRC:.c=.o}

all: autolayout

autolayout: ${OBJ}
	@${CC} -o $@ ${OBJ}

install: all
	@mkdir -p ${DESTDIR}${PREFIX}/bin
	@cp -f autolayout ${DESTDIR}${PREFIX}/bin
	@chmod 755 ${DESTDIR}${PREFIX}/bin/autolayout
	@mkdir -p ${DESTDIR}${MANPREFIX}/man1
	@cp -f autolayout.1 ${DESTDIR}${MANPREFIX}/man1
	@chmod 644 ${DESTDIR}${MANPREFIX}/man1/autolayout.1

uninstall:
	@rm -f ${DESTDIR}${PREFIX}/bin/autolayout

clean:
	@rm -f autolayout ${OBJ}


.PHONY: all clean install uninstall
