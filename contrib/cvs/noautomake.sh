#! /bin/sh

# Let autoconf run when this is set
autoconf=:

usage()
{
	echo "usage: `basename $0` --help | --version"
	echo "usage: `basename $0` [--[no]autoconf]"
}

help()
{
	echo
	echo "This program will touch the files necessary to prevent Automake, aclocal,"
	echo "and, optionally, Autoheader, Autoconf, and configure from running after a"
	echo "fresh update from the CVS repository."
	echo
	echo "    -h | --help        Display this text and exit"
	echo "    -V | --version     Display version and exit"
	echo "    -a | --autoconf    Allow Autoconf & Autoheader to run (default)"
	echo "    -A | --noautoconf  Prevent Autoconf & Autoheader from running"
	echo
	echo "Not running Automake & aclocal causes changes to the following user files"
	echo "to be ignored:"
	echo
	echo "    Makefile.am, acinclude.m4, configure.in"
	echo
	echo "Not running Autoconf & Autoheader causes changes to the following user"
	echo "files to be ignored:"
	echo
	echo "    acconfig.h, configure.in"
}

while getopts VACach-: opt; do
	if test "x$opt" = "x-"; then
		case $OPTARG in
			help)
				opt=h
				;;
			version)
				opt=V
				;;
			autoconf)
				opt=a
				;;
			noautoconf)
				opt=A
				;;
			*)
				opt=?
				;;
		esac
	fi
	case $opt in
		h)
			usage
			help
			exit 0
			;;
		V)
			echo "CVS No Automake 0.1"
			exit 0
			;;
		A)
			autoconf=false
			;;
		a)
			autoconf=:
			;;
		?)
			usage >&2
			exit 2
			;;
	esac
done

# prevent aclocal from running
find . -name aclocal.m4 -exec touch {} \;

# prevent Automake from running
find . -name Makefile.in -exec touch {} \;

# prevent Autoheader from running
if $autoconf; then :; else
	find . -name 'config.h.in' -exec touch {} \;
fi

# prevent Autoconf from running
if $autoconf; then :; else
	find . -name configure -exec touch {} \;
fi
