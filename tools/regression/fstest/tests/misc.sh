# $FreeBSD$

ntest=1

name253="_123456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789_12"
name255="${name253}34"
name256="${name255}5"
path1021="${name255}/${name255}/${name255}/${name253}"
path1023="${path1021}/x"
path1024="${path1023}x"

echo ${dir} | egrep '^/' >/dev/null 2>&1
if [ $? -eq 0 ]; then
	maindir="${dir}/../.."
else
	maindir="`pwd`/${dir}/../.."
fi
fstest="${maindir}/fstest"
. ${maindir}/tests/conf

expect()
{
	e="${1}"
	shift
	r=`${fstest} $* 2>/dev/null | tail -1`
	echo "${r}" | egrep '^'${e}'$' >/dev/null 2>&1
	if [ $? -eq 0 ]; then
		echo "ok ${ntest}"
	else
		echo "not ok ${ntest} - tried '$*', expected ${e}, got ${r}"
	fi
	ntest=`expr $ntest + 1`
}

jexpect()
{
	s="${1}"
	d="${2}"
	e="${3}"
	shift 3
	r=`jail -s ${s} / fstest 127.0.0.1 /bin/sh -c "cd ${d} && ${fstest} $* 2>/dev/null" | tail -1`
	echo "${r}" | egrep '^'${e}'$' >/dev/null 2>&1
	if [ $? -eq 0 ]; then
		echo "ok ${ntest}"
	else
		echo "not ok ${ntest} - tried '$*', expected ${e}, got ${r}"
	fi
	ntest=`expr $ntest + 1`
}

test_check()
{
	if [ $* ]; then
		echo "ok ${ntest}"
	else
		echo "not ok ${ntest}"
	fi
	ntest=`expr $ntest + 1`
}

namegen()
{
	echo "fstest_`dd if=/dev/random bs=1k count=1 2>/dev/null | openssl md5`"
}

quick_exit()
{
	echo "1..1"
	echo "ok 1"
	exit 0
}

supported()
{
	case "${1}" in
	chflags)
		if [ ${os} != "FreeBSD" -o ${fs} != "UFS" ]; then
			return 1
		fi
		;;
	lchmod)
		if [ ${os} != "FreeBSD" ]; then
			return 1
		fi
		;;
	esac
	return 0
}

require()
{
	if supported ${1}; then
		return
	fi
	quick_exit
}
