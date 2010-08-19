#!/bin/sh
#-
# Copyright (c) 2010 iXsystems, Inc.  All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
# $FreeBSD$

# Functions which runs commands on the system

. ${BACKEND}/functions.sh
. ${BACKEND}/functions-parse.sh
. ${BACKEND}/functions-ftp.sh


get_package_index()
{
	INDEX_FILE="INDEX"
	USE_BZIP2=0

	get_ftp_mirror
	FTP_SERVER="${VAL}"

	FTP_DIR="ftp://${FTP_SERVER}/pub/FreeBSD/releases/${FBSD_ARCH}/${FBSD_BRANCH}/packages"

	if [ -f "/usr/bin/bzip2" ]
	then
		INDEX_FILE="${INDEX_FILE}.bz2"
		USE_BZIP2=1
	fi

	fetch_file "${FTP_DIR}/${INDEX_FILE}" "${PKGDIR}/${INDEX_FILE}" "1"

	HERE=`pwd`
	cd "${PKGDIR}"
	if [ -f "${INDEX_FILE}" ] && [ "${USE_BZIP2}" -eq "1" ]
	then
		bzip2 -d "${INDEX_FILE}"
	fi
	cd "${HERE}"
};

parse_package_index()
{
	INDEX_FILE="${PKGDIR}/INDEX"

	exec 3<&0
	exec 0<"${INDEX_FILE}"

	while read -r line
	do
		PKGNAME=""
		CATEGORY=""
		PACKAGE=""
		DESC=""
		DEPS=""
		i=0

		SAVE_IFS="${IFS}"
		IFS="|"

		for part in ${line}
		do
			if [ "${i}" -eq "0" ]
			then
				PKGNAME="${part}"

			elif [ "${i}" -eq "1" ]
			then
				PACKAGE=`basename "${part}"`

			elif [ "${i}" -eq "3" ]
			then
				DESC="${part}"

			elif [ "${i}" -eq "6" ]
			then
				CATEGORY=`echo "${part}" | cut -f1 -d' '`

			elif [ "${i}" -eq "8" ]
			then
				DEPS="${part}"
			fi

			i=$((i+1))
		done

		echo "${CATEGORY}|${PACKAGE}|${DESC}" >> "${INDEX_FILE}.parsed"
		echo "${PACKAGE}|${PKGNAME}|${DEPS}" >> "${INDEX_FILE}.deps"

		IFS="${SAVE_IFS}"
	done

	exec 0<&3
};

show_package_file()
{
	PKGFILE="${1}"

	exec 3<&0
	exec 0<"${PKGFILE}"

	while read -r line
	do
		CATEGORY=`echo "${line}" | cut -f1 -d'|'`
		PACKAGE=`echo "${line}" | cut -f2 -d'|'`
		DESC=`echo "${line}" | cut -f3 -d'|'`

		echo "${CATEGORY}/${PACKAGE}:${DESC}"
	done

	exec 0<&3
};

show_packages_by_category()
{
	CATEGORY="${1}"
	INDEX_FILE="${PKGDIR}/INDEX.parsed"
	TMPFILE="/tmp/.pkg.cat"

	grep "^${CATEGORY}|" "${INDEX_FILE}" > "${TMPFILE}"
	show_package_file "${TMPFILE}"
	rm "${TMPFILE}"
};

show_package_by_name()
{
	CATEGORY="${1}"
	PACKAGE="${2}"
	INDEX_FILE="${PKGDIR}/INDEX.parsed"
	TMPFILE="/tmp/.pkg.cat.pak"

	grep "^${CATEGORY}|${PACKAGE}" "${INDEX_FILE}" > "${TMPFILE}"
	show_package_file "${TMPFILE}"
	rm "${TMPFILE}"
};

show_packages()
{
	show_package_file "${PKGDIR}/INDEX.parsed"
};

get_package_dependencies()
{
	PACKAGE="${1}"
	LONG="${2:-0}"
	RES=0

	INDEX_FILE="${PKGDIR}/INDEX.deps"
	REGEX="^${PACKAGE}|"

	if [ "${LONG}" -ne "0" ]
	then
		REGEX="^.*|${PACKAGE}|"
	fi

	LINE=`grep "${REGEX}" "${INDEX_FILE}" 2>/dev/null`
	DEPS=`echo "${LINE}"|cut -f3 -d'|'`

	VAL="${DEPS}"
	export VAL

	if [ -z "${VAL}" ]
	then
		RES=1
	fi

	return ${RES}
};

get_package_name()
{
	PACKAGE="${1}"
	RES=0

	INDEX_FILE="${PKGDIR}/INDEX.deps"
	REGEX="^${PACKAGE}|"
	
	LINE=`grep "${REGEX}" "${INDEX_FILE}" 2>/dev/null`
	NAME=`echo "${LINE}"|cut -f2 -d'|'`

	VAL="${NAME}"
	export VAL

	if [ -z "${VAL}" ]
	then
		RES=1
	fi

	return ${RES}
};

get_package_short_name()
{
	PACKAGE="${1}"
	RES=0

	INDEX_FILE="${PKGDIR}/INDEX.deps"
	REGEX="^.*|${PACKAGE}|"
	
	LINE=`grep "${REGEX}" "${INDEX_FILE}" 2>/dev/null`
	NAME=`echo "${LINE}"|cut -f1 -d'|'`

	VAL="${NAME}"
	export VAL

	if [ -z "${VAL}" ]
	then
		RES=1
	fi

	return ${RES}
};

get_package_category()
{
	PACKAGE="${1}"
	INDEX_FILE="${PKGDIR}/INDEX.parsed"
	RES=0

	LINE=`grep "|${PACKAGE}|" "${INDEX_FILE}" 2>/dev/null`
	NAME=`echo "${LINE}"|cut -f1 -d'|'`

	VAL="${NAME}"
	export VAL

	if [ -z "${VAL}" ]
	then
		RES=1
	fi

	return ${RES}
};

fetch_package()
{
	CATEGORY="${1}"
	PACKAGE="${2}"

	get_ftp_mirror
	FTP_SERVER="${VAL}"

	PACKAGE="${PACKAGE}.tbz"
	if [ ! -f "${PKGTMPDIR}/${PACKAGE}" ]
	then
		PKGPATH="${CATEGORY}/${PACKAGE}"
		FTP_PATH="ftp://${FTP_SERVER}/pub/FreeBSD/releases/${FBSD_ARCH}/${FBSD_BRANCH}/packages/${PKGPATH}"
		fetch_file "${FTP_PATH}" "${PKGTMPDIR}/" "0"
	fi
};
