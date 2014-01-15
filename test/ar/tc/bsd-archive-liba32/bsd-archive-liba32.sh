# $Id: bsd-archive-liba32.sh 2078 2011-10-27 04:04:27Z jkoshy $
if ! uname -m | grep -q 64; then
    inittest bsd-archive-liba32 tc/bsd-archive-liba32
    extshar ${TESTDIR}
    extshar ${RLTDIR}
    runcmd "${AR} cru --flavor bsd liba.a a1.o a2.o a3.o a4.o" work true
    rundiff false
    runcmd "plugin/teraser -c -t bsd-archive-liba32 liba.a" work false
    runcmd "plugin/ardiff -cnlt bsd-archive-liba32 ${RLTDIR}/liba.a liba.a" work false
fi
