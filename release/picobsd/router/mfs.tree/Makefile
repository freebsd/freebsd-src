#
# $Id: Makefile,v 1.2.2.1 1999/05/09 22:04:09 abial Exp $
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
	./MAKEDEV std tun2 vty3 fd0 pty0 cuaa0 cuaa1 cuaa2; \
	rm MAKEDEV)

clean:
