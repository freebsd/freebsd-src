# program to delete duplicate lines in a file
#
{
	if (old != $0)
		printf "%s\n", $0
	old = $0
}
