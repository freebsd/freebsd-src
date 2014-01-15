# $Id: elfcopy-noops-1.sh 2081 2011-10-27 04:28:29Z jkoshy $
inittest elfcopy-noops-1 tc/elfcopy-noops-1
extshar ${TESTDIR}
extshar ${RLTDIR}
runcmd "${ELFCOPY} pkill pkill.new" work true
rundiff true
