#!/bin/sh
#
#

main() {
	desc=
	comment=
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

		# -dev packages that have no corresponding non-dev package
		# as a dependency.
		libcompat-dev|libcompiler_rt-dev|liby-dev)
			outname=${outname%%-dev}
			_descr="Development Files"
			;;
		libcompat-lib32_dev|libcompiler_rt-lib32_dev|liby-lib32_dev)
			outname=${outname%%-lib32_dev}
			_descr="32-bit Libraries, Development Files"
			;;
		libcompat-man|libelftc-man)
			outname=${outname%%-man}
			_descr="Manual Pages"
			;;
		utilities)
			uclfile="${uclfile}"
			;;
		runtime)
			outname="runtime"
			_descr="$(make -C ${srctree}/release/packages -f Makefile.package -V ${outname}_DESCR)"
			;;
		*-lib32_dev)
			outname="${outname%%-lib32_dev}"
			_descr="32-bit Libraries, Development Files"
			pkgdeps="${outname}"
			;;
		*-lib32_dbg)
			outname="${outname%%-lib32_dbg}"
			_descr="32-bit Libraries, Debugging Symbols"
			pkgdeps="${outname}"
			;;
		*-lib32)
			outname="${outname%%-lib32}"
			_descr="32-bit Libraries"
			pkgdeps="${outname}"
			;;
		*-dev)
			outname="${outname%%-dev}"
			_descr="Development Files"
			pkgdeps="${outname}"
			;;
		*-dbg)
			outname="${outname%%-dbg}"
			_descr="Debugging Symbols"
			pkgdeps="${outname}"
			;;
		*-man)
			outname="${outname%%-man}"
			_descr="Manual Pages"
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

	if [ ! -z "${debug}" ]; then
		echo ""
		echo "==============================================================="
		echo "DEBUG:"
		echo "_descr=${_descr}"
		echo "outname=${outname}"
		echo "origname=${origname}"
		echo "srctree=${srctree}"
		echo "uclfile=${uclfile}"
		echo "desc=${desc}"
		echo "comment=${comment}"
		echo "vital=${vital}"
		echo "cp ${uclsource} -> ${uclfile}"
		echo "==============================================================="
		echo ""
		echo ""
		echo ""
	fi

	[ -z "${comment}" ] && comment="${outname} package"
	[ ! -z "${_descr}" ] && comment="${comment} (${_descr})"
	[ -z "${desc}" ] && desc="${outname} package"

	cp "${uclsource}" "${uclfile}"
	if [ ! -z "${pkgdeps}" ]; then
		echo 'deps: {' >> ${uclfile}
		for dep in ${pkgdeps}; do
			cat <<EOF >> ${uclfile}
	FreeBSD-${dep}: {
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
		DESC "${desc}" \
		CAP_MKDB_ENDIAN "${cap_arg}" \
		PKG_WWW "${PKG_WWW}" \
		PKG_MAINTAINER "${PKG_MAINTAINER}" \
		UCLFILES "${srctree}/release/packages/" \
		${uclsource} ${uclfile}

	return 0
}

main "${@}"
