#
# Copyright (c) 1988 Regents of the University of California.
# All rights reserved.
#
# Redistribution and use in source and binary forms are permitted
# provided that the above copyright notice and this paragraph are
# duplicated in all such forms and that any documentation,
# advertising materials, and other materials related to such
# distribution and use acknowledge that the software was developed
# by the University of California, Berkeley.  The name of the
# University may not be used to endorse or promote products derived
# from this software without specific prior written permission.
# THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
# WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
#
#	@(#)makefile	8.1 (Berkeley) 6/6/93
#
# msdos versus unix defines
O	= .o
#PC_O	= .obj

X	=
#PC_X	= .exe

L	=
#PC_L	= -link

CC	= cc
#PC_CC	= cl

MV	= mv
#PC_MV	= rename

RM	= rm -f
#PC_RM= erase

LINT_ARGS =
#PC_LINT_ARGS = -DLINT_ARGS

DEBUG_FLAGS = -g
#PC_DEBUG_FLAGS = -Zi -Od

AR	= ar
AR1	= cr
AR2	=
AR3	=
#PC_AR	= lib
#PC_AR1	=
#PC_AR2	= +
#PC_AR3	= ";"

RANLIB	= ranlib
#PC_RANLIB = echo "Done with "

DEFINES = ${LINT_ARGS}

CFLAGS = ${DEBUG_FLAGS} -I..

PRINT = lpr -p

ALLC = tnrecv.c
ALLH = tncomp.h

ALLPRINT = ${ALLH} ${ALLC}

ALLSOURCE = ${ALLPRINT} makefile makefile.mak

ALLO = tnrecv$O

.c.obj:
	${CC} ${CFLAGS} -c $<

all:	tnrecv$X

tnrecv$X:	tnrecv$O
	${CC} ${CFLAGS} -o $@ tnrecv$O $L ../api/apilib.a

clean:
	for i in makefile.bak ${ALLO} errs tnrecv$X; \
		do (${RM} $$i); done

.DEFAULT:
	sccs get $<

sccsclean:
	-sccs clean
	-sccs get makefile

action:
	${ACTION}

print:
	${PRINT} ${ALLPRINT}

sourcelist:	${ALLSOURCE} tarread.exe
	@for i in ${ALLSOURCE} tarread.exe; \
		do (echo ${DIRPATH}$$i); done

depend:
	grep '^#include' ${ALLC} | grep -v '<' | \
	sed -e 's/:[^"]*"\([^"]*\)".*/: \1/' \
	    -e 's/\.c/$$O/' | \
	awk ' { if ($$1 != prev) { print rec; rec = $$0; prev = $$1; } \
		else { if (length(rec $$2) > 78) { print rec; rec = $$0; } \
		       else rec = rec " " $$2 } } \
	      END { print rec } ' > makedep
	echo '$$r makedep' >>eddep
	echo '/^# DO NOT DELETE THIS LINE/+1,$$d' >eddep
	echo '$$r makedep' >>eddep
	echo 'w' >>eddep
	-rm -f makefile.bak
	cp makefile makefile.bak
	ed - makefile < eddep
	rm eddep makedep

# DO NOT DELETE THIS LINE

#include "tncomp.h" "../api/api.h" "../ctlr/function.h" "../ctlr/hostctlr.h"
#include "../ctlr/oia.h" "../ctlr/screen.h" "../apilib/disp_asc.h"
#include "../apilib/astosc.h" "../general/general.h"
