# dwmasyncblocks
# See LICENCE file for copyright and license details.

PREFIX  := /usr/local
MANPREFIX = ${PREFIX}/share/man
CC      := cc
CFLAGS  := -pedantic -Wall -Wno-deprecated-declarations -Os
LDFLAGS := -lX11

all: options dwmblocks

options:
	@echo dwmblocks build options:
	@echo "CFLAGS  = ${CFLAGS}"
	@echo "LDFLAGS = ${LDFLAGS}"
	@echo "CC      = ${CC}"

dwmblocks: main.c config.h
	@echo making dwmblocks
	@${CC} -o dwmblocks main.c ${CFLAGS} ${LDFLAGS}

config.h:
	@cp config.def.h config.h

clean:
	@echo cleaning
	@rm -f *.o *.gch dwmblocks

install: dwmblocks
	@echo installing executable file to ${DESTDIR}${PREFIX}/bin
	@mkdir -p ${DESTDIR}${PREFIX}/bin
	@cp -f dwmblocks ${DESTDIR}${PREFIX}/bin
	@chmod 755 ${DESTDIR}${PREFIX}/bin/dwmblocks
	@echo installing manual page to ${DESTDIR}${MANPREFIX}/man1
	@mkdir -p ${DESTDIR}${MANPREFIX}/man1
	@cp -f dwmblocks.1 ${DESTDIR}${MANPREFIX}/man1/dwmblocks.1
	@chmod 644 ${DESTDIR}${MANPREFIX}/man1/dwmblocks.1

uninstall:
	@echo removing executable file from ${DESTDIR}${PREFIX}/bin
	@rm -f ${DESTDIR}${PREFIX}/bin/dwmblocks
	@echo removing manual page from ${DESTDIR}${MANPREFIX}/man1
	@rm -f ${DESTDIR}${MANPREFIX}/man1/dwmblocks.1

.PHONY: all options clean install uninstall
