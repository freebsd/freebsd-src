# Copyright (c) 1999 WIDE Project. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. Neither the name of the project nor the names of its contributors
#    may be used to endorse or promote products derived from this software
#    without specific prior written permission.
# 
# THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.

# Copyright (c) 1998 by the University of Oregon.
# All rights reserved.
#
# Permission to use, copy, modify, and distribute this software and
# its documentation in source and binary forms for lawful
# purposes and without fee is hereby granted, provided
# that the above copyright notice appear in all copies and that both
# the copyright notice and this permission notice appear in supporting
# documentation, and that any documentation, advertising materials,
# and other materials related to such distribution and use acknowledge
# that the software was developed by the University of Oregon.
# The name of the University of Oregon may not be used to endorse or 
# promote products derived from this software without specific prior 
# written permission.
#
# THE UNIVERSITY OF OREGON DOES NOT MAKE ANY REPRESENTATIONS
# ABOUT THE SUITABILITY OF THIS SOFTWARE FOR ANY PURPOSE.  THIS SOFTWARE IS
# PROVIDED "AS IS" AND WITHOUT ANY EXPRESS OR IMPLIED WARRANTIES,
# INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, TITLE, AND 
# NON-INFRINGEMENT.
#
# IN NO EVENT SHALL UO, OR ANY OTHER CONTRIBUTOR BE LIABLE FOR ANY
# SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES, WHETHER IN CONTRACT,
# TORT, OR OTHER FORM OF ACTION, ARISING OUT OF OR IN CONNECTION WITH,
# THE USE OR PERFORMANCE OF THIS SOFTWARE.
#
# Other copyrights might apply to parts of this software and are so
# noted when applicable.
#
#
# Questions concerning this software should be directed to 
# Kurt Windisch (kurtw@antc.uoregon.edu)
#
# $Id: Makefile,v 1.3 2000/02/25 06:35:42 itojun Exp $
# $FreeBSD: src/usr.sbin/pim6sd/mtrace6/Makefile,v 1.1.2.1 2000/07/15 07:36:46 kris Exp $
#
#
#Part of this program has been derived from PIM sparse-mode pimd.
#The pimd program is covered by the license in the accompanying file
#named "LICENSE.pimd".
# 
#The pimd program is COPYRIGHT 1998 by University of Southern California.
#
#Part of this program has been derived from mrouted.
#The mrouted program is covered by the license in the accompanying file
#named "LICENSE.mrouted".
#
#The mrouted program is COPYRIGHT 1989 by The Board of Trustees of
#Leland Stanford Junior University.

PROG=	mtrace6

CFLAGS+=-Wall
CFLAGS+= -I$(.CURDIR)/.. -DHAVE_GETIFADDRS

MAN8=	mtrace6.8

.include <bsd.prog.mk>
