PLIBDIR= ${DESTDIR}/usr/share/perl

PLIB+= abbrev.pl assert.pl bigfloat.pl bigint.pl bigrat.pl cacheout.pl
PLIB+= chat2.pl complete.pl ctime.pl dumpvar.pl exceptions.pl fastcwd.pl
PLIB+= find.pl finddepth.pl flush.pl getcwd.pl gethostname.pl getopts.pl
PLIB+= importenv.pl look.pl newgetopt.pl open2.pl perldb.pl pwd.pl
PLIB+= shellwords.pl stat.pl syslog.pl termcap.pl timelocal.pl validate.pl

NOOBJ=

install:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 ${PLIB} ${PLIBDIR}

clean:
cleandir:

.include <bsd.prog.mk>
