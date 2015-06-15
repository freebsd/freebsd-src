# $FreeBSD: src/etc/root/dot.profile,v 1.21 2007/05/29 06:33:10 dougb Exp $
#
PATH=/sbin:/bin:/usr/sbin:/usr/bin:/usr/local/sbin:/usr/local/bin:~/bin
export PATH
HOME=/root; export HOME
TERM=${TERM:-xterm}; export TERM
PAGER=more; export PAGER

#set -o vi
set -o emacs
if [ `id -u` = 0 ]; then
    PS1="`hostname -s`# "
else
    PS1="`hostname -s`% "
fi
