BEGIN {
	extension("./filefuncs.so", "dlload")

#	printf "before: "
#	fflush()
#	system("pwd")
#
#	chdir("..")
#
#	printf "after: "
#	fflush()
#	system("pwd")

	chdir(".")

	data[1] = 1
	print "Info for testff.awk"
	ret = stat("testff.awk", data)
	print "ret =", ret
	for (i in data)
		printf "data[\"%s\"] = %s\n", i, data[i]
	print "testff.awk modified:", strftime("%m %d %y %H:%M:%S", data["mtime"])

	print "\nInfo for JUNK"
	ret = stat("JUNK", data)
	print "ret =", ret
	for (i in data)
		printf "data[\"%s\"] = %s\n", i, data[i]
	print "JUNK modified:", strftime("%m %d %y %H:%M:%S", data["mtime"])
}
