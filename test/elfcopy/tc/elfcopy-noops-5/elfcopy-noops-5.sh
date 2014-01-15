# $Id: elfcopy-noops-5.sh 2081 2011-10-27 04:28:29Z jkoshy $
inittest elfcopy-noops-5 tc/elfcopy-noops-5
extshar ${TESTDIR}
extshar ${RLTDIR}
runcmd "${ELFCOPY} tcsh tcsh.new" work true
rundiff true
