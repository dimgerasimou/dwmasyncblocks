#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <X11/Xlib.h>

#define CMDLENGTH    50
#define LENGTH(X)    (sizeof(X) / sizeof(X[0]))
#define MAX(a, b)    (a > b ? a : b)

typedef const struct {
	const char* command;
	const unsigned int interval;
	const unsigned int signal;
} Block;

#include "config.h"

/* Functions */
int gcd(int a, int b);
int getstatus(char* str, char* last);
void initialize();
void printhelp();
void setroot();
void setupsignals();
static int setupX();
void termhandler();

/* Variables */
static Display* dpy;
static int epollfd;
static struct epoll_event event;
static int maxinterval = 1;
static int pipes[LENGTH(blocks)][2];
static unsigned short int proccessContinue = 1;
static Window root;
static int screen;
static int timertick = 0;
static void (*writestatus) () = setroot;

static char outputs[LENGTH(blocks)][CMDLENGTH * 4 + 1 + CLICKABLE_BLOCKS];
static char statusbar[2][LENGTH(blocks) * (LENGTH(outputs[0]) - 1) + (LENGTH(blocks) - 1 + LEADING_DELIMITER) * (LENGTH(DELIMITER) - 1) + 1];

int gcd(int a, int b) {
	int temp;
	while (b > 0) {
		temp = a % b;
		a = b;
		b = temp;
	}
	return a;
}

int
getstatus(char *str, char *strold)
{
	strcpy(strold, str);
	str[0] = '\0';
	
	for (unsigned short i = 0; i < LENGTH(blocks); i++) {
		if (LEADING_DELIMITER) {
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
initialize()
{
	epollfd = epoll_create(LENGTH(blocks));
	event.events = EPOLLIN;

	for (unsigned short i = 0; i < LENGTH(blocks); i++) {
		pipe(pipes[i]);
		event.data.u32 = i;
		epoll_ctl(epollfd, EPOLL_CTL_ADD, pipes[i][0], &event);

		if(blocks[i].interval) {
			maxinterval = MAX(blocks[i].interval, maxinterval);
			timertick = gcd(blocks[i].interval, timertick);
		}
	}

	setupsignals();
}

void
printhelp()
{
	puts("This is a hackable status bar meant to be used with dwm.");
	puts("To use, run in backround by typing \"dwmblocks &\" in the terminal.");
	puts("Arguments:");
	puts("\t-h    --help    Prints this.");
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

void
setupsignals()
{
	/* Termination signals */
	signal(SIGINT, termhandler);
	signal(SIGTERM, termhandler);
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
termhandler()
{
	proccessContinue = 0;
}

int
main(int argc, char* argv[])
{
	/* Handle command line arguments */
	for (int i = 1; i < argc; i++) {
		if (!strcmp("-h", argv[i]) || !strcmp("--help", argv[i])) {
			printhelp();
			return 0;
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

	initialize();

	return 0;
}
		
