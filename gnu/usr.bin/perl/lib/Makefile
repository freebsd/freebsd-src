PLIBDIR= ${DESTDIR}/usr/share/perl

PLIB+= abbrev.pl assert.pl bigfloat.pl bigint.pl bigrat.pl cacheout.pl
PLIB+= chat2.pl complete.pl ctime.pl dumpvar.pl exceptions.pl fastcwd.pl
PLIB+= find.pl finddepth.pl flush.pl getcwd.pl getopts.pl importenv.pl
PLIB+= look.pl newgetopt.pl open2.pl perldb.pl pwd.pl shellwords.pl
PLIB+= stat.pl syslog.pl termcap.pl timelocal.pl validate.pl

NOOBJ=

install:
	install -c -o ${BINOWN} -g ${BINGRP} -m 444 ${PLIB} ${PLIBDIR}

clean:
cleandir:

.include <bsd.prog.mk>
