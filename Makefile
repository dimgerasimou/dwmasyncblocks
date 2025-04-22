# dwmasyncblocks
# See LICENCE file for copyright and license details.

PREFIX  := /usr/local
MANPREFIX = ${PREFIX}/share/man
CC      := cc
CFLAGS  := -pedantic -Wall -Wno-deprecated-declarations -Os -c99
LDFLAGS := -lX11

all: options dwmblocks dwmblocksctl

options:
	@echo dwmblocks build options:
	@echo "CFLAGS  = ${CFLAGS}"
	@echo "LDFLAGS = ${LDFLAGS}"
	@echo "CC      = ${CC}"

dwmblocks: main.c config.h
	@echo making dwmblocks
	@${CC} -o dwmblocks main.c ${CFLAGS} ${LDFLAGS}

dwmblocksctl: dwmblocksctl.c config.h
	@echo making dwmblocksctl
	@${CC} -o dwmblocksctl dwmblocksctl.c ${CFLAGS}

clean:
	@echo cleaning
	@rm -f *.o *.gch dwmblocks dwmblocksctl

install: dwmblocks dwmblocksctl
	@echo installing executable file to ${DESTDIR}${PREFIX}/bin
	@mkdir -p ${DESTDIR}${PREFIX}/bin
	@cp -f dwmblocks ${DESTDIR}${PREFIX}/bin
	@chmod 755 ${DESTDIR}${PREFIX}/bin/dwmblocks
	@cp -f dwmblocksctl ${DESTDIR}${PREFIX}/bin
	@chmod 755 ${DESTDIR}${PREFIX}/bin/dwmblocksctl
	@echo installing manual page to ${DESTDIR}${MANPREFIX}/man1
	@mkdir -p ${DESTDIR}${MANPREFIX}/man1
	@cp -f dwmblocks.1 ${DESTDIR}${MANPREFIX}/man1/dwmblocks.1 
	@chmod 644 ${DESTDIR}${MANPREFIX}/man1/dwmblocks.1
	@cp -f dwmblocksctl.1 ${DESTDIR}${MANPREFIX}/man1/dwmblocksctl.1 
	@chmod 644 ${DESTDIR}${MANPREFIX}/man1/dwmblocksctl.1



uninstall:
	@echo removing executable file from ${DESTDIR}${PREFIX}/bin
	@rm -f ${DESTDIR}${PREFIX}/bin/dwmblocks
	@rm -f ${DESTDIR}${PREFIX}/bin/dwmblocksctl
	@echo removing manual page from ${DESTDIR}${MANPREFIX}/man1
	@rm -f ${DESTDIR}${MANPREFIX}/man1/dwmblocks.1
	@rm -f ${DESTDIR}${MANPREFIX}/man1/dwmblocksctl.1

.PHONY: all options clean install uninstall
