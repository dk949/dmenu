# dmenu - dynamic menu
# See LICENSE file for copyright and license details.

include config.mk

SRC = drw.c \
	  dmenu.c \
	  util.c

SSRC = stest.c

OBJ = $(SRC:.c=.o)
SOBJ = $(SSRC:.c=.o)

all: options dmenu stest

options:
	@echo dmenu build options:
	@echo "CFLAGS   = $(CFLAGS)"
	@echo "LDFLAGS  = $(LDFLAGS)"
	@echo "CC       = $(CC)"

.c.o:
	$(CC) -c $(CFLAGS) $<

$(OBJ): arg.h config.h config.mk drw.h

dmenu: $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

stest: $(SOBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	rm -f dmenu stest $(OBJ) $(SOBJ)

install: all
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	install dmenu $(DESTDIR)$(PREFIX)/bin
	install dmenu_path $(DESTDIR)$(PREFIX)/bin
	install dmenu_run $(DESTDIR)$(PREFIX)/bin
	install stest $(DESTDIR)$(PREFIX)/bin
	mkdir -p $(DESTDIR)$(MANPREFIX)/man1
	sed "s/VERSION/$(VERSION)/g" < dmenu.1 > $(DESTDIR)$(MANPREFIX)/man1/dmenu.1
	sed "s/VERSION/$(VERSION)/g" < stest.1 > $(DESTDIR)$(MANPREFIX)/man1/stest.1
	chmod 644 $(DESTDIR)$(MANPREFIX)/man1/dmenu.1
	chmod 644 $(DESTDIR)$(MANPREFIX)/man1/stest.1

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/dmenu\
		$(DESTDIR)$(PREFIX)/bin/dmenu_path\
		$(DESTDIR)$(PREFIX)/bin/dmenu_run\
		$(DESTDIR)$(PREFIX)/bin/stest\
		$(DESTDIR)$(MANPREFIX)/man1/dmenu.1\
		$(DESTDIR)$(MANPREFIX)/man1/stest.1

.PHONY: all options clean dist install uninstall
