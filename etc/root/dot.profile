#	$Id: dot.profile,v 1.15 1997/09/26 08:28:19 joerg Exp $
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
export crt
