# $FreeBSD$

PROG=	unix_cmsg
SRCS=	${AUTOSRCS} unix_cmsg.c uc_common.h uc_common.c \
	t_generic.h t_generic.c t_peercred.h t_peercred.c \
	t_cmsgcred.h t_cmsgcred.c t_sockcred.h t_sockcred.c \
	t_cmsgcred_sockcred.h t_cmsgcred_sockcred.c t_cmsg_len.h t_cmsg_len.c
CLEANFILES+=	${AUTOSRCS}
MAN=
WARNS?=	3

REXP_bintime= 's|%%TTYPE%%|bintime|g ; s|%%DTYPE%%|bintime|g ; \
    s|%%SCM_TTYPE%%|SCM_BINTIME|g ; \
    s|%%MAJ_MEMB%%|sec|g ; s|%%MIN_MEMB%%|frac|g'
REXP_timeval= 's|%%TTYPE%%|timeval|g ; s|%%DTYPE%%|timeval|g ; \
    s|%%SCM_TTYPE%%|SCM_TIMESTAMP|g ; \
    s|%%MAJ_MEMB%%|tv_sec|g ; s|%%MIN_MEMB%%|tv_usec|g'

.for ttype in bintime timeval
AUTOSRCS+=	t_${ttype}.h t_${ttype}.c

t_${ttype}.o: t_${ttype}.c t_${ttype}.h

t_${ttype}.c: t_xxxtime.c.in
	sed ${REXP_${ttype}} < ${.ALLSRC} > ${.TARGET}

t_${ttype}.h: t_xxxtime.h.in
	sed ${REXP_${ttype}} < ${.ALLSRC} > ${.TARGET}
.endfor

.include <bsd.prog.mk>
