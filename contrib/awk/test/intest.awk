BEGIN {
	bool = ((b = 1) in c);
	print bool, b	# gawk-3.0.1 prints "0 "; should print "0 1"
} 
