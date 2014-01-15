# $Id: delete-liba-v.sh 2079 2011-10-27 04:10:55Z jkoshy $
inittest delete-liba-v tc/delete-liba-v
extshar ${TESTDIR}
extshar ${RLTDIR}
runcmd "${AR} dv liba.a a1.o a3.o" work true
runcmd "plugin/teraser -ce -t delete-liba-v liba.a" work false
runcmd "plugin/teraser -e liba.a" result false
rundiff true
