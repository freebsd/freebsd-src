/*
 * Copyright 1988 by the Massachusetts Institute of Technology.
 *
 * For copying and distribution information, please see the file
 * <mit-copyright.h>.
 *
 * Machine-type definitions: IBM PC 8086
 */

#if defined(_WIN32) && !defined(WIN32)
#define WIN32
#endif

#if ( defined(WIN16) || defined(WIN32) || defined(_WINDOWS)) && !defined(WINDOWS)
#define WINDOWS
#endif

#if defined(__OS2__) && !defined(OS2)
#define OS2
#endif

#ifdef WIN16
#define BITS16
#else
#ifdef MSDOS
#define BITS16
#else
#define BITS32
#endif
#endif
#define LSBFIRST

#define index(s,c) strchr(s,c)          /* PC version of index */
#define rindex(s,c) strrchr(s,c)
#if !defined(OS2) && !defined(LWP) /* utils.h under OS/2 */
#define bcmp(s1,s2,n) memcmp((s1),(s2),(n))
#define bcopy(a,b,c) memcpy( (b), (a), (c) )
#define bzero(a,b) memset( (a), 0, (b) )
#endif

typedef unsigned char u_char;
typedef unsigned long u_long;
typedef unsigned short u_short;
typedef unsigned int u_int;
#define NO_UIDGID_T

#if !defined(WINDOWS) && !defined(DWORD)
typedef long DWORD;
#endif

#if defined(PC)&&!defined(WINDOWS)
#ifndef LPSTR
typedef char *LPSTR;
typedef char *LPBYTE;
typedef char *CHARPTR;
typedef char *LPINT;
typedef unsigned int WORD;
#endif
#define LONG long
#define FAR
#define PASCAL
#define EXPORT
#endif

#ifdef OS2
#include <utils.h>
#define lstrcpy strcpy
#define lstrlen strlen
#define lstrcmp strcmp
#define lstrcpyn strncpy
#endif

#ifdef WIN32
#define _export
#endif

#if defined(BITS32)
#define far
#define near
#endif

#ifdef WINDOWS
#include <windows.h>
#endif

#ifdef WIN32
#include <windowsx.h>
#endif

#ifdef WIN16
#pragma message ( "WIN16 in " __FILE__ )
#include <time.h>
#include <process.h>
#ifndef KRB_INT32
#define KRB_INT32 long
#endif
#ifndef KRB_UINT32
#define KRB_UINT32 unsigned KRB_INT32
#endif
#endif


#define RANDOM_KRB_INT32_1 ((KRB_INT32) time(NULL))
#define RANDOM_KRB_INT32_2 ((KRB_INT32) getpid())
#define TIME_GMT_UNIXSEC unix_time_gmt_unixsec((unsigned KRB_INT32 *)0);
#ifndef MAXPATHLEN
#define MAXPATHLEN _MAX_PATH
#endif
