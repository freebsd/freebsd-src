# $Id: bsd-extract-liba32-v.sh 2078 2011-10-27 04:04:27Z jkoshy $
inittest bsd-extract-liba32-v tc/bsd-extract-liba32-v
extshar ${TESTDIR}
extshar ${RLTDIR}
runcmd "${AR} xv liba.a" work true
rundiff true
