# $Id: extract-liba-v.sh 2079 2011-10-27 04:10:55Z jkoshy $
inittest extract-liba-v tc/extract-liba-v
extshar ${TESTDIR}
extshar ${RLTDIR}
runcmd "${AR} vx liba.a" work true
rundiff true
