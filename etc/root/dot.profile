#	$Id: dot.profile,v 1.7 1994/09/22 08:32:39 rgrimes Exp $
#
PATH=/sbin:/usr/sbin:/bin:/usr/bin:/usr/local/bin
echo 'erase ^H, kill ^U, intr ^C'
stty crt erase ^H kill ^U intr ^C
export PATH
HOME=/root
export HOME
TERM=cons25
export TERM
