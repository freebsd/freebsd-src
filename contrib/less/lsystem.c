/*
 * Copyright (C) 1984-2026  Mark Nudelman
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Less License, as specified in the README file.
 *
 * For more information, see the README file.
 */


/*
 * Routines to execute other programs.
 * Necessarily very OS dependent.
 */

#include "less.h"
#include <signal.h>
#include "position.h"

#if MSDOS_COMPILER
#include <dos.h>
#if MSDOS_COMPILER==WIN32C && defined(__MINGW32__)
#include <direct.h>
#define setdisk(n) _chdrive((n)+1)
#else
#ifdef _MSC_VER
#include <direct.h>
#define setdisk(n) _chdrive((n)+1)
#else
#include <dir.h>
#endif
#endif
#endif

extern int sigs;
extern IFILE curr_ifile;


#if HAVE_SYSTEM

/*
 * Pass the specified command to a shell to be executed.
 * Like plain "system()", but handles resetting terminal modes, etc.
 */
public void lsystem(constant char *cmd, constant char *donemsg)
{
	int inp;
#if HAVE_SHELL
	constant char *shell;
	char *p;
#endif
	IFILE save_ifile;
#if MSDOS_COMPILER && MSDOS_COMPILER!=WIN32C
	char cwd[FILENAME_MAX+1];
#endif

	/*
	 * Print the command which is to be executed,
	 * unless the command starts with a "-".
	 */
	if (cmd[0] == '-')
		cmd++;
	else
	{
		clear_bot();
		putstr("!");
		putstr(cmd);
		putstr("\n");
	}

#if MSDOS_COMPILER
#if MSDOS_COMPILER==WIN32C
	if (*cmd == '\0')
		cmd = getenv("COMSPEC");
#else
	/*
	 * Working directory is global on MSDOS.
	 * The child might change the working directory, so we
	 * must save and restore CWD across calls to "system",
	 * or else we won't find our file when we return and
	 * try to "reedit_ifile" it.
	 */
	getcwd(cwd, FILENAME_MAX);
#endif
#endif

	/*
	 * Close the current input file.
	 */
	save_ifile = save_curr_ifile();
	(void) edit_ifile(NULL_IFILE);

	/*
	 * De-initialize the terminal and take out of raw mode.
	 */
	term_deinit();
	flush();         /* Make sure the deinit chars get out */
	raw_mode(FALSE);
#if MSDOS_COMPILER==WIN32C
	close_getchr();
#endif

#if !MSDOS_COMPILER
	/*
	 * Restore signals to their defaults.
	 * But not on Windows, since signals are received by background processes
	 * and a ctrl-C while running the child would kill this instance of less.
	 */
	init_signals(FALSE);
#endif

#if HAVE_DUP
	/*
	 * Force standard input to be the user's terminal
	 * (the normal standard input), even if less's standard input 
	 * is coming from a pipe.
	 */
	inp = dup(0);
	close(0);
#if !MSDOS_COMPILER
	if (open_tty() < 0)
#endif
		dup(inp);
#endif

	/*
	 * Pass the command to the system to be executed.
	 * If we have a SHELL environment variable, use
	 * <$SHELL -c "command"> instead of just <command>.
	 * If the command is empty, just invoke a shell.
	 */
#if HAVE_SHELL
	p = NULL;
	if ((shell = lgetenv("SHELL")) != NULL && *shell != '\0')
	{
		if (*cmd == '\0')
			p = save(shell);
		else
		{
			constant char *copt = shell_coption();
			if (copt == NULL)
				p = save(cmd);
			else
			{
				char *esccmd = shell_quote(cmd);
				if (esccmd != NULL)
				{
					size_t len = strlen(shell) + strlen(esccmd) + strlen(copt) + 3;
					p = (char *) ecalloc(len, sizeof(char));
					SNPRINTF3(p, len, "%s %s %s", shell, copt, esccmd);
					free(esccmd);
				}
			}
		}
	}
	if (p == NULL)
	{
		if (*cmd == '\0')
			p = save("sh");
		else
			p = save(cmd);
	}
	system(p);
	free(p);
#else
#if MSDOS_COMPILER==DJGPPC
	/*
	 * Make stdin of the child be in cooked mode.
	 */
	setmode(0, O_TEXT);
	/*
	 * We don't need to catch signals of the child (it
	 * also makes trouble with some DPMI servers).
	 */
	__djgpp_exception_toggle();
	system(cmd);
	__djgpp_exception_toggle();
#else
	system(cmd);
#endif
#endif

#if HAVE_DUP
	/*
	 * Restore standard input, reset signals, raw mode, etc.
	 */
	close(0);
	dup(inp);
	close(inp);
#endif

#if MSDOS_COMPILER==WIN32C
	open_getchr();
#endif
#if !MSDOS_COMPILER
	init_signals(TRUE);
#endif
	raw_mode(TRUE);
	if (donemsg != NULL)
	{
		putstr(donemsg);
		putstr("  ");
		putstr(LM(press_RETURN));
		get_return();
		putchr('\n');
		flush();
	}
	term_init();
	screen_trashed();

#if MSDOS_COMPILER && MSDOS_COMPILER!=WIN32C
	/*
	 * Restore the previous directory (possibly
	 * changed by the child program we just ran).
	 */
	chdir(cwd);
#if MSDOS_COMPILER != DJGPPC
	/*
	 * Some versions of chdir() don't change to the drive
	 * which is part of CWD.  (DJGPP does this in chdir.)
	 */
	if (cwd[1] == ':')
	{
		if (cwd[0] >= 'a' && cwd[0] <= 'z')
			setdisk(cwd[0] - 'a');
		else if (cwd[0] >= 'A' && cwd[0] <= 'Z')
			setdisk(cwd[0] - 'A');
	}
#endif
#endif

	/*
	 * Reopen the current input file.
	 */
	reedit_ifile(save_ifile);

	/*
	 * Since we were ignoring window change signals while we executed
	 * the system command, we must assume the window changed.
	 * Warning: this leaves a signal pending (in "sigs"),
	 * so psignals() should be called soon after lsystem().
	 */
	sigs |= S_WINCH;
}

#endif

#if PIPEC
/*
 * Create a pipe to the given shell command.
 * Feed it the file contents between the positions spos and epos.
 */
static int pipe_data(constant char *cmd, POSITION spos, POSITION epos)
{
	FILE *f;
	int c;

	/*
	 * This is structured much like lsystem().
	 * Since we're running a shell program, we must be careful
	 * to perform the necessary deinitialization before running
	 * the command, and reinitialization after it.
	 */
	if (ch_seek(spos) != 0)
	{
		error(LM(Cannot_seek_to_start_position), NULL_PARG);
		return (-1);
	}

	if ((f = popen(cmd, "w")) == NULL)
	{
		error(LM(Cannot_create_pipe), NULL_PARG);
		return (-1);
	}
	clear_bot();
	putstr("!");
	putstr(cmd);
	putstr("\n");

	term_deinit();
	flush();
	raw_mode(FALSE);
#if !MSDOS_COMPILER
	init_signals(FALSE);
#endif
#if MSDOS_COMPILER==WIN32C
	close_getchr();
#endif
#ifdef SIGPIPE
	LSIGNAL(SIGPIPE, SIG_IGN);
#endif

	c = EOI;
	while (epos == NULL_POSITION || spos++ <= epos)
	{
		/*
		 * Read a character from the file and give it to the pipe.
		 */
		c = ch_forw_get();
		if (c == EOI)
			break;
		if (putc(c, f) == EOF)
			break;
	}

	/*
	 * Finish up the last line.
	 */
	while (c != '\n' && c != EOI ) 
	{
		c = ch_forw_get();
		if (c == EOI)
			break;
		if (putc(c, f) == EOF)
			break;
	}

	pclose(f);

#ifdef SIGPIPE
	LSIGNAL(SIGPIPE, SIG_DFL);
#endif
#if MSDOS_COMPILER==WIN32C
	open_getchr();
#endif
#if !MSDOS_COMPILER
	init_signals(TRUE);
#endif
	raw_mode(TRUE);
	term_init();
	screen_trashed();
#if defined(SIGWINCH) || defined(SIGWIND)
	/* {{ Probably don't need this here. }} */
	lwinch(0);
#endif
	return (0);
}

/*
 * Pipe a section of the input file into the given shell command.
 * If mpos2 is not NULL_POSITION, the section between
 * mpos1 and mpos2 is piped.
 * Otherwise, if mpos1 is before the current screen,
 * the section between mpos1 and the bottom line displayed is piped.
 * Otherwise, the section between the top line displayed and
 * mpos1 is piped.
 */
public int pipe_pos(constant char *cmd, POSITION mpos1, POSITION mpos2)
{
	POSITION tpos, bpos;

	tpos = position(TOP);
	if (tpos == NULL_POSITION)
		tpos = ch_zero();
	bpos = position(BOTTOM);

	if (mpos2 != NULL_POSITION)
	{
		if (mpos1 < mpos2)
			return pipe_data(cmd, mpos1, mpos2);
		else
			return pipe_data(cmd, mpos2, mpos1);
	} else if (mpos1 < tpos)
		return pipe_data(cmd, mpos1, bpos);
	else
		return pipe_data(cmd, tpos, mpos1);
}

#endif
