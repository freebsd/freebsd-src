# $Id: elfcopy-noops-7.sh 2081 2011-10-27 04:28:29Z jkoshy $
inittest elfcopy-noops-7 tc/elfcopy-noops-7
extshar ${TESTDIR}
extshar ${RLTDIR}
runcmd "${ELFCOPY} sections.o.debug sections.o.debug.1" work true
rundiff true
