#include "EXTERN.h"
#include "common.h"
#include "INTERN.h"
#include "util.h"

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
	    fatal2("patch: internal error, can't reopen %s\n", from);
	while ((i=read(fromfd, buf, sizeof buf)) > 0)
	    if (write(1, buf, i) != 1)
		fatal1("patch: write failed\n");
	Close(fromfd);
	return 0;
    }

    Strcpy(bakname, to);
    Strcat(bakname, origext?origext:ORIGEXT);
    if (stat(to, &filestat) >= 0) {	/* output file exists */
	dev_t to_device = filestat.st_dev;
	ino_t to_inode  = filestat.st_ino;
	char *simplename = bakname;
	
	for (s=bakname; *s; s++) {
	    if (*s == '/')
		simplename = s+1;
	}
	/* find a backup name that is not the same file */
	while (stat(bakname, &filestat) >= 0 &&
		to_device == filestat.st_dev && to_inode == filestat.st_ino) {
	    for (s=simplename; *s && !islower(*s); s++) ;
	    if (*s)
		*s = toupper(*s);
	    else
		Strcpy(simplename, simplename+1);
	}
	while (unlink(bakname) >= 0) ;	/* while() is for benefit of Eunice */
#ifdef DEBUGGING
	if (debug & 4)
	    say3("Moving %s to %s.\n", to, bakname);
#endif
	if (link(to, bakname) < 0) {
	    say3("patch: can't backup %s, output is in %s\n",
		to, from);
	    return -1;
	}
	while (unlink(to) >= 0) ;
    }
#ifdef DEBUGGING
    if (debug & 4)
	say3("Moving %s to %s.\n", from, to);
#endif
    if (link(from, to) < 0) {		/* different file system? */
	Reg4 int tofd;
	
	tofd = creat(to, 0666);
	if (tofd < 0) {
	    say3("patch: can't create %s, output is in %s.\n",
	      to, from);
	    return -1;
	}
	fromfd = open(from, 0);
	if (fromfd < 0)
	    fatal2("patch: internal error, can't reopen %s\n", from);
	while ((i=read(fromfd, buf, sizeof buf)) > 0)
	    if (write(tofd, buf, i) != i)
		fatal1("patch: write failed\n");
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
	fatal2("patch: can't create %s.\n", to);
    fromfd = open(from, 0);
    if (fromfd < 0)
	fatal2("patch: internal error, can't reopen %s\n", from);
    while ((i=read(fromfd, buf, sizeof buf)) > 0)
	if (write(tofd, buf, i) != i)
	    fatal2("patch: write (%s) failed\n", to);
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
	    fatal1("patch: out of memory (savestr)\n");
    }
    else {
	t = rv;
	while (*t++ = *s++);
    }
    return rv;
}

#if defined(lint) && defined(CANVARARG)

/*VARARGS ARGSUSED*/
say(pat) char *pat; { ; }
/*VARARGS ARGSUSED*/
fatal(pat) char *pat; { ; }
/*VARARGS ARGSUSED*/
ask(pat) char *pat; { ; }

#else

/* Vanilla terminal output (buffered). */

void
say(pat,arg1,arg2,arg3)
char *pat;
int arg1,arg2,arg3;
{
    fprintf(stderr, pat, arg1, arg2, arg3);
    Fflush(stderr);
}

/* Terminal output, pun intended. */

void				/* very void */
fatal(pat,arg1,arg2,arg3)
char *pat;
int arg1,arg2,arg3;
{
    void my_exit();

    say(pat, arg1, arg2, arg3);
    my_exit(1);
}

/* Get a response from the user, somehow or other. */

void
ask(pat,arg1,arg2,arg3)
char *pat;
int arg1,arg2,arg3;
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
	r = 1;
    }
    if (r <= 0)
	buf[0] = 0;
    else
	buf[r] = '\0';
    if (!tty2)
	say1(buf);
}
#endif lint

/* How to handle certain events when not in a critical region. */

void
set_signals()
{
    void my_exit();

#ifndef lint
    if (signal(SIGHUP, SIG_IGN) != SIG_IGN)
	Signal(SIGHUP, my_exit);
    if (signal(SIGINT, SIG_IGN) != SIG_IGN)
	Signal(SIGINT, my_exit);
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

/* Make sure we'll have the directories to create a file. */

void
makedirs(filename,striplast)
Reg1 char *filename;
bool striplast;
{
    char tmpbuf[256];
    Reg2 char *s = tmpbuf;
    char *dirv[20];
    Reg3 int i;
    Reg4 int dirvp = 0;

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
	while (*s) s++;
	*s++ = ' ';
	strcpy(s, tmpbuf);
	*dirv[i] = '/';
    }
    system(buf);
}

/* Make filenames more reasonable. */

char *
fetchname(at,strip_leading,assume_exists)
char *at;
int strip_leading;
int assume_exists;
{
    char *s;
    char *name;
    Reg1 char *t;
    char tmpbuf[200];

    if (!at)
	return Nullch;
    s = savestr(at);
    for (t=s; isspace(*t); t++) ;
    name = t;
#ifdef DEBUGGING
    if (debug & 128)
	say4("fetchname %s %d %d\n",name,strip_leading,assume_exists);
#endif
    if (strnEQ(name, "/dev/null", 9))	/* so files can be created by diffing */
	return Nullch;			/*   against /dev/null. */
    for (; *t && !isspace(*t); t++)
	if (*t == '/')
	    if (--strip_leading >= 0)
		name = t+1;
    *t = '\0';
    if (name != s && *s != '/') {
	name[-1] = '\0';
	if (stat(s, &filestat) && filestat.st_mode & S_IFDIR) {
	    name[-1] = '/';
	    name=s;
	}
    }
    name = savestr(name);
    Sprintf(tmpbuf, "RCS/%s", name);
    free(s);
    if (stat(name, &filestat) < 0 && !assume_exists) {
	Strcat(tmpbuf, RCSSUFFIX);
	if (stat(tmpbuf, &filestat) < 0 && stat(tmpbuf+4, &filestat) < 0) {
	    Sprintf(tmpbuf, "SCCS/%s%s", SCCSPREFIX, name);
	    if (stat(tmpbuf, &filestat) < 0 && stat(tmpbuf+5, &filestat) < 0) {
		free(name);
		name = Nullch;
	    }
	}
    }
    return name;
}
