PROG=		kcon
DEVICE=		/dev/ttyv0
CFLAGS+= 	-I${.CURDIR}/../keycap -DKEYB_DEVICE=\"${DEVICE}\"

# the -Lfoo could be omitted if libkeycap.a were installed before
# making those programs here

LDADD =       -L${.CURDIR}/../keycap -L${.OBJDIR}/../keycap -lkeycap

.include <bsd.prog.mk>
