#       @(#)Makefile	8.3 (Berkeley) 2/27/94

SUBDIR= src mailstats makemap praliases
FTPDIR=	barad-dur:/disks/barad-dur/ftp/sendmail/.
VER=	XX

tar: Files.base Files.cf Files.misc Files.xdoc
	(cd src; ${MAKE})
	(cd doc; PRINTER=ps ${MAKE})
	(cd doc; chmod 444 op/op.ps intro/intro.ps usenix/usenix.ps)
	(cd cf/cf; ${MAKE})
	pax -w -x tar -L -f sendmail.${VER}.base.tar `grep -v ^# Files.base`
	compress sendmail.${VER}.base.tar
	pax -w -x tar -L -f sendmail.${VER}.cf.tar `grep -v ^# Files.cf`
	compress sendmail.${VER}.cf.tar
	pax -w -x tar -L -f sendmail.${VER}.misc.tar `grep -v ^# Files.misc`
	compress sendmail.${VER}.misc.tar
	pax -w -x tar -L -f sendmail.${VER}.xdoc.tar `grep -v ^# Files.xdoc`
	compress sendmail.${VER}.xdoc.tar

ftp: sendmail.${VER}.base.tar.Z sendmail.${VER}.cf.tar.Z sendmail.${VER}.misc.tar.Z sendmail.${VER}.xdoc.tar.Z
	rcp sendmail.${VER}.*.tar.Z RELEASE_NOTES FAQ KNOWNBUGS ${FTPDIR}

.include <bsd.subdir.mk>
