#	$Id: dot.profile,v 1.8 1994/10/27 05:27:00 phk Exp $
#
PATH=/sbin:/usr/sbin:/bin:/usr/bin:/usr/local/bin
echo 'erase ^H, kill ^U, intr ^C'
stty crt erase ^H kill ^U intr ^C
export PATH
TERM=cons25
export TERM
