#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#if defined(I_UNISTD) && defined(HAS_GETHOSTNAME)
# include <unistd.h>
#endif

/* a reasonable default */
#ifndef MAXHOSTNAMELEN
#  define MAXHOSTNAMELEN	256
#endif

/* swiped from POSIX.xs */
#if defined(__VMS) && !defined(__POSIX_SOURCE)
#  if ((__VMS_VER >= 70000000) && (__DECC_VER >= 50200000)) || (__CRTL_VER >= 70000000)
#    include <utsname.h>
#  endif
#endif

#ifdef I_SYSUTSNAME
#  include <sys/utsname.h>
#endif

MODULE = Sys::Hostname		PACKAGE = Sys::Hostname

void
ghname()
    PREINIT:
    IV   retval = -1;
    SV  *sv;
    PPCODE:
    EXTEND(SP, 1);
#ifdef HAS_GETHOSTNAME
    {
	char tmps[MAXHOSTNAMELEN];
	retval = PerlSock_gethostname(tmps, sizeof(tmps));
	sv = newSVpvn(tmps, strlen(tmps));
    }
#else
#  ifdef HAS_PHOSTNAME
    {
	PerlIO *io;
	char tmps[MAXHOSTNAMELEN];
	char   *p = tmps;
        char    c;
	io = PerlProc_popen(PHOSTNAME, "r");
	if (!io)
	    goto check_out;
	while (PerlIO_read(io, &c, sizeof(c)) == 1) {
	    if (isSPACE(c) || p - tmps >= sizeof(tmps))
		break;
	    *p++ = c;
	}
	PerlProc_pclose(io);
	*p = '\0';
	retval = 0;
	sv = newSVpvn(tmps, strlen(tmps));
    }
#  else
#    ifdef HAS_UNAME
    {
	struct utsname u;
	if (PerlEnv_uname(&u) == -1)
	    goto check_out;
	sv = newSVpvn(u.nodename, strlen(u.nodename));
        retval = 0;
    }
#    endif
#  endif
#endif
    check_out:
    if (retval == -1)
	XSRETURN_UNDEF;
    else
	PUSHs(sv_2mortal(sv));
