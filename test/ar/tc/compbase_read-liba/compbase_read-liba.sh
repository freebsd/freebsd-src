# $Id: compbase_read-liba.sh 2078 2011-10-27 04:04:27Z jkoshy $
inittest compbase_read-liba tc/compbase_read-liba
extshar ${TESTDIR}
extshar ${RLTDIR}
runcmd "${AR} t liba.a ./a1.o" work true
rundiff true
