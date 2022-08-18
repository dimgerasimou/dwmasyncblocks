#include <stdio.h>
#include <string.h>
#include <unistd.h>

void parsestring (char* string) {
	char temp[128];
	for (unsigned short i = 16; i < strlen(string); i++)
		temp[i-16] = string[i];
	temp[strlen(string) - 16] = '\0';
	sscanf(temp, "%s", string);
}

int main() {
	FILE* fp = popen("/bin/curl -sf \"https://wttr.in/Thessaloniki?0QTA\"", "r");
	char input[128];
	char forecast[64];
	int celsius;

	if (fp == NULL) {
		fprintf(stderr, "st-weather: Failed getting a weather report.\n");
		return 1;
	}

	fgets(input, sizeof(input), fp);
	parsestring(input);
	strcpy(forecast, input);
	
	fgets(input, sizeof(input), fp);
	parsestring(input);
	sscanf(input, "%d", &celsius);

	pclose(fp);

	printf("%s, %d°C\n", forecast, celsius);

	return 0;
}

