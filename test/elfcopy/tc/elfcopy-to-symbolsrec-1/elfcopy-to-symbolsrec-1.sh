# $Id: elfcopy-to-symbolsrec-1.sh 2081 2011-10-27 04:28:29Z jkoshy $
inittest elfcopy-to-symbolsrec-1 tc/elfcopy-to-symbolsrec-1
extshar ${TESTDIR}
extshar ${RLTDIR}
runcmd "${ELFCOPY} -O symbolsrec a64.out a64.srec" work true
rundiff true
