#!/bin/sh
#
#

main() {
	outname=""
	origname=""
	debug=
	uclsource=
	while getopts "do:s:u:" arg; do
		case ${arg} in
		d)
			debug=1
			;;
		o)
			outname="${OPTARG}"
			origname="${OPTARG}"
			;;
		s)
			srctree="${OPTARG}"
			;;
		u)
			uclfile="${OPTARG}"
			;;
		*)
			echo "Unknown argument"
			;;
		esac
	done

	shift $(( ${OPTIND} - 1 ))

	case "${outname}" in
		*-dev)
			outname="${outname%%-dev}"
			;;
		*-dbg)
			outname="${outname%%-dbg}"
			;;
		*-dev-lib32)
			outname="${outname%%-dev-lib32}"
			;;
		*-dbg-lib32)
			outname="${outname%%-dbg-lib32}"
			;;
		*-man-lib32)
			outname="${outname%%-man-lib32}"
			;;
		*-lib32)
			outname="${outname%%-lib32}"
			;;
		*-lib)
			outname="${outname%%-lib}"
			;;
		*-man)
			outname="${outname%%-man}"
			;;
		${origname})
			;;
		*)
			uclfile="${outname##*}${origname}"
			outname="${outname##*}${origname}"
			;;
	esac

	uclsource="${srctree}/release/packages/template.ucl"

	if [ -n "${debug}" ]; then
		echo ""
		echo "==============================================================="
		echo "DEBUG:"
		echo "outname=${outname}"
		echo "origname=${origname}"
		echo "srctree=${srctree}"
		echo "uclfile=${uclfile}"
		echo "vital=${vital}"
		echo "cp ${uclsource} -> ${uclfile}"
		echo "==============================================================="
		echo ""
		echo ""
		echo ""
	fi

	cap_arg="$( make -f ${srctree}/share/mk/bsd.endian.mk -VCAP_MKDB_ENDIAN )"
	${srctree}/release/packages/generate-ucl.lua \
		VERSION "${PKG_VERSION}" \
		PKGNAME "${origname}" \
		PKGGENNAME "${outname}" \
		PKG_NAME_PREFIX "${PKG_NAME_PREFIX}" \
		CAP_MKDB_ENDIAN "${cap_arg}" \
		PKG_WWW "${PKG_WWW}" \
		PKG_MAINTAINER "${PKG_MAINTAINER}" \
		UCLFILES "${srctree}/release/packages/ucl" \
		${uclsource} ${uclfile}
}

main "${@}"
