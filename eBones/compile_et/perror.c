/*
 * Copyright 1987 by MIT Student Information Processing Board
 * For copyright info, see Copyright.SIPB
 *
 *	$Id: perror.c,v 1.2 1994/07/19 19:21:30 g89r4222 Exp $
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/uio.h>
#include "error_table.h"

typedef int (*int_func)();

#if defined(mips) && defined(ultrix)
int errno;		/* this is needed to keep the loader from complaining */
#endif

int_func com_err_hook = (int_func) NULL;
char *error_message();

void
com_err(whoami, code, message)
	char *whoami;
	int code;
	char *message;
{
	struct iovec strings[6];

	if (com_err_hook) {
		(*com_err_hook)(whoami, code, message);
		return;
	}

	strings[0].iov_base = whoami;
	strings[0].iov_len = strlen(whoami);
	if (whoami) {
		strings[1].iov_base = ": ";
		strings[1].iov_len = 2;
	} else
		strings[1].iov_len = 0;
	if (code) {
		register char *errmsg = error_message(code);
		strings[2].iov_base = errmsg;
		strings[2].iov_len = strlen(errmsg);
	} else
		strings[2].iov_len = 0;
	strings[3].iov_base = " ";
	strings[3].iov_len = 1;
	strings[4].iov_base = message;
	strings[4].iov_len = strlen(message);
	strings[5].iov_base = "\n";
	strings[5].iov_len = 1;
	(void) writev(2, strings, 6);
}

int_func
set_com_err_hook(new_proc)
	int_func new_proc;
{
	register int_func x = com_err_hook;
	com_err_hook = new_proc;
	return (x);
}

reset_com_err_hook()
{
	com_err_hook = (int_func) NULL;
}

void
perror(msg)
	register const char *msg;
{
	com_err(msg, errno, (char *)NULL);
}
