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

# functions.sh
# Library of functions which pc-sysinstall may call upon

# Function which displays the help-index file
display_help()
{
  if [ -e "${PROGDIR}/doc/help-index" ]
  then
    cat ${PROGDIR}/doc/help-index
  else
    echo "Error: ${PROGDIR}/doc/help-index not found"
    exit 1
  fi
};

# Function which displays the help for a specified command
display_command_help()
{
  if [ -z "$1" ]
  then
    echo "Error: No command specified to display help for"
    exit 1
  fi
  
  if [ -e "${PROGDIR}/doc/help-${1}" ]
  then
    cat ${PROGDIR}/doc/help-${1}
  else
    echo "Error: ${PROGDIR}/doc/help-${1} not found"
    exit 1
  fi
};

# Function to convert bytes to megabytes
convert_byte_to_megabyte()
{
  if [ -z "${1}" ]
  then
    echo "Error: No bytes specified!"
    exit 1
  fi

  expr -e ${1} / 1048576
};

# Function to convert blocks to megabytes
convert_blocks_to_megabyte()
{
  if [ -z "${1}" ] ; then
    echo "Error: No blocks specified!"
    exit 1
  fi

  expr -e ${1} / 2048
};

# Takes $1 and strips the whitespace out of it, returns VAL
strip_white_space()
{
  if [ -z "${1}" ]
  then
    echo "Error: No value setup to strip whitespace from!"

    exit 1
  fi

  VAL=`echo "$1" | tr -d ' '`
  export VAL
};

# Displays an error message and exits with error 1
exit_err()
{
   # Echo the message for the users benefit
   echo "$1"

   # Save this error to the log file
   echo "${1}" >>$LOGOUT

   # Check if we need to unmount any file-systems after this failure
   unmount_all_filesystems_failure

   echo "For more details see log file: $LOGOUT"

   exit 1
};

# Run-command, don't halt if command exits with non-0
rc_nohalt()
{
  CMD="$1"

  if [ -z "${CMD}" ]
  then
    exit_err "Error: missing argument in rc_nohalt()"
  fi

  echo "Running: ${CMD}" >>${LOGOUT}
  ${CMD} >>${LOGOUT} 2>>${LOGOUT}

};

# Run-command, halt if command exits with non-0
rc_halt()
{
  CMD="$1"

  if [ -z "${CMD}" ]
  then
    exit_err "Error: missing argument in rc_halt()"
  fi

  echo "Running: ${CMD}" >>${LOGOUT}
  ${CMD} >>${LOGOUT} 2>>${LOGOUT}
  STATUS="$?"
  if [ "${STATUS}" != "0" ]
  then
    exit_err "Error ${STATUS}: ${CMD}"
  fi
};

# Run-command w/echo to screen, halt if command exits with non-0
rc_halt_echo()
{
  CMD="$1"

  if [ -z "${CMD}" ]
  then
    exit_err "Error: missing argument in rc_halt_echo()"
  fi

  echo "Running: ${CMD}" >>${LOGOUT}
  ${CMD} 2>&1 | tee -a ${LOGOUT} 
  STATUS="$?"
  if [ "$STATUS" != "0" ]
  then
    exit_err "Error ${STATUS}: $CMD"
  fi

};

# Run-command w/echo, don't halt if command exits with non-0
rc_nohalt_echo()
{
  CMD="$1"

  if [ -z "${CMD}" ]
  then
    exit_err "Error: missing argument in rc_nohalt_echo()"
  fi

  echo "Running: ${CMD}" >>${LOGOUT}
  ${CMD} 2>&1 | tee -a ${LOGOUT} 

};

# Echo to the screen and to the log
echo_log()
{
  STR="$1"

  if [ -z "${STR}" ]
  then
    exit_err "Error: missing argument in echo_log()"
  fi

  echo "${STR}" | tee -a ${LOGOUT} 
};

# Make sure we have a numeric
is_num() {
        expr $1 + 1 2>/dev/null
        return $?
}

# Function which uses "fetch" to download a file, and display a progress report
fetch_file()
{

FETCHFILE="$1"
FETCHOUTFILE="$2"
EXITFAILED="$3"

SIZEFILE="${TMPDIR}/.fetchSize"
EXITFILE="${TMPDIR}/.fetchExit"

rm ${SIZEFILE} 2>/dev/null >/dev/null
rm ${FETCHOUTFILE} 2>/dev/null >/dev/null

fetch -s "${FETCHFILE}" >${SIZEFILE}
SIZE="`cat ${SIZEFILE}`"
SIZE="`expr ${SIZE} / 1024`"
echo "FETCH: ${FETCHFILE}"
echo "FETCH: ${FETCHOUTFILE}" >>${LOGOUT}

( fetch -o ${FETCHOUTFILE} "${FETCHFILE}" >/dev/null 2>/dev/null ; echo "$?" > ${EXITFILE} ) &
PID="$!"
while
z=1
do

  if [ -e "${FETCHOUTFILE}" ]
  then
    DSIZE=`du -k ${FETCHOUTFILE} | tr -d '\t' | cut -d '/' -f 1`
    if [ $(is_num "$DSIZE") ] ; then
	if [ $SIZE -lt $DSIZE ] ; then DSIZE="$SIZE"; fi 
    	echo "SIZE: ${SIZE} DOWNLOADED: ${DSIZE}"
    	echo "SIZE: ${SIZE} DOWNLOADED: ${DSIZE}" >>${LOGOUT}
    fi
  fi

  # Check if the download is finished
  ps -p ${PID} >/dev/null 2>/dev/null
  if [ "$?" != "0" ]
  then
   break;
  fi

  sleep 2
done

echo "FETCHDONE"

EXIT="`cat ${EXITFILE}`"
if [ "${EXIT}" != "0" -a "$EXITFAILED" = "1" ]
then
  exit_err "Error: Failed to download ${FETCHFILE}"
fi

return $EXIT

};

# Function to return a the zpool name for this device
get_zpool_name()
{
  DEVICE="$1"

  # Set the base name we use for zpools
  BASENAME="tank"

  if [ ! -d "${TMPDIR}/.zpools" ] ; then
    mkdir -p ${TMPDIR}/.zpools
  fi

  if [ -e "${TMPDIR}/.zpools/${DEVICE}" ] ; then
    cat ${TMPDIR}/.zpools/${DEVICE}
    return 0
  else
    # Need to generate a zpool name for this device
    NUM=`ls ${TMPDIR}/.zpools/ | wc -l | sed 's| ||g'`
    NEWNAME="${BASENAME}${NUM}"
    echo "$NEWNAME" >${TMPDIR}/.zpools/${DEVICE} 
    echo "${NEWNAME}"
    return
  fi
};
