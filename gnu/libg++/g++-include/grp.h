#ifndef grp_h

extern "C" {

#ifdef __grp_h_recursive
#include_next <grp.h>
#else
#define __grp_h_recursive

#include <stdio.h>

#define getgrent c_proto_getgrent
#define getgrgid c_proto_getgrgid
#define getgrnam c_proto_getgrnam
#define setgrent c_proto_setgrent
#define endgrent c_proto_endgrent
#define fgetgrent c_proto_fgetgrent

#include_next <grp.h>

#define grp_h 1

#undef getgrent
#undef getgrgid
#undef getgrnam

extern struct group* getgrent();
extern struct group* fgetgrent(FILE*);
extern struct group* getgrgid(int);
extern struct group* getgrnam(const char*);
#if defined(__OSF1__) || defined (__386BSD__)
extern int	     setgrent();
#else
extern void          setgrent();
#endif
extern void          endgrent();

#endif
}

#endif
