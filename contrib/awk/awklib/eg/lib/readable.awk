# readable.awk --- library file to skip over unreadable files
#
# Arnold Robbins, arnold@gnu.org, Public Domain
# October 2000

BEGIN {
    for (i = 1; i < ARGC; i++) {
        if (ARGV[i] ~ /^[A-Za-z_][A-Za-z0-9_]*=.*/ \
            || ARGV[i] == "-")
            continue    # assignment or standard input
        else if ((getline junk < ARGV[i]) < 0) # unreadable
            delete ARGV[i]
        else
            close(ARGV[i])
    }
}
