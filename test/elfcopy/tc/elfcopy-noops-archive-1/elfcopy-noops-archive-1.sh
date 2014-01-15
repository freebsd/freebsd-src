# $Id: elfcopy-noops-archive-1.sh 2081 2011-10-27 04:28:29Z jkoshy $
inittest elfcopy-noops-archive-1 tc/elfcopy-noops-archive-1
extshar ${TESTDIR}
extshar ${RLTDIR}
runcmd "${ELFCOPY} liba.a liba.a.1" work true
rundiff false
runcmd "plugin/teraser -c -t elfcopy-noops-archive-1 liba.a.1" work false
runcmd "plugin/ardiff -cnlt elfcopy-noops-archive-1 ${RLTDIR}/liba.a.1 liba.a.1" work false
