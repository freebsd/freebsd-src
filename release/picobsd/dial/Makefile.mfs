#
# $Id: Makefile,v 1.5.2.1 1999/05/09 22:03:54 abial Exp $
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
		echo emacs >/mnt/usr/share/misc/init.ee; \
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
	(cd ${DESTDIR}/dev; \
	ln -s /dev/MAKEDEV; \
	./MAKEDEV std sysmouse tun2 cuaa0 cuaa1 cuaa2 vty10 fd0 pty0; \
	./MAKEDEV psm0; \
	./MAKEDEV wd0s1h wd0s2 wd0s3 wd0s4 wd1s1h wd1s2 wd1s3 wd1s4 ;\
	rm MAKEDEV)

clean:
