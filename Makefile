PREFIX = /usr/local
MANPREFIX = ${PREFIX}/share/man
LDLIBS = -ljson-c
LDFLAGS = -s ${LDLIBS}
INCS = -I. -I/usr/include
CFLAGS = -pedantic -Wall -Wextra -Os ${INCS}
CC = cc

SRC = autolayout.c \
	  i3.c \
	  debug.c

OBJ = ${SRC:.c=.o}

all: autolayout

${OBJ}: i3.h debug.h numdef.h

autolayout: ${OBJ}
	@${CC} -o $@ ${OBJ} ${LDFLAGS}

install: all
	@mkdir -p ${DESTDIR}${PREFIX}/bin
	@cp -f autolayout ${DESTDIR}${PREFIX}/bin
	@chmod 755 ${DESTDIR}${PREFIX}/bin/autolayout
	@mkdir -p ${DESTDIR}${MANPREFIX}/man1
	@cp -f autolayout.1 ${DESTDIR}${MANPREFIX}/man1
	@chmod 644 ${DESTDIR}${MANPREFIX}/man1/autolayout.1

uninstall:
	@rm -f ${DESTDIR}${PREFIX}/bin/autolayout
	@rm -f ${DESTDIR}${MANPREFIX}/man1/autolayout.1

clean:
	@rm -f autolayout ${OBJ}


.PHONY: all clean install uninstall
