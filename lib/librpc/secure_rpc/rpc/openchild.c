#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = 	"@(#)openchild.c	2.3 88/08/15 4.0 RPCSRC; from 1.7 88/02/08 SMI";
#endif
/*
 * Sun RPC is a product of Sun Microsystems, Inc. and is provided for
 * unrestricted use provided that this legend is included on all tape
 * media and as a part of the software program in whole or part.  Users
 * may copy or modify Sun RPC without charge, but are not authorized
 * to license or distribute it to anyone else except as part of a product or
 * program developed by the user.
 * 
 * SUN RPC IS PROVIDED AS IS WITH NO WARRANTIES OF ANY KIND INCLUDING THE
 * WARRANTIES OF DESIGN, MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE, OR ARISING FROM A COURSE OF DEALING, USAGE OR TRADE PRACTICE.
 * 
 * Sun RPC is provided with no support and without any obligation on the
 * part of Sun Microsystems, Inc. to assist in its use, correction,
 * modification or enhancement.
 * 
 * SUN MICROSYSTEMS, INC. SHALL HAVE NO LIABILITY WITH RESPECT TO THE
 * INFRINGEMENT OF COPYRIGHTS, TRADE SECRETS OR ANY PATENTS BY SUN RPC
 * OR ANY PART THEREOF.
 * 
 * In no event will Sun Microsystems, Inc. be liable for any lost revenue
 * or profits or other special, indirect and consequential damages, even if
 * Sun has been advised of the possibility of such damages.
 * 
 * Sun Microsystems, Inc.
 * 2550 Garcia Avenue
 * Mountain View, California  94043
 */

/*
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 */

/*
 * Open two pipes to a child process, one for reading, one for writing.
 * The pipes are accessed by FILE pointers. This is NOT a public
 * interface, but for internal use only!
 */
#include <stdio.h>

extern char *malloc();
extern char *rindex();
extern char *sprintf();

static char *basename();
static char SHELL[] = "/bin/sh";


/*
 * returns pid, or -1 for failure
 */
_openchild(command, fto, ffrom)
	char *command;
	FILE **fto;
	FILE **ffrom;
{
	int i;
	int pid;
	int pdto[2];
	int pdfrom[2];
	char *com;

		
	if (pipe(pdto) < 0) {
		goto error1;
	}
	if (pipe(pdfrom) < 0) {
		goto error2;
	}
	switch (pid = vfork()) {
	case -1:
		goto error3;

	case 0:
		/* 
		 * child: read from pdto[0], write into pdfrom[1]
		 */
		(void) close(0); 
		(void) dup(pdto[0]);
		(void) close(1); 
		(void) dup(pdfrom[1]);
		for (i = _rpc_dtablesize() - 1; i >= 3; i--) {
			(void) close(i);
		}
		com = malloc((unsigned) strlen(command) + 6);
		if (com == NULL) {
			_exit(~0);
		}	
		(void) sprintf(com, "exec %s", command);
		execl(SHELL, basename(SHELL), "-c", com, NULL);
		_exit(~0);
 
	default:
		/*
		 * parent: write into pdto[1], read from pdfrom[0]
		 */
		*fto = fdopen(pdto[1], "w");
		(void) close(pdto[0]);
		*ffrom = fdopen(pdfrom[0], "r");
		(void) close(pdfrom[1]);
		break;
	}
	return (pid);

	/*
	 * error cleanup and return
	 */
error3:
	(void) close(pdfrom[0]);
	(void) close(pdfrom[1]);
error2:
	(void) close(pdto[0]);
	(void) close(pdto[1]);
error1:
	return (-1);
}

static char *
basename(path)
	char *path;
{
	char *p;

	p = rindex(path, '/');
	if (p == NULL) {
		return (path);
	} else {
		return (p + 1);
	}
}
