#	$Id:$

I4BETCPROG=	answer		\
		isdntel.sh	\
		record		\
		tell		\
		tell-record

I4BETCFILE=	isdnd.rates.D	\
		isdnd.rates.F	\
		isdnd.rc	\
		isdntel.alias

install:
		for i in ${I4BETCPROG} ; do \
		  ${INSTALL} -c -o root -g wheel -m 700 $$i ${DESTDIR}/etc/isdn ; \
		done ; \
		for i in ${I4BETCFILE} ; do \
		  ${INSTALL} -c -o root -g wheel -m 600 $$i ${DESTDIR}/etc/isdn ; \
		done

