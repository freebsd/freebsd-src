# $Id: elfcopy-N-1.sh 2081 2011-10-27 04:28:29Z jkoshy $
inittest elfcopy-N-1 tc/elfcopy-N-1
extshar ${TESTDIR}
extshar ${RLTDIR}
runcmd "${ELFCOPY} -N bar2 sym.o sym.o.1" work true
rundiff true
