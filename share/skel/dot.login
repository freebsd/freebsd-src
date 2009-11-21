# $FreeBSD: src/share/skel/dot.login,v 1.17.2.1.2.1 2009/10/25 01:10:29 kensmith Exp $
#
# .login - csh login script, read by login shell, after `.cshrc' at login.
#
# see also csh(1), environ(7).
#

if ( -x /usr/games/fortune ) /usr/games/fortune freebsd-tips
