# $FreeBSD: src/tools/tools/nanobsd/gateworks/Files/root/.profile,v 1.1.2.1.6.1 2010/12/21 17:09:25 kensmith Exp $
#
PATH=/sbin:/bin:/usr/sbin:/usr/bin:/usr/games:/usr/local/sbin:/usr/local/bin:~/bin
export PATH
HOME=/root; export HOME
TERM=${TERM:-cons25}; export TERM
PAGER=more; export PAGER

#set -o vi
set -o emacs
if [ `id -u` = 0 ]; then
    PS1="`hostname -s`# "
else
    PS1="`hostname -s`% "
fi
