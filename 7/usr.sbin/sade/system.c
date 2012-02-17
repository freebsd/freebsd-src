/*
 * $FreeBSD$
 *
 * Jordan Hubbard
 *
 * My contributions are in the public domain.
 *
 * Parts of this file are also blatently stolen from Poul-Henning Kamp's
 * previous version of sysinstall, and as such fall under his "BEERWARE license"
 * so buy him a beer if you like it!  Buy him a beer for me, too!
 * Heck, get him completely drunk and send me pictures! :-)
 */

#include "sade.h"
#include <signal.h>
#include <termios.h>
#include <sys/param.h>
#include <sys/reboot.h>
#include <sys/consio.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <ufs/ufs/ufsmount.h>


/* Where we stick our temporary expanded doc file */
#define	DOC_TMP_DIR	"/tmp/.doc"
#define	DOC_TMP_FILE	"/tmp/.doc/doc.tmp"

/*
 * Handle interrupt signals - this probably won't work in all cases
 * due to our having bogotified the internal state of dialog or curses,
 * but we'll give it a try.
 */
static int
intr_continue(dialogMenuItem *self)
{
    return DITEM_LEAVE_MENU;
}

static int
intr_restart(dialogMenuItem *self)
{
    int ret, fd, fdmax;

    free_variables();
    fdmax = getdtablesize();
    for (fd = 3; fd < fdmax; fd++)
	close(fd);
    ret = execl(StartName, StartName, "-restart", (char *)NULL);
    msgDebug("execl failed (%s)\n", strerror(errno));
    /* NOTREACHED */
    return -1;
}

static dialogMenuItem intrmenu[] = {
    { "Restart", "Restart the program", NULL, intr_restart, NULL, NULL, 0, 0, 0, 0 },
    { "Continue", "Continue without restarting", NULL, intr_continue, NULL, NULL, 0, 0, 0, 0 },
};


static void
handle_intr(int sig)
{
    WINDOW *save = savescr();

    use_helpline(NULL);
    use_helpfile(NULL);
    if (OnVTY) {
        ioctl(0, VT_ACTIVATE, 1);       /* Switch back */
        msgInfo(NULL);
    }
    (void)dialog_menu("Installation interrupt",
		     "Do you want to abort the installation?",
		     -1, -1, 2, -2, intrmenu, NULL, NULL, NULL);
    restorescr(save);
}

/* Expand a file into a convenient location, nuking it each time */
static char *
expand(char *fname)
{
    char *gunzip = "/usr/bin/gunzip";

    if (!directory_exists(DOC_TMP_DIR)) {
	Mkdir(DOC_TMP_DIR);
	if (chown(DOC_TMP_DIR, 0, 0) < 0)
	    return NULL;
	if (chmod(DOC_TMP_DIR, S_IRWXU) < 0)
	    return NULL;
    }
    else
	unlink(DOC_TMP_FILE);
    if (!file_readable(fname) || vsystem("%s < %s > %s", gunzip, fname, DOC_TMP_FILE))
	return NULL;
    return DOC_TMP_FILE;
}

/* Initialize system defaults */
void
systemInitialize(int argc, char **argv)
{
    size_t i;
    int boothowto;
    sigset_t signalset;

    signal(SIGINT, SIG_IGN);
    globalsInit();

    i = sizeof(boothowto);
    if (!sysctlbyname("debug.boothowto", &boothowto, &i, NULL, 0) &&
        (i == sizeof(boothowto)) && (boothowto & RB_VERBOSE))
	variable_set2(VAR_DEBUG, "YES", 0);

    if (set_termcap() == -1) {
	printf("Can't find terminal entry\n");
	exit(-1);
    }

    /* XXX - libdialog has particularly bad return value checking */
    init_dialog();

    /* If we haven't crashed I guess dialog is running ! */
    DialogActive = TRUE;

    /* Make sure HOME is set for those utilities that need it */
    signal(SIGINT, handle_intr);
    /*
     * Make sure we can be interrupted even if we were re-executed
     * from an interrupt.
     */
    sigemptyset(&signalset);
    sigaddset(&signalset, SIGINT);
    sigprocmask(SIG_UNBLOCK, &signalset, NULL);

    (void)vsystem("rm -rf %s", DOC_TMP_DIR);
}

/* Run some general command */
int
systemExecute(char *command)
{
    int status;
    struct termios foo;
    WINDOW *w = savescr();

    dialog_clear();
    dialog_update();
    end_dialog();
    DialogActive = FALSE;
    if (tcgetattr(0, &foo) != -1) {
	foo.c_cc[VERASE] = '\010';
	tcsetattr(0, TCSANOW, &foo);
    }
    if (!Fake)
	status = system(command);
    else {
	status = 0;
	msgDebug("systemExecute:  Faked execution of `%s'\n", command);
    }
    DialogActive = TRUE;
    restorescr(w);
    return status;
}

/* suspend/resume libdialog/curses screen */
static    WINDOW *oldW;

void
systemSuspendDialog(void)
{

    oldW  = savescr();
    dialog_clear();
    dialog_update();
    end_dialog();
    DialogActive = FALSE;
}

void
systemResumeDialog(void)
{

    DialogActive = TRUE;
    restorescr(oldW);
}

/* Display a help file in a filebox */
int
systemDisplayHelp(char *file)
{
    char *fname = NULL;
    char buf[FILENAME_MAX];
    int ret = 0;
    WINDOW *w = savescr();
    
		printf("zzz");
    fname = systemHelpFile(file, buf);
    if (!fname) {
	snprintf(buf, FILENAME_MAX, "The %s file is not provided on this particular floppy image.", file);
	use_helpfile(NULL);
	use_helpline(NULL);
	dialog_mesgbox("Sorry!", buf, -1, -1);
	ret = 1;
    }
    else {
	use_helpfile(NULL);
	use_helpline(NULL);
	dialog_textbox(file, fname, LINES, COLS);
    }
    restorescr(w);
    return ret;
}

char *
systemHelpFile(char *file, char *buf)
{
    if (!file)
	return NULL;
    if (file[0] == '/')
	return file;
    snprintf(buf, FILENAME_MAX, "/stand/help/%s.hlp.gz", file);
    if (file_readable(buf)) 
	return expand(buf);
    snprintf(buf, FILENAME_MAX, "/stand/help/%s.hlp", file);
    if (file_readable(buf)) 
	return expand(buf);
    snprintf(buf, FILENAME_MAX, "/stand/help/%s.TXT.gz", file);
    if (file_readable(buf)) 
	return expand(buf);
    snprintf(buf, FILENAME_MAX, "/stand/help/%s.TXT", file);
    if (file_readable(buf)) 
	return expand(buf);
    snprintf(buf, FILENAME_MAX, "/usr/src/usr.sbin/%s/help/%s.hlp", ProgName,
	file);
    if (file_readable(buf))
	return buf;
    snprintf(buf, FILENAME_MAX, "/usr/src/usr.sbin/%s/help/%s.TXT", ProgName,
	file);
    if (file_readable(buf))
	return buf;
    return NULL;
}

int
vsystem(const char *fmt, ...)
{
    va_list args;
    int pstat;
    pid_t pid;
    int omask;
    sig_t intsave, quitsave;
    char *cmd;
    int i;
    struct stat sb;

    cmd = (char *)alloca(FILENAME_MAX);
    cmd[0] = '\0';
    va_start(args, fmt);
    vsnprintf(cmd, FILENAME_MAX, fmt, args);
    va_end(args);

    omask = sigblock(sigmask(SIGCHLD));
    if (Fake) {
	msgDebug("vsystem:  Faked execution of `%s'\n", cmd);
	return 0;
    }
    if (isDebug())
	msgDebug("Executing command `%s'\n", cmd);
    pid = fork();
    if (pid == -1) {
	(void)sigsetmask(omask);
	i = 127;
    }
    else if (!pid) {	/* Junior */
	(void)sigsetmask(omask);
	if (DebugFD != -1) {
	    dup2(DebugFD, 0);
	    dup2(DebugFD, 1);
	    dup2(DebugFD, 2);
	}
	else {
	    close(1); open("/dev/null", O_WRONLY);
	    dup2(1, 2);
	}
	if (stat("/stand/sh", &sb) == 0)
	    execl("/stand/sh", "/stand/sh", "-c", cmd, (char *)NULL);
	else
	    execl("/bin/sh", "/bin/sh", "-c", cmd, (char *)NULL);
	exit(1);
    }
    else {
	intsave = signal(SIGINT, SIG_IGN);
	quitsave = signal(SIGQUIT, SIG_IGN);
	pid = waitpid(pid, &pstat, 0);
	(void)sigsetmask(omask);
	(void)signal(SIGINT, intsave);
	(void)signal(SIGQUIT, quitsave);
	i = (pid == -1) ? -1 : WEXITSTATUS(pstat);
	if (isDebug())
	    msgDebug("Command `%s' returns status of %d\n", cmd, i);
    }
    return i;
}

