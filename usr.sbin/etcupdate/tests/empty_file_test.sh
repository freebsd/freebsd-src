#!/bin/sh

# Regression test for "etcupdate: Do not ignore empty files"

FAILED=no
WORKDIR=$(pwd)/work

usage()
{
	echo "Usage: empty_file_test.sh [-s script] [-w workdir]"
	exit 1
}

COMMAND=etcupdate
while getopts "s:w:" option; do
	case $option in
		s)
			COMMAND="sh $OPTARG"
			;;
		w)
			WORKDIR=$OPTARG
			;;
		*)
			echo
			usage
			;;
	esac
done
shift $((OPTIND - 1))
if [ $# -ne 0 ]; then
	usage
fi

SRC=$WORKDIR/src
DEST=$WORKDIR/dest
TEST=$WORKDIR/test

# Clean up
rm -rf $WORKDIR

# Create a mock source tree
mkdir -p $SRC
touch $SRC/empty_file

# Create a mock make script
cat > $WORKDIR/mock_make.sh <<EOF
#!/bin/sh

# Scan all arguments for targets
for arg in "\$@"; do
    case \$arg in
        *=*)
            # Export variable assignments
            export "\$arg"
            ;;
        distrib-dirs)
            if [ -n "\$DESTDIR" ]; then
                mkdir -p "\$DESTDIR/etc"
            fi
            ;;
        distribution)
            if [ -n "\$DESTDIR" ]; then
                 cp $SRC/empty_file "\$DESTDIR/etc/empty_file"
                 echo "./etc/empty_file type=file mode=0644 uname=root gname=wheel" > "\$DESTDIR/METALOG"
            fi
            ;;
    esac
done
exit 0
EOF
chmod +x $WORKDIR/mock_make.sh

# Run etcupdate
# Use -B to skip build targets
# Use -N to run without root privileges
# Use 'extract' command to bypass root check in 'update' command
$COMMAND extract -N -B -s $SRC -d $WORKDIR -m $WORKDIR/mock_make.sh > $WORKDIR/test.out 2>&1

if [ -f $WORKDIR/current/etc/empty_file ]; then
    echo "Empty file preserved."
else
    echo "Empty file missing from current tree."
    FAILED=yes
fi

[ "${FAILED}" = no ]
