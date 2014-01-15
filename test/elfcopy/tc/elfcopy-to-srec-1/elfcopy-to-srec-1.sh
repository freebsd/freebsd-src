# $Id: elfcopy-to-srec-1.sh 2081 2011-10-27 04:28:29Z jkoshy $
inittest elfcopy-to-srec-1 tc/elfcopy-to-srec-1
extshar ${TESTDIR}
extshar ${RLTDIR}
runcmd "${ELFCOPY} -O srec a64.out a64.srec" work true
rundiff true
