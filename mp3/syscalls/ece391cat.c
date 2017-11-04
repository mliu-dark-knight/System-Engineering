#include <stdint.h>

#include "ece391support.h"
#include "ece391syscall.h"

#define BUFSIZE 1024

int main ()
{
    int32_t fd, cnt;
    uint8_t buf[1024];

    if (0 != ece391_getargs (buf, 1024)) {
        ece391_fdputs (1, (uint8_t*)"could not read arguments\n");
	return 3;
    }

    if (-1 == (fd = ece391_open (buf))) {
        ece391_fdputs (1, (uint8_t*)"file not found\n");
	return 2;
    }

    ece391_fdputs (1, (uint8_t*)"Enter the Test Number: (0): read, (1): write\n");
    if (-1 == (cnt = ece391_read(0, buf, BUFSIZE-1)) ) {
        ece391_fdputs(1, (uint8_t*)"Can't read the number from keyboard.\n");
     return 3;
    }
    buf[cnt] = '\0';

    if ((ece391_strlen(buf) > 2) || ((ece391_strlen(buf) == 2) && ((buf[0] < '0') || (buf[0] > '1')))) {
        ece391_fdputs(1, (uint8_t*)"Wrong Choice!\n");
        return 0;
    } 

    if (buf[0] == '0') {
        while (0 != (cnt = ece391_read (fd, buf, 1024))) {
            if (-1 == cnt) {
                ece391_fdputs (1, (uint8_t*)"file read failed\n");
                return 3;
            }
            if (-1 == ece391_write (1, buf, cnt))
                return 3;
        }
    }
    else {
        while (1) {
            ece391_fdputs (1, (uint8_t*)"Content to write to file:\n");
            cnt = ece391_read (0, buf, 1024);
            ece391_write (fd, buf, cnt);

            ece391_fdputs (1, (uint8_t*)"Enter the Test Number: (0): write, (1): exit\n");

            if (-1 == (cnt = ece391_read(0, buf, BUFSIZE-1)) ) {
                ece391_fdputs(1, (uint8_t*)"Can't read the number from keyboard.\n");
                return 3;
            }

            buf[cnt] = '\0';

            if ((ece391_strlen(buf) > 2) || ((ece391_strlen(buf) == 2) && ((buf[0] < '0') || (buf[0] > '1')))) {
                ece391_fdputs(1, (uint8_t*)"Wrong Choice!\n");
                return 0;
            }
            if (buf[0] == '1')
                break;
        }
    }

    return 0;
}

