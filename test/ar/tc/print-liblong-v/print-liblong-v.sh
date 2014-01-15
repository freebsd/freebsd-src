# $Id: print-liblong-v.sh 2079 2011-10-27 04:10:55Z jkoshy $
inittest print-liblong-v tc/print-liblong-v
extshar ${TESTDIR}
extshar ${RLTDIR}
runcmd "${AR} vp liblong.a" work true
rundiff true
