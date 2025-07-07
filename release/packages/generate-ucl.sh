#!/bin/sh
#
#

mancx=" (manual pages)"
mandx="This package contains the online manual pages."

lib32mancx=" (32-bit manual pages)"
lib32mandx="This package contains the online manual pages for 32-bit components
on a 64-bit host."

lib32cx=" (32-bit libraries)"
lib32dx="This package contains 32-bit libraries for running 32-bit applications on
a 64-bit host."

devcx=" (development files)"
devdx="This package contains development files for compiling applications."

dev32cx=" (32-bit development files)"
dev32dx="This package contains development files for compiling 32-bit applications
on a 64-bit host."

dbgcx=" (debugging symbols)"
dbgdx="This package contains external debugging symbols for use with a source-level
debugger."

dbg32cx=" (32-bit debugging symbols)"
dbg32dx="This package contains 32-bit external debugging symbols for use with a
source-level debugger."

main() {
	outname=""
	origname=""
	desc_suffix=""
	comment_suffix=""
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
			comment_suffix="$devcx"
			desc_suffix="$devdx"
			;;
		*-dbg)
			outname="${outname%%-dbg}"
			comment_suffix="$dbgcx"
			desc_suffix="$dbgdx"
			;;
		*-dev-lib32)
			outname="${outname%%-dev-lib32}"
			comment_suffix="$dev32cx"
			desc_suffix="$dev32dx"
			;;
		*-dbg-lib32)
			outname="${outname%%-dbg-lib32}"
			comment_suffix="$dbg32cx"
			desc_suffix="$dbg32dx"
			;;
		*-man-lib32)
			outname="${outname%%-man-lib32}"
			comment_suffix="$lib32mancx"
			desc_suffix="$lib32mandx"
			;;
		*-lib32)
			outname="${outname%%-lib32}"
			comment_suffix="$lib32cx"
			desc_suffix="$lib32dx"
			;;
		*-man)
			outname="${outname%%-man}"
			comment_suffix="$mancx"
			desc_suffix="$mandx"
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
		echo "desc_suffix=${desc_suffix}"
		echo "comment_suffix=${comment_suffix}"
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
		COMMENT_SUFFIX "${comment_suffix}" \
		DESC_SUFFIX "$desc_suffix" \
		CAP_MKDB_ENDIAN "${cap_arg}" \
		PKG_WWW "${PKG_WWW}" \
		PKG_MAINTAINER "${PKG_MAINTAINER}" \
		UCLFILES "${srctree}/release/packages/ucl" \
		${uclsource} ${uclfile}

	return 0
}

main "${@}"
