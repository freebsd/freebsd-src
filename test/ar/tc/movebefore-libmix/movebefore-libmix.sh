# $Id: movebefore-libmix.sh 2079 2011-10-27 04:10:55Z jkoshy $
inittest movebefore-libmix tc/movebefore-libmix
extshar ${TESTDIR}
extshar ${RLTDIR}
runcmd "${AR} mb a2_non_elf.o libmix.a a1_has_a_long_file_name.o" work true
runcmd "plugin/teraser -ce -t movebefore-libmix libmix.a" work false
runcmd "plugin/teraser -e libmix.a" result false
rundiff true
