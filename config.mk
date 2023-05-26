# dmenu version
VERSION = $(shell git describe)

# paths
DESTDIR   ?=
PREFIX    ?= /usr/local
MANPREFIX ?= $(PREFIX)/share/man

REQ_LIBS = x11 fontconfig xft xrender

X11FLAGS = `pkg-config x11 --cflags`
X11LIBS  = `pkg-config x11 --libs`

# Xinerama, comment if you don't want it
XINERAMAFLAGS = `pkg-config xinerama && pkg-config xinerama --cflags` `pkg-config xinerama && echo "-DXINERAMA"`
XINERAMALIBS  = `pkg-config xinerama && pkg-config xinerama --libs`

# includes and libs
LIBFLAGS = $(XINERAMAFLAGS) `pkg-config $(REQ_LIBS) --cflags`
LIBS     = $(XINERAMALIBS) `pkg-config $(REQ_LIBS) --libs`

# flags
CPPFLAGS = -D_DEFAULT_SOURCE -D_BSD_SOURCE -D_XOPEN_SOURCE=700 -D_POSIX_C_SOURCE=200809L -DVERSION=\"$(VERSION)\" $(XINERAMAFLAGS)
CFLAGS   = -std=c99 -pedantic -Wall -Os $(LIBFLAGS) $(CPPFLAGS)
LDFLAGS  = $(LIBS)

# compiler and linker
CC ?= gcc
DC ?= ldc2
