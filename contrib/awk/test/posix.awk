BEGIN {
	a = "+2"; b = 2; c = "+2a"; d = "+2 "; e = " 2"

	printf "Test #1: "
	if (b == a) print "\"" a "\"" " compares as a number"
	else print "\"" a "\"" " compares as a string"

	printf "Test #2: "
	if (b == c) print "\"" c "\"" " compares as a number"
	else print "\"" c "\"" " compares as a string"

	printf "Test #3: "
	if (b == d) print "\"" d "\"" " compares as a number"
	else print "\"" d "\"" " compares as a string"

	printf "Test #4: "
	if (b == e) print "\"" e "\"" " compares as a number"
	else print "\"" e "\"" " compares as a string"

	f = a + b + c + d + e
	print "after addition"

	printf "Test #5: "
	if (b == a) print "\"" a "\"" " compares as a number"
	else print "\"" a "\"" " compares as a string"

	printf "Test #6: "
	if (b == c) print "\"" c "\"" " compares as a number"
	else print "\"" c "\"" " compares as a string"

	printf "Test #7: "
	if (b == d) print "\"" d "\"" " compares as a number"
	else print "\"" d "\"" " compares as a string"

	printf "Test #8: "
	if (b == e) print "\"" e "\"" " compares as a number"
	else print "\"" e "\"" " compares as a string"

	printf "Test #9: "
	if ("3e5" > "5") print "\"3e5\" > \"5\""
	else print "\"3e5\" <= \"5\""

	printf "Test #10: "
	x = 32.14
	y[x] = "test"
	OFMT = "%e"
	print y[x]

	printf "Test #11: "
	x = x + 0
	print y[x]

	printf "Test #12: "
	OFMT="%f"
	CONVFMT="%e"
	print 1.5, 1.5 ""

	printf "Test #13: "
	if ( 1000000 "" == 1000001 "") print "match"
	else print "nomatch"
}
{
	printf "Test #14: "
	FS = ":"
	print $1
	FS = ","
	printf "Test #15: "
	print $2
}
