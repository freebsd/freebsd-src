# $Id: moveafter-libmix-v.sh 2079 2011-10-27 04:10:55Z jkoshy $
inittest moveafter-libmix-v tc/moveafter-libmix-v
extshar ${TESTDIR}
extshar ${RLTDIR}
runcmd "${AR} mav a4_is_16_long_.o libmix.a a2_non_elf.o" work true
runcmd "plugin/teraser -ce -t moveafter-libmix-v libmix.a" work false
runcmd "plugin/teraser -e libmix.a" result false
rundiff true
