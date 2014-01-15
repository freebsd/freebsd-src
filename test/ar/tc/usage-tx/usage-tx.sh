# $Id: usage-tx.sh 2079 2011-10-27 04:10:55Z jkoshy $
inittest usage-tx tc/usage-tx
runcmd "${AR} tx foo.a" work true
rundiff true
