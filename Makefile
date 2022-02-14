PREFIX = /usr/local
INCS = -I. -I/usr/include
CFLAGS = -pedantic -Wall -Os ${INCS}
CC = cc

SRC = autolayout.c
OBJ = ${SRC:.c=.o}

all: autolayout 

autolayout: ${OBJ} 
	@${CC} -o $@ ${OBJ}

clean:
	@rm -f autolayout ${OBJ} 

.PHONY: all clean
