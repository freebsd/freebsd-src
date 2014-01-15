# $Id: elfcopy-N-2.sh 2081 2011-10-27 04:28:29Z jkoshy $
inittest elfcopy-N-2 tc/elfcopy-N-2
extshar ${TESTDIR}
extshar ${RLTDIR}
runcmd "${ELFCOPY} -N bar2 dup.o dup.o.1" work true
rundiff true
