#!/bin/sh
#
# $FreeBSD$
#

# Script to handle packaging cloud images for GCE.
#
# XXX:
# This script only exists to help in automating image creation,
# and reimplementing this is intended (in other words, this is
# temporary).

usage() {
	echo "Usage:"
	echo "$(basename ${0}) -D <destdir> -I <input_file> -S <src_tree>"
	exit 1
}

main() {
	while getopts "D:I:W:" opt; do
		case ${opt} in
			D)
				DESTDIR="${OPTARG}"
				;;
			I)
				INFILE="${OPTARG}"
				;;
			S)
				WORLDDIR="${OPTARG}"
				;;
			*)
				usage
				;;
		esac
	done
	shift $(( ${OPTIND} - 1 ))

	if [ -z "${DESTDIR}" -o -z "${INFILE}" -o -z "${WORLDDIR}" ]; then
		usage
	fi

	OUTFILE="$(make -C ${WORLDDIR}/release -V OSRELEASE).tar.gz"

	cd ${DESTDIR} && tar --format=gnutar -zcf ${OUTFILE} ${INFILE}
}

main "$@"
