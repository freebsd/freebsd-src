BEGIN {
	extension("./fork.so", "dlload")

	printf "before fork, pid = %d, ppid = %d\n", PROCINFO["pid"],
			PROCINFO["ppid"]

	fflush()
	ret = fork()
	if (ret < 0)
		printf("ret = %d, ERRNO = %s\n", ret, ERRNO)
	else if (ret == 0)
		printf "child, pid = %d, ppid = %d\n", PROCINFO["pid"],
			PROCINFO["ppid"]
	else {
		system("sleep 3")
		printf "parent, ret = %d\n", ret
		printf "parent, pid = %d, ppid = %d\n", PROCINFO["pid"],
			PROCINFO["ppid"]
	}
}
