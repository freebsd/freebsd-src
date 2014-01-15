# $Id: moveafter_movepos-liba-v.sh 2079 2011-10-27 04:10:55Z jkoshy $
inittest moveafter_movepos-liba-v tc/moveafter_movepos-liba-v
extshar ${TESTDIR}
extshar ${RLTDIR}
runcmd "${AR} mav a2.o liba.a a2.o" work true
runcmd "plugin/teraser -ce -t moveafter_movepos-liba-v liba.a" work false
runcmd "plugin/teraser -e liba.a" result false
rundiff true
