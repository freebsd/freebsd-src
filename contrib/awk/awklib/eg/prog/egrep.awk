# egrep.awk --- simulate egrep in awk
# Arnold Robbins, arnold@gnu.org, Public Domain
# May 1993

# Options:
#    -c    count of lines
#    -s    silent - use exit value
#    -v    invert test, success if no match
#    -i    ignore case
#    -l    print filenames only
#    -e    argument is pattern

BEGIN {
    while ((c = getopt(ARGC, ARGV, "ce:svil")) != -1) {
        if (c == "c")
            count_only++
        else if (c == "s")
            no_print++
        else if (c == "v")
            invert++
        else if (c == "i")
            IGNORECASE = 1
        else if (c == "l")
            filenames_only++
        else if (c == "e")
            pattern = Optarg
        else
            usage()
    }
    if (pattern == "")
        pattern = ARGV[Optind++]

    for (i = 1; i < Optind; i++)
        ARGV[i] = ""
    if (Optind >= ARGC) {
        ARGV[1] = "-"
        ARGC = 2
    } else if (ARGC - Optind > 1)
        do_filenames++

#    if (IGNORECASE)
#        pattern = tolower(pattern)
}
#{
#    if (IGNORECASE)
#        $0 = tolower($0)
#}
function beginfile(junk)
{
    fcount = 0
}
function endfile(file)
{
    if (! no_print && count_only)
        if (do_filenames)
            print file ":" fcount
        else
            print fcount

    total += fcount
}
{
    matches = ($0 ~ pattern)
    if (invert)
        matches = ! matches

    fcount += matches    # 1 or 0

    if (! matches)
        next

    if (no_print && ! count_only)
        nextfile

    if (filenames_only && ! count_only) {
        print FILENAME
        nextfile
    }

    if (do_filenames && ! count_only)
        print FILENAME ":" $0
    else if (! count_only)
        print
}
END    \
{
    if (total == 0)
        exit 1
    exit 0
}
function usage(    e)
{
    e = "Usage: egrep [-csvil] [-e pat] [files ...]"
    print e > "/dev/stderr"
    exit 1
}
