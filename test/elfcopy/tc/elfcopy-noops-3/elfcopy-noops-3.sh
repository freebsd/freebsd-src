# $Id: elfcopy-noops-3.sh 2081 2011-10-27 04:28:29Z jkoshy $
inittest elfcopy-noops-3 tc/elfcopy-noops-3
extshar ${TESTDIR}
extshar ${RLTDIR}
runcmd "${ELFCOPY} ps ps.new" work true
rundiff true
