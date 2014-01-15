# $Id: extract-libmix-v.sh 2079 2011-10-27 04:10:55Z jkoshy $
inittest extract-libmix-v tc/extract-libmix-v
extshar ${TESTDIR}
extshar ${RLTDIR}
runcmd "${AR} xv libmix.a" work true
rundiff true
