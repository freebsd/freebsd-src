# $Id: delete_all-liba-v.sh 2079 2011-10-27 04:10:55Z jkoshy $
inittest delete_all-liba-v tc/delete_all-liba-v
extshar ${TESTDIR}
extshar ${RLTDIR}
runcmd "${AR} dv liba.a a1.o a2.o a3.o a4.o" work true
rundiff true
