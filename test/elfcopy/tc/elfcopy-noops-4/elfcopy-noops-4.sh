# $Id: elfcopy-noops-4.sh 2081 2011-10-27 04:28:29Z jkoshy $
inittest elfcopy-noops-4 tc/elfcopy-noops-4
extshar ${TESTDIR}
extshar ${RLTDIR}
runcmd "${ELFCOPY} vi vi.new" work true
rundiff true
