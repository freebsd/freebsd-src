/*
 * privs.h - header for privileged operations
 * Copyright (c) 1993 by Thomas Koenig
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author(s) may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	$Id: privs.h,v 1.1 1994/01/05 01:09:14 nate Exp $
 */

#ifndef _PRIVS_H
#define _PRIVS_H

#include <unistd.h>

/* Relinquish privileges temporarily for a setuid program
 * with the option of getting them back later.  This is done by swapping
 * the real and effective userid BSD style.  Call RELINQUISH_PRIVS once
 * at the beginning of the main program.  This will cause all operatons
 * to be executed with the real userid.  When you need the privileges
 * of the setuid invocation, call PRIV_START; when you no longer
 * need it, call PRIV_END.  Note that it is an error to call PRIV_START
 * and not PRIV_END within the same function.
 *
 * Use RELINQUISH_PRIVS_ROOT(a) if your program started out running
 * as root, and you want to drop back the effective userid to a
 * and the effective group id to b, with the option to get them back
 * later.
 *
 * If you no longer need root privileges, but those of some other
 * userid/groupid, you can call REDUCE_PRIV(a) when your effective
 * is the user's.
 *
 * Problems: Do not use return between PRIV_START and PRIV_END; this
 * will cause the program to continue running in an unprivileged
 * state.
 *
 * It is NOT safe to call exec(), system() or popen() with a user-
 * supplied program (i.e. without carefully checking PATH and any
 * library load paths) with relinquished privileges; the called program
 * can aquire them just as easily.  Set both effective and real userid
 * to the real userid before calling any of them.
 */

#ifndef MAIN
extern
#endif
uid_t real_uid, effective_uid;

#define RELINQUISH_PRIVS { \
	real_uid = getuid(); \
	effective_uid = geteuid(); \
	setreuid(effective_uid,real_uid); \
}

#define RELINQUISH_PRIVS_ROOT(a) { \
	real_uid = (a); \
	effective_uid = geteuid(); \
	setreuid(effective_uid,real_uid); \
}

#define PRIV_START { \
	setreuid(real_uid,effective_uid);

#define PRIV_END \
	setreuid(effective_uid,real_uid); \
}

#define REDUCE_PRIV(a) { \
	setreuid(real_uid,effective_uid); \
	effective_uid = (a); \
	setreuid(effective_uid,real_uid); \
}
#endif
