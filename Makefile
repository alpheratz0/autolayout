PREFIX = /usr/local
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

uninstall:
	@rm -f ${DESTDIR}${PREFIX}/bin/autolayout

clean:
	@rm -f autolayout ${OBJ}


.PHONY: all clean install uninstall
