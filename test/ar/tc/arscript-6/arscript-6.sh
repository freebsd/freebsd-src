# $Id: arscript-6.sh 2078 2011-10-27 04:04:27Z jkoshy $
inittest arscript-6 tc/arscript-6
extshar ${TESTDIR}
extshar ${RLTDIR}
runcmd "${AR} -M < liba.script.bsd" work true
rundiff false
runcmd "plugin/teraser -c -t arscript-6 liba.a" work false
runcmd "plugin/teraser -c -t arscript-6 liblong.a" work false
runcmd "plugin/ardiff -cnlt arscript-6 ${RLTDIR}/liba.a liblong.a" work false
runcmd "plugin/ardiff -cnlt arscript-6 ${RLTDIR}/liblong.a liba.a" work false
