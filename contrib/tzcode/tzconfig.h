#ifndef TZCONFIG_H_INCLUDED
#define TZCONFIG_H_INCLUDED

#define TM_GMTOFF			tm_gmtoff
#define TM_ZONE				tm_zone

#define FREE_PRESERVES_ERRNO		false
#define HAVE_GETTEXT			false
#define HAVE_ISSETUGID			true
#define HAVE___ISTHREADED		true
#define HAVE_MEMPCPY			true
#define HAVE_PWD_H			true
#define HAVE_SETMODE			true
#define HAVE_STRUCT_STAT_ST_CTIM	true
#define HAVE_SYS_STAT_H			true
#define HAVE_UNISTD_H			true
#define HAVE_STDINT_H			true
#define OPENAT_TZDIR			true
#define THREAD_PREFER_SINGLE		true
#define THREAD_TM_MULTI			true

#define PCTS				1
#define NETBSD_INSPIRED			0
#define STD_INSPIRED			1
#define HAVE_TZNAME			2
#define USG_COMPAT			2
#define ALTZONE				0

#define GRANDPARENTED "Local time zone must be set--use tzsetup"

#endif /* !TZCONFIG_H_INCLUDED */
