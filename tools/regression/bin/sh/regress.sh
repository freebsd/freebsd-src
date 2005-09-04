# $FreeBSD$

echo '1..31'

COUNTER=1

do_test() {
	local c
	c=${COUNTER}
	COUNTER=$((COUNTER+1))
	sh $1 > tmp.stdout 2> tmp.stderr
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

SUCCESS=$(find . -name "*.0")
for i in ${SUCCESS} ; do
	do_test ${i} 0
done
	
FAILURE=$(find . -name "*.1")
for i in ${FAILURE} ; do
	do_test ${i} 1
done
