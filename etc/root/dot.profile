#	$Id: dot.profile,v 1.10.2.2 1997/09/27 15:35:46 joerg Exp $
#
PATH=/sbin:/usr/sbin:/bin:/usr/bin:/usr/local/bin
echo 'erase ^H, kill ^U, intr ^C'
stty crt erase ^H kill ^U intr ^C
export PATH
HOME=/root
export HOME
TERM=${TERM:-cons25}
export TERM
PAGER=more
export PAGER
# make mail(1) happy:
crt=24
export crt
