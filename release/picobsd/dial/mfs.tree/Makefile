#
# $Id: Makefile,v 1.3 1998/08/10 19:08:02 abial Exp $
#

DESTDIR?=/mnt

STAND_LINKS= bin sbin
USR_LINKS= bin sbin libexec
PL_LOCALE_LINKS= pl polish
US_LOCALE_LINKS= en english

.if ${LANGUAGE} == pl
LOCALE= pl_PL.ISO_8859-2
LOCALE_LINKS=${PL_LOCALE_LINKS}
FONT=iso02-8x16.fnt
KBD=pl_PL.ISO_8859-2.kbd
.else
LOCALE= en_US.ISO_8859-1
LOCALE_LINKS=${US_LOCALE_LINKS}
.endif

all: tree links

tree:
	mtree -deU -f mfs.mtree -p ${DESTDIR}

links: tree
	(cd ${DESTDIR}; \
		for i in ${STAND_LINKS}; \
		do \
			ln -s /stand $${i}; \
		done; \
		cd var/run; \
		ln -s /dev/null log; \
		cd ../../usr; \
		for i in ${USR_LINKS}; \
		do \
			ln -s /stand $${i}; \
		done; \
		cd share/misc;\
		ln -s /etc/termcap termcap; \
		echo emacs >/usr/share/misc/init.ee; \
		cd ../; \
		cd locale; \
		for i in ${LOCALE_LINKS}; \
		do \
			ln -s ${LOCALE} $${i}; \
		done; \
		cp /usr/share/locale/${LOCALE}/* ${LOCALE}/; \
		if [ "X${FONT}" != "X" ]; \
		then \
			cp /usr/share/syscons/fonts/${FONT} ../syscons/; \
			cp /usr/share/syscons/keymaps/${KBD} ../syscons/; \
		fi; \
		cd ../nls; \
		for i in ${LOCALE_LINKS}; \
		do \
			ln -s ${LOCALE} $${i}; \
		done;)


# We don't do it under 'all' because it's needed only on non-DEVFS systems
devnodes: tree
	(cp MAKEDEV ${DESTDIR}/dev/; \
		cd ${DESTDIR}/dev; \
		./MAKEDEV std tun2 vty10 fd0 wd0 wd0s1h pty0; \
		rm MAKEDEV;)

clean:
