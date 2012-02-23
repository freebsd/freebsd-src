/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 *
 * Portions Copyright 2008 John Birrell jb@freebsd.org
 *
 * $FreeBSD$
 */

inline int SIGHUP = 1;
#pragma D binding "1.0" SIGHUP
inline int SIGINT = 2;
#pragma D binding "1.0" SIGINT
inline int SIGQUIT = 3;
#pragma D binding "1.0" SIGQUIT
inline int SIGILL = 4;
#pragma D binding "1.0" SIGILL
inline int SIGTRAP = 5;
#pragma D binding "1.0" SIGTRAP
inline int SIGABRT = 6;
#pragma D binding "1.0" SIGABRT
inline int SIGEMT = 7;
#pragma D binding "1.0" SIGEMT
inline int SIGFPE = 8;
#pragma D binding "1.0" SIGFPE
inline int SIGKILL = 9;
#pragma D binding "1.0" SIGKILL
inline int SIGBUS = 10;
#pragma D binding "1.0" SIGBUS
inline int SIGSEGV = 11;
#pragma D binding "1.0" SIGSEGV
inline int SIGSYS = 12;
#pragma D binding "1.0" SIGSYS
inline int SIGPIPE = 13;
#pragma D binding "1.0" SIGPIPE
inline int SIGALRM = 14;
#pragma D binding "1.0" SIGALRM
inline int SIGTERM = 15;
#pragma D binding "1.0" SIGTERM
inline int SIGURG = 16;
#pragma D binding "1.0" SIGURG
inline int SIGSTOP = 17;
#pragma D binding "1.0" SIGSTOP
inline int SIGTSTP = 18;
#pragma D binding "1.0" SIGTSTP
inline int SIGCONT = 19;
#pragma D binding "1.0" SIGCONT
inline int SIGCHLD = 20;
#pragma D binding "1.0" SIGCHLD
inline int SIGTTIN = 21;
#pragma D binding "1.0" SIGTTIN
inline int SIGTTOU = 22;
#pragma D binding "1.0" SIGTTOU
inline int SIGIO = 23;
#pragma D binding "1.0" SIGIO
inline int SIGXCPU = 24;
#pragma D binding "1.0" SIGXCPU
inline int SIGXFSZ = 25;
#pragma D binding "1.0" SIGXFSZ
inline int SIGVTALRM = 26;
#pragma D binding "1.0" SIGVTALRM
inline int SIGPROF = 27;
#pragma D binding "1.0" SIGPROF
inline int SIGWINCH = 28;
#pragma D binding "1.0" SIGWINCH
inline int SIGINFO = 29;
#pragma D binding "1.0" SIGINFO
inline int SIGUSR1 = 30;
#pragma D binding "1.0" SIGUSR1
inline int SIGUSR2 = 31;
#pragma D binding "1.0" SIGUSR2

inline int CLD_EXITED = 1;
#pragma D binding "1.0" CLD_EXITED
inline int CLD_KILLED = 2;
#pragma D binding "1.0" CLD_KILLED
inline int CLD_DUMPED = 3;
#pragma D binding "1.0" CLD_DUMPED
inline int CLD_TRAPPED = 4;
#pragma D binding "1.0" CLD_TRAPPED
inline int CLD_STOPPED = 5;
#pragma D binding "1.0" CLD_STOPPED
inline int CLD_CONTINUED = 6;
#pragma D binding "1.0" CLD_CONTINUED
