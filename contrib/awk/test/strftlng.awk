# test file from Paul Eggert, eggert@twinsun.com
# modified for portability (%c doesn't cut it)

BEGIN {
	BUFSIZ = 1024
	simpleformat = format = "%m/%d/%y %H:%M:%S\n"
	clen = length(strftime(format, 0))
	for (i = 1; i < BUFSIZ / clen + 1; i++)
		format = format simpleformat
	printf "%s", strftime(format, 0)
}
