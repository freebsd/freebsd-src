# $FreeBSD: src/share/skel/dot.login,v 1.17.2.1.4.1 2010/06/14 02:09:06 kensmith Exp $
#
# .login - csh login script, read by login shell, after `.cshrc' at login.
#
# see also csh(1), environ(7).
#

if ( -x /usr/games/fortune ) /usr/games/fortune freebsd-tips
