# $Id: bsd-archive-libnonelf.sh 2078 2011-10-27 04:04:27Z jkoshy $
inittest bsd-archive-libnonelf tc/bsd-archive-libnonelf
extshar ${TESTDIR}
extshar ${RLTDIR}
runcmd "${AR} cruF bsd libnonelf.a ne1 ne2 ne3 ne4" work true
rundiff false
runcmd "plugin/teraser -c -t bsd-archive-libnonelf libnonelf.a" work false
runcmd "plugin/ardiff -cnlt bsd-archive-libnonelf ${RLTDIR}/libnonelf.a libnonelf.a" work false
