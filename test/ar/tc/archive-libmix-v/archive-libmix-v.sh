# $Id: archive-libmix-v.sh 2078 2011-10-27 04:04:27Z jkoshy $
inittest archive-libmix-v tc/archive-libmix-v
extshar ${TESTDIR}
extshar ${RLTDIR}
runcmd "${AR} cruv libmix.a a1_has_a_long_file_name.o a2_non_elf.o a3_non_elf_with_a_long_file_name.o a4_is_16_long_.o" work true
rundiff false
runcmd "plugin/teraser -c -t archive-libmix-v libmix.a" work false
runcmd "plugin/ardiff -cnlt archive-libmix-v ${RLTDIR}/libmix.a libmix.a" work false
