# From Gawk Manual modified by bug fix and removal of punctuation
# Record every word which is used at least once
{
	for (i = 1; i <= NF; i++) {
		tmp = tolower($i)
		if (0 != (pos = match(tmp, /([a-z]|-)+/)))
			used[substr(tmp, pos, RLENGTH)] = 1
	}
}

#Find a number of distinct words longer than 10 characters
END {
	num_long_words = 0
	for (x in used) 
		if (length(x) > 10) {
			++num_long_words
			print x
		}
	print num_long_words, "long words"
}
