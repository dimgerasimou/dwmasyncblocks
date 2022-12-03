#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int main() {
    time_t currentTime = time(NULL);
    struct tm* localTime = localtime(&currentTime);
	char month[16];
	char day[16];

    switch(++localTime->tm_mon) {
		case 1:	strcpy(month, "Jan");
				break;
		case 2:	strcpy(month, "Feb");
				break;
		case 3: strcpy(month, "Mar");
				break;
		case 4: strcpy(month, "Apr");
				break;
		case 5: strcpy(month, "May");
				break;
		case 6: strcpy(month, "Jun");
				break;
		case 7: strcpy(month, "Jul");
				break;
		case 8: strcpy(month, "Aug");
				break;
		case 9: strcpy(month, "Sep");
				break;
		case 10: strcpy(month, "Oct");
				break;	
		case 11: strcpy(month, "Nov");
				break;	
		case 12: strcpy(month, "Dec");
				break;
		default: perror("st-time: Failed to get month.");
				 exit(EXIT_FAILURE);
	}

	switch(++localTime->tm_wday) {
		case 1:	strcpy(day, "Monday");
				break;
		case 2:	strcpy(day, "Tuesday");
				break;
		case 3: strcpy(day, "Wednesday");
				break;
		case 4: strcpy(day, "Thursday");
				break;
		case 5: strcpy(day, "Friday");
				break;
		case 6: strcpy(day, "Saturday");
				break;
		case 7: strcpy(day, "Sunday");
				break;
		default: perror("st-time: Failed to get weekday.");
				 exit(EXIT_FAILURE);
	}


	printf("^b#44475a^^c#f8f8f2^  ^b#ffb86c^^c#282A36^ %s, %d %s ^d^\n", day, localTime->tm_mday, month);

    return 0;
}
