#
# $Id: Makefile,v 1.2 1998/09/20 00:46:25 abial Exp $
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
	(cd ${DESTDIR}/dev; \
	ln -s /dev/MAKEDEV; \
	./MAKEDEV std tun2 vty10 fd0 wd0s1h pty0; \
	./MAKEDEV cuaa0 cuaa1 cuaa2 cuaa3 cuaa4; \
	./MAKEDEV cuaa5 cuaa6 cuaa7 cuaa8 cuaa9; \
	rm MAKEDEV)

clean:

