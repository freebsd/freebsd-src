#	$Id$
#
# .login - csh login script, read by login shell, 
#	   after `.cshrc' at login.
#
# see also csh(1), environ(7).
#

set path = (/sbin /bin /usr/sbin /usr/bin /usr/games /usr/local/bin /usr/X11R6/bin $HOME/bin)
setenv MANPATH "/usr/share/man:/usr/X11R6/man:/usr/local/man"

# Interviews settings
setenv CPU "FREEBSD"
set path = ($path /usr/local/interviews/bin/$CPU)
setenv MANPATH "${MANPATH}:/usr/local/interviews/man"

# 8-bit locale (germany)
#setenv LANG de_DE.ISO_8859-1

# A rightous umask
umask 22

[ -x /usr/games/fortune ] && /usr/games/fortune
