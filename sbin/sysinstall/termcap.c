/*
 * Copyright (c) 1994, Paul Richards.
 * 
 * All rights reserved.
 * 
 * This software may be used, modified, copied, distributed, and sold, in both
 * source and binary form provided that the above copyright and these terms
 * are retained, verbatim, as the first lines of this file.  Under no
 * circumstances is the author responsible for the proper functioning of this
 * software, nor does the author assume any responsibility for damages
 * incurred with its use.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <fcntl.h>
#include <machine/console.h>

#include "sysinstall.h"

static char color_termcap[] = "\
:pa#64:Co#8:Sf=\\E[3%dm:Sb=\\E[4%dm:op=\\E[37;40m:md=\\E[1m:mh=\\E[30;1m:";
static char color_entry[] = "\
cons25|ansis|ansi80x25:";

static char mono_termcap[] = "\
:us=\E[4m:ue=\E[m:";
static char mono_entry[] = "\
cons25-m|ansis-mono|ansi80x25-mono:";

static char common_termcap[] = "\
:ac=l\\332m\\300k\\277j\\331u\\264t\\303v\\301w\\302q\\304x\\263n\\305`^Da\\260f\\370g\\361~\\371.^Y-^Xh\\261I^U0\\333y\\363z\\362:\
:al=\\E[L:am:bs:NP:cd=\\E[J:ce=\\E[K:cl=\\E[H\\E[J:cm=\\E[%i%d;%dH:co#80:\
:dc=\\E[P:dl=\\E[M:do=\\E[B:bt=\\E[Z:ho=\\E[H:ic=\\E[@:li#25:cb=\\E[1K:\
:ms:nd=\\E[C:pt:rs=\\E[x\\E[m\\Ec:so=\\E[7m:se=\\E[m:up=\\E[A:\
:k1=\\E[M:k2=\\E[N:k3=\\E[O:k4=\\E[P:k5=\\E[Q:k6=\\E[R:k7=\\E[S:k8=\\E[T:\
:k9=\\E[U:k;=\\E[V:F1=\\E[W:F2=\\E[X:K2=\\E[E:nw=\\E[E:ec=\\E[%dX:\
:kb=^H:kh=\\E[H:ku=\\E[A:kd=\\E[B:kl=\\E[D:kr=\\E[C:\
:le=^H:eo:sf=\\E[S:sr=\\E[T:\
:kN=\\E[G:kP=\\E[I:@7=\\E[F:kI=\\E[L:kD=\\177:kB=\\E[Z:\
:IC=\\E[%d@:DC=\\E[%dP:SF=\\E[%dS:SR=\\E[%dT:AL=\\E[%dL:DL=\\E[%dM:\
:DO=\\E[%dB:LE=\\E[%dD:RI=\\E[%dC:UP=\\E[%dA:cv=\\E[%i%dd:ch=\\E[%i%d`:bw:\
:mb=\\E[5m:mr=\\E[7m:me=\\E[m:bl=^G:ut:it#8:";

static int
emergency(int color)
{
	char tempbuf[sizeof(common_termcap)+sizeof(color_termcap)+sizeof(mono_entry)];

	strcpy(tempbuf, color ? color_entry : mono_entry);
	strcat(tempbuf, common_termcap);
	strcat(tempbuf, color ? color_termcap : mono_termcap);
	if (setenv("TERMCAP", tempbuf, 1) < 0)
		return -1;
	return 0;
}

int
set_termcap()
{
	char           *term;

	term = getenv("TERM");
	if (term == NULL) {
		int     color_display, no_termcap = 0;

		if (access("/etc/termcap.small",R_OK)) {
			no_termcap = 1;
		} else if (setenv("TERMCAP", "/etc/termcap.small", 1) < 0)
			no_termcap = 1;

		if (ioctl(STDERR_FILENO, GIO_COLOR, &color_display) < 0) {
			char            buf[64];
			int             len;

			/* serial console */
			if (no_termcap)
				return -1;
			fprintf(stderr, "Enter your terminal type (must be present in /etc/termcap.small): ");
			if (fgets(buf, sizeof(buf), stdin) == NULL)
				return -1;
			len = strlen(buf);
			if (len > 0 && buf[len - 1] == '\n')
				buf[len - 1] = '\0';
			if (setenv("TERM", buf, 1) < 0)
				return -1;
		} else if (color_display) {

			/* color console */
			if (setenv("TERM", "cons25", 1) < 0)
				return -1;
		} else {

			/* mono console */
			if (setenv("TERM", "cons25-m", 1) < 0)
				return -1;
		}
		if (no_termcap) {
			if (emergency(color_display) < 0)
				return -1;
		}
	}
	return 0;
}
