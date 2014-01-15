# $Id: arscript-8.sh 2078 2011-10-27 04:04:27Z jkoshy $
inittest arscript-8 tc/arscript-8
extshar ${TESTDIR}
extshar ${RLTDIR}
runcmd "${AR} -M < liba.script.bsd" work true
rundiff false
runcmd "plugin/teraser -c -t arscript-8 liblong.a" work false
runcmd "plugin/ardiff -cnlt arscript-8 ${RLTDIR}/liba.a liblong.a" work false
