# $Id: movebefore-liba-v.sh 2079 2011-10-27 04:10:55Z jkoshy $
inittest movebefore-liba-v tc/movebefore-liba-v
extshar ${TESTDIR}
extshar ${RLTDIR}
runcmd "${AR} mbv a2.o liba.a a4.o a1.o" work true
runcmd "plugin/teraser -ce -t movebefore-liba-v liba.a" work false
runcmd "plugin/teraser -e liba.a" result false
rundiff true
