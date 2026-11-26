/* See LICENSE file for copyright and license details.
 *
 * dwmblocks is both the status bar daemon and its own CLI client. With no
 * arguments it runs as the daemon: one pipe and one fork()/exec() per block,
 * multiplexed through a single epoll instance alongside a signalfd catching
 * SIGALRM (the block timer), SIGUSR1 (update all) and each block's
 * SIGRTMIN+signal (update one, click), and the X connection, which reports
 * RESOURCE_MANAGER changes so colours and icon modes follow xrdb live.
 * Blocks print tab-separated class=/rich=/plain= fields; the daemon picks the
 * variant, colours it and builds the status line. Any other invocation (--update, --all,
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
#include <ctype.h>
#include <unistd.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xresource.h>

#ifndef VERSION
#define VERSION "unknown"
#endif

#define CMDLENGTH    70
#define RAWLENGTH    1024
#define HEXLENGTH    8
#define ESCLENGTH    64
#define MAXSEGMENTS  8
#define LENGTH(X)    (int)(sizeof(X) / sizeof(X[0]))
#define MAX(a, b)    (a > b ? a : b)

typedef const struct {
	const char* command;
	const unsigned int interval;
	const unsigned int signal;
	const unsigned int icons;
} Block;

typedef const struct {
	const char* class;
	const char* hex;
} Colour;

typedef struct {
	const char* class;
	const char* rich;
	const char* plain;
} Segment;

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
static void appendtext(char** dst, size_t* room, const char* s, size_t len, int* count);
void closepipe(int* fds);
void cmdlist(void);
void cmdrestart(void);
void cmdupdateall(void);
void cmdupdateblock(const char* name);
void die(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void execblock(int i, const char* button);
void execblocks(unsigned int time);
static void exportcolours(void);
static int findcolour(const char* class);
unsigned int gcd(unsigned int a, unsigned int b);
const char* get_name(void);
int getblocksignal(const char* name);
pid_t getdaemonpid(void);
void getpidfilepath(char* buf, size_t len);
int getstatus(char* str, char* strold);
void initblocknames(void);
void initialize(void);
static int ishexcolour(const char* s);
static void loadresources(void);
void lockpidfile(void);
static int options_parse(Options* o, const int argc, char* argv[]);
static int parsesegments(char* line, Segment* segs);
static void renderblock(int i);
void set_name(const char* name);
void setroot(void);
void setupsignals(void);
static int setupX(void);
void signalhandler(void);
void statusloop(void);
static void xevents(void);
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

static char hexes[LENGTH(colours)][HEXLENGTH];
static unsigned int icons[LENGTH(blocks)];
static char raws[LENGTH(blocks)][RAWLENGTH];
static unsigned int classwarned = 0;

static char outputs[LENGTH(blocks)][CMDLENGTH * 4 + MAXSEGMENTS * 2 * ESCLENGTH + 2 + CLICKABLE_BLOCKS];
static char statusbar[2][LENGTH(blocks) * (LENGTH(outputs[0]) - 1) + (LENGTH(blocks) - 1 + LEADING_DELIMITER) * (LENGTH(DELIMITER) - 1) + 1];
static char blocknames[LENGTH(blocks)][CMDLENGTH];

/* Copies whole UTF-8 characters until CMDLENGTH visible ones have been
 * written in total across the block, or the buffer is full. */
static void
appendtext(char** dst, size_t* room, const char* s, size_t len, int* count)
{
	size_t j = 0;

	while (j < len && *count < CMDLENGTH) {
		size_t n = 1;
		unsigned char ch = (unsigned char)s[j];

		while ((ch & 0xc0) > 0x80) {
			ch = (unsigned char)(ch << 1);
			n++;
		}
		if (j + n > len || n >= *room)
			break;

		memcpy(*dst, s + j, n);
		*dst += n;
		*room -= n;
		j += n;
		(*count)++;
	}
	**dst = '\0';
}

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
		exportcolours();
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

static void
exportcolours(void)
{
	char name[ESCLENGTH];

	for (int i = 0; i < LENGTH(colours); i++) {
		int n = snprintf(name, sizeof(name), "BLOCK_CLR_%s", colours[i].class);

		if (n < 0 || (size_t)n >= sizeof(name))
			continue;
		for (char* p = name; *p; p++)
			*p = (char)toupper((unsigned char)*p);

		if (hexes[i][0])
			setenv(name, hexes[i], 1);
		else
			unsetenv(name);
	}
}

static int
findcolour(const char* class)
{
	for (int i = 0; i < LENGTH(colours); i++)
		if (!strcmp(class, colours[i].class))
			return i;
	return -1;
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

	loadresources();
	XSelectInput(dpy, root, PropertyChangeMask);
	event.data.u32 = (uint32_t)LENGTH(blocks) + 1;
	epoll_ctl(epollfd, EPOLL_CTL_ADD, ConnectionNumber(dpy), &event);

	raise(SIGALRM);
}

static int
ishexcolour(const char* s)
{
	if (!s || s[0] != '#' || strlen(s) != HEXLENGTH - 1)
		return 0;
	for (int i = 1; i < HEXLENGTH - 1; i++)
		if (!isxdigit((unsigned char)s[i]))
			return 0;
	return 1;
}

/* Read the RESOURCE_MANAGER property itself: XResourceManagerString() is a
 * snapshot from XOpenDisplay() and never sees a later xrdb. */
static void
loadresources(void)
{
	char name[CMDLENGTH + sizeof(XRES_PREFIX) + 8];
	unsigned char* prop = NULL;
	unsigned long nitems, after;
	XrmDatabase db = NULL;
	XrmValue val;
	Atom type;
	char* rtype;
	int format;

	for (int i = 0; i < LENGTH(colours); i++) {
		hexes[i][0] = '\0';
		if (ishexcolour(colours[i].hex))
			memcpy(hexes[i], colours[i].hex, HEXLENGTH);
	}
	for (int i = 0; i < LENGTH(blocks); i++)
		icons[i] = blocks[i].icons ? 1 : 0;

	if (XGetWindowProperty(dpy, root, XA_RESOURCE_MANAGER, 0, LONG_MAX / 4, False,
	                       XA_STRING, &type, &format, &nitems, &after, &prop) != Success
	    || !prop)
		return;

	XrmInitialize();
	db = XrmGetStringDatabase((char*)prop);
	XFree(prop);
	if (!db)
		return;

	for (int i = 0; i < LENGTH(colours); i++) {
		int w = snprintf(name, sizeof(name), XRES_PREFIX "clr_%s", colours[i].class);
		if (w > 0 && (size_t)w < sizeof(name) && XrmGetResource(db, name, name, &rtype, &val) && ishexcolour(val.addr))
			memcpy(hexes[i], val.addr, HEXLENGTH);
	}
	for (int i = 0; i < LENGTH(blocks); i++) {
		int w = snprintf(name, sizeof(name), XRES_PREFIX "icon_%s", blocknames[i]);
		if (w > 0 && (size_t)w < sizeof(name) && XrmGetResource(db, name, name, &rtype, &val) && val.addr
		    && (!strcmp(val.addr, "0") || !strcmp(val.addr, "1")))
			icons[i] = val.addr[0] == '1';
	}

	XrmDestroyDatabase(db);
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

/* Splits "key=value" fields separated by tabs into segments. A segment
 * starts at each class= and whenever a key repeats. Returns -1 for a line
 * that isn't in field format at all, so it is shown as plain text. */
static int
parsesegments(char* line, Segment* segs)
{
	static const char* const keys[] = { "class", "rich", "plain" };
	char* field = line;
	int n = 0;

	if (strncmp(line, "class=", 6) && strncmp(line, "rich=", 5) && strncmp(line, "plain=", 6))
		return -1;

	while (field) {
		char* next = strchr(field, '\t');
		char* val = strchr(field, '=');
		const char** slot = NULL;
		int k;

		if (next)
			*next++ = '\0';
		if (!val) {
			field = next;
			continue;
		}
		*val++ = '\0';

		for (k = 0; k < LENGTH(keys) && strcmp(field, keys[k]); k++)
			;
		if (k == LENGTH(keys)) {
			field = next;
			continue;
		}

		if (n > 0) {
			Segment* s = &segs[n - 1];
			slot = k == 0 ? &s->class : k == 1 ? &s->rich : &s->plain;
		}
		if (n == 0 || k == 0 || *slot) {
			if (n == MAXSEGMENTS)
				break;
			segs[n] = (Segment){ NULL, NULL, NULL };
			slot = k == 0 ? &segs[n].class : k == 1 ? &segs[n].rich : &segs[n].plain;
			n++;
		}
		*slot = val;
		field = next;
	}
	return n;
}

static void
renderblock(int i)
{
	Segment segs[MAXSEGMENTS];
	char line[RAWLENGTH];
	const char* text[MAXSEGMENTS];
	char* out = outputs[i];
	size_t room = sizeof(outputs[i]);
	int count = 0, last = -1, n;

	if (CLICKABLE_BLOCKS && raws[i][0] && blocks[i].signal > 0) {
		*out++ = (char)blocks[i].signal;
		room--;
	}
	*out = '\0';

	memcpy(line, raws[i], sizeof(line));
	n = parsesegments(line, segs);

	if (n < 0) {
		size_t len = strlen(line);
		if (TRIM_TRAILING_SPACES)
			while (len && line[len - 1] == ' ')
				len--;
		appendtext(&out, &room, line, len, &count);
		return;
	}

	for (int j = 0; j < n; j++) {
		const char* a = icons[i] ? segs[j].rich : segs[j].plain;
		const char* b = icons[i] ? segs[j].plain : segs[j].rich;

		text[j] = a ? a : b;
		if (text[j] && *text[j])
			last = j;
	}

	for (int j = 0; j <= last && count < CMDLENGTH; j++) {
		const char* hex = "";
		size_t len;

		if (!text[j] || !*text[j])
			continue;

		if (segs[j].class) {
			int c = findcolour(segs[j].class);
			if (c >= 0) {
				hex = hexes[c];
			} else if (!(classwarned & (1u << i))) {
				classwarned |= 1u << i;
				warn("block '%s': unknown colour class '%s'.", blocknames[i], segs[j].class);
			}
		}

		len = strlen(text[j]);
		if (TRIM_TRAILING_SPACES && j == last)
			while (len && text[j][len - 1] == ' ')
				len--;

		if (*hex) {
			int w = snprintf(out, room, CLR_FMT, hex);
			if (w > 0 && (size_t)w < room) {
				out += w;
				room -= (size_t)w;
			}
		}
		appendtext(&out, &room, text[j], len, &count);
		if (*hex && strlen(CLR_NRM) < room) {
			strcpy(out, CLR_NRM);
			out += strlen(CLR_NRM);
			room -= strlen(CLR_NRM);
		}
	}
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
	struct epoll_event events[LENGTH(blocks) + 2];

	while (proccesscontinue) {
		int eventCount = epoll_wait(epollfd, events, LENGTH(events), -1);
		for (int i = 0; i < eventCount; i++) {
			unsigned short id = (unsigned short)events[i].data.u32;
			if (id < LENGTH(blocks))
				updateblock(id);
			else if (id == LENGTH(blocks))
				signalhandler();
		}

		/* Every pass, not only on fd wakeups: Xlib may already hold
		 * events in its queue that epoll will never report. */
		xevents();

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
	char* raw = raws[i];
	ssize_t bytesread = read(pipes[i][0], raw, RAWLENGTH - 1);
	char* nl;

	if (bytesread < 0)
		bytesread = 0;
	raw[bytesread] = '\0';

	if ((nl = strchr(raw, '\n'))) {
		*nl = '\0';
	} else if (bytesread == RAWLENGTH - 1) {
		char ch = 0;
		while (ch != '\n' && read(pipes[i][0], &ch, 1) == 1)
			;
	}

	renderblock(i);
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

static void
xevents(void)
{
	XEvent ev;
	int reload = 0;

	while (XPending(dpy)) {
		XNextEvent(dpy, &ev);
		if (ev.type == PropertyNotify && ev.xproperty.atom == XA_RESOURCE_MANAGER)
			reload = 1;
	}

	if (!reload)
		return;

	loadresources();
	for (int i = 0; i < LENGTH(blocks); i++)
		renderblock(i);
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
