#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#ifdef I_SYSLOG
#include <syslog.h>
#endif

static double
constant_LOG_NO(char *name, int len)
{
    switch (name[6 + 0]) {
    case 'T':
	if (strEQ(name + 6, "TICE")) {	/* LOG_NO removed */
#ifdef LOG_NOTICE
	    return LOG_NOTICE;
#else
	    goto not_there;
#endif
	}
    case 'W':
	if (strEQ(name + 6, "WAIT")) {	/* LOG_NO removed */
#ifdef LOG_NOWAIT
	    return LOG_NOWAIT;
#else
	    goto not_there;
#endif
	}
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

static double
constant_LOG_N(char *name, int len)
{
    switch (name[5 + 0]) {
    case 'D':
	if (strEQ(name + 5, "DELAY")) {	/* LOG_N removed */
#ifdef LOG_NDELAY
	    return LOG_NDELAY;
#else
	    goto not_there;
#endif
	}
    case 'E':
	if (strEQ(name + 5, "EWS")) {	/* LOG_N removed */
#ifdef LOG_NEWS
	    return LOG_NEWS;
#else
	    goto not_there;
#endif
	}
    case 'F':
	if (strEQ(name + 5, "FACILITIES")) {	/* LOG_N removed */
#ifdef LOG_NFACILITIES
	    return LOG_NFACILITIES;
#else
	    goto not_there;
#endif
	}
    case 'O':
	return constant_LOG_NO(name, len);
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

static double
constant_LOG_P(char *name, int len)
{
    switch (name[5 + 0]) {
    case 'I':
	if (strEQ(name + 5, "ID")) {	/* LOG_P removed */
#ifdef LOG_PID
	    return LOG_PID;
#else
	    goto not_there;
#endif
	}
    case 'R':
	if (strEQ(name + 5, "RIMASK")) {	/* LOG_P removed */
#ifdef LOG_PRIMASK
	    return LOG_PRIMASK;
#else
	    goto not_there;
#endif
	}
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

static double
constant_LOG_AU(char *name, int len)
{
    if (6 + 2 >= len ) {
	errno = EINVAL;
	return 0;
    }
    switch (name[6 + 2]) {
    case '\0':
	if (strEQ(name + 6, "TH")) {	/* LOG_AU removed */
#ifdef LOG_AUTH
	    return LOG_AUTH;
#else
	    goto not_there;
#endif
	}
    case 'P':
	if (strEQ(name + 6, "THPRIV")) {	/* LOG_AU removed */
#ifdef LOG_AUTHPRIV
	    return LOG_AUTHPRIV;
#else
	    goto not_there;
#endif
	}
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

static double
constant_LOG_A(char *name, int len)
{
    switch (name[5 + 0]) {
    case 'L':
	if (strEQ(name + 5, "LERT")) {	/* LOG_A removed */
#ifdef LOG_ALERT
	    return LOG_ALERT;
#else
	    goto not_there;
#endif
	}
    case 'U':
	return constant_LOG_AU(name, len);
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

static double
constant_LOG_CR(char *name, int len)
{
    switch (name[6 + 0]) {
    case 'I':
	if (strEQ(name + 6, "IT")) {	/* LOG_CR removed */
#ifdef LOG_CRIT
	    return LOG_CRIT;
#else
	    goto not_there;
#endif
	}
    case 'O':
	if (strEQ(name + 6, "ON")) {	/* LOG_CR removed */
#ifdef LOG_CRON
	    return LOG_CRON;
#else
	    goto not_there;
#endif
	}
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

static double
constant_LOG_C(char *name, int len)
{
    switch (name[5 + 0]) {
    case 'O':
	if (strEQ(name + 5, "ONS")) {	/* LOG_C removed */
#ifdef LOG_CONS
	    return LOG_CONS;
#else
	    goto not_there;
#endif
	}
    case 'R':
	return constant_LOG_CR(name, len);
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

static double
constant_LOG_D(char *name, int len)
{
    switch (name[5 + 0]) {
    case 'A':
	if (strEQ(name + 5, "AEMON")) {	/* LOG_D removed */
#ifdef LOG_DAEMON
	    return LOG_DAEMON;
#else
	    goto not_there;
#endif
	}
    case 'E':
	if (strEQ(name + 5, "EBUG")) {	/* LOG_D removed */
#ifdef LOG_DEBUG
	    return LOG_DEBUG;
#else
	    goto not_there;
#endif
	}
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

static double
constant_LOG_U(char *name, int len)
{
    switch (name[5 + 0]) {
    case 'S':
	if (strEQ(name + 5, "SER")) {	/* LOG_U removed */
#ifdef LOG_USER
	    return LOG_USER;
#else
	    goto not_there;
#endif
	}
    case 'U':
	if (strEQ(name + 5, "UCP")) {	/* LOG_U removed */
#ifdef LOG_UUCP
	    return LOG_UUCP;
#else
	    goto not_there;
#endif
	}
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

static double
constant_LOG_E(char *name, int len)
{
    switch (name[5 + 0]) {
    case 'M':
	if (strEQ(name + 5, "MERG")) {	/* LOG_E removed */
#ifdef LOG_EMERG
	    return LOG_EMERG;
#else
	    goto not_there;
#endif
	}
    case 'R':
	if (strEQ(name + 5, "RR")) {	/* LOG_E removed */
#ifdef LOG_ERR
	    return LOG_ERR;
#else
	    goto not_there;
#endif
	}
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

static double
constant_LOG_F(char *name, int len)
{
    switch (name[5 + 0]) {
    case 'A':
	if (strEQ(name + 5, "ACMASK")) {	/* LOG_F removed */
#ifdef LOG_FACMASK
	    return LOG_FACMASK;
#else
	    goto not_there;
#endif
	}
    case 'T':
	if (strEQ(name + 5, "TP")) {	/* LOG_F removed */
#ifdef LOG_FTP
	    return LOG_FTP;
#else
	    goto not_there;
#endif
	}
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

static double
constant_LOG_LO(char *name, int len)
{
    if (6 + 3 >= len ) {
	errno = EINVAL;
	return 0;
    }
    switch (name[6 + 3]) {
    case '0':
	if (strEQ(name + 6, "CAL0")) {	/* LOG_LO removed */
#ifdef LOG_LOCAL0
	    return LOG_LOCAL0;
#else
	    goto not_there;
#endif
	}
    case '1':
	if (strEQ(name + 6, "CAL1")) {	/* LOG_LO removed */
#ifdef LOG_LOCAL1
	    return LOG_LOCAL1;
#else
	    goto not_there;
#endif
	}
    case '2':
	if (strEQ(name + 6, "CAL2")) {	/* LOG_LO removed */
#ifdef LOG_LOCAL2
	    return LOG_LOCAL2;
#else
	    goto not_there;
#endif
	}
    case '3':
	if (strEQ(name + 6, "CAL3")) {	/* LOG_LO removed */
#ifdef LOG_LOCAL3
	    return LOG_LOCAL3;
#else
	    goto not_there;
#endif
	}
    case '4':
	if (strEQ(name + 6, "CAL4")) {	/* LOG_LO removed */
#ifdef LOG_LOCAL4
	    return LOG_LOCAL4;
#else
	    goto not_there;
#endif
	}
    case '5':
	if (strEQ(name + 6, "CAL5")) {	/* LOG_LO removed */
#ifdef LOG_LOCAL5
	    return LOG_LOCAL5;
#else
	    goto not_there;
#endif
	}
    case '6':
	if (strEQ(name + 6, "CAL6")) {	/* LOG_LO removed */
#ifdef LOG_LOCAL6
	    return LOG_LOCAL6;
#else
	    goto not_there;
#endif
	}
    case '7':
	if (strEQ(name + 6, "CAL7")) {	/* LOG_LO removed */
#ifdef LOG_LOCAL7
	    return LOG_LOCAL7;
#else
	    goto not_there;
#endif
	}
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

static double
constant_LOG_L(char *name, int len)
{
    switch (name[5 + 0]) {
    case 'F':
	if (strEQ(name + 5, "FMT")) {	/* LOG_L removed */
#ifdef LOG_LFMT
	    return LOG_LFMT;
#else
	    goto not_there;
#endif
	}
    case 'O':
	return constant_LOG_LO(name, len);
    case 'P':
	if (strEQ(name + 5, "PR")) {	/* LOG_L removed */
#ifdef LOG_LPR
	    return LOG_LPR;
#else
	    goto not_there;
#endif
	}
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

static double
constant(char *name, int len)
{
    errno = 0;
    if (0 + 4 >= len ) {
	errno = EINVAL;
	return 0;
    }
    switch (name[0 + 4]) {
    case 'A':
	if (!strnEQ(name + 0,"LOG_", 4))
	    break;
	return constant_LOG_A(name, len);
    case 'C':
	if (!strnEQ(name + 0,"LOG_", 4))
	    break;
	return constant_LOG_C(name, len);
    case 'D':
	if (!strnEQ(name + 0,"LOG_", 4))
	    break;
	return constant_LOG_D(name, len);
    case 'E':
	if (!strnEQ(name + 0,"LOG_", 4))
	    break;
	return constant_LOG_E(name, len);
    case 'F':
	if (!strnEQ(name + 0,"LOG_", 4))
	    break;
	return constant_LOG_F(name, len);
    case 'I':
	if (strEQ(name + 0, "LOG_INFO")) {	/*  removed */
#ifdef LOG_INFO
	    return LOG_INFO;
#else
	    goto not_there;
#endif
	}
    case 'K':
	if (strEQ(name + 0, "LOG_KERN")) {	/*  removed */
#ifdef LOG_KERN
	    return LOG_KERN;
#else
	    goto not_there;
#endif
	}
    case 'L':
	if (!strnEQ(name + 0,"LOG_", 4))
	    break;
	return constant_LOG_L(name, len);
    case 'M':
	if (strEQ(name + 0, "LOG_MAIL")) {	/*  removed */
#ifdef LOG_MAIL
	    return LOG_MAIL;
#else
	    goto not_there;
#endif
	}
    case 'N':
	if (!strnEQ(name + 0,"LOG_", 4))
	    break;
	return constant_LOG_N(name, len);
    case 'O':
	if (strEQ(name + 0, "LOG_ODELAY")) {	/*  removed */
#ifdef LOG_ODELAY
	    return LOG_ODELAY;
#else
	    goto not_there;
#endif
	}
    case 'P':
	if (!strnEQ(name + 0,"LOG_", 4))
	    break;
	return constant_LOG_P(name, len);
    case 'S':
	if (strEQ(name + 0, "LOG_SYSLOG")) {	/*  removed */
#ifdef LOG_SYSLOG
	    return LOG_SYSLOG;
#else
	    goto not_there;
#endif
	}
    case 'U':
	if (!strnEQ(name + 0,"LOG_", 4))
	    break;
	return constant_LOG_U(name, len);
    case 'W':
	if (strEQ(name + 0, "LOG_WARNING")) {	/*  removed */
#ifdef LOG_WARNING
	    return LOG_WARNING;
#else
	    goto not_there;
#endif
	}
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}


MODULE = Sys::Syslog		PACKAGE = Sys::Syslog		

char *
_PATH_LOG()
    CODE:
#ifdef _PATH_LOG
	RETVAL = _PATH_LOG;
#else
	RETVAL = "";
#endif
    OUTPUT:
	RETVAL

int
LOG_FAC(p)
    INPUT:
	int		p
    CODE:
#ifdef LOG_FAC
	RETVAL = LOG_FAC(p);
#else
	croak("Your vendor has not defined the Sys::Syslog macro LOG_FAC");
	RETVAL = -1;
#endif
    OUTPUT:
	RETVAL

int
LOG_PRI(p)
    INPUT:
	int		p
    CODE:
#ifdef LOG_PRI
	RETVAL = LOG_PRI(p);
#else
	croak("Your vendor has not defined the Sys::Syslog macro LOG_PRI");
	RETVAL = -1;
#endif
    OUTPUT:
	RETVAL

int
LOG_MAKEPRI(fac,pri)
    INPUT:
	int		fac
	int		pri
    CODE:
#ifdef LOG_MAKEPRI
	RETVAL = LOG_MAKEPRI(fac,pri);
#else
	croak("Your vendor has not defined the Sys::Syslog macro LOG_MAKEPRI");
	RETVAL = -1;
#endif
    OUTPUT:
	RETVAL

int
LOG_MASK(pri)
    INPUT:
	int		pri
    CODE:
#ifdef LOG_MASK
	RETVAL = LOG_MASK(pri);
#else
	croak("Your vendor has not defined the Sys::Syslog macro LOG_MASK");
	RETVAL = -1;
#endif
    OUTPUT:
	RETVAL

int
LOG_UPTO(pri)
    INPUT:
	int		pri
    CODE:
#ifdef LOG_UPTO
	RETVAL = LOG_UPTO(pri);
#else
	croak("Your vendor has not defined the Sys::Syslog macro LOG_UPTO");
	RETVAL = -1;
#endif
    OUTPUT:
	RETVAL


double
constant(sv)
    PREINIT:
	STRLEN		len;
    INPUT:
	SV *		sv
	char *		s = SvPV(sv, len);
    CODE:
	RETVAL = constant(s,len);
    OUTPUT:
	RETVAL

