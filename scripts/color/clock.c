#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int main() {
    time_t currentTime = time(NULL);
    struct tm* localTime = localtime(&currentTime);

	printf("^b#44475a^^c#f8f8f2^  ^b#FF5555^^c#282A36^ %.2d:%.2d:%.2d ^d^\n", localTime->tm_hour, localTime->tm_min, localTime->tm_sec);

    return 0;
}
