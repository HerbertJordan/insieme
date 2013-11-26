#include <sys/param.h>
#include <sys/types.h>
#include <sys/times.h>
#include <time.h>

double mysecond()
{
    long sec;
    double secx;
    struct tms realbuf;

    times(&realbuf);
    secx = ( realbuf.tms_stime + realbuf.tms_utime ) / (float) CLOCKS_PER_SEC;
    return ((double) secx);
}
