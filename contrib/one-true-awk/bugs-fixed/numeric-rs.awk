BEGIN {
	RS = 1;
	while ("echo a1b1c1d" | getline > 0) {
		print $1;
	}
}
