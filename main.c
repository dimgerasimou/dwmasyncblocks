#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <X11/Xlib.h>

#define CMDLENGTH    50
#define LENGTH(X)    (sizeof(X) / sizeof(X[0]))

typedef const struct {
	const char *command;
	const unsigned int interval;
	const unsigned int signal;
} Block;

#include "config.h"

/* Functions */
void debug();
int getstatus(char *str, char *last);
void printhelp();
void setroot();
static int setupX();

/* Variables */
static Display *dpy;
static Window root;
static int screen;
static void (*writestatus) () = setroot;

static char outputs[LENGTH(blocks)][CMDLENGTH * 4 + 1 + CLICKABLE_BLOCKS];
static char statusbar[2][LENGTH(blocks) * (LENGTH(outputs[0]) - 1) + (LENGTH(blocks) - 1 + leadingdelimiter) * (LENGTH(DELIMITER) - 1) + 1];

void
debug()
{
	// Only write out if text has changed
	if (!getstatus(statusbar[0], statusbar[1]))
		return;

	write(STDOUT_FILENO, statusbar[0], strlen(statusbar[0]));
	write(STDOUT_FILENO, "\n", 1);
}

int
getstatus(char *str, char *strold)
{
	strcpy(strold, str);
	str[0] = '\0';
	
	for (int i = 0; i < LENGTH(blocks); i++) {
		if (leadingdelimiter) {
			if (strlen(outputs[i]))
				strcat(str, DELIMITER);
		} else {
			if (strlen(str) && strlen(outputs[i]))
				strcat(str, DELIMITER);
		}
		strcat(str, outputs[i]);
	}
	return strcmp(str, strold);
}

void
printhelp() {
	puts("This is a hackable status bar meant to be used with dwm.");
	puts("To use, run in backround by typing \"dwmblocks &\" in the terminal.");
	puts("Arguments:");
	puts("\t--help    Prints this.");
	puts("\t-h");
	puts("\t--debug   Starts dwmblocks in debug mode.");
}

int
setupX()
{
	dpy = XOpenDisplay(NULL);
	if (!dpy)
		return 0;

	screen = DefaultScreen(dpy);
	root = RootWindow(dpy, screen);
	return 1;
}

void
setroot()
{
	/* Only set root if text has changed */
	if (!getstatus(statusbar[0], statusbar[1]))
		return;

	XStoreName(dpy, root, statusbar[0]);
	XFlush(dpy);
}

int
main(int argc, char* argv[])
{
	/* Handle command line arguments */
	for (int i = 0; i < argc; i++) {
		if (!strcmp("-h", argv[i]) || !strcmp("--help", argv[i])) {
			printhelp();
			return 0;
		} else if (!strcmp("--debug", argv[i])) {
			writestatus = debug;
		} else {
			fprintf(stderr, "dwmblocks: Invalid arguments.\n");
			fprintf(stdout, "Use '-h' or \"--help\"\n");
			return 1;
		}
	}

	if (!setupX()) {
		fprintf(stderr, "dwmblocks: Failed to open display.\n");
		return 1;
	}

	return 0;
}
		
