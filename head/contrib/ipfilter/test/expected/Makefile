#
# (C)opyright 2007 by Darren Reed.
#
# See the IPFILTER.LICENCE file for details on licencing.
#
all: i19

i19: i19.dist Makefile
	-if [ "`grep LOG_SECURITY /usr/include/sys/syslog.h 2>&1`" = "" ] ; then \
		if [ "`grep LOG_AUDIT /usr/include/sys/syslog.h 2>&1`" = "" ] ; then \
				sed -e 's/security/!!!/g' i19.dist > i19.p1; \
		else \
				sed -e 's/security/audit/g' i19.dist > i19.p1; \
		fi \
	else \
		/bin/cp i19.dist i19.p1; \
	fi
	-if [ "`grep LOG_AUTHPRIV /usr/include/sys/syslog.h 2>&1`" = "" ] ; then \
		sed -e 's/authpriv/!!!/g' i19.p1 > i19.p2; \
	else \
		/bin/cp i19.p1 i19.p2; \
	fi
	-if [ "`grep LOG_LOGALERT /usr/include/sys/syslog.h 2>&1`" = "" ] ; then \
		sed -e 's/logalert/!!!/g' i19.p2 > i19.p1; \
	else \
		/bin/cp i19.p2 i19.p1; \
	fi
	-if [ "`grep LOG_FTP /usr/include/sys/syslog.h 2>&1`" = "" ] ; then \
		sed -e 's/ftp/!!!/g' i19.p1 > i19.p2; \
	else \
		/bin/cp i19.p1 i19.p2; \
	fi
	-if [ "`egrep 'LOG_CRON.*15' /usr/include/sys/syslog.h 2>&1`" != "" ] ; then \
		sed -e 's/cron/cron2/g' i19.p2 > i19; \
	else \
		/bin/cp i19.p2 i19; \
	fi
	/bin/rm i19.p?

clean:
	/bin/rm -f i19
