PACKAGE=	efi-tools

PROG=	efisign
SRCS=	efisign.c child.c pe.c
MAN=	efisign.8

# Compatibility links
LINKS=	$(BINDIR)/efisign $(BINDIR)/uefisign
MLINKS=	efisign.8 uefisign.8

LIBADD=	crypto

.include <bsd.prog.mk>
