#	$Id: dot.profile,v 1.12 1997/02/23 09:21:14 peter Exp $
#
PATH=/sbin:/usr/sbin:/bin:/usr/bin:/usr/local/bin
echo 'erase ^H, kill ^U, intr ^C'
stty crt erase ^H kill ^U intr ^C
export PATH
HOME=/root
export HOME
TERM=${TERM:-cons25}
export TERM
