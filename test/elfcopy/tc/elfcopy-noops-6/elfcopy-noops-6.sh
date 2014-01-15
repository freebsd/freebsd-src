# $Id: elfcopy-noops-6.sh 2081 2011-10-27 04:28:29Z jkoshy $
inittest elfcopy-noops-6 tc/elfcopy-noops-6
extshar ${TESTDIR}
extshar ${RLTDIR}
runcmd "${ELFCOPY} mcs.o mcs.o.1" work true
rundiff true
