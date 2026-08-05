/* See LICENSE file for copyright and license details.
 *
 * dwmasyncblocks is an asynchronous status bar for dwm. It is partitioned in blocks,
 * where each block runs asynchronusly (thus the bar does not "freeze" whenever a block
 * takes time to update). It has clickable blocks, where using the environemnt variable
 * "BLOCK_BUTTON", the scripts can respond to mouse clicks, giving you endless possibilities
 * for customization. The blocks update either by the given period at the config.h file,
 * or by giving dwmblocks the corresponding block signal (SIGRTMIN+signal_number).
 */

#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/file.h>
#include <sys/signalfd.h>
#include <unistd.h>
#include <X11/Xlib.h>

#ifndef VERSION
#define VERSION "unknown"
#endif

#define CMDLENGTH    70
#define LENGTH(X)    (int)(sizeof(X) / sizeof(X[0]))
#define MAX(a, b)    (a > b ? a : b)

typedef const struct {
	const char* command;
	const unsigned int interval;
	const unsigned int signal;
} Block;

#include "config.h"

typedef enum {
	ACT_DAEMON = 0,
	ACT_ALL,
	ACT_RESTART,
	ACT_LIST,
	ACT_UPDATE,
} Action;

enum {
	OPT_ALL = 1000,
	OPT_RESTART,
	OPT_LIST,
	OPT_UPDATE,
	OPT_VERSION,
	OPT_HELP,
};

typedef struct {
	Action action;
	const char* blockname;
} Options;

/* Functions */
void closepipe(int* fds);
void cmdlist(void);
void cmdrestart(void);
void cmdupdateall(void);
void cmdupdateblock(const char* name);
void die(const char* fmt, ...);
void execblock(int i, const char* button);
void execblocks(unsigned int time);
unsigned int gcd(unsigned int a, unsigned int b);
const char* get_name(void);
int getblocksignal(const char* name);
pid_t getdaemonpid(void);
void getpidfilepath(char* buf, size_t len);
int getstatus(char* str, char* strold);
void initblocknames(void);
void initialize(void);
void lockpidfile(void);
static int options_parse(Options* o, const int argc, char* argv[]);
void set_name(const char* name);
void setroot(void);
void setupsignals(void);
static int setupX(void);
void signalhandler(void);
void statusloop(void);
void termhandler(int signum);
void termination(void);
void updateblock(int i);
static void usage(void);
void warn(const char* fmt, ...);

/* Variables */
static Display* dpy;
static int epollfd;
static struct epoll_event event;
static unsigned int execlock = 0;
static unsigned int maxinterval = 1;
static int pidfd = -1;
static char pidfilepath[PATH_MAX];
static int pipes[LENGTH(blocks)][2];
static unsigned short int proccesscontinue = 1;
static const char* program_name;
static Window root;
static int screen;
static int signalFD;
static unsigned int timer = 0;
static unsigned int timertick = 0;
static void (*writestatus) (void) = setroot;

/* +2, not +1: updateblock()'s trim loop can advance its cursor up to
 * CMDLENGTH*4 (all 4-byte UTF-8 chars), then writes a terminator one
 * past that — +1 alone leaves no room for that terminator byte. */
static char outputs[LENGTH(blocks)][CMDLENGTH * 4 + 2 + CLICKABLE_BLOCKS];
static char statusbar[2][LENGTH(blocks) * (LENGTH(outputs[0]) - 1) + (LENGTH(blocks) - 1 + LEADING_DELIMITER) * (LENGTH(DELIMITER) - 1) + 1];
static char blocknames[LENGTH(blocks)][CMDLENGTH];

void
closepipe(int* fds)
{
	close(fds[0]);
	close(fds[1]);
}

void
cmdlist(void)
{
	for (int i = 0; i < LENGTH(blocks); i++)
		puts(blocknames[i]);
}

void
cmdrestart(void)
{
	char path[PATH_MAX];
	char self[PATH_MAX];
	ssize_t len;
	pid_t pid;
	int lockfd;

	pid = getdaemonpid();
	if (kill(pid, SIGTERM) < 0 && errno != ESRCH)
		die("Failed to stop the running daemon:");

	/* Wait until the running daemon releases its pidfile lock */
	getpidfilepath(path, sizeof(path));
	lockfd = open(path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
	if (lockfd >= 0) {
		flock(lockfd, LOCK_EX);
		close(lockfd);
	}

	len = readlink("/proc/self/exe", self, sizeof(self) - 1);
	if (len < 0)
		die("Failed to resolve executable path:");
	self[len] = '\0';

	pid = fork();
	if (pid < 0) {
		die("Failed to fork:");
	} else if (pid == 0) {
		setsid();
		execl(self, "dwmblocks", (char*)NULL);
		_exit(EXIT_FAILURE);
	}
}

void
cmdupdateall(void)
{
	if (kill(getdaemonpid(), SIGUSR1) < 0)
		die("Failed to signal the daemon:");
}

void
cmdupdateblock(const char* name)
{
	int sig = getblocksignal(name);

	if (sig < 0)
		die("Unknown block '%s'.", name);
	if (sig == 0)
		die("Block '%s' has no update signal assigned.", name);
	if (kill(getdaemonpid(), SIGRTMIN + sig) < 0)
		die("Failed to signal block '%s':", name);
}

void
die(const char* fmt, ...)
{
	va_list ap;
	int saved_errno = errno;

	fprintf(stderr, "%s: ", program_name);

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	if (fmt[0] && fmt[strlen(fmt) - 1] == ':')
		fprintf(stderr, " %s", strerror(saved_errno));
	fputc('\n', stderr);

	exit(1);
}

void
execblock(int i, const char* button)
{
	pid_t pid;
	/* Ensure only one child process exists per block at an instance */
	if (execlock & (1u << i))
		return;
	/* Lock execution of block until current instance finishes execution */
	execlock |= 1u << i;

	pid = fork();
	if (pid < 0) {
		/* fork() failed: nothing will ever clear this bit otherwise,
		 * permanently disabling the block until the daemon restarts. */
		execlock &= ~(1u << i);
		warn("execblock: fork() failed for block %d:", i);
	} else if (pid == 0) {
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

unsigned int
gcd(unsigned int a, unsigned int b)
{
	unsigned int temp;
	while (b > 0) {
		temp = a % b;
		a = b;
		b = temp;
	}
	return a;
}

const char*
get_name(void)
{
	return program_name;
}

int
getblocksignal(const char* name)
{
	for (int i = 0; i < LENGTH(blocks); i++)
		if (!strcmp(name, blocknames[i]))
			return (int)blocks[i].signal;
	return -1;
}

pid_t
getdaemonpid(void)
{
	char path[PATH_MAX];
	FILE* fp;
	int fd;
	long pid;

	getpidfilepath(path, sizeof(path));

	/* O_NOFOLLOW: the pidfile path is predictable (falls back to /tmp
	 * without XDG_RUNTIME_DIR), so refuse to follow a symlink planted
	 * there instead of trusting whatever it points to. */
	fd = open(path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
	if (fd < 0)
		die("Failed to open pidfile '%s' (is the daemon running?):", path);

	fp = fdopen(fd, "r");
	if (!fp) {
		close(fd);
		die("Failed to open pidfile '%s':", path);
	}

	if (fscanf(fp, "%ld", &pid) != 1) {
		fclose(fp);
		die("Failed to read pid from pidfile '%s'.", path);
	}
	fclose(fp);

	/* A pid <= 0 has special meaning to kill(2) (process group / all
	 * processes) instead of naming a single process - never trust one
	 * out of the pidfile. */
	if (pid <= 0)
		die("Invalid pid in pidfile '%s'.", path);

	return (pid_t)pid;
}

void
getpidfilepath(char* buf, size_t len)
{
	const char* runtimedir = getenv("XDG_RUNTIME_DIR");

	if (runtimedir && *runtimedir)
		snprintf(buf, len, "%s/dwmblocks.pid", runtimedir);
	else
		snprintf(buf, len, "/tmp/dwmblocks.pid");
}

int
getstatus(char* str, char* strold)
{
	strcpy(strold, str);
	str[0] = '\0';

	for (int i = 0; i < LENGTH(blocks); i++) {
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
initblocknames(void)
{
	char temp[CMDLENGTH];
	const char* src;
	char* ptr;

	for (int i = 0; i < LENGTH(blocks); i++) {
		src = strrchr(blocks[i].command, '/');
		/* A command with no '/' (e.g. a bare command found via $PATH)
		 * has no directory prefix to strip; use it as-is. */
		src = src ? src + 1 : blocks[i].command;
		strcpy(temp, src);
		if ((ptr = strrchr(temp, '"')) != NULL)
			*ptr = '\0';

		if ((ptr = strchr(temp, '"')) == NULL)
			ptr = temp;
		else
			ptr++;
		strcpy(blocknames[i], ptr);
	}
}

void
initialize(void)
{
	/* CLOEXEC: block scripts are exec'd via /bin/sh below and have no
	 * business inheriting the epoll fd. */
	epollfd = epoll_create1(EPOLL_CLOEXEC);
	event.events = EPOLLIN;

	for (int i = 0; i < LENGTH(blocks); i++) {
		/* CLOEXEC here too: each block's own pipe ends are explicitly
		 * handled (dup2'd or closed) in execblock() right before exec,
		 * but without this every *other* block's pipe fds would leak
		 * into the child unexplained. */
		pipe2(pipes[i], O_CLOEXEC);
		event.data.u32 = (uint32_t)i;
		epoll_ctl(epollfd, EPOLL_CTL_ADD, pipes[i][0], &event);

		if(blocks[i].interval) {
			maxinterval = MAX(blocks[i].interval, maxinterval);
			timertick = gcd(blocks[i].interval, timertick);
		}
	}

	setupsignals();

	/* Initialize Blocks */
	raise(SIGALRM);
}

void
lockpidfile(void)
{
	char pidstr[16];
	int len;

	getpidfilepath(pidfilepath, sizeof(pidfilepath));

	/* O_NOFOLLOW: refuse to follow a symlink planted at this predictable
	 * path (falls back to /tmp without XDG_RUNTIME_DIR) instead of us.
	 * O_CLOEXEC: block scripts exec'd later have no business inheriting
	 * our lock on this file. */
	pidfd = open(pidfilepath, O_CREAT | O_RDWR | O_CLOEXEC | O_NOFOLLOW, 0644);
	if (pidfd < 0)
		die("Failed to open pidfile '%s':", pidfilepath);

	if (flock(pidfd, LOCK_EX | LOCK_NB) < 0) {
		if (errno == EWOULDBLOCK)
			die("Another instance is already running.");
		else
			die("Failed to lock pidfile '%s':", pidfilepath);
	}

	len = snprintf(pidstr, sizeof(pidstr), "%d\n", getpid());
	if (ftruncate(pidfd, 0) < 0 || write(pidfd, pidstr, (size_t)len) < 0)
		die("Failed to write pidfile '%s':", pidfilepath);
}

static int
options_parse(Options* o, const int argc, char* argv[])
{
	int nactions = 0;
	int opt;

	o->action = ACT_DAEMON;
	o->blockname = NULL;

	struct option longopts[] = {
		{ "all",     no_argument,       0, OPT_ALL     },
		{ "restart", no_argument,       0, OPT_RESTART },
		{ "list",    no_argument,       0, OPT_LIST    },
		{ "update",  required_argument, 0, OPT_UPDATE  },
		{ "help",    no_argument,       0, OPT_HELP    },
		{ "version", no_argument,       0, OPT_VERSION },
		{ 0,         0,                 0, 0           },
	};

	while ((opt = getopt_long(argc, argv, "", longopts, NULL)) != -1) {
		switch (opt) {
		case OPT_ALL:
			nactions++;
			o->action = ACT_ALL;
			break;

		case OPT_RESTART:
			nactions++;
			o->action = ACT_RESTART;
			break;

		case OPT_LIST:
			nactions++;
			o->action = ACT_LIST;
			break;

		case OPT_UPDATE:
			nactions++;
			o->action = ACT_UPDATE;
			o->blockname = optarg;
			break;

		case OPT_HELP:
			usage();
			exit(0);

		case OPT_VERSION:
			puts("dwmblocks-" VERSION);
			exit(0);

		default:
			fputc('\n', stderr);
			usage();
			exit(1);
		}
	}

	if (optind < argc)
		die("unexpected argument \"%s\"", argv[optind]);

	if (nactions > 1)
		die("only one action may be given.");

	return 0;
}

void
set_name(const char* name)
{
	program_name = name;
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
	for (int i = 0; i < LENGTH(blocks); i++)
		if (blocks[i].signal > 0)
			sigaddset(&handledsignals, SIGRTMIN + (int)blocks[i].signal);

	/* Create a signal file descriptor for epoll to watch. CLOEXEC: block
	 * scripts exec'd later have no business inheriting this. */
	signalFD = signalfd(-1, &handledsignals, SFD_CLOEXEC);
	event.data.u32 = (uint32_t)LENGTH(blocks);
	epoll_ctl(epollfd, EPOLL_CTL_ADD, signalFD, &event);

	/* Block all realtime and handled signals */
	for (int i = SIGRTMIN; i <= SIGRTMAX; i++)
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
		if (blocks[j].signal == signal - (unsigned int)SIGRTMIN) {
			char button[] = {(char)(('0' + info.ssi_int) & 0xff), 0};
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
			unsigned short id = (unsigned short)events[i].data.u32;
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
	(void)signum;
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

	close(pidfd);
	unlink(pidfilepath);
}

void
updateblock(int i)
{
	char* output = outputs[i];
	char buffer[LENGTH(outputs[0]) - CLICKABLE_BLOCKS];
	ssize_t bytesread = read(pipes[i][0], buffer, LENGTH(buffer));

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
	if (bytesread == LENGTH(buffer)) {
		while (ch != '\n' && read(pipes[i][0], &ch, 1) == 1)
			;
	}

	if (CLICKABLE_BLOCKS) {
		if (bytesread > 1 && blocks[i].signal > 0) {
			output[0] = (char)blocks[i].signal;
			output++;
		}
	}

	strcpy(output, buffer);

	/* Remove execution lock for the current block */
	execlock &= ~(1u << i);
}

static void
usage(void)
{
	printf("usage: %s [options]\n%s", get_name(),
	      "\tOptions:\n"
	      "\t\t[--help][--version]\n"
	      "\t\t[--all][--restart][--list]\n"
	      "\t\t[--update block]\n"
	      "\n"
	      "--help              Print this message and exit\n"
	      "--version           Print version and exit\n"
	      "--all               Update all blocks\n"
	      "--restart           Restart the daemon\n"
	      "--list              List the configured block names\n"
	      "--update block      Update the named block\n");
}

void
warn(const char* fmt, ...)
{
	va_list ap;
	int saved_errno = errno;

	fprintf(stderr, "%s: ", program_name);

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	if (fmt[0] && fmt[strlen(fmt) - 1] == ':')
		fprintf(stderr, " %s", strerror(saved_errno));
	fputc('\n', stderr);
}

int
main(int argc, char* argv[])
{
	Options opt;
	const char* prog;

	prog = strrchr(argv[0], '/');
	set_name(prog ? prog + 1 : argv[0]);

	options_parse(&opt, argc, argv);

	switch (opt.action) {
	case ACT_DAEMON:
		lockpidfile();

		if (!setupX()) {
			unlink(pidfilepath);
			die("Failed to open display.");
		}

		initialize();
		statusloop();
		termination();
		break;

	case ACT_ALL:
		cmdupdateall();
		break;

	case ACT_RESTART:
		cmdrestart();
		break;

	case ACT_LIST:
		initblocknames();
		cmdlist();
		break;

	case ACT_UPDATE:
		initblocknames();
		cmdupdateblock(opt.blockname);
		break;
	}

	return 0;
}
