/* See LICENSE file for copyright and license details.
 *
 * dwmblocksctl is just a helper for updating blocks, usefull
 * when combined with commands that change the blocks' text,
 * thus eliminating the need for them to constantly upgrade.
 *
 * It can send the corresponding update signal to dwm of a given
 * block name, print the update signals along with the period and
 * restart dwmblocks.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <unistd.h>
#include <signal.h>

#define CMDLENGTH    70
#define LENGTH(X)    (int)(sizeof(X) / sizeof(X[0]))
#define MAX(a, b)    (a > b ? a : b)

typedef const struct {
	const char *command;
	const unsigned int interval;
	const unsigned int signal;
} Block;

#include "config.h"

char cmds[LENGTH(blocks)][CMDLENGTH];
static pid_t dwmblocks_pID = -1;

void
structinit(void)
{
	char temp[CMDLENGTH];
	char *ptr;

	for (int i = 0; i < LENGTH(blocks); i++) {
		ptr = strrchr(blocks[i].command,'/');
		strcpy(temp, ++ptr);
		if ((ptr = strrchr(temp,'"')) != NULL)
			*ptr = '\0';
		
		if ((ptr = strchr(temp,'"')) == NULL)
			ptr = temp;
		else
		 	ptr++;
		strcpy(cmds[i], ptr);			
	}
}

int
getdigits(int num)
{
	int ret = 0;
	int tempnum = num;

	do {
		++ret; 
		tempnum /= 10;
	} while (tempnum);
	return ret;
}

void
structstat(void)
{
	int cmdl = 0, intl = 0, sigl = 0;
	int cmdt, intt, sigt;
	for (int i = 0; i < LENGTH(blocks); i++) {
		cmdt = strlen(cmds[i]);
		intt = getdigits(blocks[i].interval);
		sigt = getdigits(blocks[i].signal);
		cmdl = MAX(cmdt, cmdl);
		intl = MAX(intt, intl);
		sigl = MAX(sigt, sigl);
	}
	cmdl = MAX(9, cmdl+2);
	intl = MAX(16, intl+2);
	sigl = MAX(14, sigl+2);
	for(int i = 0; i < cmdl + intl + sigl + 4; i++)
		putchar('-');
	putchar('\n');
	printf("|%*s|%*s|%*s|\n", cmdl, "Command", intl, "Update interval", sigl, "Update signal");
	for(int i = 0; i < cmdl + intl + sigl + 4; i++)
		putchar('-');
	putchar('\n');
	for (int i = 0; i < LENGTH(blocks); i++) {
		printf("|%*s|%*d|%*d|\n", cmdl, cmds[i], intl, blocks[i].interval, sigl, blocks[i].signal);
	}
	for(int i = 0; i < cmdl + intl + sigl + 4; i++)
		putchar('-');
	putchar('\n');
}

int
stringisnum(char *str)
{
	for (int i = 0; i < strlen(str); i++)
		if (str[i] > '9' || str[i] < '0')
			return 0;
	return 1;
}

void
freedirstruct(struct dirent** input, int n)
{
	while (n--) {
		if (input[n] != NULL)
			free(input[n]);
	}
	if (input != NULL)
		free(input);
}

int
getsignalnum(char *name)
{
	for(int i = 0; i < LENGTH(blocks); i++) {
		if(strcmp(name, cmds[i]) == 0) {
			return blocks[i].signal;
		}
	}
	return -1;
}

void
getpidofdwmblocks(void)
{
	struct dirent** proclist;
	FILE *fp;
	char buffer[128];
	char filename[256];
	int proccesscount;

	proccesscount = scandir("/proc", &proclist, NULL, NULL);
	if (proccesscount == -1) {
		perror("dwmblocksctl:Failed to scan '/proc' directory");
		exit(EXIT_FAILURE);
	}
	for (int i = 0 ; i < proccesscount; i++) {
		if (stringisnum(proclist[i]->d_name)) {
			strcpy(filename, "/proc/");
			strcat(filename, proclist[i]->d_name);
			strcat(filename, "/cmdline");
			fp = fopen(filename, "r");
			if (fp == NULL) {
				continue;
			}
			fgets(buffer, sizeof(buffer), fp);
			fclose(fp);
			if (strstr(buffer, "dwmblocks") != NULL) { 
				dwmblocks_pID = strtol(proclist[i]->d_name, NULL, 10);
				break;
			}
		}
	}
	freedirstruct(proclist, proccesscount);
}

void
pkill(char *name)
{
	
	if (dwmblocks_pID == -1) {
		getpidofdwmblocks();
		if(dwmblocks_pID == -1) {
			perror("dwmblocksctl:Cannot get pId of dwmblocks");
			exit(EXIT_FAILURE);
		}
	}
	if (!strcmp(name,"dwmblocks-all")) {
		kill(dwmblocks_pID, SIGUSR1);
		return;
	}
	if (!strcmp(name,"dwmblocks")) {
		kill(dwmblocks_pID, SIGTERM);
		return;
	}
	int signalnum = getsignalnum(name);
	kill(dwmblocks_pID, SIGRTMIN + signalnum);
}

void
usage(void)
{
	puts("usage: dwmblocksctl [-arth] [-s block]");
}

void
executedwmblocks(void)
{
	pid_t pID;
	pID = fork();
	if (pID < 0) {
		perror("dwmblocksctl:Failed in forking");
		exit(EXIT_FAILURE);
	} else if (pID == 0) {
		setsid();
		execl("/usr/local/bin/dwmblocks", "dwmblocks", NULL);
		exit(EXIT_SUCCESS);
	} else {
		exit(EXIT_SUCCESS);
	}
}

void
help(void)
{
	puts("Program that controls the dwmblocks status bar.");
	puts("usage: dwmblocksctl [-arth] [-s block]");
	puts("");
	puts("Options:");
	puts("\t-a         Update all blocks.");
	puts("\t-r         Restart dwmblocks.");
	puts("\t-t         Show the active commands, along with their details.");
	puts("\t-h         Prints this help prompt.");
	puts("\t-s [block] Specify a block to update.");
}

int
main(int argc, char *argv[])
{
	int i = 0;

	if (argc < 2)
		usage();

	structinit();
	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-a")) {
			pkill("dwmblocks-all");	
		} else if (!strcmp(argv[i], "-r")) {
			pkill("dwmblocks");
			executedwmblocks();
		} else if (!strcmp(argv[i], "-t")) {
			structstat();
		} else if (!strcmp(argv[i], "-h")) {
			help();
		} else if (!strcmp(argv[i], "-s")) {
			pkill(argv[++i]);
		} else {
			usage();
		}
	}
	return 0;
}
