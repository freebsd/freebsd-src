#ifndef __SYS_RESOURCE_H__
#define __SYS_RESOURCE_H__

struct rusage {
  struct timeval ru_utime;
  struct timeval ru_stime;
};

#endif /* __SYS_RESOURCE_H__ */
