# dmenu version
DATE         = $(shell git log -1 --format='%cd' --date=format:'%F')
DATE_TIME    = $(DATE) 00:00
COMMIT_COUNT = $(shell git rev-list --count HEAD --since="$(DATE_TIME)")
VERSION      = 5.0.$(shell date -d "$(DATE)" +'%Y%m%d')_$(COMMIT_COUNT)

# paths
DESTDIR   ?=
PREFIX    ?= /usr/local
MANPREFIX ?= $(PREFIX)/share/man

REQ_LIBS = x11 fontconfig xft xrender

# Xinerama, comment if you don't want it
XINERAMAFLAGS = `pkg-config xinerama --cflags --silence-errors && echo "-DXINERAMA"`
XINERAMALIBS  = `pkg-config xinerama --libs --silence-errors`

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
