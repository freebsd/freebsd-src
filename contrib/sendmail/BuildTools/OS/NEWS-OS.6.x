#	@(#)NEWS-OS.6.x	8.8	(Berkeley)	3/12/1998
define(`confCC', `/bin/cc')
define(`confBEFORE', `sysexits.h ndbm.o')
define(`confMAPDEF', `-DNDBM -DNIS')
define(`confENVDEF', `-DSYSLOG_BUFSIZE=256 # -DSPT_TYPE=SPT_NONE ')
define(`confLIBS', `ndbm.o -lelf -lsocket -lnsl #  # with NDBM')
define(`confMBINDIR', `/usr/lib')
define(`confSBINDIR', `/usr/etc')
define(`confUBINDIR', `/usr/ucb')
define(`confEBINDIR', `/usr/lib')
define(`confSBINGRP', `sys')
define(`confSTDIR', `/etc')
define(`confHFDIR', `/usr/lib')
define(`confINSTALL', `/usr/ucb/install')
PUSHDIVERT(3)
sysexits.h:
	ln -s /usr/ucbinclude/sysexits.h .

ndbm.o:
	if [ ! -f /usr/include/ndbm.h ]; then \
		ln -s /usr/ucbinclude/ndbm.h .; \
	fi; \
	if [ -f /usr/lib/libndbm.a ]; then \
		ar x /usr/lib/libndbm.a ndbm.o; \
	else \
		ar x /usr/ucblib/libucb.a ndbm.o; \
	fi;
POPDIVERT
