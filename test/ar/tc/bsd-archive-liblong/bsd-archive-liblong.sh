# $Id: bsd-archive-liblong.sh 2078 2011-10-27 04:04:27Z jkoshy $
inittest bsd-archive-liblong tc/bsd-archive-liblong
extshar ${TESTDIR}
extshar ${RLTDIR}
runcmd "${AR} cruF bsd liblong.a ne1 ne2_long_name15 ne3_long_name_16 ne4_very_very_very_long" work true
rundiff false
runcmd "plugin/teraser -c -t bsd-archive-liblong liblong.a" work false
runcmd "plugin/ardiff -cnlt bsd-archive-liblong ${RLTDIR}/liblong.a liblong.a" work false
