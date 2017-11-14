# $FreeBSD$
#
PATH=/sbin:/bin:/usr/sbin:/usr/bin:/usr/local/sbin:/usr/local/bin:~/bin
export PATH
HOME=/root
export HOME
TERM=${TERM:-xterm}
export TERM
PAGER=more
export PAGER

if [ -x /usr/bin/resizewin ] ; then /usr/bin/resizewin -z ; fi
