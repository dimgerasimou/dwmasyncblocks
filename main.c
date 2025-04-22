/* See LICENSE file for copyright and license details.
 *
 * dwmasyncblocks is an asynchronous status bar for dwm. It is partitioned in blocks,
 * where each block runs asynchronusly (thus the bar does not "freeze" whenever a block
 * takes time to update). It has clickable blocks, where using the environemnt variable
 * "BLOCK_BUTTON", the scripts can respond to mouse clicks, giving you endless possibilities
 * for customization. The blocks update either by the given period at the config.h file,
 * or by giving dwmblocks the corresponding block signal (SIGRTMIN+signal_number).
 */

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/signalfd.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <errno.h>

#define CMDLENGTH    70
#define LENGTH(X)    (int)(sizeof(X) / sizeof(X[0]))
#define MAX(a, b)    (a > b ? a : b)

typedef const struct {
	const char* command;
	const unsigned int interval;
	const unsigned int signal;
} Block;

#include "config.h"

/* Functions */
void closepipe(int* pipe);
void execblock(int i, const char* button);
void execblocks(unsigned int time);
int gcd(int a, int b);
int getstatus(char* str, char* last);
void initialize(void);
void printhelp(void);
void setroot(void);
void setupsignals(void);
static int setupX(void);
void signalhandler(void);
void statusloop(void);
void termhandler(int signum);
void termination(void);
void updateblock(int i);

/* Variables */
static Display* dpy;
static int epollfd;
static struct epoll_event event;
static int execlock = 0;
static int maxinterval = 1;
static int pipes[LENGTH(blocks)][2];
static unsigned short int proccesscontinue = 1;
static Window root;
static int screen;
static int signalFD;
static int timer = 0;
static int timertick = 0;
static void (*writestatus) (void) = setroot;

static char outputs[LENGTH(blocks)][CMDLENGTH * 4 + 1 + CLICKABLE_BLOCKS];
static char statusbar[2][LENGTH(blocks) * (LENGTH(outputs[0]) - 1) + (LENGTH(blocks) - 1 + LEADING_DELIMITER) * (LENGTH(DELIMITER) - 1) + 1];

void
closepipe(int* pipe)
{
	close(pipe[0]);
	close(pipe[1]);
}

void
execblock(int i, const char* button)
{
	pid_t pID;
	/* Ensure only one child process exists per block at an instance */
	if (execlock & 1 << i)
		return;
	/* Lock execution of block until current instance finishes execution */
	execlock |= 1 << i;
	
	pID = fork();
	if (pID < 0) {
		fprintf(stderr, "dwmblocks:execblock: fork() failed for block %d: %s\n", i, strerror(errno));
	} else if (pID == 0) {
		close(pipes[i][0]);
		dup2(pipes[i][1], STDOUT_FILENO);
		close(pipes[i][1]);

		if (button)
			setenv("BLOCK_BUTTON", button, 1);
		execl("/bin/sh", "sh", "-c", blocks[i].command, (char*)NULL);
		exit(EXIT_SUCCESS);
	}
}

void
execblocks(unsigned int time)
{
	for (int i = 0; i < LENGTH(blocks); i++)
		if (time == 0 || (blocks[i].interval != 0 && time % blocks[i].interval == 0))
			execblock(i, NULL);
}

int
gcd(int a, int b)
{
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
initialize(void)
{
	epollfd = epoll_create(LENGTH(blocks));
	event.events = EPOLLIN;

	for (unsigned short i = 0; i < LENGTH(blocks); i++) {
		pipe(pipes[i]);
		event.data.u32 = i;
		epoll_ctl(epollfd, EPOLL_CTL_ADD, pipes[i][0], &event);

		if(blocks[i].interval) {
			maxinterval = MAX((int)(blocks[i].interval), maxinterval);
			timertick = gcd(blocks[i].interval, timertick);
		}
	}

	setupsignals();

	/* Initialize Blocks */
	raise(SIGALRM);
}

void
printhelp(void)
{
	puts("This is a hackable status bar meant to be used with dwm.");
	puts("To use, run in backround by typing \"dwmblocks &\" in the terminal.");
	puts("Arguments:");
	puts("\t-h    --help    Prints this.");
}

void
setroot(void)
{
	/* Only set root if text has changed */
	if (!getstatus(statusbar[0], statusbar[1]))
		return;

	XStoreName(dpy, root, statusbar[0]);
	XFlush(dpy);
}

void
setupsignals(void)
{
	/* Termination signals */
	signal(SIGINT, termhandler);
	signal(SIGTERM, termhandler);

	sigset_t handledsignals;
	sigemptyset(&handledsignals);
	sigaddset(&handledsignals, SIGUSR1);
	sigaddset(&handledsignals, SIGALRM);

	/* Append all block signals to `handledsignals` */
	for (unsigned short i = 0; i < LENGTH(blocks); i++)
		if (blocks[i].signal > 0)
			sigaddset(&handledsignals, SIGRTMIN + blocks[i].signal);

	/* Create a signal file descriptor for epoll to watch */
	signalFD = signalfd(-1, &handledsignals, 0);
	event.data.u32 = LENGTH(blocks);
	epoll_ctl(epollfd, EPOLL_CTL_ADD, signalFD, &event);

	/* Block all realtime and handled signals */
	for (unsigned short i = SIGRTMIN; i <= SIGRTMAX; i++)
		sigaddset(&handledsignals, i);
	sigprocmask(SIG_BLOCK, &handledsignals, NULL);

	/* Avoid zombie subprocesses */
	struct sigaction sa;
	sa.sa_handler = SIG_DFL;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_NOCLDWAIT;
	sigaction(SIGCHLD, &sa, 0);
}

int
setupX(void)
{
	dpy = XOpenDisplay(NULL);
	if (!dpy)
		return 0;

	screen = DefaultScreen(dpy);
	root = RootWindow(dpy, screen);
	return 1;
}

void
signalhandler(void)
{
	struct signalfd_siginfo info;
	read(signalFD, &info, sizeof(info));
	unsigned int signal = info.ssi_signo;

	switch (signal) {
	case SIGALRM:
		/* Schedule the next timer event and execute blocks */
		alarm(timertick);
		execblocks(timer);

		/* Wrap `timer` to the interval [1, `maxInterval`] */
		timer = (timer + timertick - 1) % maxinterval + 1;
		return;
	case SIGUSR1:
		/* Update all blocks on receiving SIGUSR1 */
		execblocks(0);
		return;
	}

	for (int j = 0; j < LENGTH(blocks); j++) {
		if (blocks[j].signal == signal - SIGRTMIN) {
			char button[] = {('0' + info.ssi_int) & 0xff, 0};
			execblock(j, button);
			break;
		}
	}
}

void
statusloop(void)
{
	struct epoll_event events[LENGTH(blocks) + 1];

	while (proccesscontinue) {
		int eventCount = epoll_wait(epollfd, events, LENGTH(events), -1);
		for (int i = 0; i < eventCount; i++) {
			unsigned short id = events[i].data.u32;
			if (id < LENGTH(blocks))
				updateblock(id);
			else
				signalhandler();
		}

		if (eventCount != -1)
			writestatus();
	}
}

void
termhandler(int signum)
{
	proccesscontinue = 0;
}

void
termination(void)
{
	XCloseDisplay(dpy);
	close(epollfd);
	close(signalFD);
	for (int i = 0; i < LENGTH(pipes); i++)
		closepipe(pipes[i]);
}

void
updateblock(int i)
{
	char* output = outputs[i];
	char buffer[LENGTH(outputs[0]) - CLICKABLE_BLOCKS];
	int bytesRead = read(pipes[i][0], buffer, LENGTH(buffer));

	/* Trim UTF-8 string to desired length */
	int count = 0, j = 0;
	while (buffer[j] != '\n' && count < CMDLENGTH) {
		count++;

		/* Skip continuation bytes, if any */
		char ch = buffer[j];
		int skip = 1;
		while ((ch & 0xc0) > 0x80)
			ch <<= 1, skip++;
		j += skip;
	}

	/* Cache last character and replace it with a trailing space */
	char ch = buffer[j];
	buffer[j] = ' ';

	/* Trim trailing spaces */
	if (TRIM_TRAILING_SPACES) {
		while (j >= 0 && buffer[j] == ' ')
			j--;
	}
	buffer[j + 1] = 0;

	/* Clear the pipe */
	if (bytesRead == LENGTH(buffer)) {
		while (ch != '\n' && read(pipes[i][0], &ch, 1) == 1)
			;
	}

	if (CLICKABLE_BLOCKS) {
		if (bytesRead > 1 && blocks[i].signal > 0) {
			output[0] = blocks[i].signal;
			output++;
		}
	}

	strcpy(output, buffer);

	/* Remove execution lock for the current block */
	execlock &= ~(1 << i);
}

int
main(int argc, char* argv[])
{
	/* Handle command line arguments */
	for (int i = 1; i < argc; i++) {
		if (!strcmp("-h", argv[i]) || !strcmp("--help", argv[i])) {
			printhelp();
			return 0;
		} else if (!strcmp("-v", argv[i])) {
			fprintf(stderr, "dwmblocks-1.0\n");
			return 0;
		}else {
			fprintf(stderr, "dwmblocks: Invalid arguments.\n");
			fprintf(stderr, "Use '-h' or \"--help\"\n");
			return 1;
		}
	}

	if (!setupX()) {
		fprintf(stderr, "dwmblocks: Failed to open display.\n");
		return 1;
	}

	initialize();

	statusloop();

	termination();

	return 0;
}
