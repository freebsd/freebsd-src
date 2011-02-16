# $FreeBSD: src/share/skel/dot.login,v 1.17.2.1.6.1 2010/12/21 17:09:25 kensmith Exp $
#
# .login - csh login script, read by login shell, after `.cshrc' at login.
#
# see also csh(1), environ(7).
#

if ( -x /usr/games/fortune ) /usr/games/fortune freebsd-tips
