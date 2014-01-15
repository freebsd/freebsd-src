# $Id: arscript-5.sh 2078 2011-10-27 04:04:27Z jkoshy $
inittest arscript-5 tc/arscript-5
extshar ${TESTDIR}
extshar ${RLTDIR}
runcmd "${AR} -M < liba.script.bsd" work true
rundiff false
runcmd "plugin/teraser -c -t arscript-5 liba.a" work false
runcmd "plugin/teraser -c -t arscript-5 libb.a" work false
runcmd "plugin/ardiff -cnlt arscript-5 ${RLTDIR}/liba.a liba.a" work false
runcmd "plugin/ardiff -cnlt arscript-5 ${RLTDIR}/libb.a libb.a" work false
