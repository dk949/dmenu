# dmenu - dynamic menu
# See LICENSE file for copyright and license details.

include config.mk

SRC = drw.c \
	  dmenu.c \
	  util.c

OBJ = $(SRC:.c=.o)

all: options dmenu dmenu_path

options:
	@echo dmenu build options:
	@echo "CFLAGS   = $(CFLAGS)"
	@echo "LDFLAGS  = $(LDFLAGS)"
	@echo "CC       = $(CC)"
	@echo "DC       = $(DC)"

.c.o:
	$(CC) -c $(CFLAGS) $<

$(OBJ): config.h config.mk drw.h

dmenu: $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

dmenu_path: dmenu_path.d
	$(DC) -of=$@ $< -O
	strip $@

clean:
	rm -f dmenu dmenu_path *.o

install: all
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	install dmenu $(DESTDIR)$(PREFIX)/bin
	install dmenu_path $(DESTDIR)$(PREFIX)/bin
	install dmenu_run $(DESTDIR)$(PREFIX)/bin
	mkdir -p $(DESTDIR)$(MANPREFIX)/man1
	sed "s/VERSION/$(VERSION)/g" < dmenu.1 > $(DESTDIR)$(MANPREFIX)/man1/dmenu.1
	chmod 644 $(DESTDIR)$(MANPREFIX)/man1/dmenu.1

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/dmenu\
		$(DESTDIR)$(PREFIX)/bin/dmenu_path\
		$(DESTDIR)$(PREFIX)/bin/dmenu_run\
		$(DESTDIR)$(MANPREFIX)/man1/dmenu.1\

.PHONY: all options clean dist install uninstall
