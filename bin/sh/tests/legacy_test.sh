# $FreeBSD$

: ${SH:="__SH__"}
export SH

# TODO(jmmv): The Kyua TAP interface should be passing us the value of
# "srcdir" as an environment variable, just as it does with the ATF
# interface in the form of a configuration variable.  For now, just try
# to guess this.
: ${TESTS_DATA:=$(dirname ${0})}

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
	sed -I '' -e "s|^${TESTS_DATA}|.|" tmp.stderr
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

TESTS=$(find -Es ${TESTS_DATA} -regex ".*\.[0-9]+")
printf "1..%d\n" $(echo ${TESTS} | wc -w)

for i in ${TESTS} ; do
	do_test ${i} ${i##*.}
done
