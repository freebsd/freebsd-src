BEGIN {
	if (ARGV[1]) print 1
	ARGV[1] = ""
	if (ARGV[2]) print 2
	ARGV[2] = ""
	if ("0") print "zero"
	if ("") print "null"
	if (0) print 0
}
{
	if ($0) print $0
	if ($1) print $1
}
