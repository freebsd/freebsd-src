/*  Copyright 1992 Simmule Turner and Rich Salz.  All rights reserved. 
 *
 *  This software is not subject to any license of the American Telephone 
 *  and Telegraph Company or of the Regents of the University of California. 
 *
 *  Permission is granted to anyone to use this software for any purpose on
 *  any computer system, and to alter it and redistribute it freely, subject
 *  to the following restrictions:
 *  1. The authors are not responsible for the consequences of use of this
 *     software, no matter how awful, even if they arise from flaws in it.
 *  2. The origin of this software must not be misrepresented, either by
 *     explicit claim or by omission.  Since few users ever read sources,
 *     credits must appear in the documentation.
 *  3. Altered versions must be plainly marked as such, and must not be
 *     misrepresented as being the original software.  Since few users
 *     ever read sources, credits must appear in the documentation.
 *  4. This notice may not be removed or altered.
 */

/*
**  Unix system-dependant routines for editline library.
*/
#include <config.h>
#include "editline.h"

#ifdef HAVE_TERMIOS_H
#include <termios.h>
#else
#include <sgtty.h>
#endif

RCSID("$Id: sysunix.c,v 1.4 1999/04/08 13:08:24 joda Exp $");

#ifdef HAVE_TERMIOS_H

void
rl_ttyset(int Reset)
{
    static struct termios	old;
    struct termios		new;
    
    if (Reset == 0) {
	tcgetattr(0, &old);
	rl_erase = old.c_cc[VERASE];
	rl_kill = old.c_cc[VKILL];
	rl_eof = old.c_cc[VEOF];
	rl_intr = old.c_cc[VINTR];
	rl_quit = old.c_cc[VQUIT];

	new = old;
	new.c_cc[VINTR] = -1;
	new.c_cc[VQUIT] = -1;
	new.c_lflag &= ~(ECHO | ICANON);
	new.c_iflag &= ~(ISTRIP | INPCK);
	new.c_cc[VMIN] = 1;
	new.c_cc[VTIME] = 0;
	tcsetattr(0, TCSANOW, &new);
    }
    else
	tcsetattr(0, TCSANOW, &old);
}

#else /* !HAVE_TERMIOS_H */

void
rl_ttyset(int Reset)
{
       static struct sgttyb old;
       struct sgttyb new;

       if (Reset == 0) {
               ioctl(0, TIOCGETP, &old);
               rl_erase = old.sg_erase;
               rl_kill = old.sg_kill;
               new = old;
               new.sg_flags &= ~(ECHO | ICANON);
               new.sg_flags &= ~(ISTRIP | INPCK);
               ioctl(0, TIOCSETP, &new);
       } else {
               ioctl(0, TIOCSETP, &old);
       }
}
#endif /* HAVE_TERMIOS_H */

void
rl_add_slash(char *path, char *p)
{
    struct stat	Sb;
    
    if (stat(path, &Sb) >= 0)
	strcat(p, S_ISDIR(Sb.st_mode) ? "/" : " ");
}
