#	$Id: Makefile,v 1.2 1999/01/19 15:33:15 hm Exp $

I4BETCPROG=	answer		\
		isdntel.sh	\
		record		\
		tell		\
		tell-record

I4BETCFILE=	isdnd.rates.D		\
		isdnd.rates.F		\
		isdnd.rates.UK.BT	\
		isdnd.rc.sample		\
		isdntel.alias.sample

install:
		for i in ${I4BETCPROG} ; do \
		  ${INSTALL} -c -o root -g wheel -m 700 $$i ${DESTDIR}/etc/isdn ; \
		done ; \
		for i in ${I4BETCFILE} ; do \
		  ${INSTALL} -c -o root -g wheel -m 600 $$i ${DESTDIR}/etc/isdn ; \
		done

