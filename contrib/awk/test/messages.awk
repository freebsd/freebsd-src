# This is a demo of different ways of printing with gawk.  Try it
# with and without -c (compatibility) flag, redirecting output
# from gawk to a file or not.  Some results can be quite unexpected. 
BEGIN {
	print "Goes to a file out1" > "out1"
	print "Normal print statement"
	print "This printed on stdout" > "/dev/stdout"
	print "You blew it!" > "/dev/stderr"
}
