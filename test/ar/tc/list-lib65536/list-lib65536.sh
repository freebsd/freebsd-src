# $Id: list-lib65536.sh 2079 2011-10-27 04:10:55Z jkoshy $
inittest list-lib65536 tc/list-lib65536
extshar ${TESTDIR}
extshar ${RLTDIR}
runcmd "${AR} t lib65536.a" work true
rundiff true
