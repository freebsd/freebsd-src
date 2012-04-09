# $FreeBSD: src/share/skel/dot.login,v 1.17.2.1.8.1 2012/03/03 06:15:13 kensmith Exp $
#
# .login - csh login script, read by login shell, after `.cshrc' at login.
#
# see also csh(1), environ(7).
#

if ( -x /usr/games/fortune ) /usr/games/fortune freebsd-tips
