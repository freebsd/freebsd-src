# $FreeBSD$
#
# csh .login file
#

# Interviews settings
#setenv CPU "FREEBSD"
#set path = ($path /usr/local/interviews/bin/$CPU)
#setenv MANPATH "${MANPATH}:/usr/local/interviews/man"

# 8-bit locale (Germany)
#setenv LANG de_DE.ISO_8859-1

# A righteous umask
umask 22

[ -x /usr/games/fortune ] && /usr/games/fortune
