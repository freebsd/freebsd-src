.PATH: ${SRCTOP}/share/termcap

PACKAGE=	runtime
CLEANFILES+=	termcap.small
CONFS=		termcap.small

TERMCAP_SMALL_ENTS?=	SC cons25 cons25-m cons25-w cons25l1 cons25l1-m \
    cons25l1-w cons25l2 cons25l2-m cons25l7 cons25l7-m cons25r cons25r-m \
    cons25r-mv cons25r-v cons25w cons30 cons30-m cons30-w cons30l1 cons30l1-m \
    cons30l1-w cons30l2 cons30l2-m cons30l7 cons30l7-m cons30r cons30r-m \
    cons30r-mv cons30r-v cons43 cons43-m cons43-w cons43l1 cons43l1-m \
    cons43l1-w cons43l2 cons43l2-m cons43l7 cons43l7-m cons43r cons43r-m \
    cons43r-mv cons43r-v cons50 cons50-m cons50-w cons50l1 cons50l1-m \
    cons50l1-w cons50l2 cons50l2-m cons50l7 cons50l7-m cons50r cons50r-m \
    cons50r-mv cons50r-v cons60 cons60-m cons60-w cons60l1 cons60l1-m \
    cons60l1-w cons60l2 cons60l2-m cons60l7 cons60l7-m cons60r cons60r-m \
    cons60r-mv cons60r-v ecma[+]italics vt100 xterm xterm-256color xterm-basic \
    xterm-clear xterm-color xterm-new xterm-r6 xterm-r6-clear

termcap.small: termcap
	set -e; \
	(echo "# AUTOMATICALLY GENERATED FROM `basename ${.ALLSRC}`"; \
	for tcname in ${TERMCAP_SMALL_ENTS}; \
	do \
		echo; awk "/^$${tcname}[:|]/{ f = 1; print; next } /^[^\t]/{ f = 0 } f" ${.ALLSRC}; \
	done) > ${.TARGET}

.include <bsd.prog.mk>
