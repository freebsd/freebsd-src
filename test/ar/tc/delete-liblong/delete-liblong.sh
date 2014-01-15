# $Id: delete-liblong.sh 2079 2011-10-27 04:10:55Z jkoshy $
inittest delete-liblong tc/delete-liblong
extshar ${TESTDIR}
extshar ${RLTDIR}
runcmd "${AR} d liblong.a a2_is_15_long.o a4_is_16_long_.o" work true
runcmd "plugin/teraser -ce -t delete-liblong liblong.a" work false
runcmd "plugin/teraser -e liblong.a" result false
rundiff true
