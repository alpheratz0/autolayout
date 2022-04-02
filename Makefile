VERSION = 0.1.0
PREFIX = /usr/local
MANPREFIX = ${PREFIX}/share/man
LDLIBS = -ljson-c
LDFLAGS = -s ${LDLIBS}
INCS = -I. -I/usr/include
CFLAGS = -pedantic -Wall -Wextra -Os ${INCS} -DVERSION=\"${VERSION}\"
CC = cc

SRC = src/autolayout.c \
	  src/i3.c \
	  src/debug.c

OBJ = ${SRC:.c=.o}

all: autolayout

${OBJ}: src/i3.h \
		src/debug.h \
		src/numdef.h

autolayout: ${OBJ}
	@${CC} -o $@ ${OBJ} ${LDFLAGS}

install: all
	@mkdir -p ${DESTDIR}${PREFIX}/bin
	@cp -f autolayout ${DESTDIR}${PREFIX}/bin
	@chmod 755 ${DESTDIR}${PREFIX}/bin/autolayout
	@mkdir -p ${DESTDIR}${MANPREFIX}/man1
	@cp -f man/autolayout.1 ${DESTDIR}${MANPREFIX}/man1
	@chmod 644 ${DESTDIR}${MANPREFIX}/man1/autolayout.1

uninstall:
	@rm -f ${DESTDIR}${PREFIX}/bin/autolayout
	@rm -f ${DESTDIR}${MANPREFIX}/man1/autolayout.1

clean:
	@rm -f autolayout ${OBJ}


.PHONY: all clean install uninstall
