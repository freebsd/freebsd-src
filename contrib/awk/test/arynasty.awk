BEGIN {
	a = 12.153
#print "-- stroring test[a]" > "/dev/stderr" ; fflush("/dev/stderr")
	test[a] = "hi"
#print "-- setting CONVFMT" > "/dev/stderr" ; fflush("/dev/stderr")
	CONVFMT = "%.0f"
#print "-- setting a" > "/dev/stderr" ; fflush("/dev/stderr")
	a = 5
#stopme()
#print "-- starting loop" > "/dev/stderr" ; fflush("/dev/stderr")
	for (i in test) {
#print("-- i =", i) > "/dev/stderr" ; fflush("/dev/stderr");
#printf("-- i = <%s>\n", i) > "/dev/stderr" ; fflush("/dev/stderr");
		printf ("test[%s] = %s\n", i, test[i])
	}
}
