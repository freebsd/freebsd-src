#
# Set a bunch of things to hardcoded paths so that we don't accidently
# pick up a user's own version of some utility and hose ourselves.
#
BINDIR?=		/usr/bin
libdir=			/etc
bindir=			${BINDIR}
pager=			less -sCe
manpath_config_file=	/etc/manpath.config
troff=			/usr/bin/groff -Tps -man
nroff=			/usr/bin/groff -Tascii -man
apropos=		/usr/bin/apropos
whatis=			/usr/bin/whatis
neqn=			/usr/bin/eqn -Tascii
tbl=			/usr/bin/tbl
col=			/usr/bin/col
vgrind=			/usr/bin/vgrind
refer=			/usr/bin/refer
grap=			# no grap
pic=			/usr/bin/pic
zcat=			/usr/bin/zcat
