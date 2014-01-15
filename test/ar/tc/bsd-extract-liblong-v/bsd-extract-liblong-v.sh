# $Id: bsd-extract-liblong-v.sh 2078 2011-10-27 04:04:27Z jkoshy $
inittest bsd-extract-liblong-v tc/bsd-extract-liblong-v
extshar ${TESTDIR}
extshar ${RLTDIR}
runcmd "${AR} xv liblong.a" work true
rundiff true
