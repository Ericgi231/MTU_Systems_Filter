#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <sys/time.h>

void main () {
    char buff[100];
    int res = read(0, buff, 100);
    write(2, buff, res);
    sleep(2);
    return;
}