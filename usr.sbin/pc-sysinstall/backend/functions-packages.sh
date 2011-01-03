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


get_package_index_by_ftp()
{
  local INDEX_FILE
  local FTP_SERVER

  FTP_SERVER="${1}"
  INDEX_FILE="INDEX"
  USE_BZIP2=0

  if [ -f "/usr/bin/bzip2" ]
  then
    INDEX_FILE="${INDEX_FILE}.bz2"
    USE_BZIP2=1
  fi

  INDEX_PATH="${CONFDIR}/${INDEX_FILE}"
  fetch_file "${FTP_SERVER}/${INDEX_FILE}" "${INDEX_PATH}" "1"
  if [ -f "${INDEX_PATH}" ] && [ "${USE_BZIP2}" -eq "1" ]
  then
    bzip2 -d "${INDEX_PATH}"
  fi
};

get_package_index_by_fs()
{
  local INDEX_FILE

  INDEX_FILE="${CDMNT}/packages/INDEX"
  fetch_file "${INDEX_FILE}" "${CONFDIR}/" "0"
};

get_package_index_size()
{
  if [ -f "${CONFDIR}/INDEX" ]
  then
    SIZE=`ls -l ${CONFDIR}/INDEX | awk '{ print $5 }'`
  else
    get_ftp_mirror
    FTPHOST="${VAL}"

    FTPDIR="/pub/FreeBSD/releases/${FBSD_ARCH}/${FBSD_BRANCH}"
    FTPPATH="ftp://${FTPHOST}${FTPDIR}/packages"

    fetch -s "${FTPPATH}/INDEX.bz2"
  fi
};

get_package_index()
{
  RES=0

  if [ -z "${INSTALLMODE}" ]
  then
    get_ftp_mirror
    FTPHOST="${VAL}"

    FTPDIR="/pub/FreeBSD/releases/${FBSD_ARCH}/${FBSD_BRANCH}"
    FTPPATH="ftp://${FTPHOST}${FTPDIR}/packages"

    get_package_index_by_ftp "${FTPPATH}"

  else
    get_value_from_cfg ftpHost
    if [ -z "$VAL" ]
    then
      exit_err "ERROR: Install medium was set to ftp, but no ftpHost was provided!" 
    fi

    FTPHOST="${VAL}"

    get_value_from_cfg ftpDir
    if [ -z "$VAL" ]
    then
      exit_err "ERROR: Install medium was set to ftp, but no ftpDir was provided!" 
    fi

    FTPDIR="${VAL}"
    FTPPATH="ftp://${FTPHOST}${FTPDIR}"

    case "${INSTALLMEDIUM}" in
      usb|dvd) get_package_index_by_fs ;;
      ftp) get_package_index_by_ftp "${FTPPATH}" ;;
      sftp) ;;
      *) RES=1 ;;
    esac

  fi

  return ${RES}
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

  echo "Available Packages:"

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

fetch_package_by_ftp()
{
  CATEGORY="${1}"
  PACKAGE="${2}"
  SAVEDIR="${3}"

  get_value_from_cfg ftpHost
  if [ -z "$VAL" ]
  then
    exit_err "ERROR: Install medium was set to ftp, but no ftpHost was provided!" 
  fi
  FTPHOST="${VAL}"

  get_value_from_cfg ftpDir
  if [ -z "$VAL" ]
  then
    exit_err "ERROR: Install medium was set to ftp, but no ftpDir was provided!" 
  fi
  FTPDIR="${VAL}"

  PACKAGE="${PACKAGE}.tbz"
  FTP_SERVER="ftp://${FTPHOST}${FTPDIR}"

  if [ ! -f "${SAVEDIR}/${PACKAGE}" ]
  then
    PKGPATH="${CATEGORY}/${PACKAGE}"
    FTP_PATH="${FTP_HOST}/packages/${PKGPATH}"
    fetch_file "${FTP_PATH}" "${SAVEDIR}/" "0"
  fi
};

fetch_package_by_fs()
{
  CATEGORY="${1}"
  PACKAGE="${2}"
  SAVEDIR="${3}"

  PACKAGE="${PACKAGE}.tbz"
  if [ ! -f "${SAVEDIR}/${PACKAGE}" ]
  then
    fetch_file "${CDMNT}/packages/${CATEGORY}/${PACKAGE}" "${SAVEDIR}/" "0"
  fi
};

fetch_package()
{
  CATEGORY="${1}"
  PACKAGE="${2}"
  SAVEDIR="${3}"

  case "${INSTALLMEDIUM}" in
    usb|dvd) fetch_package_by_fs "${CATEGORY}" "${PACKAGE}" "${SAVEDIR}" ;;
    ftp) fetch_package_by_ftp "${CATEGORY}" "${PACKAGE}" "${SAVEDIR}" ;;
    sftp) ;;
  esac
};
