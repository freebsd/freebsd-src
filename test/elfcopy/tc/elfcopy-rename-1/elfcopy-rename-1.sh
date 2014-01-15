# $Id: elfcopy-rename-1.sh 2081 2011-10-27 04:28:29Z jkoshy $
inittest elfcopy-rename-1 tc/elfcopy-rename-1
extshar ${TESTDIR}
extshar ${RLTDIR}
runcmd "${ELFCOPY} --rename-section .text=.text.newname sym.o" work true
rundiff true
