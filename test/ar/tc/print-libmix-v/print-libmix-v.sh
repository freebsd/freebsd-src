# $Id: print-libmix-v.sh 2079 2011-10-27 04:10:55Z jkoshy $
inittest print-libmix-v tc/print-libmix-v
extshar ${TESTDIR}
extshar ${RLTDIR}
runcmd "${AR} pv libmix.a" work true
rundiff true
