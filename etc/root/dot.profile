#	$Id: dot.profile,v 1.6 1994/09/16 04:20:14 rgrimes Exp $
#
PATH=/sbin:/usr/sbin:/bin:/usr/bin:/usr/local/bin
echo 'erase ^H, kill ^U, intr ^C'
stty crt erase ^H kill ^U intr ^C
export PATH
HOME=/root
export HOME
TERM=pc3
export TERM
