#!/bin/sh
#
#

mancx=" (manual pages)"
mandx="This package contains the online manual pages."

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
	desc=
	desc_suffix=""
	comment=
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
		bootloader)
			pkgdeps=""
			;;
		certctl)
			pkgdeps="caroot openssl"
			;;
		clang)
			pkgdeps="lld libcompiler_rt-dev"
			;;
		periodic)
			pkgdeps="cron"
			;;
		rcmds)
			# the RPC daemons require rpcbind
			pkgdeps="utilities"
			;;

		# -dev packages that have no corresponding non-dev package
		# as a dependency.
		libcompat-dev|libcompiler_rt-dev|liby-dev)
			outname=${outname%%-dev}
			comment_suffix="$devcx"
			desc_suffix="$devdx"
			;;
		libcompat-dev-lib32|libcompiler_rt-dev-lib32|liby-dev-lib32)
			outname=${outname%%-dev-lib32}
			comment_suffix="$dev32cx"
			desc_suffix="$dev32dx"
			;;
		libcompat-man|libelftc-man)
			outname=${outname%%-man}
			comment_suffix="$mancx"
			desc_suffix="$mandx"
			;;
		*-dev)
			outname="${outname%%-dev}"
			comment_suffix="$devcx"
			desc_suffix="$devdx"
			pkgdeps="${outname}"
			;;
		*-dbg)
			outname="${outname%%-dbg}"
			comment_suffix="$dbgcx"
			desc_suffix="$dbgdx"
			pkgdeps="${outname}"
			;;
		*-dev-lib32)
			outname="${outname%%-dev-lib32}"
			comment_suffix="$dev32cx"
			desc_suffix="$dev32dx"
			pkgdeps="${outname}"
			;;
		*-dbg-lib32)
			outname="${outname%%-dbg-lib32}"
			comment_suffix="$dbg32cx"
			desc_suffix="$dbg32dx"
			pkgdeps="${outname}"
			;;
		*-lib32)
			outname="${outname%%-lib32}"
			comment_suffix="$lib32cx"
			desc_suffix="$lib32dx"
			pkgdeps="${outname}"
			;;
		*-man)
			outname="${outname%%-man}"
			comment_suffix="$mancx"
			desc_suffix="$mandx"
			pkgdeps="${outname}"
			;;
		${origname})
			;;
		*)
			uclfile="${outname##*}${origname}"
			outname="${outname##*}${origname}"
			;;
	esac

	desc="$(make -C ${srctree}/release/packages -f Makefile.package -V ${outname}_DESC)"
	comment="$(make -C ${srctree}/release/packages -f Makefile.package -V ${outname}_COMMENT)"

	uclsource="${srctree}/release/packages/template.ucl"

	if [ -n "${debug}" ]; then
		echo ""
		echo "==============================================================="
		echo "DEBUG:"
		echo "outname=${outname}"
		echo "origname=${origname}"
		echo "srctree=${srctree}"
		echo "uclfile=${uclfile}"
		echo "desc=${desc}"
		echo "desc_suffix=${desc_suffix}"
		echo "comment=${comment}"
		echo "comment_suffix=${comment_suffix}"
		echo "vital=${vital}"
		echo "cp ${uclsource} -> ${uclfile}"
		echo "==============================================================="
		echo ""
		echo ""
		echo ""
	fi

	[ -z "${comment}" ] && comment="${outname} package"
	[ -z "${desc}" ] && desc="${outname} package"

	cp "${uclsource}" "${uclfile}"
	if [ -n "${pkgdeps}" ]; then
		echo 'deps: {' >> ${uclfile}
		for dep in ${pkgdeps}; do
			cat <<EOF >> ${uclfile}
	${PKG_NAME_PREFIX}-${dep}: {
		origin: "base",
		version: "${PKG_VERSION}"
	}
EOF
		done
		echo '}' >> ${uclfile}
	fi
	cap_arg="$( make -f ${srctree}/share/mk/bsd.endian.mk -VCAP_MKDB_ENDIAN )"
	${srctree}/release/packages/generate-ucl.lua \
		VERSION "${PKG_VERSION}" \
		PKGNAME "${origname}" \
		PKGGENNAME "${outname}" \
		PKG_NAME_PREFIX "${PKG_NAME_PREFIX}" \
		COMMENT "${comment}" \
		COMMENT_SUFFIX "${comment_suffix}" \
		DESC "${desc}" \
		DESC_SUFFIX "$desc_suffix" \
		CAP_MKDB_ENDIAN "${cap_arg}" \
		PKG_WWW "${PKG_WWW}" \
		PKG_MAINTAINER "${PKG_MAINTAINER}" \
		UCLFILES "${srctree}/release/packages/ucl" \
		${uclfile} ${uclfile}

	return 0
}

main "${@}"
