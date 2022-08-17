#include <stdio.h>
#include <string.h>
#include <time.h>

int main() {
    time_t currentTime = time(NULL);
    struct tm * localTime;

    localTime = localtime(&currentTime);

    char icon[5];

    if (localTime->tm_hour == 1 || localTime->tm_hour == 13)
		strcpy(icon, "🕐");
    else if (localTime->tm_hour == 2 || localTime->tm_hour == 14)
		strcpy(icon, "🕑");
    else if (localTime->tm_hour == 3 || localTime->tm_hour == 15)
		strcpy(icon, "🕒");
    else if (localTime->tm_hour == 4 || localTime->tm_hour == 16)
		strcpy(icon, "🕓");
    else if (localTime->tm_hour == 5 || localTime->tm_hour == 17)
		strcpy(icon, "🕔");
    else if (localTime->tm_hour == 6 || localTime->tm_hour == 18)
		strcpy(icon, "🕕");
    else if (localTime->tm_hour == 7 || localTime->tm_hour == 19)
		strcpy(icon, "🕖");
    else if (localTime->tm_hour == 8 || localTime->tm_hour == 20)
		strcpy(icon, "🕗");
    else if (localTime->tm_hour == 9 || localTime->tm_hour == 21)
		strcpy(icon, "🕘");
    else if (localTime->tm_hour == 10 || localTime->tm_hour == 22)
		strcpy(icon, "🕙");
    else if (localTime->tm_hour == 11 || localTime->tm_hour == 23)
		strcpy(icon, "🕚");
    else
		strcpy(icon, "🕛");

    printf("%s %.2d:%.2d\n", icon, localTime->tm_hour, localTime->tm_min);

    return 0;
}