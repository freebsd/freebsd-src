#ifndef _G_time_h
#define _G_time_h

extern "C" {

#ifdef __time_h_recursive
#include_next <time.h>
#else
#define __time_h_recursive

#include <_G_config.h>

// A clean way to use and/or define time_t might allow removal of this crud.
#ifndef __sys_time_h_recursive
#define time __hide_time
#define clock __hide_clock
#define difftime __hide_difftime
#define gmtime __hide_gmtime
#define localtime __hide_localtime
#define ctime __hide_ctime
#define asctime __hide_asctime
#define strftime __hide_strftime
#endif
#define mktime __hide_mktime
#define tzset __hide_tzset
#define tzsetwall __hide_tzsetwall
#define getitimer __hide_getitimer
#define setitimer __hide_setitimer
#define gettimeofday __hide_gettimeofday
#define settimeofday __hide_settimeofday

#ifdef VMS
	struct  unix_time
	{
		long int	tv_sec;
		long int	tv_usec;
	};

	struct rusage
	{
		struct unix_time	ru_utime;
	};

#define RUSAGE_SELF 0		//define it, it will be unused
#else
#ifdef hpux
#define _INCLUDE_POSIX_SOURCE
#endif

#include_next <time.h>
#endif
#undef __time_h_recursive

#define time_h 1

#undef time
#undef clock
#undef difftime
#undef gmtime 
#undef localtime 
#undef asctime 
#undef ctime 
#undef mktime
#undef strftime 
#undef tzset 
#undef tzsetwall 
#undef getitimer
#undef setitimer
#undef gettimeofday
#undef settimeofday

extern char* asctime _G_ARGS((const struct tm*));
extern char* ctime _G_ARGS((const _G_time_t*));
double difftime _G_ARGS((_G_time_t, _G_time_t));
extern struct tm* gmtime _G_ARGS((const _G_time_t*));
extern struct tm* localtime _G_ARGS((const _G_time_t*));
extern _G_time_t mktime(struct tm*);
extern _G_size_t strftime _G_ARGS((char*,_G_size_t,const char*,const struct tm*));
extern void tzset();
extern void tzsetwall();

extern int getitimer(int, struct itimerval*);
extern int setitimer _G_ARGS((int, const struct itimerval*,struct itimerval*));
extern int gettimeofday(struct timeval*, struct timezone*);
extern int settimeofday _G_ARGS((const struct timeval*,const struct timezone*));
extern int stime _G_ARGS((const _G_time_t*));
extern int dysize(int);

#if defined(___AIX__)
int clock (void);
#elif defined(hpux)
unsigned long      clock(void);
#else
long      clock(void);
#endif
_G_time_t      time(_G_time_t*);
unsigned  ualarm(unsigned, unsigned);
#ifndef __386BSD__
unsigned   usleep(unsigned);
void       profil _G_ARGS((unsigned short*, _G_size_t, unsigned int, unsigned));
#else
void      usleep(unsigned);
int       profil _G_ARGS((char*, int, int, int));
#endif

#endif
}
#endif
