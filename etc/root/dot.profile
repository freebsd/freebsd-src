#	$Id: dot.profile,v 1.5 1994/06/15 22:58:49 jkh Exp $
#
PATH=/sbin:/usr/sbin:/bin:/usr/bin:/usr/local/bin:.
echo 'erase ^H, kill ^U, intr ^C'
stty crt erase ^H kill ^U intr ^C
export PATH
HOME=/root
export HOME
TERM=pc3
export TERM
