# $FreeBSD$

# Read global symbols from object file.
BEGIN {
        while ("nm -g " ARGV[1] | getline) {
                if (match($0, /^[^[:space:]]+ [^AU] (.*)$/)) {
                        syms[$3] = $2
                }
        }
}

# De-list symbols from the export list.
// {
        if (ARGIND == 1)
                nextfile
        delete syms[$0]
}

# Strip commons, make everything else local.
END {
        for (member in syms) {
                if (syms[member] == "C")
                        print "-N" member
                else
                        print "-L" member
        }
}
