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

# Functions which check and load any optional packages specified in the config

. ${BACKEND}/functions.sh
. ${BACKEND}/functions-parse.sh

# Recursively determine all dependencies for this package
determine_package_dependencies()
{
  local PKGNAME="${1}"
  local DEPFILE="${2}"

  grep "${PKGNAME}" "${DEPFILE}" >/dev/null
  if [ "$?" -ne "0" ]
  then
    echo "${PKGNAME}" >> "${DEPFILE}"
    get_package_dependencies "${PKGNAME}" "1"

    local DEPS="${VAL}"
    for d in ${DEPS}
    do
      determine_package_dependencies "${d}" "${DEPFILE}"
    done
  fi
};

# Fetch packages dependencies from a file
fetch_package_dependencies()
{
  local DEPFILE
  local DEPS
  local SAVEDIR

  DEPFILE="${1}"
  DEPS=`cat "${DEPFILE}"`
  SAVEDIR="${2}"

  for d in ${DEPS}
  do
    get_package_short_name "${d}"
    SNAME="${VAL}"

    get_package_category "${SNAME}"
    CATEGORY="${VAL}"

    fetch_package "${CATEGORY}" "${d}" "${SAVEDIR}"
  done
};

# Check for any packages specified, and begin loading them
install_packages()
{
  # First, lets check and see if we even have any packages to install
  get_value_from_cfg installPackages
  if [ ! -z "${VAL}" ]
  then
    HERE=`pwd`
    rc_nohalt "mkdir -p ${FSMNT}/${PKGTMPDIR}"
    rc_nohalt "cd ${FSMNT}/${PKGTMPDIR}"

    if [ ! -f "${CONFDIR}/INDEX" ]
    then
      get_package_index
    fi

    if [ ! -f "${CONFDIR}/INDEX.parsed" ]
    then
      parse_package_index
    fi

    # Lets start by cleaning up the string and getting it ready to parse
    strip_white_space ${VAL}
    PACKAGES=`echo ${VAL} | sed -e "s|,| |g"`
    for i in $PACKAGES
    do
      if get_package_name "${i}"
      then
        PKGNAME="${VAL}"
        DEPFILE="${FSMNT}/${PKGTMPDIR}/.${PKGNAME}.deps"

        rc_nohalt "touch ${DEPFILE}"
        determine_package_dependencies "${PKGNAME}" "${DEPFILE}"
        fetch_package_dependencies "${DEPFILE}" "${FSMNT}/${PKGTMPDIR}"

        # If the package is not already installed, install it!
        if ! run_chroot_cmd "pkg_info -e ${PKGNAME}"
        then
          rc_nohalt "pkg_add -C ${FSMNT} ${PKGTMPDIR}/${PKGNAME}.tbz"
        fi

        rc_nohalt "rm ${DEPFILE}"
      fi

      rc_nohalt "cd ${HERE}"
    done

  rm -rf "${FSMNT}/${PKGTMPDIR}"
  fi
};
