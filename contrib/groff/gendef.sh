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

t=/tmp/groff.$$

sed -e 's/=/ /' >$t <<EOF
$defs
EOF

test -r $file && cmp -s $t $file || cp $t $file

rm -f $t

exit 0
