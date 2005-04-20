#!/bin/sh
#
# Fake Root Solaris/SVR4/SVR5 Build System - Prototype
#
# The following code has been provide under Public Domain License.  I really
# don't care what you use it for.  Just as long as you don't complain to me
# nor my employer if you break it. - Ben Lindstrom (mouring@eviladmin.org)
#
umask 022
#
# Options for building the package
# You can create a config.local with your customized options
#
# uncommenting TEST_DIR and using
# configure --prefix=/var/tmp --with-privsep-path=/var/tmp/empty
# and
# PKGNAME=tOpenSSH should allow testing a package without interfering
# with a real OpenSSH package on a system. This is not needed on systems
# that support the -R option to pkgadd.
#TEST_DIR=/var/tmp	# leave commented out for production build
PKGNAME=OpenSSH
SYSVINIT_NAME=opensshd
MAKE=${MAKE:="make"}
SSHDUID=67	# Default privsep uid
SSHDGID=67	# Default privsep gid
# uncomment these next three as needed
#PERMIT_ROOT_LOGIN=no
#X11_FORWARDING=yes
#USR_LOCAL_IS_SYMLINK=yes
# list of system directories we do NOT want to change owner/group/perms
# when installing our package
SYSTEM_DIR="/etc	\
/etc/init.d		\
/etc/rcS.d		\
/etc/rc0.d		\
/etc/rc1.d		\
/etc/rc2.d		\
/etc/opt		\
/opt			\
/opt/bin		\
/usr			\
/usr/bin		\
/usr/lib		\
/usr/sbin		\
/usr/share		\
/usr/share/man		\
/usr/share/man/man1	\
/usr/share/man/man8	\
/usr/local		\
/usr/local/bin		\
/usr/local/etc		\
/usr/local/libexec	\
/usr/local/man		\
/usr/local/man/man1	\
/usr/local/man/man8	\
/usr/local/sbin		\
/usr/local/share	\
/var			\
/var/opt		\
/var/run		\
/var/tmp		\
/tmp"

# We may need to build as root so we make sure PATH is set up
# only set the path if it's not set already
[ -d /usr/local/bin ]  &&  {
	echo $PATH | grep ":/usr/local/bin"  > /dev/null 2>&1
	[ $? -ne 0 ] && PATH=$PATH:/usr/local/bin
}
[ -d /usr/ccs/bin ]  &&  {
	echo $PATH | grep ":/usr/ccs/bin"  > /dev/null 2>&1
	[ $? -ne 0 ] && PATH=$PATH:/usr/ccs/bin
}
export PATH
#

[ -f Makefile ]  ||  {
	echo "Please run this script from your build directory"
	exit 1
}

# we will look for config.local to override the above options
[ -s ./config.local ]  &&  . ./config.local

## Start by faking root install
echo "Faking root install..."
START=`pwd`
OPENSSHD_IN=`dirname $0`/opensshd.in
FAKE_ROOT=$START/package
[ -d $FAKE_ROOT ]  &&  rm -fr $FAKE_ROOT
mkdir $FAKE_ROOT
${MAKE} install-nokeys DESTDIR=$FAKE_ROOT
if [ $? -gt 0 ]
then
	echo "Fake root install failed, stopping."
	exit 1
fi

## Fill in some details, like prefix and sysconfdir
for confvar in prefix exec_prefix bindir sbindir libexecdir datadir mandir sysconfdir piddir
do
	eval $confvar=`grep "^$confvar=" Makefile | cut -d = -f 2`
done


## Collect value of privsep user
for confvar in SSH_PRIVSEP_USER
do
	eval $confvar=`awk '/#define[ \t]'$confvar'/{print $3}' config.h`
done

## Set privsep defaults if not defined
if [ -z "$SSH_PRIVSEP_USER" ]
then
	SSH_PRIVSEP_USER=sshd
fi

## Extract common info requires for the 'info' part of the package.
VERSION=`./ssh -V 2>&1 | sed -e 's/,.*//'`

UNAME_S=`uname -s`
case ${UNAME_S} in
	SunOS)	UNAME_S=Solaris
		ARCH=`uname -p`
		RCS_D=yes
		DEF_MSG="(default: n)"
		;;
	*)	ARCH=`uname -m`
		DEF_MSG="\n" ;;
esac

## Setup our run level stuff while we are at it.
mkdir -p $FAKE_ROOT${TEST_DIR}/etc/init.d

## setup our initscript correctly
sed -e "s#%%configDir%%#${sysconfdir}#g" 	\
    -e "s#%%openSSHDir%%#$prefix#g"		\
    -e "s#%%pidDir%%#${piddir}#g"		\
	${OPENSSHD_IN}	> $FAKE_ROOT${TEST_DIR}/etc/init.d/${SYSVINIT_NAME}
chmod 744 $FAKE_ROOT${TEST_DIR}/etc/init.d/${SYSVINIT_NAME}

[ "${PERMIT_ROOT_LOGIN}" = no ]  &&  \
	perl -p -i -e "s/#PermitRootLogin yes/PermitRootLogin no/" \
		$FAKE_ROOT/${sysconfdir}/sshd_config
[ "${X11_FORWARDING}" = yes ]  &&  \
	perl -p -i -e "s/#X11Forwarding no/X11Forwarding yes/" \
		$FAKE_ROOT/${sysconfdir}/sshd_config
# fix PrintMotd
perl -p -i -e "s/#PrintMotd yes/PrintMotd no/" \
	$FAKE_ROOT/${sysconfdir}/sshd_config

# We don't want to overwrite config files on multiple installs
mv $FAKE_ROOT/${sysconfdir}/ssh_config $FAKE_ROOT/${sysconfdir}/ssh_config.default
mv $FAKE_ROOT/${sysconfdir}/sshd_config $FAKE_ROOT/${sysconfdir}/sshd_config.default
[ -f $FAKE_ROOT/${sysconfdir}/ssh_prng_cmds ]  &&  \
mv $FAKE_ROOT/${sysconfdir}/ssh_prng_cmds $FAKE_ROOT/${sysconfdir}/ssh_prng_cmds.default

cd $FAKE_ROOT

## Ok, this is outright wrong, but it will work.  I'm tired of pkgmk
## whining.
for i in *; do
  PROTO_ARGS="$PROTO_ARGS $i=/$i";
done

## Build info file
echo "Building pkginfo file..."
cat > pkginfo << _EOF
PKG=$PKGNAME
NAME="OpenSSH Portable for ${UNAME_S}"
DESC="Secure Shell remote access utility; replaces telnet and rlogin/rsh."
VENDOR="OpenSSH Portable Team - http://www.openssh.com/portable.html"
ARCH=$ARCH
VERSION=$VERSION
CATEGORY="Security,application"
BASEDIR=/
CLASSES="none"
_EOF

## Build preinstall file
echo "Building preinstall file..."
cat > preinstall << _EOF
#! /sbin/sh
#
[ "\${PRE_INS_STOP}" = "yes" ]  &&  ${TEST_DIR}/etc/init.d/${SYSVINIT_NAME} stop
exit 0
_EOF

## Build postinstall file
echo "Building postinstall file..."
cat > postinstall << _EOF
#! /sbin/sh
#
[ -f \${PKG_INSTALL_ROOT}${sysconfdir}/ssh_config ]  ||  \\
	cp -p \${PKG_INSTALL_ROOT}${sysconfdir}/ssh_config.default \\
		\${PKG_INSTALL_ROOT}${sysconfdir}/ssh_config
[ -f \${PKG_INSTALL_ROOT}${sysconfdir}/sshd_config ]  ||  \\
	cp -p \${PKG_INSTALL_ROOT}${sysconfdir}/sshd_config.default \\
		\${PKG_INSTALL_ROOT}${sysconfdir}/sshd_config
[ -f \${PKG_INSTALL_ROOT}${sysconfdir}/ssh_prng_cmds.default ]  &&  {
	[ -f \${PKG_INSTALL_ROOT}${sysconfdir}/ssh_prng_cmds ]  ||  \\
	cp -p \${PKG_INSTALL_ROOT}${sysconfdir}/ssh_prng_cmds.default \\
		\${PKG_INSTALL_ROOT}${sysconfdir}/ssh_prng_cmds
}

# make rc?.d dirs only if we are doing a test install
[ -n "${TEST_DIR}" ]  &&  {
	[ "$RCS_D" = yes ]  &&  mkdir -p ${TEST_DIR}/etc/rcS.d
	mkdir -p ${TEST_DIR}/etc/rc0.d
	mkdir -p ${TEST_DIR}/etc/rc1.d
	mkdir -p ${TEST_DIR}/etc/rc2.d
}

if [ "\${USE_SYM_LINKS}" = yes ]
then
	[ "$RCS_D" = yes ]  &&  \
installf ${PKGNAME} \${PKG_INSTALL_ROOT}$TEST_DIR/etc/rcS.d/K30${SYSVINIT_NAME}=../init.d/${SYSVINIT_NAME} s
	installf ${PKGNAME} \${PKG_INSTALL_ROOT}$TEST_DIR/etc/rc0.d/K30${SYSVINIT_NAME}=../init.d/${SYSVINIT_NAME} s
	installf ${PKGNAME} \${PKG_INSTALL_ROOT}$TEST_DIR/etc/rc1.d/K30${SYSVINIT_NAME}=../init.d/${SYSVINIT_NAME} s
	installf ${PKGNAME} \${PKG_INSTALL_ROOT}$TEST_DIR/etc/rc2.d/S98${SYSVINIT_NAME}=../init.d/${SYSVINIT_NAME} s
else
	[ "$RCS_D" = yes ]  &&  \
installf ${PKGNAME} \${PKG_INSTALL_ROOT}$TEST_DIR/etc/rcS.d/K30${SYSVINIT_NAME}=$TEST_DIR/etc/init.d/${SYSVINIT_NAME} l
	installf ${PKGNAME} \${PKG_INSTALL_ROOT}$TEST_DIR/etc/rc0.d/K30${SYSVINIT_NAME}=$TEST_DIR/etc/init.d/${SYSVINIT_NAME} l
	installf ${PKGNAME} \${PKG_INSTALL_ROOT}$TEST_DIR/etc/rc1.d/K30${SYSVINIT_NAME}=$TEST_DIR/etc/init.d/${SYSVINIT_NAME} l
	installf ${PKGNAME} \${PKG_INSTALL_ROOT}$TEST_DIR/etc/rc2.d/S98${SYSVINIT_NAME}=$TEST_DIR/etc/init.d/${SYSVINIT_NAME} l
fi

# If piddir doesn't exist we add it. (Ie. --with-pid-dir=/var/opt/ssh)
[ -d $piddir ]  ||  installf ${PKGNAME} \${PKG_INSTALL_ROOT}$TEST_DIR$piddir d 755 root sys

installf -f ${PKGNAME}

# Use chroot to handle PKG_INSTALL_ROOT
if [ ! -z "\${PKG_INSTALL_ROOT}" ]
then
	chroot="chroot \${PKG_INSTALL_ROOT}"
fi
# If this is a test build, we will skip the groupadd/useradd/passwd commands
if [ ! -z "${TEST_DIR}" ]
then
	chroot=echo
fi

if egrep '^[ \t]*UsePrivilegeSeparation[ \t]+no' \${PKG_INSTALL_ROOT}/$sysconfdir/sshd_config >/dev/null
then
	echo "UsePrivilegeSeparation disabled in config, not creating PrivSep user"
	echo "or group."
else
	echo "UsePrivilegeSeparation enabled in config (or defaulting to on)."

	# create group if required
	if cut -f1 -d: \${PKG_INSTALL_ROOT}/etc/group | egrep '^'$SSH_PRIVSEP_USER'\$' >/dev/null
	then
		echo "PrivSep group $SSH_PRIVSEP_USER already exists."
	else
		# Use gid of 67 if possible
		if cut -f3 -d: \${PKG_INSTALL_ROOT}/etc/group | egrep '^'$SSHDGID'\$' >/dev/null
		then
			:
		else
			sshdgid="-g $SSHDGID"
		fi
		echo "Creating PrivSep group $SSH_PRIVSEP_USER."
		\$chroot /usr/sbin/groupadd \$sshdgid $SSH_PRIVSEP_USER
	fi

	# Create user if required
	if cut -f1 -d: \${PKG_INSTALL_ROOT}/etc/passwd | egrep '^'$SSH_PRIVSEP_USER'\$' >/dev/null
	then
		echo "PrivSep user $SSH_PRIVSEP_USER already exists."
	else
		# Use uid of 67 if possible
		if cut -f3 -d: \${PKG_INSTALL_ROOT}/etc/passwd | egrep '^'$SSHDGID'\$' >/dev/null
		then
			:
		else
			sshduid="-u $SSHDUID"
		fi
		echo "Creating PrivSep user $SSH_PRIVSEP_USER."
		\$chroot /usr/sbin/useradd -c 'SSHD PrivSep User' -s /bin/false -g $SSH_PRIVSEP_USER \$sshduid $SSH_PRIVSEP_USER
		\$chroot /usr/bin/passwd -l $SSH_PRIVSEP_USER
	fi
fi

[ "\${POST_INS_START}" = "yes" ]  &&  ${TEST_DIR}/etc/init.d/${SYSVINIT_NAME} start
exit 0
_EOF

## Build preremove file
echo "Building preremove file..."
cat > preremove << _EOF
#! /sbin/sh
#
${TEST_DIR}/etc/init.d/${SYSVINIT_NAME} stop
exit 0
_EOF

## Build request file
echo "Building request file..."
cat > request << _EOF
trap 'exit 3' 15
USE_SYM_LINKS=no
PRE_INS_STOP=no
POST_INS_START=no
# Use symbolic links?
ans=\`ckyorn -d n \
-p "Do you want symbolic links for the start/stop scripts? ${DEF_MSG}"\` || exit \$?
case \$ans in
	[y,Y]*)	USE_SYM_LINKS=yes ;;
esac

# determine if should restart the daemon
if [ -s ${piddir}/sshd.pid  -a  -f ${TEST_DIR}/etc/init.d/${SYSVINIT_NAME} ]
then
	ans=\`ckyorn -d n \
-p "Should the running sshd daemon be restarted? ${DEF_MSG}"\` || exit \$?
	case \$ans in
		[y,Y]*)	PRE_INS_STOP=yes
			POST_INS_START=yes
			;;
	esac

else

# determine if we should start sshd
	ans=\`ckyorn -d n \
-p "Start the sshd daemon after installing this package? ${DEF_MSG}"\` || exit \$?
	case \$ans in
		[y,Y]*)	POST_INS_START=yes ;;
	esac
fi

# make parameters available to installation service,
# and so to any other packaging scripts
cat >\$1 <<!
USE_SYM_LINKS='\$USE_SYM_LINKS'
PRE_INS_STOP='\$PRE_INS_STOP'
POST_INS_START='\$POST_INS_START'
!
exit 0

_EOF

## Build space file
echo "Building space file..."
cat > space << _EOF
# extra space required by start/stop links added by installf in postinstall
$TEST_DIR/etc/rc0.d/K30${SYSVINIT_NAME} 0 1
$TEST_DIR/etc/rc1.d/K30${SYSVINIT_NAME} 0 1
$TEST_DIR/etc/rc2.d/S98${SYSVINIT_NAME} 0 1
_EOF
[ "$RCS_D" = yes ]  &&  \
echo "$TEST_DIR/etc/rcS.d/K30${SYSVINIT_NAME} 0 1" >> space

## Next Build our prototype
echo "Building prototype file..."
cat >mk-proto.awk << _EOF
	    BEGIN { print "i pkginfo"; print "i preinstall"; \\
		    print "i postinstall"; print "i preremove"; \\
		    print "i request"; print "i space"; \\
		    split("$SYSTEM_DIR",sys_files); }
	    {
	     for (dir in sys_files) { if ( \$3 != sys_files[dir] )
		     { \$5="root"; \$6="sys"; }
		else
		     { \$4="?"; \$5="?"; \$6="?"; break;}
	    } }
	    { print; }
_EOF
find . | egrep -v "prototype|pkginfo|mk-proto.awk" | sort | \
	pkgproto $PROTO_ARGS | nawk -f mk-proto.awk > prototype

# /usr/local is a symlink on some systems
[ "${USR_LOCAL_IS_SYMLINK}" = yes ]  &&  {
	grep -v "^d none /usr/local ? ? ?$" prototype > prototype.new
	mv prototype.new prototype
}

## Step back a directory and now build the package.
echo "Building package.."
cd ..
pkgmk -d ${FAKE_ROOT} -f $FAKE_ROOT/prototype -o
echo | pkgtrans -os ${FAKE_ROOT} ${START}/$PKGNAME-$UNAME_S-$ARCH-$VERSION.pkg
rm -rf $FAKE_ROOT

