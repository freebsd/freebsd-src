/* pam_time module */

/*
 * $Id: pam_time.c,v 1.3 2000/11/26 07:32:39 agmorgan Exp $
 *
 * Written by Andrew Morgan <morgan@linux.kernel.org> 1996/6/22
 * (File syntax and much other inspiration from the shadow package
 * shadow-960129)
 */

const static char rcsid[] =
"$Id: pam_time.c,v 1.3 2000/11/26 07:32:39 agmorgan Exp $;\n"
"\t\tVersion 0.22 for Linux-PAM\n"
"Copyright (C) Andrew G. Morgan 1996 <morgan@linux.kernel.org>\n";

#include <security/_pam_aconf.h>

#include <sys/file.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <stdarg.h>
#include <time.h>
#include <syslog.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifdef DEFAULT_CONF_FILE
# define PAM_TIME_CONF         DEFAULT_CONF_FILE /* from external define */
#else
# define PAM_TIME_CONF         "/etc/security/time.conf"
#endif
#define PAM_TIME_BUFLEN        1000
#define FIELD_SEPARATOR        ';'   /* this is new as of .02 */

typedef enum { FALSE, TRUE } boolean;
typedef enum { AND, OR } operator;

/*
 * here, we make definitions for the externally accessible functions
 * in this file (these definitions are required for static modules
 * but strongly encouraged generally) they are used to instruct the
 * modules include file to define their prototypes.
 */

#define PAM_SM_ACCOUNT

#include <security/_pam_macros.h>
#include <security/pam_modules.h>

/* --- static functions for checking whether the user should be let in --- */

static void _log_err(const char *format, ... )
{
    va_list args;

    va_start(args, format);
    openlog("pam_time", LOG_CONS|LOG_PID, LOG_AUTH);
    vsyslog(LOG_CRIT, format, args);
    va_end(args);
    closelog();
}

static void shift_bytes(char *mem, int from, int by)
{
    while (by-- > 0) {
	*mem = mem[from];
	++mem;
    }
}

static int read_field(int fd, char **buf, int *from, int *to)
{
    /* is buf set ? */

    if (! *buf) {
	*buf = (char *) malloc(PAM_TIME_BUFLEN);
	if (! *buf) {
	    _log_err("out of memory");
	    D(("no memory"));
	    return -1;
	}
	*from = *to = 0;
	fd = open(PAM_TIME_CONF, O_RDONLY);
    }

    /* do we have a file open ? return error */

    if (fd < 0 && *to <= 0) {
	_log_err( PAM_TIME_CONF " not opened");
	memset(*buf, 0, PAM_TIME_BUFLEN);
	_pam_drop(*buf);
	return -1;
    }

    /* check if there was a newline last time */

    if ((*to > *from) && (*to > 0)
	&& ((*buf)[*from] == '\0')) { /* previous line ended */
	(*from)++;
	(*buf)[0] = '\0';
	return fd;
    }

    /* ready for more data: first shift the buffer's remaining data */

    *to -= *from;
    shift_bytes(*buf, *from, *to);
    *from = 0;
    (*buf)[*to] = '\0';

    while (fd >= 0 && *to < PAM_TIME_BUFLEN) {
	int i;

	/* now try to fill the remainder of the buffer */

	i = read(fd, *to + *buf, PAM_TIME_BUFLEN - *to);
	if (i < 0) {
	    _log_err("error reading " PAM_TIME_CONF);
	    return -1;
	} else if (!i) {
	    close(fd);
	    fd = -1;          /* end of file reached */
	} else
	    *to += i;
    
	/*
	 * contract the buffer. Delete any comments, and replace all
	 * multiple spaces with single commas
	 */

	i = 0;
#ifdef DEBUG_DUMP
	D(("buffer=<%s>",*buf));
#endif
	while (i < *to) {
	    if ((*buf)[i] == ',') {
		int j;

		for (j=++i; j<*to && (*buf)[j] == ','; ++j);
		if (j!=i) {
		    shift_bytes(i + (*buf), j-i, (*to) - j);
		    *to -= j-i;
		}
	    }
	    switch ((*buf)[i]) {
		int j,c;
	    case '#':
		for (j=i; j < *to && (c = (*buf)[j]) != '\n'; ++j);
		if (j >= *to) {
		    (*buf)[*to = ++i] = '\0';
		} else if (c == '\n') {
		    shift_bytes(i + (*buf), j-i, (*to) - j);
		    *to -= j-i;
		    ++i;
		} else {
		    _log_err("internal error in " __FILE__
			     " at line %d", __LINE__ );
		    return -1;
		}
		break;
	    case '\\':
		if ((*buf)[i+1] == '\n') {
		    shift_bytes(i + *buf, 2, *to - (i+2));
		    *to -= 2;
		} else {
		    ++i;   /* we don't escape non-newline characters */
		}
		break;
	    case '!':
	    case ' ':
	    case '\t':
		if ((*buf)[i] != '!')
		    (*buf)[i] = ',';
		/* delete any trailing spaces */
		for (j=++i; j < *to && ( (c = (*buf)[j]) == ' '
					 || c == '\t' ); ++j);
		shift_bytes(i + *buf, j-i, (*to)-j );
		*to -= j-i;
		break;
	    default:
		++i;
	    }
	}
    }

    (*buf)[*to] = '\0';

    /* now return the next field (set the from/to markers) */
    {
	int i;

	for (i=0; i<*to; ++i) {
	    switch ((*buf)[i]) {
	    case '#':
	    case '\n':               /* end of the line/file */
		(*buf)[i] = '\0';
		*from = i;
		return fd;
	    case FIELD_SEPARATOR:    /* end of the field */
		(*buf)[i] = '\0';
	    *from = ++i;
	    return fd;
	    }
	}
	*from = i;
	(*buf)[*from] = '\0';
    }

    if (*to <= 0) {
	D(("[end of text]"));
	*buf = NULL;
    }

    return fd;
}

/* read a member from a field */

static int logic_member(const char *string, int *at)
{
     int len,c,to;
     int done=0;
     int token=0;

     len=0;
     to=*at;
     do {
	  c = string[to++];

	  switch (c) {

	  case '\0':
	       --to;
	       done = 1;
	       break;

	  case '&':
	  case '|':
	  case '!':
	       if (token) {
		    --to;
	       }
	       done = 1;
	       break;

	  default:
	       if (isalpha(c) || c == '*' || isdigit(c) || c == '_'
		    || c == '-' || c == '.' || c == '/') {
		    token = 1;
	       } else if (token) {
		    --to;
		    done = 1;
	       } else {
		    ++*at;
	       }
	  }
     } while (!done);

     return to - *at;
}

typedef enum { VAL, OP } expect;

static boolean logic_field(const void *me, const char *x, int rule,
			   boolean (*agrees)(const void *, const char *
					     , int, int))
{
     boolean left=FALSE, right, not=FALSE;
     operator oper=OR;
     int at=0, l;
     expect next=VAL;

     while ((l = logic_member(x,&at))) {
	  int c = x[at];

	  if (next == VAL) {
	       if (c == '!')
		    not = !not;
	       else if (isalpha(c) || c == '*') {
		    right = not ^ agrees(me, x+at, l, rule);
		    if (oper == AND)
			 left &= right;
		    else
			 left |= right;
		    next = OP;
	       } else {
		    _log_err("garbled syntax; expected name (rule #%d)", rule);
		    return FALSE;
	       }
	  } else {   /* OP */
	       switch (c) {
	       case '&':
		    oper = AND;
		    break;
	       case '|':
		    oper = OR;
		    break;
	       default:
		    _log_err("garbled syntax; expected & or | (rule #%d)"
			     , rule);
		    D(("%c at %d",c,at));
		    return FALSE;
	       }
	       next = VAL;
	  }
	  at += l;
     }

     return left;
}

static boolean is_same(const void *A, const char *b, int len, int rule)
{
     int i;
     const char *a;

     a = A;
     for (i=0; len > 0; ++i, --len) {
	  if (b[i] != a[i]) {
	       if (b[i++] == '*') {
		    return (!--len || !strncmp(b+i,a+strlen(a)-len,len));
	       } else
		    return FALSE;
	  }
     }
     return ( !len );
}

typedef struct {
     int day;             /* array of 7 bits, one set for today */
     int minute;            /* integer, hour*100+minute for now */
} TIME;

struct day {
     const char *d;
     int bit;
} static const days[11] = {
     { "su", 01 },
     { "mo", 02 },
     { "tu", 04 },
     { "we", 010 },
     { "th", 020 },
     { "fr", 040 },
     { "sa", 0100 },
     { "wk", 076 },
     { "wd", 0101 },
     { "al", 0177 },
     { NULL, 0 }
};

static TIME time_now(void)
{
     struct tm *local;
     time_t the_time;
     TIME this;

     the_time = time((time_t *)0);                /* get the current time */
     local = localtime(&the_time);
     this.day = days[local->tm_wday].bit;
     this.minute = local->tm_hour*100 + local->tm_min;

     D(("day: 0%o, time: %.4d", this.day, this.minute));
     return this;
}

/* take the current date and see if the range "date" passes it */
static boolean check_time(const void *AT, const char *times, int len, int rule)
{
     boolean not,pass;
     int marked_day, time_start, time_end;
     const TIME *at;
     int i,j=0;

     at = AT;
     D(("chcking: 0%o/%.4d vs. %s", at->day, at->minute, times));

     if (times == NULL) {
	  /* this should not happen */
	  _log_err("internal error: " __FILE__ " line %d", __LINE__);
	  return FALSE;
     }

     if (times[j] == '!') {
	  ++j;
	  not = TRUE;
     } else {
	  not = FALSE;
     }

     for (marked_day = 0; len > 0 && isalpha(times[j]); --len) {
	  int this_day=-1;

	  D(("%c%c ?", times[j], times[j+1]));
	  for (i=0; days[i].d != NULL; ++i) {
	       if (tolower(times[j]) == days[i].d[0]
		   && tolower(times[j+1]) == days[i].d[1] ) {
		    this_day = days[i].bit;
		    break;
	       }
	  }
	  j += 2;
	  if (this_day == -1) {
	       _log_err("bad day specified (rule #%d)", rule);
	       return FALSE;
	  }
	  marked_day ^= this_day;
     }
     if (marked_day == 0) {
	  _log_err("no day specified");
	  return FALSE;
     }
     D(("day range = 0%o", marked_day));

     time_start = 0;
     for (i=0; len > 0 && i < 4 && isdigit(times[i+j]); ++i, --len) {
	  time_start *= 10;
	  time_start += times[i+j]-'0';        /* is this portable? */
     }
     j += i;

     if (times[j] == '-') {
	  time_end = 0;
	  for (i=1; len > 0 && i < 5 && isdigit(times[i+j]); ++i, --len) {
	       time_end *= 10;
	       time_end += times[i+j]-'0';    /* is this portable */
	  }
	  j += i;
     } else
	  time_end = -1;

     D(("i=%d, time_end=%d, times[j]='%c'", i, time_end, times[j]));
     if (i != 5 || time_end == -1) {
	  _log_err("no/bad times specified (rule #%d)", rule);
	  return TRUE;
     }
     D(("times(%d to %d)", time_start,time_end));
     D(("marked_day = 0%o", marked_day));

     /* compare with the actual time now */

     pass = FALSE;
     if (time_start < time_end) {    /* start < end ? --> same day */
	  if ((at->day & marked_day) && (at->minute >= time_start)
	      && (at->minute < time_end)) {
	       D(("time is listed"));
	       pass = TRUE;
	  }
     } else {                                    /* spans two days */
	  if ((at->day & marked_day) && (at->minute >= time_start)) {
	       D(("caught on first day"));
	       pass = TRUE;
	  } else {
	       marked_day <<= 1;
	       marked_day |= (marked_day & 0200) ? 1:0;
	       D(("next day = 0%o", marked_day));
	       if ((at->day & marked_day) && (at->minute <= time_end)) {
		    D(("caught on second day"));
		    pass = TRUE;
	       }
	  }
     }

     return (not ^ pass);
}

static int check_account(const char *service
			 , const char *tty, const char *user)
{
     int from=0,to=0,fd=-1;
     char *buffer=NULL;
     int count=0;
     TIME here_and_now;
     int retval=PAM_SUCCESS;

     here_and_now = time_now();                     /* find current time */
     do {
	  boolean good=TRUE,intime;

	  /* here we get the service name field */

	  fd = read_field(fd,&buffer,&from,&to);

	  if (!buffer || !buffer[0]) {
	       /* empty line .. ? */
	       continue;
	  }
	  ++count;

	  good = logic_field(service, buffer, count, is_same);
	  D(("with service: %s", good ? "passes":"fails" ));

	  /* here we get the terminal name field */

	  fd = read_field(fd,&buffer,&from,&to);
	  if (!buffer || !buffer[0]) {
	       _log_err(PAM_TIME_CONF "; no tty entry #%d", count);
	       continue;
	  }
	  good &= logic_field(tty, buffer, count, is_same);
	  D(("with tty: %s", good ? "passes":"fails" ));

	  /* here we get the username field */

	  fd = read_field(fd,&buffer,&from,&to);
	  if (!buffer || !buffer[0]) {
	       _log_err(PAM_TIME_CONF "; no user entry #%d", count);
	       continue;
	  }
	  good &= logic_field(user, buffer, count, is_same);
	  D(("with user: %s", good ? "passes":"fails" ));

	  /* here we get the time field */

	  fd = read_field(fd,&buffer,&from,&to);
	  if (!buffer || !buffer[0]) {
	       _log_err(PAM_TIME_CONF "; no time entry #%d", count);
	       continue;
	  }

	  intime = logic_field(&here_and_now, buffer, count, check_time);
	  D(("with time: %s", intime ? "passes":"fails" ));

	  fd = read_field(fd,&buffer,&from,&to);
	  if (buffer && buffer[0]) {
	       _log_err(PAM_TIME_CONF "; poorly terminated rule #%d", count);
	       continue;
	  }

	  if (good && !intime) {
	       /*
		* for security parse whole file..  also need to ensure
		* that the buffer is free()'d and the file is closed.
		*/
	       retval = PAM_PERM_DENIED;
	  } else {
	       D(("rule passed"));
	  }
     } while (buffer);

     return retval;
}

/* --- public account management functions --- */

PAM_EXTERN int pam_sm_acct_mgmt(pam_handle_t *pamh,int flags,int argc
		     ,const char **argv)
{
    const char *service=NULL, *tty=NULL;
    const char *user=NULL;

    /* set service name */

    if (pam_get_item(pamh, PAM_SERVICE, (const void **)&service)
	!= PAM_SUCCESS || service == NULL) {
	_log_err("cannot find the current service name");
	return PAM_ABORT;
    }

    /* set username */

    if (pam_get_user(pamh, &user, NULL) != PAM_SUCCESS || user == NULL
	|| *user == '\0') {
	_log_err("cannot determine the user's name");
	return PAM_USER_UNKNOWN;
    }

    /* set tty name */

    if (pam_get_item(pamh, PAM_TTY, (const void **)&tty) != PAM_SUCCESS
	|| tty == NULL) {
	D(("PAM_TTY not set, probing stdin"));
	tty = ttyname(STDIN_FILENO);
	if (tty == NULL) {
	    _log_err("couldn't get the tty name");
	    return PAM_ABORT;
	}
	if (pam_set_item(pamh, PAM_TTY, tty) != PAM_SUCCESS) {
	    _log_err("couldn't set tty name");
	    return PAM_ABORT;
	}
    }

    if (strncmp("/dev/",tty,5) == 0) {          /* strip leading /dev/ */
	tty += 5;
    }

    /* good, now we have the service name, the user and the terminal name */

    D(("service=%s", service));
    D(("user=%s", user));
    D(("tty=%s", tty));

    return check_account(service,tty,user);
}

/* end of module definition */

#ifdef PAM_STATIC

/* static module data */

struct pam_module _pam_time_modstruct = {
    "pam_time",
    NULL,
    NULL,
    pam_sm_acct_mgmt,
    NULL,
    NULL,
    NULL
};
#endif
