# $FreeBSD$

if [ -z "${SH}" ]; then
	echo '${SH} is not set, please correct and re-run.'
	exit 1
fi
export SH=${SH}

COUNTER=1

do_test() {
	c=${COUNTER}
	COUNTER=$((COUNTER+1))
	${SH} $1 > tmp.stdout 2> tmp.stderr
	if [ $? -ne $2 ]; then
		echo "not ok ${c} - ${1} # wrong exit status"
		rm tmp.stdout tmp.stderr
		return
	fi
	for i in stdout stderr; do
		if [ -f ${1}.${i} ]; then
			if ! cmp -s tmp.${i} ${1}.${i}; then
				echo "not ok ${c} - ${1} # wrong output on ${i}"
				rm tmp.stdout tmp.stderr
				return
			fi
		elif [ -s tmp.${i} ]; then
			echo "not ok ${c} - ${1} # wrong output on ${i}"
			rm tmp.stdout tmp.stderr
			return
		fi
	done
	echo "ok ${c} - ${1}"
	rm tmp.stdout tmp.stderr
}

TESTS=$(find -Es . -regex ".*\.[0-9]+")
printf "1..%d\n" $(echo ${TESTS} | wc -w)

for i in ${TESTS} ; do
	do_test ${i} ${i##*.}
done
