#	$FreeBSD$
#
# Copyright (c) 2002, 2003	Sam Leffler, Errno Consulting
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
BINDIR=	/usr/local/bin

ALL=	athstats 80211stats 80211watch athdebug 80211debug

all:	${ALL}

athstats: athstats.c
	${CC} -o athstats athstats.c
athdebug: athdebug.c
	${CC} -o athdebug athdebug.c
80211stats: 80211stats.c
	${CC} -o 80211stats 80211stats.c
80211watch: 80211watch.c
	${CC} -o 80211watch 80211watch.c
80211debug: 80211debug.c
	${CC} -o 80211debug 80211debug.c
install: ${ALL}
	install athstats ${DESTDIR}${BINDIR}
	install 80211stats ${DESTDIR}${BINDIR}
	install 80211watch ${DESTDIR}${BINDIR}
	install 80211debug ${DESTDIR}${BINDIR}
	install athdebug ${DESTDIR}${BINDIR}

clean:
	rm -f ${ALL} core a.out
