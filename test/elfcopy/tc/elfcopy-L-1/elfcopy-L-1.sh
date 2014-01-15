# $Id: elfcopy-L-1.sh 2081 2011-10-27 04:28:29Z jkoshy $
inittest elfcopy-L-1 tc/elfcopy-L-1
extshar ${TESTDIR}
extshar ${RLTDIR}
runcmd "${ELFCOPY} -L bar -L foo2 sym.o sym.o.1" work true
rundiff true
