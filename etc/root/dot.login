# $FreeBSD$
#
# .login - csh login script, read by login shell, after `.cshrc' at login.
#
# see also csh(1), environ(7).
#

# Query terminal size; useful for serial lines.
if ( -x /usr/bin/resizewin ) /usr/bin/resizewin -z

# Uncomment to display a random cookie each login:
# if ( -x /usr/bin/fortune ) /usr/bin/fortune -s
