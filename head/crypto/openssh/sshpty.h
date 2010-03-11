/* $OpenBSD: sshpty.h,v 1.12 2010/01/09 05:04:24 djm Exp $ */

/*
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * Functions for allocating a pseudo-terminal and making it the controlling
 * tty.
 *
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 */

#include <termios.h>

struct termios *get_saved_tio(void);
void	 leave_raw_mode(int);
void	 enter_raw_mode(int);

int	 pty_allocate(int *, int *, char *, size_t);
void	 pty_release(const char *);
void	 pty_make_controlling_tty(int *, const char *);
void	 pty_change_window_size(int, u_int, u_int, u_int, u_int);
void	 pty_setowner(struct passwd *, const char *);
