#
# $FreeBSD$
#
# This Makefile provides an easy way to generate the configuration
# file and database maps for the sendmail(8) daemon.
#
# The user-driven targets are:
#
# all     - Build cf, maps and aliases
# cf      - Build the .cf file from .mc file
# maps    - Build the feature maps
# aliases - Build the sendmail aliases
# install - Install the .cf file as /etc/mail/sendmail.cf
# start   - Start the sendmail daemon with the flags defined in
#           /etc/defaults/rc.conf or /etc/rc.conf
# stop    - Stop the sendmail daemon
# restart - Restart the sendmail daemon
#
# Calling `make' will generate the updated versions when either the
# aliases or one of the map files were changed.
#
# A `make install` is only necessary after modifying the .mc file. In
# this case one would normally also call `make restart' to allow the
# running sendmail to pick up the changes as well.
#
# ------------------------------------------------------------------------
#
# This makefile uses `<HOSTNAME>.mc' as the default .mc file.  This can
# be changed by defining SENDMAIL_MC in /etc/make.conf, e.g.:
#
#		   SENDMAIL_MC=/etc/mail/myconfig.mc
#
# If '<HOSTNAME>.mc' does not exist, it is created using 'freebsd.mc'
# as a template.
# ------------------------------------------------------------------------
#
# The Makefile knows about the following maps:
# access, bitdomain, domaintable, genericstable, mailertable, userdb,
# uucpdomain, virtusertable
#

.ifndef SENDMAIL_MC
SENDMAIL_MC!=           hostname
SENDMAIL_MC:=           ${SENDMAIL_MC}.mc

${SENDMAIL_MC}: freebsd.mc
	cp freebsd.mc ${SENDMAIL_MC}
.endif

INSTALL_CF=		${SENDMAIL_MC:R}.cf

SENDMAIL_ALIASES?=	/etc/mail/aliases

#
# This is the directory where the sendmail configuration files are
# located.
#
.if exists(/usr/share/sendmail/cf)
SENDMAIL_CF_DIR?=	/usr/share/sendmail/cf
.elif exists(/usr/src/contrib/sendmail/cf)
SENDMAIL_CF_DIR?=	/usr/src/contrib/sendmail/cf
.endif

#
# The pid is used to stop and restart the running daemon.
#
SENDMAIL_PIDFILE?=	/var/run/sendmail.pid

#
# Some useful programs we need.
#
SENDMAIL?=		/usr/sbin/sendmail
MAKEMAP?=		/usr/sbin/makemap
M4?=			/usr/bin/m4
KILL?=			/bin/kill

# Set a reasonable default
.MAIN:	all

#
# ------------------------------------------------------------------------
#
# The Makefile picks up the list of files from SENDMAIL_MAP_SRC and
# stores the matching .db filenames in SENDMAIL_MAP_OBJ if the file
# exists in the current directory.  SENDMAIL_MAP_TYPE is the database
# type to use when calling makemap.
#
SENDMAIL_MAP_SRC+=	mailertable domaintable bitdomain uucpdomain \
			genericstable virtusertable access
SENDMAIL_MAP_OBJ=
SENDMAIL_MAP_TYPE?=	hash

.for _f in ${SENDMAIL_MAP_SRC} userdb
.if exists(${_f})
SENDMAIL_MAP_OBJ+=	${_f}.db
.endif
.endfor

#
# The makemap command is used to generate a hashed map from the textfile.
#
.for _f in ${SENDMAIL_MAP_SRC}
.if (exists(${_f}.sample) && !exists(${_f}))
${_f}:		${_f}.sample
	sed -e 's/^/#/' < ${.OODATE} > ${.TARGET}
.endif

${_f}.db:	${_f}
	${MAKEMAP} ${SENDMAIL_MAP_TYPE} ${.TARGET} < ${.OODATE}
.endfor

userdb.db:	userdb
	${MAKEMAP} btree ${.TARGET} < ${.OODATE}


#
# The .cf file needs to be recreated if the templates were modified.
#
M4FILES!=	find ${SENDMAIL_CF_DIR} -type f -name '*.m4' -print

#
# M4(1) is used to generate the .cf file from the .mc file.
#
.SUFFIXES:	.cf .mc

.mc.cf:		${M4FILES}
	${M4} -D_CF_DIR_=${SENDMAIL_CF_DIR}/ ${SENDMAIL_CF_DIR}/m4/cf.m4 \
		${@:R}.mc > ${.TARGET}

#
# Aliases are handled separately since they normally reside in /etc
# and can be rebuild without the help of makemap.
#
${SENDMAIL_ALIASES}.db:	${SENDMAIL_ALIASES}
	${SENDMAIL} -bi

#
# ------------------------------------------------------------------------
#

all:		cf maps aliases

clean:

depend:

cf:		${INSTALL_CF}

maps:		${SENDMAIL_MAP_OBJ}

aliases:	${SENDMAIL_ALIASES}.db

install:	${INSTALL_CF}
	${INSTALL} -c -m ${SHAREMODE} ${INSTALL_CF} /etc/mail/sendmail.cf

start:
	(. /etc/defaults/rc.conf; source_rc_confs; \
	   if [ "$${sendmail_enable}" = "YES" -a -r /etc/mail/sendmail.cf ];\
	   then \
	     ${SENDMAIL} $${sendmail_flags}; \
	   fi \
	)

stop:
	${KILL} -TERM `head -1 ${SENDMAIL_PIDFILE}`

restart:
	${KILL} -HUP `head -1 ${SENDMAIL_PIDFILE}`

# User defined targets
.if exists(Makefile.local)
.include "Makefile.local"
.endif
