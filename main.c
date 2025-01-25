#include <stdio.h>
#include <time.h>

int main() {
    time_t t;
    time(&t);

    printf("current time is: %s", ctime(&t));

    return 0;
}