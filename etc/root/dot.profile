#	$Id: dot.profile,v 1.10.2.3 1998/02/15 14:31:55 jkh Exp $
#
PATH=/sbin:/usr/sbin:/bin:/usr/bin:/usr/local/bin
export PATH
HOME=/root
export HOME
TERM=${TERM:-cons25}
export TERM
PAGER=more
export PAGER
# make mail(1) happy:
#crt=24
#set crt with no value. mail(1) will then use the system value ( stty(1) )
crt='' 

