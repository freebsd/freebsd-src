# $Id: movebefore_movepos-liba-v.sh 2079 2011-10-27 04:10:55Z jkoshy $
inittest movebefore_movepos-liba-v tc/movebefore_movepos-liba-v
extshar ${TESTDIR}
extshar ${RLTDIR}
runcmd "${AR} mbv a2.o liba.a a4.o a2.o a1.o" work true
runcmd "plugin/teraser -ce -t movebefore_movepos-liba-v liba.a" work false
runcmd "plugin/teraser -e liba.a" result false
rundiff true
