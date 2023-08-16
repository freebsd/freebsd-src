
# Read global symbols from object file.
BEGIN {
	modname = ARGV[1]
        while ("${NM:='nm'} -g " ARGV[1] | getline) {
                if (match($0, /^[^[:space:]]+ [^AU] (.*)$/)) {
                        syms[$3] = $2
                }
        }
        delete ARGV[1]
}

# De-list symbols from the export list.
{
	smbl = $0
	if (!(smbl in syms)) {
		printf "Symbol %s is not present in %s\n",	\
		    smbl, modname > "/dev/stderr"
	}
	delete syms[smbl]
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
