#!/bin/sh

export PATH="$(pwd):${PATH}"
${1:+cd} ${1:-:}
for cmd in *.sh
do
	printf .
	t=${cmd%.sh}

	sh ./${cmd} >${t}.out 2>${t}.err
	echo $? >${t}.rc

	# strip carriage returns from error output
	# in case we are trying to run on MinGW
	tr -d '' >${t}.xerr <${t}.err
	mv ${t}.xerr ${t}.err

	ok=true
	for e in out err rc
	do
		exp=${t}.exp${e}
		got=${t}.${e}
		if ! cmp -s ${exp} ${got}
		then
			echo
			echo FAILED: ${got}: $(cat ${cmd})
			diff -u ${exp} ${got}
			ok=false
		fi
	done

	if ${ok}
	then rm -f ${t}.out ${t}.err ${t}.rc
	else rc=1
	fi
done
echo
exit ${rc}
