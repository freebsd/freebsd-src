# $Id: elfcopy-to-ihex-1.sh 2081 2011-10-27 04:28:29Z jkoshy $
inittest elfcopy-to-ihex-1 tc/elfcopy-to-ihex-1
extshar ${TESTDIR}
extshar ${RLTDIR}
runcmd "${ELFCOPY} -O ihex a64.out a64.ihex" work true
rundiff true
