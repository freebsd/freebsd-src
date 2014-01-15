# $Id: compbase-liba-v.sh 2078 2011-10-27 04:04:27Z jkoshy $
inittest compbase-liba-v tc/compbase-liba-v
extshar ${TESTDIR}
extshar ${RLTDIR}
runcmd "${AR} mv liba.a ./a1.o" work true
runcmd "plugin/teraser -ce -t compbase-liba-v liba.a" work false
runcmd "plugin/teraser -e liba.a" result false
rundiff true
