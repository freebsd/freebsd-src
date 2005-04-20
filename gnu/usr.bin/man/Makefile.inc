# $FreeBSD$
#
# Set a bunch of things to hardcoded paths so that we don't accidently
# pick up a user's own version of some utility and hose ourselves.
#
libdir=			/etc
bindir=			${BINDIR}
pager=			more -s
manpath_config_file=	/etc/manpath.config
troff=			/usr/bin/groff -S -man
# -Tascii or localized encoding added automatically
nroff=			/usr/bin/groff -S -Wall -mtty-char -man
apropos=		/usr/bin/apropos
whatis=			/usr/bin/whatis
eqn=                    /usr/bin/eqn
# -Tascii or localized encoding added automatically
neqn=                   /usr/bin/eqn
tbl=			/usr/bin/tbl
col=			/usr/bin/col
vgrind=			/usr/bin/vgrind
refer=			/usr/bin/refer
grap=			# no grap
pic=			/usr/bin/pic
zcat=			/usr/bin/zcat
compress=		/usr/bin/gzip -c
compext=		.gz

.if exists(${.OBJDIR}/../lib)
LIBDESTDIR=		${.OBJDIR}/../lib
.else
LIBDESTDIR=		${.CURDIR}/../lib
.endif

LIBMAN=	${LIBDESTDIR}/libman.a

.include "../Makefile.inc"
