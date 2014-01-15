# $Id: arscript-7.sh 2078 2011-10-27 04:04:27Z jkoshy $
inittest arscript-7 tc/arscript-7
extshar ${TESTDIR}
extshar ${RLTDIR}
runcmd "${AR} -M < liba.script.bsd" work true
rundiff false
runcmd "plugin/teraser -c -t arscript-7 liba.a" work false
runcmd "plugin/teraser -c -t arscript-7 liblong.a" work false
runcmd "plugin/ardiff -cnlt arscript-7 ${RLTDIR}/liba.a liblong.a" work false
runcmd "plugin/ardiff -cnlt arscript-7 ${RLTDIR}/liblong.a liba.a" work false
