#       @(#)Makefile	8.1 (Berkeley) 6/7/93

SUBDIR= mailstats makemap praliases
VER=	XX

# don't trivially install sendmail
.if !make(install)
SUBDIR+=src
.endif

tar: Files.base Files.cf Files.misc Files.xdoc
	(cd src; ${MAKE})
	(cd doc; PRINTER=ps ${MAKE})
	(cd cf/cf; ${MAKE})
	pax -w -x tar -L -f sendmail.${VER}.base.tar `cat Files.base`
	compress sendmail.${VER}.base.tar
	pax -w -x tar -L -f sendmail.${VER}.cf.tar `cat Files.cf`
	compress sendmail.${VER}.cf.tar
	pax -w -x tar -L -f sendmail.${VER}.misc.tar `cat Files.misc`
	compress sendmail.${VER}.misc.tar
	pax -w -x tar -L -f sendmail.${VER}.xdoc.tar `cat Files.xdoc`
	compress sendmail.${VER}.xdoc.tar

ftp: sendmail.${VER}.base.tar.Z sendmail.${VER}.cf.tar.Z sendmail.${VER}.misc.tar.Z sendmail.${VER}.xdoc.tar.Z
	rcp sendmail.${VER}.*.tar.Z RELEASE_NOTES barad-dur:/disks/barad-dur/ftp/sendmail/.

.include <bsd.subdir.mk>
