# $Id: arscript-4.sh 2078 2011-10-27 04:04:27Z jkoshy $
inittest arscript-4 tc/arscript-4
extshar ${TESTDIR}
extshar ${RLTDIR}
runcmd "${AR} -M < liba.script.bsd" work true
rundiff true
