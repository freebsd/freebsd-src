# $FreeBSD$
#
# .login - csh login script, read by login shell, after `.cshrc' at login.
#
# see also csh(1), environ(7).
#

if ( -x /usr/bin/fortune ) /usr/bin/fortune freebsd-tips
