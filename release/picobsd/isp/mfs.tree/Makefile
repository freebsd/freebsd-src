#
# $Id: Makefile,v 1.1.1.1 1998/07/14 07:30:47 abial Exp $
#

DESTDIR?=/mnt

STAND_LINKS= bin sbin
USR_LINKS= bin sbin libexec
PL_LOCALE_LINKS= pl polish

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
		ln -s /etc/termcap termcap; )


# We don't do it under 'all' because it's needed only on non-DEVFS systems
devnodes: tree
	(cp MAKEDEV ${DESTDIR}/dev/; \
		cd ${DESTDIR}/dev; \
		./MAKEDEV std tun2 vty10 fd0 wd0 wd0s1h pty0 vn0; \
		./MAKEDEV rp0 rp1 cy0 cy1; \
		./MAKEDEV cuaa0 cuaa1 cuaa2 cuaa3 cuaa4; \
		./MAKEDEV cuaa5 cuaa6 cuaa7 cuaa8 cuaa9; \
		rm MAKEDEV;)
