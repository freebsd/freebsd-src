/*
 * (C) Copyright 1993, Herve Schauer Consultants
 *
 * This module written by Christophe.Wolfhugel@hsc-sec.fr
 * is to be used under the same conditions and terms (and absence
 * or warranties) than the other modules of the Sendmail package.
 *
 * ABSOLUTELY NO WARRANTY. USE AT YOUR OWN RISKS.
 *
 */


#ifdef XLA

#ifndef MAXLARULES
# define MAXLARULES	20
#endif

# include "sendmail.h"

typedef struct {
		short	queue;		/* # of connexions to have queueing */
		short	reject;		/* # of conn. to reject */
		short	num;		/* # of increments this process */
		char	*mask;		/* Mask (domain) */
        } XLARULE;

char	*XlaFname;			/* Work file name */
char	XlaHostName[1024];		/* Temporary */
int	XlaNext;			/* # of XlaRules */
pid_t	XlaPid;				/* Pid updating the tables */
XLARULE	XlaRules[MAXLARULES];		/* The rules themselves */
short   XlaCtr[MAXLARULES];		/* Counter (for work file only) */

extern	bool	lockfile();

/*
** XLAMATCH -- Matches a fnmatch like expression.
**
**	Parameters:
**		mask -- mask to match the string too;
**		name -- string.
**
** Mask can either be a plain string or a simplified fnmatch like mask:
**	*.string     or  string.*
** No other alternatives are accepted.
**
**	Returns:
**		none.
**
**	Side Effects:
**		none.
*/

bool
XlaMatch(mask, name)
	char *mask, *name;
{
	int l1, l2;

	l1 = strlen(mask);  l2 = strlen(name);
	if (l1 == 1 && mask[0] == '*') return(TRUE);
	if (mask[0] == '*' && mask[1] == '.') {
		if (l2 < (l1 - 2)) return(FALSE);
		if (strcasecmp(&mask[2], name) == 0) return(TRUE);
		if (strcasecmp(&mask[1], name + l2 - l1 + 1) == 0) return(TRUE);
		return(FALSE);
	}
	if (l1 < 3) return(FALSE);
	if (mask[l1 -1] == '*') {
		if (l2 < l1 - 1) return(FALSE);
		if (strncasecmp(mask, name, l1 - 1) == 0) return(TRUE);
		return(FALSE);
	}
	if (strcasecmp(mask, name) == 0) return(TRUE);
	return(FALSE);
}

/*
** XLAZERO -- Zeroes the used variables
**
** 	Just initializes some variables, called once at sendmail
**	startup.
**
**	Parameters:
**		none.
**
**	Returns:
**		none.
**
**	Side Effects:
**		none.
*/

xla_zero()
{
	if (tTd(59, 1)) {
		printf("xla_zero\n");
	}
	XlaFname = NULL;
	XlaNext  = 0;
	XlaPid   = 0;
	memset(&XlaRules[0], 0, sizeof(XLARULE) * MAXLARULES);
}


/*
**  XLAINIT -- initialized extended load average stuff
**
**  This routine handles the L lines appearing in the configuration
**  file.
**
**  L/etc/sendmail.la     indicates the working file
**  Lmask   #1   #2	  Xtended LA to apply to mask
**	#1 = Queueing # of connections
**	#2 = Reject connections.
**
**	Parameters:
**		line -- the cf file line to parse.
**
**	Returns:
**		none.
**
**	Side Effects:
**		Builds several internal tables.
*/

xla_init(line)
	char *line;
{
	char *s;

	if (tTd(59, 1)) {
		printf("xla_init line: %s\n", line);
	}
	if (XlaFname == NULL && *line == '/') {	/* Work file name */
                XlaFname = newstr(line);
		if (tTd(59, 10))
			printf("xla_init: fname = %s\n", XlaFname);
		return;
	}
	if (XlaNext == MAXLARULES) {
		syserr("too many xla rules defined (%d max)", MAXLARULES);
		return;
	}
	s = strtok(line, " \t");
	if (s == NULL) {
		syserr("xla: line unparseable");
		return;
	}
	XlaRules[XlaNext].mask = newstr(s);
	s = strtok(NULL, " \t");
	if (s == NULL) {
		syserr("xla: line unparseable");
		return;
	}
	XlaRules[XlaNext].queue = atoi(s);
	s = strtok(NULL, " \t");
	if (s == NULL) {
		syserr("xla: line unparseable");
		return;
	}
	XlaRules[XlaNext].reject = atoi(s);
	if (tTd(59, 10))
		printf("xla_init: rule #%d = %s q=%d r=%d\n", XlaNext, 
			XlaRules[XlaNext].mask,
			XlaRules[XlaNext].queue, XlaRules[XlaNext].reject);
	XlaNext++;
}


/*
**  XLACREATEFILE -- Create the working file
**
**	Tries to create the working file, called each time sendmail is
**	invoked with the -bd option.
**
**	Parameters:
**		none.
**
**	Returns:
**		none.
**
**	Side Effects:
**		Creates the working file (sendmail.la) and zeroes it.
*/

xla_create_file()
{
	int	fd, i;

	if (tTd(59, 1))
		printf("xla_create_file:\n");
	if (XlaFname == NULL) return;
	fd = open(XlaFname, O_RDWR|O_CREAT, 0644);
	if (fd == -1) {
		XlaFname = NULL;
		syserr("xla_create_file: open failed");
		return;
	}
	if (!lockfile(fd, XlaFname, LOCK_EX)) {
		close(fd);
		XlaFname = NULL;
		syserr("xla_create_file: can't set lock");
		return;
	}
	if (ftruncate(fd, 0) == -1) {
		close(fd);
		XlaFname = NULL;
		syserr("xla_create_file: can't truncate XlaFname");
		return;
	}
	if (write(fd, XlaCtr, sizeof(XlaCtr)) != sizeof(XlaCtr)) {
		XlaFname == NULL;
		syserr("xla_create_file: can't write XlaFname");
	}
	close(fd);
}


/*
**  XLASMTPOK -- Checks if all slots are in use
**
**	Check is there are still some slots available for an SMTP
**	connection.
**
**	Parameters:
**		none.
**
**	Returns:
**		TRUE -- slots are available;
**		FALSE -- no more slots.
**
**	Side Effects:
**		Reads a file, uses a lock and updates sendmail.la if a slot
**		is free for use.
*/

bool
xla_smtp_ok()
{
	int	fd, i;

	if (tTd(59, 1))
		printf("xla_smtp_ok:\n");
	if (XlaFname == NULL) return(TRUE);
	fd = open(XlaFname, O_RDWR, 0644);
	if (fd == -1) {
		XlaFname = NULL;
		syserr("xla_smtp_ok: open failed");
		return(TRUE);
	}
	if (!lockfile(fd, XlaFname, LOCK_EX)) {
		close(fd);
		XlaFname = NULL;
		syserr("xla_smtp_ok: can't set lock");
		return(TRUE);
	}
	if (read(fd, XlaCtr, sizeof(XlaCtr)) != sizeof(XlaCtr)) {
		close(fd);
                XlaFname = NULL;
                syserr("xla_smtp_ok: can't read XlaFname");
                return(TRUE);
	}
	close(fd);
	for (i = 0; i < XlaNext; i++) {
		if (XlaCtr[i] < XlaRules[i].reject)
			return(TRUE);
	}
	return(FALSE);
}


/*
**  XLAHOSTOK -- Can we accept a connection from this host
**
**	Check the quota for the indicated host
**
**	Parameters:
**		name -- host name or IP# (string)
**
**	Returns:
**		TRUE -- we can accept the connection;
**		FALSE -- we must refuse the connection.1
**
**	Side Effects:
**		Reads and writes a file, uses a lock and still updates
**		sendmail.la is a slot gets assigned.
*/

bool
xla_host_ok(name)
	char *name;
{
	int	fd, i;

	if (tTd(59, 1))
		printf("xla_host_ok:\n");
	if (XlaFname == NULL) return(TRUE);
	fd = open(XlaFname, O_RDWR, 0644);
	if (fd == -1) {
		XlaFname = NULL;
		syserr("xla_host_ok: open failed");
		return(TRUE);
	}
	XlaPid = getpid();
	if (!lockfile(fd, XlaFname, LOCK_EX)) {
		close(fd);
		XlaFname = NULL;
		syserr("xla_host_ok: can't set lock");
		return(TRUE);
	}
	if (read(fd, XlaCtr, sizeof(XlaCtr)) != sizeof(XlaCtr)) {
		close(fd);
                XlaFname = NULL;
                syserr("xla_smtp_ok: can't read XlaFname");
                return(TRUE);
	}
	strncpy(XlaHostName, name, sizeof(XlaHostName) -1);
	XlaHostName[sizeof(XlaHostName) -1] = 0;
	i = strlen(name) - 1;
	if (i >= 0 && XlaHostName[i] == '.') XlaHostName[i] = 0;
	for (i = 0; i < XlaNext; i++) {
		if (XlaMatch(XlaRules[i].mask, XlaHostName)) {
			if (XlaCtr[i] < XlaRules[i].reject) {
				if (XlaRules[i].num++ == 0) {
					XlaCtr[i]++;
					lseek(fd, i*sizeof(XlaCtr[i]), SEEK_SET);
					if (write(fd, &XlaCtr[i], sizeof(XlaCtr[i])) !=  sizeof(XlaCtr[i]))
						XlaFname = NULL;
				}
				close(fd);
				return(TRUE);
			}
			close(fd);
			return(FALSE);
		}
	}
	close(fd);
	return(TRUE);
}

/*
**  XLANOQUEUEOK -- Can we sent this message to the remote host
**
**	Check if we can send to the remote host
**
**	Parameters:
**		name -- host name or IP# (string)
**
**	Returns:
**		TRUE -- we can send the message to the remote site;
**		FALSE -- we can't connect the remote host, queue.
**
**	Side Effects:
**		Reads and writes a file, uses a lock.
**		And still updates the sendmail.la file.
*/

bool
xla_noqueue_ok(name)
	char *name;
{
	int	fd, i;

	if (tTd(59, 1))
		printf("xla_noqueue_ok:\n");
        if (XlaFname == NULL) return(TRUE);
	fd = open(XlaFname, O_RDWR, 0644);
	if (fd == -1) {
		XlaFname = NULL;
		syserr("xla_noqueue_ok: open failed");
		return(TRUE);
	}
	if (!lockfile(fd, XlaFname, LOCK_EX)) {
		close(fd);
		XlaFname = NULL;
		syserr("xla_noqueue_ok: can't set lock");
		return(TRUE);
	}
	XlaPid = getpid();
        if (read(fd, XlaCtr, sizeof(XlaCtr)) != sizeof(XlaCtr)) {
                close(fd);
                XlaFname = NULL;
                syserr("xla_noqueue_ok: can't read XlaFname");
                return(TRUE);
        }
	strncpy(XlaHostName, name, sizeof(XlaHostName) -1);
	XlaHostName[sizeof(XlaHostName) -1] = 0;
	i = strlen(name) - 1;
	if (i >= 0 && XlaHostName[i] == '.') XlaHostName[i] = 0;
	for (i = 0; i < XlaNext; i++) {
		if (XlaMatch(XlaRules[i].mask, XlaHostName)) {
			if (XlaCtr[i] < XlaRules[i].queue) {
				if (XlaRules[i].num++ == 0) {
					XlaCtr[i]++;
                                	lseek(fd, i*sizeof(XlaCtr[i]), SEEK_SET);
                                	if (write(fd, &XlaCtr[i], sizeof(XlaCtr[i])) !=  sizeof(XlaCtr[i]))
                                        	XlaFname = NULL;
				}
				close(fd);
				return(TRUE);
			}
			close(fd);
			return(FALSE);
		}
	}
	close(fd);
	return(TRUE);
}


/*
**  XLAHOSTEND -- Notice that a connection is terminated.
**
**	Updates the counters to reflect the end of an SMTP session
**	(in or outgoing).
**
**	Parameters:
**		name -- host name or IP# (string)
**
**	Returns:
**		none.
**
**	Side Effects:
**		Reads and writes a file, uses a lock.
**		And still updates sendmail.la.
*/

xla_host_end(name)
	char	*name;
{
	int	fd, i;

	if (tTd(59, 1))
		printf("xla_host_end:\n");
	if (XlaFname == NULL || XlaPid != getpid()) return;
	fd = open(XlaFname, O_RDWR, 0644);
	if (fd == -1) {
		XlaFname = NULL;
		syserr("xla_host_end: open failed");
		return;
	}
	if (!lockfile(fd, XlaFname, LOCK_EX)) {
                close(fd);
                XlaFname = NULL;
		syserr("xla_host_end: can't set lock");
		return;
	}
        if (read(fd, XlaCtr, sizeof(XlaCtr)) != sizeof(XlaCtr)) {
                close(fd);
                XlaFname = NULL;
                syserr("xla_host_end: can't read XlaFname");
                return(TRUE);
	}
	strncpy(XlaHostName, name, sizeof(XlaHostName) -1);
	XlaHostName[sizeof(XlaHostName) -1] = 0;
	i = strlen(name) - 1;
	if (i >= 0 && XlaHostName[i] == '.') XlaHostName[i] = 0;
	for (i = 0; i < XlaNext; i++) {
		if (XlaMatch(XlaRules[i].mask, XlaHostName)) {
			if (XlaRules[i].num > 0 && XlaRules[i].num-- == 1) {
				if (XlaCtr[i]) XlaCtr[i]--;
                                lseek(fd, i*sizeof(XlaCtr[i]), SEEK_SET);
                                if (write(fd, &XlaCtr[i], sizeof(XlaCtr[i]))
                                        !=  sizeof(XlaCtr[i]))
                                        XlaFname = NULL;
			}
			close(fd);
			return;
		}
	}
	close(fd);
}

/*
**  XLAALLEND -- Mark all connections as closed.
**
**	Generally due to an emergency exit.
**
**	Parameters:
**		name -- host name or IP# (string)
**
**	Returns:
**		none.
**
**	Side Effects:
**		Reads and writes a file, uses a lock.
**		And guess what: updates sendmail.la.
*/

xla_all_end()
{
	int	fd, i;

	if (tTd(59, 1))
		printf("xla_all_end:\n");
	if (XlaFname == NULL || XlaPid != getpid()) return;
	fd = open(XlaFname, O_RDWR, 0644);
        if (fd == -1) {
                XlaFname = NULL;
		syserr("xla_all_end: open failed");
                return;
        }
        if (!lockfile(fd, XlaFname, LOCK_EX)) {
                close(fd);
                XlaFname = NULL;
                syserr("xla_all_end: can't set lock");
                return;
        }
        if (read(fd, XlaCtr, sizeof(XlaCtr)) != sizeof(XlaCtr)) {
                close(fd);
                XlaFname = NULL;
                syserr("xla_all_end: can't read XlaFname");
                return(TRUE);
        }
	for (i = 0; i < XlaNext; i++) {
		if (XlaCtr[i] > 0 && XlaRules[i].num > 0) {
			XlaCtr[i]--; XlaRules[i].num = 0;
		}
	}
	lseek(fd, 0, SEEK_SET);
	if (write(fd, XlaCtr, sizeof(XlaCtr)) != sizeof(XlaCtr)) {
		XlaFname = NULL;
	}
	close(fd);
}
#endif /* XLA */
