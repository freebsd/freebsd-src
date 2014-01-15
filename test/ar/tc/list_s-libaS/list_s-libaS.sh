# $Id: list_s-libaS.sh 2079 2011-10-27 04:10:55Z jkoshy $
inittest list_s-libaS tc/list_s-libaS
extshar ${TESTDIR}
extshar ${RLTDIR}
runcmd "${AR} ts libaS.a" work true
runcmd "plugin/teraser -ce -t list_s-libaS libaS.a" work false
runcmd "plugin/teraser -e libaS.a" result false
rundiff true
