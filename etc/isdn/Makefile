#	$Id: Makefile,v 1.1 1999/01/10 14:30:44 hm Exp $

I4BETCPROG=	answer		\
		isdntel.sh	\
		record		\
		tell		\
		tell-record

I4BETCFILE=	isdnd.rates.D	\
		isdnd.rates.F	\
		isdnd.rc.sample	\
		isdntel.alias.sample

install:
		for i in ${I4BETCPROG} ; do \
		  ${INSTALL} -c -o root -g wheel -m 700 $$i ${DESTDIR}/etc/isdn ; \
		done ; \
		for i in ${I4BETCFILE} ; do \
		  ${INSTALL} -c -o root -g wheel -m 600 $$i ${DESTDIR}/etc/isdn ; \
		done

