#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int main() {
    time_t currentTime = time(NULL);
    struct tm* localTime = localtime(&currentTime);
	char month[16];

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
   		
	printf("%s %d - %.2d:%.2d\n", month, localTime->tm_mday, localTime->tm_hour, localTime->tm_min);

    return 0;
}
