#
# Set a bunch of things to hardcoded paths so that we don't accidently
# pick up a user's own version of some utility and hose ourselves.
#
BINDIR?=		/usr/bin
libdir=			/etc
bindir=			${BINDIR}
pager=			more -cs
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

# For scripts.
.if !target(obj)
obj:
	@cd ${.CURDIR}; rm -rf obj; \
	here=`pwd`; dest=/usr/obj/`echo $$here | sed 's,/usr/src/,,'`; \
	echo "$$here -> $$dest"; ln -s $$dest obj; \
	if test -d /usr/obj -a ! -d $$dest; then \
		mkdir -p $$dest; \
	else \
		true; \
	fi;
.endif
