# $Id: usage-bi.sh 2079 2011-10-27 04:10:55Z jkoshy $
inittest usage-bi tc/usage-bi
runcmd "${AR} bi bar.o foo.a bar2.o" work true
rundiff true
