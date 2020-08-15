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

	outname="$(echo ${outname} | tr '-' '_')"

	case "${outname}" in
		clibs)
			# clibs should not have any dependencies or anything
			# else imposed on it.
			;;
		caroot)
			pkgdeps="utilities"
			;;
		runtime)
			outname="runtime"
			uclfile="${uclfile}"
			;;
		runtime_manuals)
			outname="${origname}"
			pkgdeps="runtime"
			;;
		runtime_*)
			outname="${origname}"
			uclfile="${outname##*}${uclfile}"
			pkgdeps="runtime"
			_descr="$(make -C ${srctree}/release/packages -f Makefile.package -V ${outname}_DESCR)"
			;;
		jail_*)
			outname="${origname}"
			uclfile="${outname##*}${uclfile}"
			pkgdeps="runtime"
			_descr="$(make -C ${srctree}/release/packages -f Makefile.package -V ${outname}_DESCR)"
			;;
		*_lib32_dev)
			outname="${outname%%_lib32_dev}"
			_descr="32-bit Libraries, Development Files"
			pkgdeps="${outname}"
			;;
		*_lib32_dbg)
			outname="${outname%%_lib32_dbg}"
			_descr="32-bit Libraries, Debugging Symbols"
			pkgdeps="${outname}"
			;;
		*_lib32)
			outname="${outname%%_lib32}"
			_descr="32-bit Libraries"
			pkgdeps="${outname}"
			;;
		*_dev)
			outname="${outname%%_dev}"
			_descr="Development Files"
			pkgdeps="${outname}"
			;;
		*_dbg)
			outname="${outname%%_dbg}"
			_descr="Debugging Symbols"
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

	outname="${outname%%_*}"

	pkgdeps="$(echo ${pkgdeps} | tr '_' '-')"

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
		-e "s/%CAP_MKDB_ENDIAN%/${cap_arg}/g" \
		-e "s/%PKG_NAME_PREFIX%/${PKG_NAME_PREFIX}/" \
		-e "s|%PKG_WWW%|${PKG_WWW}|" \
		-e "s/%PKG_MAINTAINER%/${PKG_MAINTAINER}/" \
		${uclfile}
	return 0
}

main "${@}"
