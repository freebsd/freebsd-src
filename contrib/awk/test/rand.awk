BEGIN {
	srand(2)
	for (i = 0; i < 19; i++) 
		printf "%3d ", (1 + int(100 * rand()))
	print ""
}
