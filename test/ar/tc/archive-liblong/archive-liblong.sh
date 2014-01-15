# $Id: archive-liblong.sh 2078 2011-10-27 04:04:27Z jkoshy $
inittest archive-liblong tc/archive-liblong
extshar ${TESTDIR}
extshar ${RLTDIR}
runcmd "${AR} cruv liblong.a a1_has_a_long_file_name.o a2_is_15_long.o a3_normal.o a4_is_16_long_.o" work true
rundiff false
runcmd "plugin/teraser -c -t archive-liblong liblong.a" work false
runcmd "plugin/ardiff -cnlt archive-liblong ${RLTDIR}/liblong.a liblong.a" work false
