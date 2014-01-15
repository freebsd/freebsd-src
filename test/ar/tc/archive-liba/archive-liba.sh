# $Id: archive-liba.sh 2078 2011-10-27 04:04:27Z jkoshy $
inittest archive-liba tc/archive-liba
extshar ${TESTDIR}
extshar ${RLTDIR}
runcmd "${AR} cru liba.a a1.o a2.o a3.o a4.o" work true
rundiff false
runcmd "plugin/teraser -c -t archive-liba liba.a" work false
runcmd "plugin/ardiff -cnlt archive-liba ${RLTDIR}/liba.a liba.a" work false
