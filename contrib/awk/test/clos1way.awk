BEGIN {
	command = "LC_ALL=C sort"

	n = split("abcdefghijklmnopqrstuvwxyz", a, "")
	for (i = n; i > 0; i--) {
#		print "printing", a[i] > "/dev/stderr"
		print a[i] |& command
	}

	close(command, "to")

#	print "starting read loop" > "/dev/stderr"
	do {
		if (line)
			print "got", line
#		stopme();
	} while ((command |& getline line) > 0)

#	print "doing final close" > "/dev/stderr"
	close(command)
}
