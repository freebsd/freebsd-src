#!/bin/sh

boot_DEV=cfid0
boot_OFFSET=0x01F00000
boot_MAXLEN=0x100000

fpga_DEV=cfid0
fpga_OFFSET=0x00020000
fpga_MAXLEN=0x00C00000
fpga_SKIP=0x20000

fpga2_DEV=cfid0
fpga2_OFFSET=0x00C20000
fpga2_MAXLEN=0x00C00000
fpga2_SKIP=0x20000

osconfig_DEV=cfid0
osconfig_OFFSET=0x01820000
osconfig_MAXLEN=0x00020000

kernel_DEV=cfid0
kernel_OFFSET=0x02000000
kernel_MAXLEN=0x02000000

TARGETS="boot:fpga:fpga2:osconfig:kernel"

prog=`basename "$0"`

usage()
{
	echo "usage: $prog [-n] <target> file" 1>&2
	exit 1
}

clean_exit()
{
	if [ -n "${tmpdir}" ]; then
		rm -rf "${tmpdir}"
	fi
}

err()
{
	ret=$1
	shift
	echo "$prog: $@" 1>&2
	exit $ret
}

warn()
{
	echo "$prog: $@" 1>&2
}

DRYRUN=0
while getopts "n" opt; do
	case "$opt" in
	n)
		DRYRUN=1
		;;
	*)
		warn "invalid option '$opt'"
		usage
		;;
	esac
done
shift $((OPTIND - 1))

if [ $# -ne 2 ]; then
	usage
fi

target=$1
source=$2

case ":${TARGETS}:" in
*:${target}:*)
	eval DEV="\${${target}_DEV}"
	eval OFFSET="\${${target}_OFFSET}"
	eval MAXLEN="\${${target}_MAXLEN}"
	eval SKIP="\${${target}_SKIP}"
	if [ -z "${SKIP}" ]; then
		SKIP=0
	fi
	if [ -z "${DEV}" -o -z "${OFFSET}" -o -z "${MAXLEN}" ]; then
		err 1 "Internal error: invalid target config"
	fi
	;;
*)
	warn "invalid target"
	usage
	;;
esac

if [ ! -r "${source}" ]; then
	err 1 "Source not found '${source}'"
fi
if [ ! -r "${source}.md5" ]; then
	err 1 "No source MD5 found '${source}'"
fi
echo "Validating ${source}"
expected_md5=`cat "${source}.md5"`
expected_md5="${expected_md5#*=}"
if [ -z "${expected_md5}" ]; then
	err 1 "invalid md5 file '${source}.md5'"
fi
if ! md5 -c ${expected_md5} "${source}" > /dev/null; then
	err 1 "MD5 check failed"
fi

source=`realpath "${source}"`
src_name=`basename "${source}"`

trap clean_exit EXIT INT
tmpdir=`mktemp -d -t "$prog"`

case "${source}" in
*.bz2)
	if [ -z "${tmpdir}" ]; then
		err 1 "Can't make a temporary directory, is /tmp writable?"
	fi
	binfile="${tmpdir}/${src_name%.bz2}"
	echo "Extracting to ${binfile}"
	bunzip2 -c "${source}" > "${binfile}"
	;;
*.gz)
	if [ -z "${tmpdir}" ]; then
		err 1 "Can't make a temporary directory, is /tmp writable?"
	fi
	binfile="${tmpdir}/${src_name%.gz}"
	echo "Extracting to ${binfile}"
	gunzip -c "${source}" > "${binfile}"
	;;
*)
	binfile="${source}"
	;;
esac
size=`BLOCKSIZE=512 ls -s "${binfile}"`
size=$((${size%% *} * 512))
maxlen=`printf "%d" ${MAXLEN}`
if [ ${size} -gt ${maxlen} ]; then
	err 1 "Source too large for target (${size} > ${MAXLEN})"
fi

if [ ${DRYRUN} -eq 0 ]; then
	if [ ! -e "/dev/${DEV}" ]; then
		err 1 "Flash device not found ${DEV}"
	fi
fi

echo "Writing ${binfile} to ${DEV} @ ${OFFSET}"
if [ ${DRYRUN} -eq 0 ]; then
	iseek=$((`printf "%d" ${SKIP}` / 512))
	oseek=$((`printf "%d" ${OFFSET}` / 512))

	echo dd if="${binfile}" of="/dev/${DEV}" \
	    iseek=${iseek} oseek=${oseek} conv=osync
	dd if="${binfile}" of="/dev/${DEV}" \
	    iseek=${iseek} oseek=${oseek} conv=osync
	if [ $? != 0 ]; then
		err 1 "Write failed"
	fi
fi

