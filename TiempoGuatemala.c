#include <stdio.h>
#include <sys/time.h>
#include <time.h>

int main() {
    struct timeval tv;
    gettimeofday(&tv, NULL);

    time_t current_time = tv.tv_sec;
    struct tm *tm_info = localtime(&current_time);

    int hour = tm_info->tm_hour;        // Hours (0-23)
    int minute = tm_info->tm_min;       // Minutes (0-59)
    int second = tm_info->tm_sec;       // Seconds (0-59)

    // Extract the fractional part of the second from the timeval structure
    int fraction = tv.tv_usec / 1000; // Convert microseconds to milliseconds

    printf("Local Time: %02d:%02d:%02d.%03d\n", hour, minute, second, fraction);

    return 0;
}