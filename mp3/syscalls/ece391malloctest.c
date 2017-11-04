#include <stdint.h>

#include "ece391support.h"
#include "ece391syscall.h"

#define BUFSIZE 1024

int main ()
{
    uint8_t buf[BUFSIZE];

	int32_t* a = (int32_t*) ece391_malloc(64);
	int32_t* b = (int32_t*) ece391_malloc(64);

    ece391_fdputs (1, (uint8_t*)"malloc test: a, b\n");
    ece391_itoa((int32_t)a, buf, 16);
    ece391_fdputs(1, buf);
    ece391_fdputs(1, (uint8_t*)"\n");
    ece391_itoa((int32_t)b, buf, 16);
    ece391_fdputs(1, buf);
    ece391_fdputs(1, (uint8_t*)"\n");

	*a = 12;
	*b = 39;

    ece391_free(a);
    ece391_free(b);

    a = (int32_t*) ece391_malloc(64);

    ece391_fdputs (1, (uint8_t*)"malloc test: a:\n");
    ece391_itoa((int32_t)a, buf, 16);
    ece391_fdputs(1, buf);
    ece391_fdputs(1, (uint8_t*)"\n");

    return 0;
}

