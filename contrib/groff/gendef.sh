# gendef filename var=val var=val

file=$1
shift

defs="#define $1"
shift
for def
do
	defs="$defs
#define $def"
done

# Use $TMPDIR if defined.  Default to cwd, for non-Unix systems
# which don't have /tmp on each drive (we are going to remove
# the file before we exit anyway).  Put the PID in the basename,
# since the extension can only hold 3 characters on MS-DOS.
t=${TMPDIR-.}/gro$$.tmp

sed -e 's/=/ /' >$t <<EOF
$defs
EOF

test -r $file && cmp -s $t $file || cp $t $file

rm -f $t

exit 0
