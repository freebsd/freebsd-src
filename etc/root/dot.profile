#	$Id: dot.profile,v 1.10 1996/03/06 05:23:00 jkh Exp $
#
PATH=/sbin:/usr/sbin:/bin:/usr/bin:/usr/local/bin
echo 'erase ^H, kill ^U, intr ^C'
stty crt erase ^H kill ^U intr ^C
export PATH
HOME=/root
export HOME
TERM=${TERM:-cons25}
export TERM
