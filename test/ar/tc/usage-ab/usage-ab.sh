# $Id: usage-ab.sh 2079 2011-10-27 04:10:55Z jkoshy $
inittest usage-ab tc/usage-ab
runcmd "${AR} mab bar.o foo.a bar2.o" work true
rundiff true
