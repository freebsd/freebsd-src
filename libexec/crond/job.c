#if !defined(lint) && !defined(LINT)
static char rcsid[] = "$Header: /a/cvs/386BSD/src/libexec/crond/job.c,v 1.1.1.1 1993/06/12 14:55:03 rgrimes Exp $";
#endif

/* Copyright 1988,1990 by Paul Vixie
 * All rights reserved
 *
 * Distribute freely, except: don't remove my name from the source or
 * documentation (don't take credit for my work), mark your changes (don't
 * get me blamed for your possible bugs), don't alter or remove this
 * notice.  May be sold if buildable source is provided to buyer.  No
 * warrantee of any kind, express or implied, is included with this
 * software; use at your own risk, responsibility for damages (if any) to
 * anyone resulting from the use of this software rests entirely with the
 * user.
 *
 * Send bug reports, bug fixes, enhancements, requests, flames, etc., and
 * I'll try to keep a version up to date.  I can be reached as follows:
 * Paul Vixie, 329 Noe Street, San Francisco, CA, 94114, (415) 864-7013,
 * paul@vixie.sf.ca.us || {hoptoad,pacbell,decwrl,crash}!vixie!paul
 */


#include "cron.h"


typedef	struct	_job
	{
		struct _job	*next;
		char		*cmd;
		user		*u;
	}
	job;


static job	*jhead = NULL, *jtail = NULL;


void
job_add(cmd, u)
	register char *cmd;
	register user *u;
{
	register job *j;

	/* if already on queue, keep going */
	for (j=jhead; j; j=j->next)
		if (j->cmd == cmd && j->u == u) { return; }

	/* build a job queue element */
	j = (job*)malloc(sizeof(job));
	j->next = (job*) NULL;
	j->cmd = cmd;
	j->u = u;

	/* add it to the tail */
	if (!jhead) { jhead=j; }
	else { jtail->next=j; }
	jtail = j;
}


int
job_runqueue()
{
	register job	*j;
	register int	run = 0;

	for (j=jhead; j; j=j->next) {
		do_command(j->cmd, j->u);
		free(j);
		run++;
	}
	jhead = jtail = NULL;
	return run;
}
