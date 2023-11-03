#include <stdio.h>
#include <string.h>
#include <sys/time.h>

int main() {
    struct timeval currentTime;
    gettimeofday(&currentTime, NULL);

    printf("time: %ld.%ld\n ", currentTime.tv_sec, currentTime.tv_usec);

    char timeStrings[20], timeStringm[20];
    sprintf(timeStrings, "%ld", currentTime.tv_sec);
    sprintf(timeStringm, "%ld", currentTime.tv_usec);

    char lastFourDigitss[5], lastThreeDigitsm[4];
    strncpy(lastFourDigitss, &timeStrings[strlen(timeStrings) - 4], 4);
    lastFourDigitss[4] = '\0'; // Null-terminate the string

    strncpy(lastThreeDigitsm, &timeStringm[strlen(timeStringm) - 3], 3);
    lastThreeDigitsm[3] = '\0'; // Null-terminate the string

    printf("Last four digits seconds: %s\n", lastFourDigitss);
    printf("Last three digits microseconds: %s\n", lastThreeDigitsm);

    return 0;
}