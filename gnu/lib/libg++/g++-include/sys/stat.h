#ifndef __libgxx_sys_stat_h

extern "C"
{
#ifdef __sys_stat_h_recursive
#include_next <sys/stat.h>
#else
#define __sys_stat_h_recursive
#include <_G_config.h>
#define chmod __hide_chmod
#ifdef VMS
#include "GNU_CC_INCLUDE:[sys]stat.h"
#else
#include_next <sys/stat.h>
#endif
#undef chmod

#define __libgxx_sys_stat_h 1

extern int       chmod  _G_ARGS((const char*, _G_mode_t));
extern int       stat _G_ARGS((const char *path, struct stat *buf));
extern int       lstat _G_ARGS((const char *path, struct stat *buf));
extern int       fstat _G_ARGS((int fd, struct stat *buf));

#ifndef S_ISDIR
#define S_ISDIR(mode) (((mode) & S_IFMT) == S_IFDIR)
#endif
#ifndef S_ISBLK
#define S_ISBLK(mode) (((mode) & S_IFMT) == S_IFBLK)
#endif
#ifndef S_ISCHR
#define S_ISCHR(mode) (((mode) & S_IFMT) == S_IFCHR)
#endif
#ifndef S_ISFIFO
#define S_ISFIFO(mode) (((mode) & S_IFMT) == S_IFFIFO)
#endif
#ifndef S_ISREG
#define S_ISREG(mode) (((mode) & S_IFMT) == S_IFREG)
#endif
#if !defined(S_ISLNK) && defined(S_IFLNK)
#define S_ISLNK(mode) (((mode) & S_IFMT) == S_IFLNK)
#endif

#endif
}

#endif
