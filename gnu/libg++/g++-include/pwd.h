#ifndef _pwd_h

// the Interviews-based standard kludge again

extern "C" {

#ifdef __pwd_h_recursive
#include_next <pwd.h>
#else
#define getpwent c_proto_getpwent
#define getpwuid c_proto_getpwuid
#define getpwnam c_proto_getpwnam
#define setpwent c_proto_setpwent
#define endpwent c_proto_endpwent

#define __pwd_h_recursive
#include_next <pwd.h>

#define _pwd_h 1

#undef getpwent
#undef getpwuid
#undef getpwnam
#undef setpwent
#undef endpwent

extern struct passwd* getpwent();
extern struct passwd* getpwuid(int);
extern struct passwd* getpwnam(const char*);
extern int            setpwent();
extern int            endpwent();

#endif
}

#endif
