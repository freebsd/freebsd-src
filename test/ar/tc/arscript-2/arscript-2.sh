# $Id: arscript-2.sh 2078 2011-10-27 04:04:27Z jkoshy $
inittest arscript-2 tc/arscript-2
extshar ${TESTDIR}
extshar ${RLTDIR}
runcmd "${AR} -M < kUtil.a.ar-script.bsd" work true
rundiff false
runcmd "plugin/teraser -c -t arscript-2 kUtil.a" work false
runcmd "plugin/ardiff -cnlt arscript-2 ${RLTDIR}/kUtil.a kUtil.a" work false
