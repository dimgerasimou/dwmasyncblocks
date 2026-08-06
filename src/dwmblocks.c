/* See LICENSE file for copyright and license details.
 *
 * dwmblocks is both the status bar daemon and its own CLI client. With no
 * arguments it runs as the daemon: one pipe and one fork()/exec() per block,
 * multiplexed through a single epoll instance alongside a signalfd catching
 * SIGALRM (the block timer), SIGUSR1 (update all) and each block's
 * SIGRTMIN+signal (update one, click). Any other invocation (--update, --all,
 * --restart, --list) instead acts as a client: it finds the running daemon's
 * pid through a pidfile, signals or restarts it, and exits. Blocks are
 * declared in config.h, read by both modes.
 *
 * To understand everything else, start reading main().
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
void die(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
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
void validateblocks(void);
void warn(const char* fmt, ...) __attribute__((format(printf, 1, 2)));

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

	/* Block until the old daemon's lock is released, so our own
	 * lockpidfile() below doesn't race it and fail. */
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
	if (execlock & (1u << i))
		return;
	execlock |= 1u << i;

	pid = fork();
	if (pid < 0) {
		/* Nothing else clears this bit; without it, a failed fork()
		 * disables the block until the daemon restarts. */
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

	if (pid <= 0)
		die("Invalid pid in pidfile '%s'.", path);

	return (pid_t)pid;
}

void
getpidfilepath(char* buf, size_t len)
{
	const char* runtimedir = getenv("XDG_RUNTIME_DIR");

	if (runtimedir && *runtimedir)
		snprintf(buf, len, "%s/dwmblocks.lock", runtimedir);
	else
		snprintf(buf, len, "/tmp/dwmblocks.lock");
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
	epollfd = epoll_create1(EPOLL_CLOEXEC);
	event.events = EPOLLIN;

	for (int i = 0; i < LENGTH(blocks); i++) {
		pipe2(pipes[i], O_CLOEXEC);
		event.data.u32 = (uint32_t)i;
		epoll_ctl(epollfd, EPOLL_CTL_ADD, pipes[i][0], &event);

		if(blocks[i].interval) {
			maxinterval = MAX(blocks[i].interval, maxinterval);
			timertick = gcd(blocks[i].interval, timertick);
		}
	}

	setupsignals();

	raise(SIGALRM);
}

void
lockpidfile(void)
{
	char pidstr[16];
	int len;

	getpidfilepath(pidfilepath, sizeof(pidfilepath));

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
	if (!getstatus(statusbar[0], statusbar[1]))
		return;

	XStoreName(dpy, root, statusbar[0]);
	XFlush(dpy);
}

void
setupsignals(void)
{
	signal(SIGINT, termhandler);
	signal(SIGTERM, termhandler);

	sigset_t handledsignals;
	sigemptyset(&handledsignals);
	sigaddset(&handledsignals, SIGUSR1);
	sigaddset(&handledsignals, SIGALRM);

	for (int i = 0; i < LENGTH(blocks); i++)
		if (blocks[i].signal > 0)
			sigaddset(&handledsignals, SIGRTMIN + (int)blocks[i].signal);

	signalFD = signalfd(-1, &handledsignals, SFD_CLOEXEC);
	event.data.u32 = (uint32_t)LENGTH(blocks);
	epoll_ctl(epollfd, EPOLL_CTL_ADD, signalFD, &event);

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
		alarm(timertick);
		execblocks(timer);
		timer = (timer + timertick - 1) % maxinterval + 1;
		return;
	case SIGUSR1:
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

	int count = 0, j = 0;
	while (buffer[j] != '\n' && count < CMDLENGTH) {
		count++;

		unsigned char ch = (unsigned char)buffer[j];
		int skip = 1;
		while ((ch & 0xc0) > 0x80) {
			ch = (unsigned char)(ch << 1);
			skip++;
		}
		j += skip;
	}

	char ch = buffer[j];
	buffer[j] = ' ';

	if (TRIM_TRAILING_SPACES) {
		while (j >= 0 && buffer[j] == ' ')
			j--;
	}
	buffer[j + 1] = 0;

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
	      "\t--help              Print this message and exit\n"
	      "\t--version           Print version and exit\n"
	      "\t--all               Update all blocks\n"
	      "\t--restart           Restart the daemon\n"
	      "\t--list              List the configured block names\n"
	      "\t--update block      Update the named block\n");
}

void
validateblocks(void)
{
	for (int i = 0; i < LENGTH(blocks); i++) {
		for (int j = i + 1; j < LENGTH(blocks); j++) {
			if (blocks[i].signal != 0 && blocks[i].signal == blocks[j].signal)
				die("config.h: blocks '%s' and '%s' share update signal %u.",
				    blocknames[i], blocknames[j], blocks[i].signal);
			if (!strcmp(blocknames[i], blocknames[j]))
				die("config.h: blocks '%s' and '%s' resolve to the same name.",
				    blocknames[i], blocknames[j]);
		}
	}
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

	initblocknames();
	validateblocks();

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
		cmdlist();
		break;

	case ACT_UPDATE:
		cmdupdateblock(opt.blockname);
		break;
	}

	return 0;
}
