#include <stdio.h>
#include <string.h>

int main() {
	FILE* fp = popen("/bin/free -m", "r");
	char buffer[256];
	char temp[64];
	int memused;

	if (fp == NULL) {
		fprintf(stderr, "st-memory: Can't exec free.\n");
		return 1;
	}

	fgets(buffer, sizeof(buffer), fp);
	fgets(buffer, sizeof(buffer), fp);
	fclose(fp);
	sscanf(buffer, "%s %d %d", temp, &memused, &memused);

	if (memused > 1024) {
		printf("  %.1lf GiB\n", (double)(memused) / 1024.0);
	} else {
		printf("  %d MiB\n", memused);
	}

	return 0;
}
