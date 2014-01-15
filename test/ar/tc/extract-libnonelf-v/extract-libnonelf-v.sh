# $Id: extract-libnonelf-v.sh 2079 2011-10-27 04:10:55Z jkoshy $
inittest extract-libnonelf-v tc/extract-libnonelf-v
extshar ${TESTDIR}
extshar ${RLTDIR}
runcmd "${AR} xv libnonelf.a" work true
rundiff true
