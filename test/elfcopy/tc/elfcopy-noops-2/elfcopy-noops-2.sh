# $Id: elfcopy-noops-2.sh 2081 2011-10-27 04:28:29Z jkoshy $
inittest elfcopy-noops-2 tc/elfcopy-noops-2
extshar ${TESTDIR}
extshar ${RLTDIR}
runcmd "${ELFCOPY} ls ls.new" work true
rundiff true
