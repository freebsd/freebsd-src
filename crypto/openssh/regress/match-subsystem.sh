#	$OpenBSD: match-subsystem.sh,v 1.1 2023/09/06 23:36:09 djm Exp $
#	Placed in the Public Domain.

tid="sshd_config match subsystem"

cp $OBJ/sshd_proxy $OBJ/sshd_proxy_bak

try_subsystem() {
	_id=$1
	_subsystem=$2
	_expect=$3
	${SSHD} -tf $OBJ/sshd_proxy || fatal "$_id: bad config"
	${SSH} -sF $OBJ/ssh_proxy somehost $_subsystem
	_exit=$?
	trace "$_id subsystem $_subsystem"
	if [ $_exit -ne $_expect ] ; then
		fail "$_id: subsystem $_subsystem exit $_exit expected $_expect"
	fi
	return $?
}

# Simple case: subsystem in main config.
cp $OBJ/sshd_proxy_bak $OBJ/sshd_proxy
cat >> $OBJ/sshd_proxy << _EOF
Subsystem xxx /bin/sh -c "exit 23"
_EOF
try_subsystem "main config" xxx 23

# No clobber in main config.
cp $OBJ/sshd_proxy_bak $OBJ/sshd_proxy
cat >> $OBJ/sshd_proxy << _EOF
Subsystem xxx /bin/sh -c "exit 23"
Subsystem xxx /bin/sh -c "exit 24"
_EOF
try_subsystem "main config no clobber" xxx 23

# Subsystem in match all block
cp $OBJ/sshd_proxy_bak $OBJ/sshd_proxy
cat >> $OBJ/sshd_proxy << _EOF
Match all
Subsystem xxx /bin/sh -c "exit 21"
_EOF
try_subsystem "match all" xxx 21

# No clobber in match all block
cp $OBJ/sshd_proxy_bak $OBJ/sshd_proxy
cat >> $OBJ/sshd_proxy << _EOF
Match all
Subsystem xxx /bin/sh -c "exit 21"
Subsystem xxx /bin/sh -c "exit 24"
_EOF
try_subsystem "match all no clobber" xxx 21

# Subsystem in match user block
cp $OBJ/sshd_proxy_bak $OBJ/sshd_proxy
cat >> $OBJ/sshd_proxy << _EOF
Match user *
Subsystem xxx /bin/sh -c "exit 20"
_EOF
try_subsystem "match user" xxx 20

# No clobber in match user block
cp $OBJ/sshd_proxy_bak $OBJ/sshd_proxy
cat >> $OBJ/sshd_proxy << _EOF
Match user *
Subsystem xxx /bin/sh -c "exit 20"
Subsystem xxx /bin/sh -c "exit 24"
Match all
Subsystem xxx /bin/sh -c "exit 24"
_EOF
try_subsystem "match user no clobber" xxx 20

# Override main with match all
cp $OBJ/sshd_proxy_bak $OBJ/sshd_proxy
cat >> $OBJ/sshd_proxy << _EOF
Subsystem xxx /bin/sh -c "exit 23"
Match all
Subsystem xxx /bin/sh -c "exit 19"
_EOF
try_subsystem "match all override" xxx 19

# Override main with match user
cp $OBJ/sshd_proxy_bak $OBJ/sshd_proxy
cat >> $OBJ/sshd_proxy << _EOF
Subsystem xxx /bin/sh -c "exit 23"
Match user *
Subsystem xxx /bin/sh -c "exit 18"
_EOF
try_subsystem "match user override" xxx 18

