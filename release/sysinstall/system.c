/*
 * The new sysinstall program.
 *
 * This is probably the last program in the `sysinstall' line - the next
 * generation being essentially a complete rewrite.
 *
 * $Id: system.c,v 1.59 1996/05/16 11:47:46 jkh Exp $
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

#include "sysinstall.h"
#include <signal.h>
#include <sys/reboot.h>
#include <machine/console.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/wait.h>


/* Where we stick our temporary expanded doc file */
#define	DOC_TMP_FILE	"/tmp/doc.tmp"

/*
 * Handle interrupt signals - this probably won't work in all cases
 * due to our having bogotified the internal state of dialog or curses,
 * but we'll give it a try.
 */
static void
handle_intr(int sig)
{
    if (!msgYesNo("Are you sure you want to abort the installation?"))
	systemShutdown(1);
}

/* Expand a file into a convenient location, nuking it each time */
static char *
expand(char *fname)
{
    unlink(DOC_TMP_FILE);
    if (vsystem("gzip -c -d %s > %s", fname, DOC_TMP_FILE))
	return NULL;
    return DOC_TMP_FILE;
}

/* Initialize system defaults */
void
systemInitialize(int argc, char **argv)
{
    int i;

    signal(SIGINT, SIG_IGN);
    globalsInit();

    /* Are we running as init? */
    if (getpid() == 1) {
	setsid();
	close(0); open("/dev/ttyv0", O_RDWR);
	close(1); dup(0);
	close(2); dup(0);
	printf("%s running as init\n", argv[0]);
	RunningAsInit = 1;
	i = ioctl(0, TIOCSCTTY, (char *)NULL);
	setlogin("root");
	setenv("PATH", "/stand:/bin:/sbin:/usr/sbin:/usr/bin:/mnt/bin:/mnt/sbin:/mnt/usr/sbin:/mnt/usr/bin:/usr/X11R6/bin", 1);
	setbuf(stdin, 0);
	setbuf(stderr, 0);
    }

    if (set_termcap() == -1) {
	printf("Can't find terminal entry\n");
	exit(-1);
    }

    /* XXX - libdialog has particularly bad return value checking */
    init_dialog();

    /* If we haven't crashed I guess dialog is running ! */
    DialogActive = TRUE;

    /* Make sure HOME is set for those utilities that need it */
    if (!getenv("HOME"))
	setenv("HOME", "/", 1);
    signal(SIGINT, handle_intr);
}

/* Close down and prepare to exit */
void
systemShutdown(int status)
{
    /* If some media is open, close it down */
    if (mediaDevice)
	mediaDevice->shutdown(mediaDevice);

    /* Shut down the dialog library */
    if (DialogActive) {
	end_dialog();
	DialogActive = FALSE;
    }

    /* Shut down curses */
    endwin();

    /* If we have a temporary doc file lying around, nuke it */
    unlink(DOC_TMP_FILE);

    /* REALLY exit! */
    if (RunningAsInit) {
	/* Put the console back */
	ioctl(0, VT_ACTIVATE, 2);
	reboot(0);
    }
    else
	exit(status);
}

/* Run some general command */
int
systemExecute(char *command)
{
    int status;
    struct termios foo;

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
    return status;
}

/* Display a help file in a filebox */
int
systemDisplayHelp(char *file)
{
    char *fname = NULL;
    char buf[FILENAME_MAX];
    WINDOW *old = savescr();
    int ret = 0;

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
    restorescr(old);
    return ret;
}

char *
systemHelpFile(char *file, char *buf)
{
    if (!file)
	return NULL;

    snprintf(buf, FILENAME_MAX, "/stand/help/%s.hlp.gz", file);
    if (file_readable(buf)) 
	return expand(buf);
    snprintf(buf, FILENAME_MAX, "/usr/src/release/sysinstall/help/%s.hlp", file);
    if (file_readable(buf))
	return buf;
    return NULL;
}

void
systemChangeTerminal(char *color, const u_char c_term[],
		     char *mono, const u_char m_term[])
{
    extern void init_acs(void);

    if (OnVTY) {
	if (ColorDisplay) {
	    setenv("TERM", color, 1);
	    setenv("TERMCAP", c_term, 1);
	    reset_shell_mode();
	    setterm(color);
	    init_acs();
	    cbreak(); noecho();
	}
	else {
	    setenv("TERM", mono, 1);
	    setenv("TERMCAP", m_term, 1);
	    reset_shell_mode();
	    setterm(mono);
	    init_acs();
	    cbreak(); noecho();
	}
    }
    clear();
    refresh();
    dialog_clear();
}

int
vsystem(char *fmt, ...)
{
    va_list args;
    int pstat;
    pid_t pid;
    int omask;
    sig_t intsave, quitsave;
    char *cmd;
    int i;

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
	    if (OnVTY && isDebug() && RunningAsInit)
		msgInfo("Command output is on VTY2 - type ALT-F2 to see it");
	    dup2(DebugFD, 0);
	    dup2(DebugFD, 1);
	    dup2(DebugFD, 2);
	}
	else {
	    close(1); open("/dev/null", O_WRONLY);
	    dup2(1, 2);
	}
	if (!RunningAsInit)
	    execl("/bin/sh", "/bin/sh", "-c", cmd, (char *)NULL);
	else
	    execl("/stand/sh", "/stand/sh", "-c", cmd, (char *)NULL);
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

void
systemCreateHoloshell(void)
{
    if (OnVTY && RunningAsInit) {
	if (!fork()) {
	    int i, fd;
	    struct termios foo;
	    extern int login_tty(int);
	    
	    for (i = 0; i < 64; i++)
		close(i);
	    DebugFD = fd = open("/dev/ttyv3", O_RDWR);
	    ioctl(0, TIOCSCTTY, &fd);
	    dup2(0, 1);
	    dup2(0, 2);
	    if (login_tty(fd) == -1)
		msgDebug("Doctor: I can't set the controlling terminal.\n");
	    signal(SIGTTOU, SIG_IGN);
	    if (tcgetattr(fd, &foo) != -1) {
		foo.c_cc[VERASE] = '\010';
		if (tcsetattr(fd, TCSANOW, &foo) == -1)
		    msgDebug("Doctor: I'm unable to set the erase character.\n");
	    }
	    else
		msgDebug("Doctor: I'm unable to get the terminal attributes!\n");
	    execlp("sh", "-sh", 0);
	    msgDebug("Was unable to execute sh for Holographic shell!\n");
	    exit(1);
	}
	else {
	    msgNotify("Starting an emergency holographic shell on VTY4");
	    sleep(2);
	}
    }
}
