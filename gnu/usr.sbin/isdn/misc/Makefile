CFLAGS+=	-DBSD -O

ETCFILES = isdn.ip remote isdn.itt ttys
CLEANFILES+=	tst stime *.o

all:		tst stime

tst:		tst.c
		${CC} $(CFLAGS) -o tst tst.c

stime:		stime.c
		${CC} $(CFLAGS) -o stime stime.c

install: all
		${INSTALL} ${COPY} tst ${BINDIR}/isdn_test
		${INSTALL} ${COPY} stime ${BINDIR}/isdn_stime
		${INSTALL} -c -m 755 setnic.sh ${LIBDIR}/setnic
		${INSTALL} -c -m 755 sisdn.sh ${LIBDIR}/sisdn.example
		${INSTALL} -c ${ETCFILES} ${LIBDIR}

.include <bsd.prog.mk>
