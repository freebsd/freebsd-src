BEGIN {
	argn =  " argument" (ARGC > 1 ? "s" : "")
	are  = ARGC > 1 ? "are" : "is"
	print "here we have " ARGC argn
	print "which " are
	for (x = 0; x < ARGC; x++)
		print "\t", ARGV[x]
	print "Environment variable TEST=" ENVIRON["TEST"]
	print "and the current input file is called \"" FILENAME "\""
}

FNR == 1 {
	print "in main loop, this input file is known as \"" FILENAME "\""
}
