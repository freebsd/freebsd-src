function returns_a_str() { print "<in function>" ; return "'A STRING'" }
BEGIN {
	print "partial line:", returns_a_str()
}
