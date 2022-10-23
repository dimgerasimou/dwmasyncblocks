#include <stdio.h>
#include <string.h>
#include <unistd.h>

void parsestring(char* string) {
	char temp[128];
	for (unsigned short i = 16; i < strlen(string); i++)
		temp[i-16] = string[i];
	temp[strlen(string) - 16] = '\0';
	sscanf(temp, "%[^\n]s", string);
}

void geticon(char* icon, char* string) {
	if (strstr(string, "Sunny") != 0 || strstr(string, "Clear") != 0)
		strcpy(icon, "☀");
	if (strstr(string, "rain") != 0 || strstr(string, "Rain") != 0)
		strcpy(icon, "🌧");
	if (strstr(string, "Cloudy") != 0)
		strcpy(icon, "☁");
	if (strstr(string, "Patchy rain") != 0)
		strcpy(icon, "🌦");
	if (strstr(string, "Partly cloudy") != 0)
		strcpy(icon, "⛅");
	if (strstr(string, "Thunderstorm") != 0 || strstr(string, "thunderstorm") != 0)
		strcpy(icon, "🌩");
	if (strstr(string, "rain with thunderstorm") != 0 || strstr(string, "Patchy light rain") != 0)
		strcpy(icon, "⛈");


}


int main(void) {
	FILE* fp = popen("/bin/curl -sf \"https://wttr.in/Thessaloniki?0QTA\"", "r");
	char input[128];
	char forecast[64];
	char icon[4] = "\0";
	int celsius;

	if (fp == NULL) {
		fprintf(stderr, "st-weather: Failed getting a weather report.\n");
		return 1;
	}

	fgets(input, sizeof(input), fp);
	geticon(icon, input);
	parsestring(input);
	strcpy(forecast, input);
	
	fgets(input, sizeof(input), fp);
	parsestring(input);
	sscanf(input, "%d", &celsius);

	pclose(fp);
	if (icon[0] != '\0')
		printf("%s %d°C\n", icon, celsius);
	else
		printf("%s, %d°C\n", forecast, celsius);

	return 0;
}

