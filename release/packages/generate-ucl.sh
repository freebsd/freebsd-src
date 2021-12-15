#!/bin/sh
#
# $FreeBSD$
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

	vital="false"

	case "${outname}" in
		clibs)
			vital="true"
			# clibs should not have any dependencies or anything
			# else imposed on it.
			;;
		caroot)
			pkgdeps="openssl"
			;;
		utilities)
			uclfile="${uclfile}"
			;;
		runtime)
			outname="runtime"
			_descr="$(make -C ${srctree}/release/packages -f Makefile.package -V ${outname}_DESCR)"
			vital="true"
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
			pkgdeps="runtime"
			;;
		*)
			uclfile="${outname##*}${origname}"
			outname="${outname##*}${origname}"
			;;
	esac

	desc="$(make -C ${srctree}/release/packages -f Makefile.package -V ${outname}_DESC)"
	comment="$(make -C ${srctree}/release/packages -f Makefile.package -V ${outname}_COMMENT)"

	uclsource="${srctree}/release/packages/${outname}.ucl"
	if [ ! -e "${uclsource}" ]; then
		uclsource="${srctree}/release/packages/template.ucl"
	fi

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
		cat <<EOF >> ${uclfile}
deps: {
	FreeBSD-${pkgdeps}: {
		origin: "base",
		version: "${PKG_VERSION}"
	}
}
EOF
	fi
	cap_arg="$( make -f ${srctree}/share/mk/bsd.endian.mk -VCAP_MKDB_ENDIAN )"
	sed -i '' -e "s/%VERSION%/${PKG_VERSION}/" \
		-e "s/%PKGNAME%/${origname}/" \
		-e "s/%COMMENT%/${comment}/" \
		-e "s/%DESC%/${desc}/" \
		-e "s/%VITAL%/${vital}/" \
		-e "s/%CAP_MKDB_ENDIAN%/${cap_arg}/g" \
		-e "s/%PKG_NAME_PREFIX%/${PKG_NAME_PREFIX}/" \
		-e "s|%PKG_WWW%|${PKG_WWW}|" \
		-e "s/%PKG_MAINTAINER%/${PKG_MAINTAINER}/" \
		${uclfile}
	return 0
}

main "${@}"
