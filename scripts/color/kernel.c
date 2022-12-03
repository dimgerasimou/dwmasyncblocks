#include <stdio.h>
#include <string.h>
#include <sys/utsname.h>

int main() {
	struct utsname buffer;
	char* release;

	if (uname(&buffer)) {
		puts("Kern err.");
		return 1;
	}

	release = strtok(buffer.release, "-");

	printf("^b#44475a^^c#f8f8f2^ 🐧 ^b#dfb86c^^c#282A36^ %s ^d^\n", release);
	
	return 0;
}
