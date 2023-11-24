#	Placed in the Public Domain.

tid="server config include"

cat > $OBJ/sshd_config.i << _EOF
HostKey $OBJ/host.ssh-ed25519
Match host a
	Banner /aa

Match host b
	Banner /bb
	Include $OBJ/sshd_config.i.* # comment

Match host c
	Include $OBJ/sshd_config.i.* # comment
	Banner /cc

Match host m
	Include $OBJ/sshd_config.i.*

Match Host d
	Banner /dd # comment

Match Host e
	Banner /ee
	Include $OBJ/sshd_config.i.*

Match Host f
	Include $OBJ/sshd_config.i.*
	Banner /ff

Match Host n
	Include $OBJ/sshd_config.i.*
_EOF

cat > $OBJ/sshd_config.i.0 << _EOF
Match host xxxxxx
_EOF

cat > $OBJ/sshd_config.i.1 << _EOF
Match host a
	Banner /aaa

Match host b
	Banner /bbb

Match host c
	Banner /ccc

Match Host d
	Banner /ddd

Match Host e
	Banner /eee

Match Host f
	Banner /fff
_EOF

cat > $OBJ/sshd_config.i.2 << _EOF
Match host a
	Banner /aaaa

Match host b
	Banner /bbbb

Match host c # comment
	Banner /cccc

Match Host d
	Banner /dddd

Match Host e
	Banner /eeee

Match Host f
	Banner /ffff

Match all
	Banner /xxxx
_EOF

trial() {
	_host="$1"
	_exp="$2"
	_desc="$3"
	test -z "$_desc" && _desc="test match"
	trace "$_desc host=$_host expect=$_exp"
	${SUDO} ${REAL_SSHD} -f $OBJ/sshd_config.i -T \
	    -C "host=$_host,user=test,addr=127.0.0.1" > $OBJ/sshd_config.out ||
		fatal "ssh config parse failed: $_desc host=$_host expect=$_exp"
	_got=`grep -i '^banner ' $OBJ/sshd_config.out | awk '{print $2}'`
	if test "x$_exp" != "x$_got" ; then
		fail "$desc_ host $_host include fail: expected $_exp got $_got"
	fi
}

trial a /aa
trial b /bb
trial c /ccc
trial d /dd
trial e /ee
trial f /fff
trial m /xxxx
trial n /xxxx
trial x none

# Prepare an included config with an error.

cat > $OBJ/sshd_config.i.3 << _EOF
Banner xxxx
	Junk
_EOF

trace "disallow invalid config host=a"
${SUDO} ${REAL_SSHD} -f $OBJ/sshd_config.i \
    -C "host=a,user=test,addr=127.0.0.1" 2>/dev/null && \
	fail "sshd include allowed invalid config"

trace "disallow invalid config host=x"
${SUDO} ${REAL_SSHD} -f $OBJ/sshd_config.i \
    -C "host=x,user=test,addr=127.0.0.1" 2>/dev/null && \
	fail "sshd include allowed invalid config"

rm -f $OBJ/sshd_config.i.*

# Ensure that a missing include is not fatal.
cat > $OBJ/sshd_config.i << _EOF
HostKey $OBJ/host.ssh-ed25519
Include $OBJ/sshd_config.i.*
Banner /aa
_EOF

trial a /aa "missing include non-fatal"

# Ensure that Match/Host in an included config does not affect parent.
cat > $OBJ/sshd_config.i.x << _EOF
Match host x
_EOF

trial a /aa "included file does not affect match state"

# Ensure the empty include directive is not accepted
cat > $OBJ/sshd_config.i.x << _EOF
Include
_EOF

trace "disallow invalid with no argument"
${SUDO} ${REAL_SSHD} -f $OBJ/sshd_config.i.x -T \
    -C "host=x,user=test,addr=127.0.0.1" 2>/dev/null && \
	fail "sshd allowed Include with no argument"

# Ensure the Include before any Match block works as expected (bug #3122)
cat > $OBJ/sshd_config.i << _EOF
Banner /xx
HostKey $OBJ/host.ssh-ed25519
Include $OBJ/sshd_config.i.2
Match host a
	Banner /aaaa
_EOF
cat > $OBJ/sshd_config.i.2 << _EOF
Match host a
	Banner /aa
_EOF

trace "Include before match blocks"
trial a /aa "included file before match blocks is properly evaluated"

# Port in included file is correctly interpretted (bug #3169)
cat > $OBJ/sshd_config.i << _EOF
Include $OBJ/sshd_config.i.2
Port 7722
_EOF
cat > $OBJ/sshd_config.i.2 << _EOF
HostKey $OBJ/host.ssh-ed25519
_EOF

trace "Port after included files"
${SUDO} ${REAL_SSHD} -f $OBJ/sshd_config.i -T \
    -C "host=x,user=test,addr=127.0.0.1" > $OBJ/sshd_config.out || \
	fail "failed to parse Port after included files"
_port=`grep -i '^port ' $OBJ/sshd_config.out | awk '{print $2}'`
if test "x7722" != "x$_port" ; then
	fail "The Port in included file was intertepretted wrongly. Expected 7722, got $_port"
fi

# cleanup
rm -f $OBJ/sshd_config.i $OBJ/sshd_config.i.* $OBJ/sshd_config.out
