BEGIN { print "failures=0"; }
!/^#/ && NF == 3 {
	print "echo '" $3 "' | $1/egrep -e '" $2 "' > /dev/null 2>&1";
	print "if [ $? != " $1 " ]"
	print "then"
	printf "\techo Spencer test \\#%d failed\n", ++n
	print "\tfailures=1"
	print "fi"
}
END { print "exit $failures"; }
