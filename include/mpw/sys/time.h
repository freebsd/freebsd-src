/* Imitation sys/time.h. */

#ifndef __SYS_TIME_H__
#define __SYS_TIME_H__

#include <time.h>

struct timeval {
  long tv_sec;
  long tv_usec;
};

#endif /* __SYS_TIME_H__ */
