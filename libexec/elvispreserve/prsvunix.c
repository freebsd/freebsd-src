/* prsvunix.c */

/* This file contains the UNIX-specific parts of the "elvprsv" program. */

#if OSK
#define ELVPRSV
#include "osk.c"
#else
#include <sys/stat.h>
#include <pwd.h>
#endif
#ifndef __STDC__
/* some older systems don't declare this in pwd.h, I guess. */
extern struct passwd *getpwuid();
#endif

#if ANY_UNIX /* { */
/* Since elvprsv runs as SUID-root, we need a *secure* version of popen() */
#define popen safe_popen
#define pclose safe_pclose

/* This function is similar to the standard popen() function, except for...
 *	1) It doesn't use the shell, for security reasons.
 *	2) Shell services are not supported, including quoting.
 *	3) The mode can only be "w".  "r" is not supported.
 *	4) No more than 9 arguments can be given, including the command.
 */
/*ARGSUSED*/
static FILE *safe_popen(cmd, mode)
	char	*cmd;	/* the filename of the program to be run */
	char	*mode;	/* "w", ignored */
{
	char	path[100];/* full pathname of argv[0] */
	char	*argv[10];/* the arguments */
	int	r0w1[2];/* the pipe fd's */
	int	i;
	FILE	*fp;

	/* parse the arguments */
	for (i = 0; i < 9 && *cmd; i++)
	{
		/* remember where this arg starts */
		argv[i] = cmd;

		/* move to the end of the argument */
		do
		{
			cmd++;
		} while (*cmd && *cmd != ' ');

		/* then mark end of arg & skip to next */
		while (*cmd && *cmd == ' ')
		{
			*cmd++ = '\0';
		}
printf("argv[%d]=\"%s\"\n", i, argv[i]);
	}
	argv[i] = (char *)0;

	/* make the pipe */
	if (pipe(r0w1) < 0)
	{
perror("pipe()");
		return (FILE *)0;	/* pipe failed */
	}

	switch (fork())
	{
	  case -1:						/* error */
perror("fork()");
		return (FILE *)0;

	  case 0:						/* child */
		/* close the "write" end of the pipe */
		close(r0w1[1]);

		/* redirect stdin to come from the "read" end of the pipe */
		close(0);
		dup(r0w1[0]);
		close(r0w1[0]);

		/* exec the shell to run the command */
		if (*argv[0] != '/')
		{
			/* no path, try "/bin/argv[0]" */
			strcpy(path, "/bin/");
			strcat(path, argv[0]);
			execv(path, argv);
perror(path);

			/* if that failed, then try "/usr/bin/argv[0]" */
			strcpy(path, "/usr/bin/");
			strcat(path, argv[0]);
			execv(path, argv);
perror(path);
		}
		else
		{
			/* full pathname given, so use it */
			execv(argv[0], argv);
perror(argv[0]);
		}

		/* if we get here, exec failed */
		exit(1);

	  default:						/* parent */
		/* close the "read" end of the pipe */	
		close(r0w1[0]);

		/* convert the "write" fd into a (FILE *) */
		fp = fdopen(r0w1[1], "w");
		return fp;
	}
	/*NOTREACHED*/
}


/* This function closes the pipe opened by popen(), and returns 0 for success */
static int safe_pclose(fp)
	FILE	*fp;	/* value returned by popen() */
{
	int	status;

	/* close the file, and return the defunct child's exit status */
	fclose(fp);
	wait(&status);
	return status;
}
#endif /* } ANY UNIX */


/* This variable is used to add extra error messages for mail sent to root */
char *ps;

/* This function returns the login name of the owner of a file */
char *ownername(filename)
	char	*filename;	/* name of a file */
{
	struct stat	st;
	struct passwd	*pw;

	/* stat the file, to get its uid */
	if (stat(filename, &st) < 0)
	{
		ps = "stat() failed";
		return "root";
	}

	/* get the /etc/passwd entry for that user */
	pw = getpwuid(st.st_uid);
	if (!pw)
	{
		ps = "uid not found in password file";
		return "root";
	}

	/* return the user's name */
	return pw->pw_name;
}


/* This function sends a mail message to a given user, saying that a file
 * has been preserved.
 */
void mail(user, file, when)
	char	*user;	/* name of user who should receive the mail */
	char	*file;	/* name of original text file that was preserved */
	char	*when;	/* description of why the file was preserved */
{
	char	cmd[80];/* buffer used for constructing a "mail" command */
	FILE	*m;	/* stream used for giving text to the "mail" program */
	char	*base;	/* basename of the file */

	/* separate the directory name from the basename. */
	for (base = file + strlen(file); --base > file && *base != SLASH; )
	{
	}
	if (*base == SLASH)
	{
		*base++ = '\0';
	}

	/* for anonymous buffers, pretend the name was "foo" */
	if (!strcmp(base, "*"))
	{
		base = "foo";
	}

	/* open a pipe to the "mail" program */
#if OSK
	sprintf(cmd, "mail \"-s=%s preserved!\" %s", base, user);
#else /* ANY_UNIX */
	sprintf(cmd, "mail -s Graceland %s", user);
#endif
	m = popen(cmd, "w");
	if (!m)
	{
		perror(cmd);
		/* Can't send mail!  Hope the user figures it out. */
		return;
	}

	/* Tell the user that the file was preserved */
	fprintf(m, "A version of your file \"%s%c%s\"\n", file, SLASH, base);
	fprintf(m, "was preserved when %s.\n", when);
	fprintf(m, "To recover this file, do the following:\n");
	fprintf(m, "\n");
#if OSK
	fprintf(m, "     chd %s\n", file);
#else /* ANY_UNIX */
	fprintf(m, "     cd %s\n", file);
#endif
	fprintf(m, "     elvisrecover %s\n", base);
	fprintf(m, "\n");
	fprintf(m, "With fond wishes for a speedy recovery,\n");
	fprintf(m, "                                    Elvis\n");
	if (ps)
	{
		fprintf(m, "\nP.S. %s\n", ps);
		ps = (char *)0;
	}

	/* close the stream */
	pclose(m);
}
