# $Id: print-libmix.sh 2079 2011-10-27 04:10:55Z jkoshy $
inittest print-libmix tc/print-libmix
extshar ${TESTDIR}
extshar ${RLTDIR}
runcmd "${AR} p libmix.a" work true
rundiff true
