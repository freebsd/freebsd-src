include(confBUILDTOOLSDIR`/M4/switch.m4')

bldPRODUCT_START(`executable', `sendmail')
define(`bldBIN_TYPE', `S')
define(`bldINSTALL_DIR', `')
define(`bldSOURCES', `main.c alias.c arpadate.c bf_'ifdef(`confSTDIO_TYPE', `confSTDIO_TYPE', `portable')`.c clock.c collect.c conf.c control.c convtime.c daemon.c deliver.c domain.c envelope.c err.c headers.c macro.c map.c mci.c milter.c mime.c parseaddr.c queue.c readcf.c recipient.c savemail.c sfsasl.c shmticklib.c srvrsmtp.c stab.c stats.c sysexits.c timers.c trace.c udb.c usersmtp.c util.c version.c ')
PREPENDDEF(`confENVDEF', `confMAPDEF')
bldPUSH_SMLIB(`smutil')

define(`bldTARGET_LINKS', ifdef(`confLINKS', `confLINKS',
`${DESTDIR}${UBINDIR}/newaliases ${DESTDIR}${UBINDIR}/mailq ${DESTDIR}${UBINDIR}/hoststat ${DESTDIR}${UBINDIR}/purgestat')
)dnl

# location of sendmail statistics file (usually /etc/mail/ or /var/log)
STDIR= ifdef(`confSTDIR', `confSTDIR', `/etc/mail')

# full path to installed statistics file (usually ${STDIR}/statistics)
STFILE= ${STDIR}/ifdef(`confSTFILE', `confSTFILE', `statistics')

# location of sendmail helpfile file (usually /etc/mail)
HFDIR= ifdef(`confHFDIR', `confHFDIR', `/etc/mail')

# full path to installed help file (usually ${HFDIR}/helpfile)
HFFILE= ${HFDIR}/ifdef(`confHFFILE', `confHFFILE', `helpfile')

ifdef(`confSMSRCADD', `APPENDDEF(`confSRCADD', `confSMSRCADD')')
ifdef(`confSMOBJADD', `APPENDDEF(`confOBJADD', `confSMOBJADD')')

bldPUSH_TARGET(`statistics')
divert(bldTARGETS_SECTION)
statistics:
	${CP} /dev/null statistics

divert(0)

ifdef(`confNO_HELPFILE_INSTALL',, `bldPUSH_INSTALL_TARGET(`install-hf')')
ifdef(`confNO_STATISTICS_INSTALL',, `bldPUSH_INSTALL_TARGET(`install-st')')
divert(bldTARGETS_SECTION)
install-hf:
	if [ ! -d ${DESTDIR}${HFDIR} ]; then mkdir -p ${DESTDIR}${HFDIR}; fi
	${INSTALL} -c -o ${UBINOWN} -g ${UBINGRP} -m 444 helpfile ${DESTDIR}${HFFILE}

install-st: statistics
	if [ ! -d ${DESTDIR}${STDIR} ]; then mkdir -p ${DESTDIR}${STDIR}; fi
	${INSTALL} -c -o ${SBINOWN} -g ${UBINGRP} -m 644 statistics ${DESTDIR}${STFILE}
divert(0)
bldPRODUCT_END

bldPRODUCT_START(`manpage', `sendmail')
define(`bldSOURCES', `sendmail.8 aliases.5 mailq.1 newaliases.1')
bldPRODUCT_END

bldFINISH
