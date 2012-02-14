# $FreeBSD$

BEGIN {
	FS = "_"
}

/RELENG_.*_RELEASE/ {
	if (NF == 5) {
		printf "release/%s.%s.%s", $2, $3, $4
		exit
	}
}

/RELENG_.*/ {
	if (NF == 3) {
		printf "releng/%s.%s", $2, $3
		exit
	}

	if (NF == 2) {
		printf "stable/%s", $2
		exit
	}
}

// {
	printf "unknown_branch"
}
