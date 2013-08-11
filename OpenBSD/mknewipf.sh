#!/bin/sh
# documented from
# http://www.tfsb.org/ipf-openbsd/
ARCH=sparc
KERNEL=MULAN
IPF=ip-fil3.4.17
rm -rf $IPF
tar zxf $IPF.tar.gz
cd $IPF
perl -pi -e "s/#STATETOP_CFLAGS=/STATETOP_CFLAGS=/" Makefile
perl -pi -e "s/#STATETOP_INC=$/STATETOP_INC=/" Makefile
perl -pi -e "s/#STATETOP_LIB=-lncurses/STATETOP_LIB=-lcurses/" Makefile
perl -pi -e "s/#INET6/INET6/" Makefile
make openbsd
make install-bsd
cd OpenBSD
echo $KERNEL | ./kinstall >/dev/null 2>&1
cd /usr/src/sys/arch/$ARCH/conf
config $KERNEL
cd /usr/src/sys/arch/$ARCH/compile/$KERNEL
make clean && make depend && make && mv /bsd /bsd.old && mv bsd /bsd && reboot
