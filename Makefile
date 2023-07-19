PREFIX  := /usr/local
CC      := cc
CFLAGS  := -pedantic -Wall -Wno-deprecated-declarations -Os
LDFLAGS := -lX11

all: options dwmblocks dwmblocksctl

options:
	@echo dwmblocks build options:
	@echo "CFLAGS  = ${CFLAGS}"
	@echo "LDFLAGS = ${LDFLAGS}"
	@echo "CC      = ${CC}"

dwmblocks: main.c config.h
	${CC} -o dwmblocks main.c ${CFLAGS} ${LDFLAGS}

dwmblocksctl: dwmblocksctl.c config.h
	${CC} -o dwmblocksctl dwmblocksctl.c ${CFLAGS}

clean:
	rm -f *.o *.gch dwmblocks dwmblocksctl

install: dwmblocks dwmblocksctl
	mkdir -p ${DESTDIR}${PREFIX}/bin
	cp -f dwmblocks ${DESTDIR}${PREFIX}/bin
	chmod 755 ${DESTDIR}${PREFIX}/bin/dwmblocks
	cp -f dwmblocksctl ${DESTDIR}${PREFIX}/bin
	chmod 755 ${DESTDIR}${PREFIX}/bin/dwmblocksctl


uninstall:
	rm -f ${DESTDIR}${PREFIX}/bin/dwmblocks
	rm -f ${DESTDIR}${PREFIX}/bin/dwmblocksctl

.PHONY: all options clean install uninstall
