#include "EXTERN.h"
#include "common.h"
#include "INTERN.h"
#include "util.h"
#include "backupfile.h"

void my_exit();

#ifndef HAVE_STRERROR
static char *
private_strerror (errnum)
     int errnum;
{
  extern char *sys_errlist[];
  extern int sys_nerr;

  if (errnum > 0 && errnum <= sys_nerr)
    return sys_errlist[errnum];
  return "Unknown system error";
}
#define strerror private_strerror
#endif /* !HAVE_STRERROR */

/* Rename a file, copying it if necessary. */

int
move_file(from,to)
char *from, *to;
{
    char bakname[512];
    Reg1 char *s;
    Reg2 int i;
    Reg3 int fromfd;

    /* to stdout? */

    if (strEQ(to, "-")) {
#ifdef DEBUGGING
	if (debug & 4)
	    say2("Moving %s to stdout.\n", from);
#endif
	fromfd = open(from, 0);
	if (fromfd < 0)
	    pfatal2("internal error, can't reopen %s", from);
	while ((i=read(fromfd, buf, sizeof buf)) > 0)
	    if (write(1, buf, i) != 1)
		pfatal1("write failed");
	Close(fromfd);
	return 0;
    }

    if (origprae) {
	Strcpy(bakname, origprae);
	Strcat(bakname, to);
    } else {
#ifndef NODIR
	char *backupname = find_backup_file_name(to);
	if (backupname == (char *) 0)
	    fatal1("out of memory\n");
	Strcpy(bakname, backupname);
	free(backupname);
#else /* NODIR */
	Strcpy(bakname, to);
    	Strcat(bakname, simple_backup_suffix);
#endif /* NODIR */
    }

    if (stat(to, &filestat) == 0) {	/* output file exists */
	dev_t to_device = filestat.st_dev;
	ino_t to_inode  = filestat.st_ino;
	char *simplename = bakname;

	for (s=bakname; *s; s++) {
	    if (*s == '/')
		simplename = s+1;
	}
	/* Find a backup name that is not the same file.
	   Change the first lowercase char into uppercase;
	   if that isn't sufficient, chop off the first char and try again.  */
	while (stat(bakname, &filestat) == 0 &&
		to_device == filestat.st_dev && to_inode == filestat.st_ino) {
	    /* Skip initial non-lowercase chars.  */
	    for (s=simplename; *s && !islower((unsigned char)*s); s++) ;
	    if (*s)
		*s = toupper((unsigned char)*s);
	    else
		Strcpy(simplename, simplename+1);
	}
	while (unlink(bakname) >= 0) ;	/* while() is for benefit of Eunice */
#ifdef DEBUGGING
	if (debug & 4)
	    say3("Moving %s to %s.\n", to, bakname);
#endif
	if (rename(to, bakname) < 0) {
	    say4("Can't backup %s, output is in %s: %s\n", to, from,
		 strerror(errno));
	    return -1;
	}
	while (unlink(to) >= 0) ;
    }
#ifdef DEBUGGING
    if (debug & 4)
	say3("Moving %s to %s.\n", from, to);
#endif
    if (rename(from, to) < 0) {		/* different file system? */
	Reg4 int tofd;

	tofd = creat(to, 0666);
	if (tofd < 0) {
	    say4("Can't create %s, output is in %s: %s\n",
	      to, from, strerror(errno));
	    return -1;
	}
	fromfd = open(from, 0);
	if (fromfd < 0)
	    pfatal2("internal error, can't reopen %s", from);
	while ((i=read(fromfd, buf, sizeof buf)) > 0)
	    if (write(tofd, buf, i) != i)
		pfatal1("write failed");
	Close(fromfd);
	Close(tofd);
    }
    Unlink(from);
    return 0;
}

/* Copy a file. */

void
copy_file(from,to)
char *from, *to;
{
    Reg3 int tofd;
    Reg2 int fromfd;
    Reg1 int i;

    tofd = creat(to, 0666);
    if (tofd < 0)
	pfatal2("can't create %s", to);
    fromfd = open(from, 0);
    if (fromfd < 0)
	pfatal2("internal error, can't reopen %s", from);
    while ((i=read(fromfd, buf, sizeof buf)) > 0)
	if (write(tofd, buf, i) != i)
	    pfatal2("write to %s failed", to);
    Close(fromfd);
    Close(tofd);
}

/* Allocate a unique area for a string. */

char *
savestr(s)
Reg1 char *s;
{
    Reg3 char *rv;
    Reg2 char *t;

    if (!s)
	s = "Oops";
    t = s;
    while (*t++);
    rv = malloc((MEM) (t - s));
    if (rv == Nullch) {
	if (using_plan_a)
	    out_of_mem = TRUE;
	else
	    fatal1("out of memory\n");
    }
    else {
	t = rv;
	while ((*t++ = *s++));
    }
    return rv;
}

#if defined(lint) && defined(CANVARARG)

/*VARARGS ARGSUSED*/
say(pat) char *pat; { ; }
/*VARARGS ARGSUSED*/
fatal(pat) char *pat; { ; }
/*VARARGS ARGSUSED*/
pfatal(pat) char *pat; { ; }
/*VARARGS ARGSUSED*/
ask(pat) char *pat; { ; }

#else

/* Vanilla terminal output (buffered). */

void
say(pat,arg1,arg2,arg3)
char *pat;
long arg1,arg2,arg3;
{
    fprintf(stderr, pat, arg1, arg2, arg3);
    Fflush(stderr);
}

/* Terminal output, pun intended. */

void				/* very void */
fatal(pat,arg1,arg2,arg3)
char *pat;
long arg1,arg2,arg3;
{
    fprintf(stderr, "patch: **** ");
    fprintf(stderr, pat, arg1, arg2, arg3);
    my_exit(1);
}

/* Say something from patch, something from the system, then silence . . . */

void				/* very void */
pfatal(pat,arg1,arg2,arg3)
char *pat;
long arg1,arg2,arg3;
{
    int errnum = errno;

    fprintf(stderr, "patch: **** ");
    fprintf(stderr, pat, arg1, arg2, arg3);
    fprintf(stderr, ": %s\n", strerror(errnum));
    my_exit(1);
}

/* Get a response from the user, somehow or other. */

int
ask(pat,arg1,arg2,arg3)
char *pat;
long arg1,arg2,arg3;
{
    int ttyfd;
    int r;
    bool tty2 = isatty(2);

    Sprintf(buf, pat, arg1, arg2, arg3);
    Fflush(stderr);
    write(2, buf, strlen(buf));
    if (tty2) {				/* might be redirected to a file */
	r = read(2, buf, sizeof buf);
    }
    else if (isatty(1)) {		/* this may be new file output */
	Fflush(stdout);
	write(1, buf, strlen(buf));
	r = read(1, buf, sizeof buf);
    }
    else if ((ttyfd = open("/dev/tty", 2)) >= 0 && isatty(ttyfd)) {
					/* might be deleted or unwriteable */
	write(ttyfd, buf, strlen(buf));
	r = read(ttyfd, buf, sizeof buf);
	Close(ttyfd);
    }
    else if (isatty(0)) {		/* this is probably patch input */
	Fflush(stdin);
	write(0, buf, strlen(buf));
	r = read(0, buf, sizeof buf);
    }
    else {				/* no terminal at all--default it */
	buf[0] = '\n';
	buf[1] = 0;
	say1(buf);
	return 0;			/* signal possible error */
    }
    if (r <= 0)
	buf[0] = 0;
    else
	buf[r] = '\0';
    if (!tty2)
	say1(buf);

    if (r <= 0)
	return 0;			/* if there was an error, return it */
    else
	return 1;
}
#endif /* lint */

/* How to handle certain events when not in a critical region. */

void
set_signals(reset)
int reset;
{
#ifndef lint
    static RETSIGTYPE (*hupval)(),(*intval)();

    if (!reset) {
	hupval = signal(SIGHUP, SIG_IGN);
	if (hupval != SIG_IGN)
	    hupval = (RETSIGTYPE(*)())my_exit;
	intval = signal(SIGINT, SIG_IGN);
	if (intval != SIG_IGN)
	    intval = (RETSIGTYPE(*)())my_exit;
    }
    Signal(SIGHUP, hupval);
    Signal(SIGINT, intval);
#endif
}

/* How to handle certain events when in a critical region. */

void
ignore_signals()
{
#ifndef lint
    Signal(SIGHUP, SIG_IGN);
    Signal(SIGINT, SIG_IGN);
#endif
}

/* Make sure we'll have the directories to create a file.
   If `striplast' is TRUE, ignore the last element of `filename'.  */

void
makedirs(filename,striplast)
Reg1 char *filename;
bool striplast;
{
    char tmpbuf[256];
    Reg2 char *s = tmpbuf;
    char *dirv[20];		/* Point to the NULs between elements.  */
    Reg3 int i;
    Reg4 int dirvp = 0;		/* Number of finished entries in dirv. */

    /* Copy `filename' into `tmpbuf' with a NUL instead of a slash
       between the directories.  */
    while (*filename) {
	if (*filename == '/') {
	    filename++;
	    dirv[dirvp++] = s;
	    *s++ = '\0';
	}
	else {
	    *s++ = *filename++;
	}
    }
    *s = '\0';
    dirv[dirvp] = s;
    if (striplast)
	dirvp--;
    if (dirvp < 0)
	return;

    strcpy(buf, "mkdir");
    s = buf;
    for (i=0; i<=dirvp; i++) {
	struct stat sbuf;

	if (stat(tmpbuf, &sbuf) && errno == ENOENT) {
	    while (*s) s++;
	    *s++ = ' ';
	    strcpy(s, tmpbuf);
	}
	*dirv[i] = '/';
    }
    if (s != buf)
	system(buf);
}

/* Make filenames more reasonable. */

char *
fetchname(at,strip_leading,assume_exists)
char *at;
int strip_leading;
int assume_exists;
{
    char *fullname;
    char *name;
    Reg1 char *t;
    char tmpbuf[200];
    int sleading = strip_leading;

    if (!at)
	return Nullch;
    while (isspace((unsigned char)*at))
	at++;
#ifdef DEBUGGING
    if (debug & 128)
	say4("fetchname %s %d %d\n",at,strip_leading,assume_exists);
#endif
    if (strnEQ(at, "/dev/null", 9))	/* so files can be created by diffing */
	return Nullch;			/*   against /dev/null. */
    name = fullname = t = savestr(at);

    /* Strip off up to `sleading' leading slashes and null terminate.  */
    for (; *t && !isspace((unsigned char)*t); t++)
	if (*t == '/')
	    if (--sleading >= 0)
		name = t+1;
    *t = '\0';

    /* If no -p option was given (957 is the default value!),
       we were given a relative pathname,
       and the leading directories that we just stripped off all exist,
       put them back on.  */
    if (strip_leading == 957 && name != fullname && *fullname != '/') {
	name[-1] = '\0';
	if (stat(fullname, &filestat) == 0 && S_ISDIR (filestat.st_mode)) {
	    name[-1] = '/';
	    name=fullname;
	}
    }

    name = savestr(name);
    free(fullname);

    if (stat(name, &filestat) && !assume_exists) {
	char *filebase = basename(name);
	int pathlen = filebase - name;

	/* Put any leading path into `tmpbuf'.  */
	strncpy(tmpbuf, name, pathlen);

#define try(f, a1, a2) (Sprintf(tmpbuf + pathlen, f, a1, a2), stat(tmpbuf, &filestat) == 0)
	if (   try("RCS/%s%s", filebase, RCSSUFFIX)
	    || try("RCS/%s%s", filebase,        "")
	    || try(    "%s%s", filebase, RCSSUFFIX)
	    || try("SCCS/%s%s", SCCSPREFIX, filebase)
	    || try(     "%s%s", SCCSPREFIX, filebase))
	  return name;
	free(name);
	name = Nullch;
    }

    return name;
}

char *
xmalloc (size)
     unsigned size;
{
  register char *p = (char *) malloc (size);
  if (!p)
    fatal("out of memory");
  return p;
}
