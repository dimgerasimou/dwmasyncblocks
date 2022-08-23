#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct memorystruct {
	int total;
	int free;
	int buffers;
	int cached;
	int sreclaimable;
	int shmem;
};

int main() {
	FILE* fp = fopen("/proc/meminfo", "r");
	if (fp == NULL) {
		perror("st-memory: Unable to get meminfo.");
		exit(EXIT_FAILURE);
	}

	char buffer[128];
	char temp[128];
	char end = 0;
	
	struct memorystruct meminfo;

	while (fgets(buffer, sizeof(buffer), fp) != NULL && !end) {
		switch (buffer[0]) {
			case 'M':	if (strstr(buffer, "MemTotal:") != NULL)
							sscanf(buffer, "%s %d kB", temp, &meminfo.total);
						if (strstr(buffer, "MemFree:") != NULL)
							sscanf(buffer, "%s %d kB", temp, &meminfo.free);
						break;

			case 'B':	if (strstr(buffer, "Buffers:") != NULL)
							sscanf(buffer, "%s %d kB", temp, &meminfo.buffers);
						break;

			case 'C':	if (strstr(buffer, "Cached:") != NULL)
							sscanf(buffer, "%s %d kB", temp, &meminfo.cached);
						break;

			case 'S':	if (strstr(buffer, "Shmem:") != NULL)
							sscanf(buffer, "%s %d kB", temp, &meminfo.shmem);
						if (strstr(buffer, "SReclaimable") != NULL) {
							sscanf(buffer, "%s %d kB", temp, &meminfo.sreclaimable);
							end = 1;
						}
						break;
			default:	break;
		}
	}
	fclose(fp);
	int usedmem = meminfo.total - meminfo.free - meminfo.buffers - meminfo.cached - meminfo.shmem - meminfo.sreclaimable;
	usedmem = usedmem/1024;
	if (usedmem > 1000)
		printf("  %.1f GiB\n", usedmem/1024.0);
	else
		printf("  %d MiB\n", usedmem);
	return 0;
}

