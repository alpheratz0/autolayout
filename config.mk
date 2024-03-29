# Copyright (C) 2022-2023 <alpheratz99@protonmail.com>
# This program is free software.

VERSION = 0.1.1

PREFIX = /usr/local
MANPREFIX = $(PREFIX)/share/man

PKG_CONFIG = pkg-config

DEPENDENCIES = json-c

INCS = $(shell $(PKG_CONFIG) --cflags $(DEPENDENCIES))
LIBS = $(shell $(PKG_CONFIG) --libs $(DEPENDENCIES))

CFLAGS = -std=c99 -pedantic -Wall -Wextra -Os $(INCS) -DVERSION=\"$(VERSION)\"
LDFLAGS = -s $(LIBS)

CC = cc
