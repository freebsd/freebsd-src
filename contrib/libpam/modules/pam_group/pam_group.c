/* pam_group module */

/*
 * $Id: pam_group.c,v 1.3 2000/11/26 07:32:39 agmorgan Exp $
 *
 * Written by Andrew Morgan <morgan@linux.kernel.org> 1996/7/6
 */

const static char rcsid[] =
"$Id: pam_group.c,v 1.3 2000/11/26 07:32:39 agmorgan Exp $;\n"
"Version 0.5 for Linux-PAM\n"
"Copyright (c) Andrew G. Morgan 1996 <morgan@linux.kernel.org>\n";

#define _BSD_SOURCE

#include <sys/file.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <stdarg.h>
#include <time.h>
#include <syslog.h>
#include <string.h>

#include <grp.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifdef DEFAULT_CONF_FILE
# define PAM_GROUP_CONF         DEFAULT_CONF_FILE /* from external define */
#else
# define PAM_GROUP_CONF         "/etc/security/group.conf"
#endif
#define PAM_GROUP_BUFLEN        1000
#define FIELD_SEPARATOR         ';'   /* this is new as of .02 */

typedef enum { FALSE, TRUE } boolean;
typedef enum { AND, OR } operator;

/*
 * here, we make definitions for the externally accessible functions
 * in this file (these definitions are required for static modules
 * but strongly encouraged generally) they are used to instruct the
 * modules include file to define their prototypes.
 */

#define PAM_SM_AUTH

#include <security/pam_modules.h>
#include <security/_pam_macros.h>

/* --- static functions for checking whether the user should be let in --- */

static void _log_err(const char *format, ... )
{
    va_list args;

    va_start(args, format);
    openlog("pam_group", LOG_CONS|LOG_PID, LOG_AUTH);
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
	*buf = (char *) malloc(PAM_GROUP_BUFLEN);
	if (! *buf) {
	    _log_err("out of memory");
	    return -1;
	}
	*from = *to = 0;
	fd = open(PAM_GROUP_CONF, O_RDONLY);
    }

    /* do we have a file open ? return error */

    if (fd < 0 && *to <= 0) {
	_log_err( PAM_GROUP_CONF " not opened");
	memset(*buf, 0, PAM_GROUP_BUFLEN);
	_pam_drop(*buf);
	return -1;
    }

    /* check if there was a newline last time */

    if ((*to > *from) && (*to > 0)
	&& ((*buf)[*from] == '\0')) {   /* previous line ended */
	(*from)++;
	(*buf)[0] = '\0';
	return fd;
    }

    /* ready for more data: first shift the buffer's remaining data */

    *to -= *from;
    shift_bytes(*buf, *from, *to);
    *from = 0;
    (*buf)[*to] = '\0';

    while (fd >= 0 && *to < PAM_GROUP_BUFLEN) {
	int i;

	/* now try to fill the remainder of the buffer */

	i = read(fd, *to + *buf, PAM_GROUP_BUFLEN - *to);
	if (i < 0) {
	    _log_err("error reading " PAM_GROUP_CONF);
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
     D(("checking: 0%o/%.4d vs. %s", at->day, at->minute, times));

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
	  time_start += times[i+j]-'0';       /* is this portable? */
     }
     j += i;

     if (times[j] == '-') {
	  time_end = 0;
	  for (i=1; len > 0 && i < 5 && isdigit(times[i+j]); ++i, --len) {
	       time_end *= 10;
	       time_end += times[i+j]-'0';    /* is this portable? */
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

static int find_member(const char *string, int *at)
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
               if (isalpha(c) || isdigit(c) || c == '_' || c == '*'
                    || c == '-') {
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

#define GROUP_BLK 10
#define blk_size(len) (((len-1 + GROUP_BLK)/GROUP_BLK)*GROUP_BLK)

static int mkgrplist(char *buf, gid_t **list, int len)
{
     int l,at=0;
     int blks;

     blks = blk_size(len);
     D(("cf. blks=%d and len=%d", blks,len));

     while ((l = find_member(buf,&at))) {
	  int edge;

	  if (len >= blks) {
	       gid_t *tmp;

	       D(("allocating new block"));
	       tmp = (gid_t *) realloc((*list)
				       , sizeof(gid_t) * (blks += GROUP_BLK));
	       if (tmp != NULL) {
		    (*list) = tmp;
	       } else {
		    _log_err("out of memory for group list");
		    free(*list);
		    (*list) = NULL;
		    return -1;
	       }
	  }

	  /* '\0' terminate the entry */

	  edge = (buf[at+l]) ? 1:0;
	  buf[at+l] = '\0';
	  D(("found group: %s",buf+at));

	  /* this is where we convert a group name to a gid_t */
#ifdef WANT_PWDB
	  {
	      int retval;
	      const struct pwdb *pw=NULL;

	      retval = pwdb_locate("group", PWDB_DEFAULT, buf+at
				   , PWDB_ID_UNKNOWN, &pw);
	      if (retval != PWDB_SUCCESS) {
		  _log_err("bad group: %s; %s", buf+at, pwdb_strerror(retval));
	      } else {
		  const struct pwdb_entry *pwe=NULL;

		  D(("group %s exists", buf+at));
		  retval = pwdb_get_entry(pw, "gid", &pwe);
		  if (retval == PWDB_SUCCESS) {
		      D(("gid = %d [%p]",* (const gid_t *) pwe->value,list));
		      (*list)[len++] = * (const gid_t *) pwe->value;
		      pwdb_entry_delete(&pwe);                  /* tidy up */
		  } else {
		      _log_err("%s group entry is bad; %s"
			       , pwdb_strerror(retval));
		  }
		  pw = NULL;          /* break link - cached for later use */
	      }
	  }
#else
	  {
	      const struct group *grp;

	      grp = getgrnam(buf+at);
	      if (grp == NULL) {
		  _log_err("bad group: %s", buf+at);
	      } else {
		  D(("group %s exists", buf+at));
		  (*list)[len++] = grp->gr_gid;
	      }
	  }
#endif

	  /* next entry along */

	  at += l + edge;
     }
     D(("returning with [%p/len=%d]->%p",list,len,*list));
     return len;
}


static int check_account(const char *service, const char *tty
     , const char *user)
{
    int from=0,to=0,fd=-1;
    char *buffer=NULL;
    int count=0;
    TIME here_and_now;
    int retval=PAM_SUCCESS;
    gid_t *grps;
    int no_grps;

    /*
     * first we get the current list of groups - the application
     * will have previously done an initgroups(), or equivalent.
     */

    D(("counting supplementary groups"));
    no_grps = getgroups(0, NULL);      /* find the current number of groups */
    if (no_grps > 0) {
	grps = calloc( blk_size(no_grps) , sizeof(gid_t) );
	D(("copying current list into grps [%d big]",blk_size(no_grps)));
	(void) getgroups(no_grps, grps);
#ifdef DEBUG
	{
	    int z;
	    for (z=0; z<no_grps; ++z) {
		D(("gid[%d]=%d", z, grps[z]));
	    }
	}
#endif
    } else {
	D(("no supplementary groups known"));
	no_grps = 0;
	grps = NULL;
    }

    here_and_now = time_now();                         /* find current time */

    /* parse the rules in the configuration file */
    do {
	int good=TRUE;

	/* here we get the service name field */

	fd = read_field(fd,&buffer,&from,&to);
	if (!buffer || !buffer[0]) {
	    /* empty line .. ? */
	    continue;
	}
	++count;
	D(("working on rule #%d",count));

	good = logic_field(service, buffer, count, is_same);
	D(("with service: %s", good ? "passes":"fails" ));

	/* here we get the terminal name field */

	fd = read_field(fd,&buffer,&from,&to);
	if (!buffer || !buffer[0]) {
	    _log_err(PAM_GROUP_CONF "; no tty entry #%d", count);
	    continue;
	}
	good &= logic_field(tty, buffer, count, is_same);
	D(("with tty: %s", good ? "passes":"fails" ));

	/* here we get the username field */

	fd = read_field(fd,&buffer,&from,&to);
	if (!buffer || !buffer[0]) {
	    _log_err(PAM_GROUP_CONF "; no user entry #%d", count);
	    continue;
	}
	good &= logic_field(user, buffer, count, is_same);
	D(("with user: %s", good ? "passes":"fails" ));

	/* here we get the time field */

	fd = read_field(fd,&buffer,&from,&to);
	if (!buffer || !buffer[0]) {
	    _log_err(PAM_GROUP_CONF "; no time entry #%d", count);
	    continue;
	}

	good &= logic_field(&here_and_now, buffer, count, check_time);
	D(("with time: %s", good ? "passes":"fails" ));

	fd = read_field(fd,&buffer,&from,&to);
	if (!buffer || !buffer[0]) {
	    _log_err(PAM_GROUP_CONF "; no listed groups for rule #%d"
		     , count);
	    continue;
	}

	/*
	 * so we have a list of groups, we need to turn it into
	 * something to send to setgroups(2)
	 */

	if (good) {
	    D(("adding %s to gid list", buffer));
	    good = mkgrplist(buffer, &grps, no_grps);
	    if (good < 0) {
		no_grps = 0;
	    } else {
		no_grps = good;
	    }
	}

	/* check the line is terminated correctly */

	fd = read_field(fd,&buffer,&from,&to);
	if (buffer && buffer[0]) {
	    _log_err(PAM_GROUP_CONF "; poorly terminated rule #%d", count);
	}

	if (good > 0) {
	    D(("rule #%d passed, added %d groups", count, good));
	} else if (good < 0) {
	    retval = PAM_BUF_ERR;
	} else {
	    D(("rule #%d failed", count));
	}

    } while (buffer);

    /* now set the groups for the user */

    if (no_grps > 0) {
	int err;
	D(("trying to set %d groups", no_grps));
#ifdef DEBUG
	for (err=0; err<no_grps; ++err) {
	    D(("gid[%d]=%d", err, grps[err]));
	}
#endif
	if ((err = setgroups(no_grps, grps))) {
	    D(("but couldn't set groups %d", err));
	    _log_err("unable to set the group membership for user (err=%d)"
		     , err);
	    retval = PAM_CRED_ERR;
	}
    }

    if (grps) {                                          /* tidy up */
	memset(grps, 0, sizeof(gid_t) * blk_size(no_grps));
	_pam_drop(grps);
	no_grps = 0;
    }

    return retval;
}

/* --- public authentication management functions --- */

PAM_EXTERN int pam_sm_authenticate(pam_handle_t *pamh, int flags
				   , int argc, const char **argv)
{
    return PAM_IGNORE;
}

PAM_EXTERN int pam_sm_setcred(pam_handle_t *pamh, int flags
			      , int argc, const char **argv)
{
    const char *service=NULL, *tty=NULL;
    const char *user=NULL;
    int retval;
    unsigned setting;

    /* only interested in establishing credentials */

    setting = flags;
    if (!(setting & PAM_ESTABLISH_CRED)) {
	D(("ignoring call - not for establishing credentials"));
	return PAM_SUCCESS;            /* don't fail because of this */
    }

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

#ifdef WANT_PWDB

    /* We initialize the pwdb library and check the account */
    retval = pwdb_start();                             /* initialize */
    if (retval == PWDB_SUCCESS) {
	retval = check_account(service,tty,user);      /* get groups */
	(void) pwdb_end();                                /* tidy up */
    } else {
	D(("failed to initialize pwdb; %s", pwdb_strerror(retval)));
	_log_err("unable to initialize libpwdb");
	retval = PAM_ABORT;
    }

#else /* WANT_PWDB */
    retval = check_account(service,tty,user);          /* get groups */
#endif /* WANT_PWDB */

    return retval;
}

/* end of module definition */

#ifdef PAM_STATIC

/* static module data */

struct pam_module _pam_group_modstruct = {
    "pam_group",
    pam_sm_authenticate,
    pam_sm_setcred,
    NULL,
    NULL,
    NULL,
    NULL
};
#endif
