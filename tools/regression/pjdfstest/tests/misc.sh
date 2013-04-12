# $FreeBSD$

ntest=1

case "${dir}" in
/*)
	maindir="${dir}/../.."
	;;
*)
	maindir="`pwd`/${dir}/../.."
	;;
esac
fstest="${maindir}/pjdfstest"
. ${maindir}/tests/conf

expect()
{
	e="${1}"
	shift
	r=`${fstest} $* 2>/dev/null | tail -1`
	echo "${r}" | ${GREP} -Eq '^'${e}'$'
	if [ $? -eq 0 ]; then
		if [ -z "${todomsg}" ]; then
			echo "ok ${ntest}"
		else
			echo "ok ${ntest} # TODO ${todomsg}"
		fi
	else
		if [ -z "${todomsg}" ]; then
			echo "not ok ${ntest} - tried '$*', expected ${e}, got ${r}"
		else
			echo "not ok ${ntest} # TODO ${todomsg}"
		fi
	fi
	todomsg=""
	ntest=$((ntest+1))
}

jexpect()
{
	s="${1}"
	d="${2}"
	e="${3}"
	shift 3
	r=`jail -s ${s} / pjdfstest 127.0.0.1 /bin/sh -c "cd ${d} && ${fstest} $* 2>/dev/null" | tail -1`
	echo "${r}" | ${GREP} -Eq '^'${e}'$'
	if [ $? -eq 0 ]; then
		if [ -z "${todomsg}" ]; then
			echo "ok ${ntest}"
		else
			echo "ok ${ntest} # TODO ${todomsg}"
		fi
	else
		if [ -z "${todomsg}" ]; then
			echo "not ok ${ntest} - tried '$*', expected ${e}, got ${r}"
		else
			echo "not ok ${ntest} # TODO ${todomsg}"
		fi
	fi
	todomsg=""
	ntest=$((ntest+1))
}

test_check()
{
	if [ $* ]; then
		if [ -z "${todomsg}" ]; then
			echo "ok ${ntest}"
		else
			echo "ok ${ntest} # TODO ${todomsg}"
		fi
	else
		if [ -z "${todomsg}" ]; then
			echo "not ok ${ntest}"
		else
			echo "not ok ${ntest} # TODO ${todomsg}"
		fi
	fi
	todomsg=""
	ntest=$((ntest+1))
}

todo()
{
	if [ "${os}" = "${1}" -o "${os}:${fs}" = "${1}" ]; then
		todomsg="${2}"
	fi
}

namegen()
{
	echo "pjdfstest_`dd if=/dev/urandom bs=1k count=1 2>/dev/null | openssl md5 | awk '{print $NF}'`"
}

namegen_len()
{
	len="${1}"

	name=""
	while :; do
		namepart="`dd if=/dev/urandom bs=64 count=1 2>/dev/null | openssl md5 | awk '{print $NF}'`"
		name="${name}${namepart}"
		curlen=`printf "%s" "${name}" | wc -c`
		[ ${curlen} -lt ${len} ] || break
	done
	name=`echo "${name}" | cut -b -${len}`
	printf "%s" "${name}"
}

# POSIX:
# {NAME_MAX}
#     Maximum number of bytes in a filename (not including terminating null).
namegen_max()
{
	name_max=`${fstest} pathconf . _PC_NAME_MAX`
	namegen_len ${name_max}
}

# POSIX:
# {PATH_MAX}
#     Maximum number of bytes in a pathname, including the terminating null character.
dirgen_max()
{
	name_max=`${fstest} pathconf . _PC_NAME_MAX`
	complen=$((name_max/2))
	path_max=`${fstest} pathconf . _PC_PATH_MAX`
	# "...including the terminating null character."
	path_max=$((path_max-1))

	name=""
	while :; do
		name="${name}`namegen_len ${complen}`/"
		curlen=`printf "%s" "${name}" | wc -c`
		[ ${curlen} -lt ${path_max} ] || break
	done
	name=`echo "${name}" | cut -b -${path_max}`
	name=`echo "${name}" | sed -E 's@/$@x@'`
	printf "%s" "${name}"
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
	lchmod)
		if [ "${os}" != "FreeBSD" ]; then
			return 1
		fi
		;;
	chflags)
		if [ "${os}" != "FreeBSD" ]; then
			return 1
		fi
		;;
	chflags_SF_SNAPSHOT)
		if [ "${os}" != "FreeBSD" -o "${fs}" != "UFS" ]; then
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

# usage:
#	create_file <type> <name>
#	create_file <type> <name> <mode>
#	create_file <type> <name> <uid> <gid>
#	create_file <type> <name> <mode> <uid> <gid>
create_file() {
	type="${1}"
	name="${2}"

	case "${type}" in
	none)
		return
		;;
	regular)
		expect 0 create ${name} 0644
		;;
	dir)
		expect 0 mkdir ${name} 0755
		;;
	fifo)
		expect 0 mkfifo ${name} 0644
		;;
	block)
		expect 0 mknod ${name} b 0644 1 2
		;;
	char)
		expect 0 mknod ${name} c 0644 1 2
		;;
	socket)
		expect 0 bind ${name}
		;;
	symlink)
		expect 0 symlink test ${name}
		;;
	esac
	if [ -n "${3}" -a -n "${4}" -a -n "${5}" ]; then
		expect 0 lchmod ${name} ${3}
		expect 0 lchown ${name} ${4} ${5}
	elif [ -n "${3}" -a -n "${4}" ]; then
		expect 0 lchown ${name} ${3} ${4}
	elif [ -n "${3}" ]; then
		expect 0 lchmod ${name} ${3}
	fi
}
