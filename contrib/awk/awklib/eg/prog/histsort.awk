# histsort.awk --- compact a shell history file
# Thanks to Byron Rakitzis for the general idea
#
# Arnold Robbins, arnold@gnu.org, Public Domain
# May 1993

{
    if (data[$0]++ == 0)
        lines[++count] = $0
}

END {
    for (i = 1; i <= count; i++)
        print lines[i]
}
