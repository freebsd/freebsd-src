# $Id: elfcopy-L-2.sh 2081 2011-10-27 04:28:29Z jkoshy $
inittest elfcopy-L-2 tc/elfcopy-L-2
extshar ${TESTDIR}
extshar ${RLTDIR}
runcmd "${ELFCOPY} -L _end a.out a.out.1" work true
rundiff true
