# $Id: moveafter-liba-v.sh 2079 2011-10-27 04:10:55Z jkoshy $
inittest moveafter-liba-v tc/moveafter-liba-v
extshar ${TESTDIR}
extshar ${RLTDIR}
runcmd "${AR} mav a2.o liba.a a3.o a4.o" work true
runcmd "plugin/teraser -ce -t moveafter-liba-v liba.a" work false
runcmd "plugin/teraser -e liba.a" result false
rundiff true
