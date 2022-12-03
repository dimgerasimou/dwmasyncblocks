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
	
	printf("^b#44475a^^c#f8f8f2^  ^b#f1fa8c^^c#282A36^%.1f GiB^d^\n", (double)(memused) / 1024.0);

	return 0;
}
