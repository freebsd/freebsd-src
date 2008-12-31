# $FreeBSD: src/sys/tools/emu10k1-mkalsa.sh,v 1.1.26.1 2008/11/25 02:59:29 kensmith Exp $

GREP=${GREP:-grep}
CC=${CC:-cc}
AWK=${AWK:-awk}
MV=${MV:=mv}
RM=${RM:=rm}
IN=$1
OUT=$2

trap "${RM} -f $OUT.tmp" EXIT

$GREP -v '#include' $IN | \
$CC -E -D__KERNEL__ -dM -  | \
$AWK -F"[     (]" '
/define/  {
	print "#ifndef " $2;
	print;
	print "#endif";
}' > $OUT.tmp
${MV} -f $OUT.tmp $OUT
