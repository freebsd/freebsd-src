#       @(#)Makefile	8.12 (Berkeley) 5/29/95

VER=	XX
SUBDIR= src mailstats makemap praliases smrsh cf/cf
FTPDIR=	mastodon:/disks/barad-dur/ftp/sendmail/.
DISTFILES=sendmail.${VER}.tar.Z sendmail.${VER}.tar.gz \
	RELEASE_NOTES FAQ KNOWNBUGS
FILES=	Files.base Files.cf Files.misc Files.xdoc

tar: sccs-check compile-world run-pax

sccs-check:
	sccs check
	(cd src; sccs check)
	(cd doc/op; sccs check)
	(cd doc/intro; sccs check)
	(cd doc/usenix; sccs check)
	(cd cf; sccs check)
	(cd cf/m4; sccs check)
	(cd cf/mailer; sccs check)
	(cd cf/feature; sccs check)
	(cd cf/cf; sccs check)
	(cd cf/ostype; sccs check)
	(cd cf/domain; sccs check)

compile-world:
	(cd src; ${MAKE})
	(cd doc; PRINTER=ps ${MAKE})
	(cd doc; chmod 444 op/op.ps intro/intro.ps usenix/usenix.ps)
	(cd cf/cf; ${MAKE})

run-pax: Files.base Files.cf Files.misc Files.xdoc
	chmod +x src/makesendmail
	pax -w -x tar -L \
		-s ",cf/domain/unspecified-domain,sendmail-${VER}/cf/domain/berkeley-only,p" \
		-s ",^,sendmail-${VER}/," \
		-f sendmail.${VER}.tar \
		`cat ${FILES} | grep -v ^#`
	gzip -c sendmail.${VER}.tar > sendmail.${VER}.tar.gz
	compress sendmail.${VER}.tar

ftp: sendmail.${VER}.tar.Z
	rcp ${DISTFILES} ${FTPDIR}

.include <bsd.subdir.mk>
