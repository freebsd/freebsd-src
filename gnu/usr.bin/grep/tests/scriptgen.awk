BEGIN { print "failures=0"; }
$0 !~ /^#/ && NF == 3 {
	print "echo '" $3 "' | ./grep -E -e '" $2 "' > /dev/null 2>&1";
	print "if [ $? != " $1 " ]"
	print "then"
	printf "\techo Spencer test \\#%d failed\n", ++n
	print "\tfailures=1"
	print "fi"
}
END { print "exit $failures"; }
