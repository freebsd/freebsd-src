#! /bin/sh
# Things like
# '-s 10 file...' should plot Maxlife vs Wastage, Real, user + sys for all
# file for Maxsize == 10.
# '-l 10 file...' should plot Maxsize vs ... for Maxlife == 10.
usage="Usage: $0 [-s size | -l life] file..."
case $# in
[012])
	echo $usage >&2
	exit 1
	;;
esac
tmp=./tmp.$$
case $1 in
-s)
	const='Maxsize'
	indep='Maxlife'
	units='iterations'
	;;
-l)
	const='Maxlife'
	indep='Maxsize'
	units='words'
	;;
*)
	echo $usage >&2
	exit 1
	;;
esac
constval=$2
shift
shift
mkdir $tmp
for i
do
	base=`basename $i`
	echo $base
	ext=`expr "$base" : "res\.\(.*\)"`
	awk '$1 == "Maxtime" {
		for(i = 1; i <= NF; i++) {
			field[$i] = i;
		}
		f1="'$tmp/W.$base'";
		f2="'$tmp/R.$base'";
		f3="'$tmp/US.$base'";
		print "\"" "'$ext'" > f1
		print "\"" "'$ext'" > f2
		print "\"" "'$ext'" > f3
		cfld=field["'$const'"];
		cval='$constval';
		xfld=field["'$indep'"];
		y1=field["Wastage"];
		y2=field["Real"];
		y3=field["User"];
		y4=field["Sys"];
	}
	$cfld == cval {
		print $xfld, $y1 * 100 >> f1;
		print $xfld, $y2 >> f2;
		print $xfld, $y3 + $y4 >> f3;
	}
	END {
		print "" >> f1;
		print "" >> f2;
		print "" >> f3;
	}' $i
done
cat $tmp/W.* > $tmp/W
rm -f $tmp/W.*
cat $tmp/R.* > $tmp/R
rm -f $tmp/R.*
cat $tmp/US.* > $tmp/US
rm -f $tmp/US.*
cd $tmp
xgraph -tk -bb -t "$const = $constval" -x "$indep ($units)" \
	-y 'User + System time (seconds)' US &
xgraph -tk -bb -t "$const = $constval" -x "$indep ($units)" \
	-y 'Elapsed time (seconds)' R &
xgraph -tk -bb -t "$const = $constval" -x "$indep ($units)" \
	-y 'Wastage (percent of data segment)' W &

