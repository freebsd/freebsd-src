#!/bin/sh

# mergemaster

# Compare files created by /usr/src/etc/Makefile (or the directory
# the user specifies) with the currently installed copies.

# Copyright 1998-2004 Douglas Barton
# DougB@FreeBSD.org

# $FreeBSD$

PATH=/bin:/usr/bin:/usr/sbin

display_usage () {
  VERSION_NUMBER=`grep "[$]FreeBSD:" $0 | cut -d ' ' -f 4`
  echo "mergemaster version ${VERSION_NUMBER}"
  echo 'Usage: mergemaster [-scrvahipCP] [-m /path]'
  echo '         [-t /path] [-d] [-u N] [-w N] [-D /path]'
  echo "Options:"
  echo "  -s  Strict comparison (diff every pair of files)"
  echo "  -c  Use context diff instead of unified diff"
  echo "  -r  Re-run on a previously cleaned directory (skip temproot creation)"
  echo "  -v  Be more verbose about the process, include additional checks"
  echo "  -a  Leave all files that differ to merge by hand"
  echo "  -h  Display more complete help"
  echo '  -i  Automatically install files that do not exist in destination directory'
  echo '  -p  Pre-buildworld mode, only compares crucial files'
  echo '  -C  Compare local rc.conf variables to the defaults'
  echo '  -P  Preserve files that are overwritten'
  echo "  -m /path/directory  Specify location of source to do the make in"
  echo "  -t /path/directory  Specify temp root directory"
  echo "  -d  Add date and time to directory name (e.g., /var/tmp/temproot.`date +%m%d.%H.%M`)"
  echo "  -u N  Specify a numeric umask"
  echo "  -w N  Specify a screen width in columns to sdiff"
  echo "  -A architecture  Alternative architecture name to pass to make"
  echo '  -D /path/directory  Specify the destination directory to install files to'
  echo "  -U Attempt to auto upgrade files that have not been user modified."
  echo ''
}

display_help () {
  echo "* To specify a directory other than /var/tmp/temproot for the"
  echo "  temporary root environment, use -t /path/to/temp/root"
  echo "* The -w option takes a number as an argument for the column width"
  echo "  of the screen.  The default is 80."
  echo '* The -a option causes mergemaster to run without prompting.'
}

# Loop allowing the user to use sdiff to merge files and display the merged
# file.
merge_loop () {
  case "${VERBOSE}" in
  '') ;;
  *)
      echo "   *** Type h at the sdiff prompt (%) to get usage help"
      ;;
  esac
  echo ''
  MERGE_AGAIN=yes
  while [ "${MERGE_AGAIN}" = "yes" ]; do
    # Prime file.merged so we don't blat the owner/group id's
    cp -p "${COMPFILE}" "${COMPFILE}.merged"
    sdiff -o "${COMPFILE}.merged" --text --suppress-common-lines \
      --width=${SCREEN_WIDTH:-80} "${DESTDIR}${COMPFILE#.}" "${COMPFILE}"
    INSTALL_MERGED=V
    while [ "${INSTALL_MERGED}" = "v" -o "${INSTALL_MERGED}" = "V" ]; do
      echo ''
      echo "  Use 'i' to install merged file"
      echo "  Use 'r' to re-do the merge"
      echo "  Use 'v' to view the merged file"
      echo "  Default is to leave the temporary file to deal with by hand"
      echo ''
      echo -n "    *** How should I deal with the merged file? [Leave it for later] "
      read INSTALL_MERGED

      case "${INSTALL_MERGED}" in
      [iI])
        mv "${COMPFILE}.merged" "${COMPFILE}"
        echo ''
        if mm_install "${COMPFILE}"; then
          echo "     *** Merged version of ${COMPFILE} installed successfully"
        else
          echo "     *** Problem installing ${COMPFILE}, it will remain to merge by hand later"
        fi
        unset MERGE_AGAIN
        ;;
      [rR])
        rm "${COMPFILE}.merged"
        ;;
      [vV])
        ${PAGER} "${COMPFILE}.merged"
        ;;
      '')
        echo "   *** ${COMPFILE} will remain for your consideration"
        unset MERGE_AGAIN
        ;;
      *)
        echo "invalid choice: ${INSTALL_MERGED}"
        INSTALL_MERGED=V
        ;;
      esac
    done
  done
}

# Loop showing user differences between files, allow merge, skip or install
# options
diff_loop () {

  HANDLE_COMPFILE=v

  while [ "${HANDLE_COMPFILE}" = "v" -o "${HANDLE_COMPFILE}" = "V" -o \
    "${HANDLE_COMPFILE}" = "NOT V" ]; do
    if [ -f "${DESTDIR}${COMPFILE#.}" -a -f "${COMPFILE}" ]; then
      if [ -n "${AUTO_UPGRADE}" ]; then
        if echo "${CHANGED}" | grep -qsv ${DESTDIR}${COMPFILE#.}; then
          echo ''
          echo "  *** ${COMPFILE} has not been user modified."
          echo ''

          if mm_install "${COMPFILE}"; then
            echo "   *** ${COMPFILE} upgraded successfully"
            echo ''
            # Make the list print one file per line
            AUTO_UPGRADED_FILES="${AUTO_UPGRADED_FILES}      ${DESTDIR}${COMPFILE#.}
"
          else
          echo "   *** Problem upgrading ${COMPFILE}, it will remain to merge by hand"
          fi
          return
        fi
      fi
      if [ "${HANDLE_COMPFILE}" = "v" -o "${HANDLE_COMPFILE}" = "V" ]; then
	echo ''
	echo '   ======================================================================   '
	echo ''
        (
          echo "  *** Displaying differences between ${COMPFILE} and installed version:"
          echo ''
          diff ${DIFF_FLAG} ${DIFF_OPTIONS} "${DESTDIR}${COMPFILE#.}" "${COMPFILE}"
        ) | ${PAGER}
        echo ''
      fi
    else
      echo ''
      echo "  *** There is no installed version of ${COMPFILE}"
      echo ''
      case "${AUTO_INSTALL}" in
      [Yy][Ee][Ss])
        echo ''
        if mm_install "${COMPFILE}"; then
          echo "   *** ${COMPFILE} installed successfully"
          echo ''
          # Make the list print one file per line
          AUTO_INSTALLED_FILES="${AUTO_INSTALLED_FILES}      ${DESTDIR}${COMPFILE#.}
"
        else
          echo "   *** Problem installing ${COMPFILE}, it will remain to merge by hand"
        fi
        return
        ;;
      *)
        NO_INSTALLED=yes
        ;;
      esac
    fi

    echo "  Use 'd' to delete the temporary ${COMPFILE}"
    echo "  Use 'i' to install the temporary ${COMPFILE}"
    case "${NO_INSTALLED}" in
    '')
      echo "  Use 'm' to merge the temporary and installed versions"
      echo "  Use 'v' to view the diff results again"
      ;;
    esac
    echo ''
    echo "  Default is to leave the temporary file to deal with by hand"
    echo ''
    echo -n "How should I deal with this? [Leave it for later] "
    read HANDLE_COMPFILE

    case "${HANDLE_COMPFILE}" in
    [dD])
      rm "${COMPFILE}"
      echo ''
      echo "   *** Deleting ${COMPFILE}"
      ;;
    [iI])
      echo ''
      if mm_install "${COMPFILE}"; then
        echo "   *** ${COMPFILE} installed successfully"
      else
        echo "   *** Problem installing ${COMPFILE}, it will remain to merge by hand"
      fi
      ;;
    [mM])
      case "${NO_INSTALLED}" in
      '')
        # interact with user to merge files
        merge_loop
        ;;
      *)
        echo ''
        echo "   *** There is no installed version of ${COMPFILE}"
        echo ''
        HANDLE_COMPFILE="NOT V"
        ;;
      esac # End of "No installed version of file but user selected merge" test
      ;;
    [vV])
      continue
      ;;
    '')
      echo ''
      echo "   *** ${COMPFILE} will remain for your consideration"
      ;;
    *)
      # invalid choice, show menu again.
      echo "invalid choice: ${HANDLE_COMPFILE}"
      echo ''
      HANDLE_COMPFILE="NOT V"
      continue
      ;;
    esac  # End of "How to handle files that are different"
  done
  unset NO_INSTALLED
  echo ''
  case "${VERBOSE}" in
  '') ;;
  *)
    sleep 3
    ;;
  esac
}

press_to_continue () {
  local DISCARD
  echo -n ' *** Press the [Enter] or [Return] key to continue '
  read DISCARD
}

# Set the default path for the temporary root environment
#
TEMPROOT='/var/tmp/temproot'

# Assign the location of the mtree database
#
MTREEDB='/var/db/mergemaster.mtree'

# Read /etc/mergemaster.rc first so the one in $HOME can override
#
if [ -r /etc/mergemaster.rc ]; then
  . /etc/mergemaster.rc
fi

# Read .mergemasterrc before command line so CLI can override
#
if [ -r "$HOME/.mergemasterrc" ]; then
  . "$HOME/.mergemasterrc"
fi

# Check the command line options
#
while getopts ":ascrvhipCPm:t:du:w:D:A:U" COMMAND_LINE_ARGUMENT ; do
  case "${COMMAND_LINE_ARGUMENT}" in
  A)
    ARCHSTRING='MACHINE_ARCH='${OPTARG}
    ;;
  U)
    AUTO_UPGRADE=yes
    ;;
  s)
    STRICT=yes
    unset DIFF_OPTIONS
    ;;
  c)
    DIFF_FLAG='-c'
    ;;
  r)
    RERUN=yes
    ;;
  v)
    case "${AUTO_RUN}" in
    '') VERBOSE=yes ;;
    esac
    ;;
  a)
    AUTO_RUN=yes
    unset VERBOSE
    ;;
  h)
    display_usage
    display_help
    exit 0
    ;;
  i)
    AUTO_INSTALL=yes
    ;;
  C)
    COMP_CONFS=yes
    ;;
  P)
    PRESERVE_FILES=yes
    ;;
  p)
    PRE_WORLD=yes
    unset COMP_CONFS
    unset AUTO_RUN
    ;;
  m)
    SOURCEDIR=${OPTARG}
    ;;
  t)
    TEMPROOT=${OPTARG}
    ;;
  d)
    TEMPROOT=${TEMPROOT}.`date +%m%d.%H.%M`
    ;;
  u)
    NEW_UMASK=${OPTARG}
    ;;
  w)
    SCREEN_WIDTH=${OPTARG}
    ;;
  D)
    DESTDIR=${OPTARG}
    ;;
  *)
    display_usage
    exit 1
    ;;
  esac
done

# Don't force the user to set this in the mergemaster rc file
if [ -n "${PRESERVE_FILES}" -a -z "${PRESERVE_FILES_DIR}" ]; then
  PRESERVE_FILES_DIR=/var/tmp/mergemaster/preserved-files-`date +%y%m%d-%H%M%S`
fi

# Check the for the mtree database in DESTDIR.
if [ ! -f ${DESTDIR}${MTREEDB} ]; then
  echo "*** Unable to find mtree database. Skipping auto-upgrade."
  unset AUTO_UPGRADE
fi

echo ''

# If the user has a pager defined, make sure we can run it
#
case "${DONT_CHECK_PAGER}" in
'')
  while ! type "${PAGER%% *}" >/dev/null && [ -n "${PAGER}" ]; do
    echo " *** Your PAGER environment variable specifies '${PAGER}', but"
    echo "     due to the limited PATH that I use for security reasons,"
    echo "     I cannot execute it.  So, what would you like to do?"
    echo ''
    echo "  Use 'e' to exit mergemaster and fix your PAGER variable"
    if [ -x /usr/bin/less -o -x /usr/local/bin/less ]; then
    echo "  Use 'l' to set PAGER to 'less' for this run"
    fi
    echo "  Use 'm' to use plain old 'more' as your PAGER for this run"
    echo ''
    echo "  Default is to use plain old 'more' "
    echo ''
    echo -n "What should I do? [Use 'more'] "
    read FIXPAGER

    case "${FIXPAGER}" in
    [eE])
       exit 0
       ;;
    [lL])
       if [ -x /usr/bin/less ]; then
         PAGER=/usr/bin/less
       elif [ -x /usr/local/bin/less ]; then
         PAGER=/usr/local/bin/less
       else
         echo ''
         echo " *** Fatal Error:"
         echo "     You asked to use 'less' as your pager, but I can't"
         echo "     find it in /usr/bin or /usr/local/bin"
         exit 1
       fi
       ;;
    [mM]|'')
       PAGER=more
       ;;
    *)
       echo ''
       echo "invalid choice: ${FIXPAGER}"
    esac
    echo ''
  done
  ;;
esac

# If user has a pager defined, or got assigned one above, use it.
# If not, use more.
#
PAGER=${PAGER:-more}

if [ -n "${VERBOSE}" -a ! "${PAGER}" = "more" ]; then
  echo " *** You have ${PAGER} defined as your pager so we will use that"
  echo ''
  sleep 3
fi

# Assign the diff flag once so we will not have to keep testing it
#
DIFF_FLAG=${DIFF_FLAG:--u}

# Assign the source directory
#
SOURCEDIR=${SOURCEDIR:-/usr/src/etc}

# Check DESTDIR against the mergemaster mtree database to see what
# files the user changed from the reference files.
#
CHANGED=
if [ -n "${AUTO_UPGRADE}" -a -f "${DESTDIR}${MTREEDB}" ]; then
	for file in `mtree -eq -f ${DESTDIR}${MTREEDB} -p ${DESTDIR}/ \
		2>/dev/null | awk '($2 == "changed") {print $1}'`; do
		if [ -f "${DESTDIR}/$file" ]; then
			CHANGED="${CHANGED} ${DESTDIR}/$file"
		fi
	done
fi

# Check the width of the user's terminal
#
if [ -t 0 ]; then
  w=`tput columns`
  case "${w}" in
  0|'') ;; # No-op, since the input is not valid
  *)
    case "${SCREEN_WIDTH}" in
    '') SCREEN_WIDTH="${w}" ;;
    "${w}") ;; # No-op, since they are the same
    *)
      echo -n "*** You entered ${SCREEN_WIDTH} as your screen width, but stty "
      echo "thinks it is ${w}."
      echo ''
      echo -n "What would you like to use? [${w}] "
      read SCREEN_WIDTH
      case "${SCREEN_WIDTH}" in
      '') SCREEN_WIDTH="${w}" ;;
      esac
      ;;
    esac
  esac
fi

# Define what CVS $Id tag to look for to aid portability.
#
CVS_ID_TAG=FreeBSD

delete_temproot () {
  rm -rf "${TEMPROOT}" 2>/dev/null
  chflags -R 0 "${TEMPROOT}" 2>/dev/null
  rm -rf "${TEMPROOT}" || exit 1
}

case "${RERUN}" in
'')
  # Set up the loop to test for the existence of the
  # temp root directory.
  #
  TEST_TEMP_ROOT=yes
  while [ "${TEST_TEMP_ROOT}" = "yes" ]; do
    if [ -d "${TEMPROOT}" ]; then
      echo "*** The directory specified for the temporary root environment,"
      echo "    ${TEMPROOT}, exists.  This can be a security risk if untrusted"
      echo "    users have access to the system."
      echo ''
      case "${AUTO_RUN}" in
      '')
        echo "  Use 'd' to delete the old ${TEMPROOT} and continue"
        echo "  Use 't' to select a new temporary root directory"
        echo "  Use 'e' to exit mergemaster"
        echo ''
        echo "  Default is to use ${TEMPROOT} as is"
        echo ''
        echo -n "How should I deal with this? [Use the existing ${TEMPROOT}] "
        read DELORNOT

        case "${DELORNOT}" in
        [dD])
          echo ''
          echo "   *** Deleting the old ${TEMPROOT}"
          echo ''
          delete_temproot || exit 1
          unset TEST_TEMP_ROOT
          ;;
        [tT])
          echo "   *** Enter new directory name for temporary root environment"
          read TEMPROOT
          ;;
        [eE])
          exit 0
          ;;
        '')
          echo ''
          echo "   *** Leaving ${TEMPROOT} intact"
          echo ''
          unset TEST_TEMP_ROOT
          ;;
        *)
          echo ''
          echo "invalid choice: ${DELORNOT}"
          echo ''
          ;;
        esac
        ;;
      *)
        # If this is an auto-run, try a hopefully safe alternative then
        # re-test anyway.
        TEMPROOT=/var/tmp/temproot.`date +%m%d.%H.%M.%S`
        ;;
      esac
    else
      unset TEST_TEMP_ROOT
    fi
  done

  echo "*** Creating the temporary root environment in ${TEMPROOT}"

  if mkdir -p "${TEMPROOT}"; then
    echo " *** ${TEMPROOT} ready for use"
  fi

  if [ ! -d "${TEMPROOT}" ]; then
    echo ''
    echo "  *** FATAL ERROR: Cannot create ${TEMPROOT}"
    echo ''
    exit 1
  fi

  echo " *** Creating and populating directory structure in ${TEMPROOT}"
  echo ''

  case "${VERBOSE}" in
  '') ;;
  *)
    press_to_continue
    ;;
  esac

  case "${PRE_WORLD}" in
  '')
    { cd ${SOURCEDIR} &&
      case "${DESTDIR}" in
      '') ;;
      *)
      make DESTDIR=${DESTDIR} ${ARCHSTRING} distrib-dirs
        ;;
      esac
      make DESTDIR=${TEMPROOT} ${ARCHSTRING} distrib-dirs &&
      MAKEOBJDIRPREFIX=${TEMPROOT}/usr/obj make ${ARCHSTRING} obj &&
      MAKEOBJDIRPREFIX=${TEMPROOT}/usr/obj make ${ARCHSTRING} all &&
      MAKEOBJDIRPREFIX=${TEMPROOT}/usr/obj make ${ARCHSTRING} \
	  DESTDIR=${TEMPROOT} distribution;} ||
    { echo '';
     echo "  *** FATAL ERROR: Cannot 'cd' to ${SOURCEDIR} and install files to";
      echo "      the temproot environment";
      echo '';
      exit 1;}
    ;;
  *)
    # Only set up files that are crucial to {build|install}world
    { mkdir -p ${TEMPROOT}/etc &&
      cp -p ${SOURCEDIR}/master.passwd ${TEMPROOT}/etc &&
      cp -p ${SOURCEDIR}/group ${TEMPROOT}/etc;} ||
    { echo '';
      echo '  *** FATAL ERROR: Cannot copy files to the temproot environment';
      echo '';
      exit 1;}
    ;;
  esac

  # Doing the inventory and removing files that we don't want to compare only
  # makes sense if we are not doing a rerun, since we have no way of knowing
  # what happened to the files during previous incarnations.
  case "${VERBOSE}" in
  '') ;;
  *)
    echo ''
    echo ' *** The following files exist only in the installed version of'
    echo "     ${DESTDIR}/etc.  In the vast majority of cases these files"
    echo '     are necessary parts of the system and should not be deleted.'
    echo '     However because these files are not updated by this process you'
    echo '     might want to verify their status before rebooting your system.'
    echo ''
    press_to_continue
    diff -qr ${DESTDIR}/etc ${TEMPROOT}/etc | grep "^Only in ${DESTDIR}/etc" | ${PAGER}
    echo ''
    press_to_continue
    ;;
  esac

  # Avoid comparing the motd if the user specifies it in .mergemasterrc
  case "${IGNORE_MOTD}" in
  '') ;;
  *) rm -f ${TEMPROOT}/etc/motd
     ;;
  esac

  # Avoid trying to update MAKEDEV if /dev is on a devfs
  if /sbin/sysctl vfs.devfs.generation > /dev/null 2>&1 ; then
    rm -f ${TEMPROOT}/dev/MAKEDEV ${TEMPROOT}/dev/MAKEDEV.local
  fi

  ;; # End of the "RERUN" test
esac

# We really don't want to have to deal with files like login.conf.db, pwd.db,
# or spwd.db.  Instead, we want to compare the text versions, and run *_mkdb.
# Prompt the user to do so below, as needed.
#
rm -f ${TEMPROOT}/etc/*.db ${TEMPROOT}/etc/passwd

# We only need to compare things like freebsd.cf once
find ${TEMPROOT}/usr/obj -type f -delete 2>/dev/null

# Delete 0 length files to make the mtree database as small as possible.
find ${TEMPROOT} -type f -size 0 -delete 2>/dev/null

# Build the mtree database in a temporary location.
# TODO: Possibly use mktemp instead for security reasons?
case "${PRE_WORLD}" in
'') mtree -ci -p ${TEMPROOT} -k size,md5digest > ${DESTDIR}${MTREEDB}.new 2>/dev/null
    ;;
*) # We don't want to mess with the mtree database on a pre-world run.
   ;;
esac

# Get ready to start comparing files

# Check umask if not specified on the command line,
# and we are not doing an autorun
#
if [ -z "${NEW_UMASK}" -a -z "${AUTO_RUN}" ]; then
  USER_UMASK=`umask`
  case "${USER_UMASK}" in
  0022|022) ;;
  *)
    echo ''
    echo " *** Your umask is currently set to ${USER_UMASK}.  By default, this script"
    echo "     installs all files with the same user, group and modes that"
    echo "     they are created with by ${SOURCEDIR}/Makefile, compared to"
    echo "     a umask of 022.  This umask allows world read permission when"
    echo "     the file's default permissions have it."
    echo ''
    echo "     No world permissions can sometimes cause problems.  A umask of"
    echo "     022 will restore the default behavior, but is not mandatory."
    echo "     /etc/master.passwd is a special case.  Its file permissions"
    echo "     will be 600 (rw-------) if installed."
    echo ''
    echo -n "What umask should I use? [${USER_UMASK}] "
    read NEW_UMASK

    NEW_UMASK="${NEW_UMASK:-$USER_UMASK}"
    ;;
  esac
  echo ''
fi

CONFIRMED_UMASK=${NEW_UMASK:-0022}

#
# Warn users who still have old rc files
#
for file in atm devfs diskless1 diskless2 network network6 pccard \
  serial syscons sysctl alpha amd64 i386 ia64 sparc64; do
  if [ -f "${DESTDIR}/etc/rc.${file}" ]; then
    OLD_RC_PRESENT=1
    break
  fi
done

case "${OLD_RC_PRESENT}" in
1)
  echo ''
  echo " *** There are elements of the old rc system in ${DESTDIR}/etc/."
  echo ''
  echo '     While these scripts will not hurt anything, they are not'
  echo '     functional on an up to date system, and can be removed.'
  echo ''

  case "${AUTO_RUN}" in
  '')
    echo -n 'Move these files to /var/tmp/mergemaster/old_rc? [yes] '
    read MOVE_OLD_RC

    case "${MOVE_OLD_RC}" in
    [nN]*) ;;
    *)
      mkdir -p /var/tmp/mergemaster/old_rc
        for file in atm devfs diskless1 diskless2 network network6 pccard \
          serial syscons sysctl alpha amd64 i386 ia64 sparc64; do
          if [ -f "${DESTDIR}/etc/rc.${file}" ]; then
            mv ${DESTDIR}/etc/rc.${file} /var/tmp/mergemaster/old_rc/
          fi
        done
      echo '  The files have been moved'
      press_to_continue
      ;;
    esac
    ;;
  *) ;;
  esac
esac

# Use the umask/mode information to install the files
# Create directories as needed
#
do_install_and_rm () {
  case "${PRESERVE_FILES}" in
  [Yy][Ee][Ss])
    if [ -f "${3}/${2##*/}" ]; then
      mkdir -p ${PRESERVE_FILES_DIR}/${2%/*}
      cp ${3}/${2##*/} ${PRESERVE_FILES_DIR}/${2%/*}
    fi
    ;;
  esac

  install -m "${1}" "${2}" "${3}" &&
  rm -f "${2}"
}

# 4095 = "obase=10;ibase=8;07777" | bc
find_mode () {
  local OCTAL
  OCTAL=$(( ~$(echo "obase=10; ibase=8; ${CONFIRMED_UMASK}" | bc) & 4095 &
    $(echo "obase=10; ibase=8; $(stat -f "%OMp%OLp" ${1})" | bc) ))
  printf "%04o\n" ${OCTAL}
}

mm_install () {
  local INSTALL_DIR
  INSTALL_DIR=${1#.}
  INSTALL_DIR=${INSTALL_DIR%/*}

  case "${INSTALL_DIR}" in
  '')
    INSTALL_DIR=/
    ;;
  esac

  if [ -n "${DESTDIR}${INSTALL_DIR}" -a ! -d "${DESTDIR}${INSTALL_DIR}" ]; then
    DIR_MODE=`find_mode "${TEMPROOT}/${INSTALL_DIR}"`
    install -d -o root -g wheel -m "${DIR_MODE}" "${DESTDIR}${INSTALL_DIR}"
  fi

  FILE_MODE=`find_mode "${1}"`

  if [ ! -x "${1}" ]; then
    case "${1#.}" in
    /etc/mail/aliases)
      NEED_NEWALIASES=yes
      ;;
    /etc/login.conf)
      NEED_CAP_MKDB=yes
      ;;
    /etc/master.passwd)
      do_install_and_rm 600 "${1}" "${DESTDIR}${INSTALL_DIR}"
      NEED_PWD_MKDB=yes
      DONT_INSTALL=yes
      ;;
    /.cshrc | /.profile)
    case "${AUTO_INSTALL}" in
    '')
      case "${LINK_EXPLAINED}" in
      '')
        echo "   *** Historically BSD derived systems have had a"
        echo "       hard link from /.cshrc and /.profile to"
        echo "       their namesakes in /root.  Please indicate"
        echo "       your preference below for bringing your"
        echo "       installed files up to date."
        echo ''
        LINK_EXPLAINED=yes
        ;;
      esac

      echo "   Use 'd' to delete the temporary ${COMPFILE}"
      echo "   Use 'l' to delete the existing ${DESTDIR}${COMPFILE#.} and create the link"
      echo ''
      echo "   Default is to leave the temporary file to deal with by hand"
      echo ''
      echo -n "  How should I handle ${COMPFILE}? [Leave it to install later] "
      read HANDLE_LINK
      ;;
    *)  # Part of AUTO_INSTALL
      HANDLE_LINK=l
      ;;
    esac

      case "${HANDLE_LINK}" in
      [dD]*)
        rm "${COMPFILE}"
        echo ''
        echo "   *** Deleting ${COMPFILE}"
        ;;
      [lL]*)
        echo ''
        rm -f "${DESTDIR}${COMPFILE#.}"
        if ln "${DESTDIR}/root/${COMPFILE##*/}" "${DESTDIR}${COMPFILE#.}"; then
          echo "   *** Link from ${DESTDIR}${COMPFILE#.} to ${DESTDIR}/root/${COMPFILE##*/} installed successfully"
          rm "${COMPFILE}"
        else
          echo "   *** Error linking ${DESTDIR}${COMPFILE#.} to ${DESTDIR}/root/${COMPFILE##*/}, ${COMPFILE} will remain to install by hand"
        fi
        ;;
      *)
        echo "   *** ${COMPFILE} will remain for your consideration"
        ;;
      esac
      DONT_INSTALL=yes
      ;;
    esac

    case "${DONT_INSTALL}" in
    '')
      do_install_and_rm "${FILE_MODE}" "${1}" "${DESTDIR}${INSTALL_DIR}"
      ;;
    *)
      unset DONT_INSTALL
      ;;
    esac
  else	# File matched -x
    case "${1#.}" in
    /dev/MAKEDEV)
      NEED_MAKEDEV=yes
      ;;
    esac
    do_install_and_rm "${FILE_MODE}" "${1}" "${DESTDIR}${INSTALL_DIR}"
  fi
  return $?
}

if [ ! -d "${TEMPROOT}" ]; then
	echo "*** FATAL ERROR: The temproot directory (${TEMPROOT})"
	echo '                 has disappeared!'
	echo ''
	exit 1
fi

echo ''
echo "*** Beginning comparison"
echo ''

# Pre-world does not populate /etc/rc.d.
# It is very possible that a previous run would have deleted files in
# ${TEMPROOT}/etc/rc.d, thus creating a lot of false positives.
if [ -z "${PRE_WORLD}" -a -z "${RERUN}" ]; then
  echo "   *** Checking ${DESTDIR}/etc/rc.d for stale files"
  echo ''
  cd "${DESTDIR}/etc/rc.d" &&
  for file in *; do
    if [ ! -e "${TEMPROOT}/etc/rc.d/${file}" ]; then
      STALE_RC_FILES="${STALE_RC_FILES} ${file}"
    fi
  done
  case "${STALE_RC_FILES}" in
  ''|' *')
    echo '   *** No stale files found'
    ;;
  *)
    echo "   *** The following files exist in ${DESTDIR}/etc/rc.d but not in"
    echo "       ${TEMPROOT}/etc/rc.d/:"
    echo ''
    echo "${STALE_RC_FILES}"
    echo ''
    echo '       The presence of stale files in this directory can cause the'
    echo '       dreaded unpredictable results, and therefore it is highly'
    echo '       recommended that you delete them.'
    case "${AUTO_RUN}" in
    '')
      echo ''
      echo -n '   *** Delete them now? [n] '
      read DELETE_STALE_RC_FILES
      case "${DELETE_STALE_RC_FILES}" in
      [yY])
        echo '      *** Deleting ... '
        rm ${STALE_RC_FILES}
        echo '                       done.'
        ;;
      *)
        echo '      *** Files will not be deleted'
        ;;
      esac
      sleep 2
      ;;
    esac
    ;;
  esac
  echo ''
fi

cd "${TEMPROOT}"

if [ -r "${MM_PRE_COMPARE_SCRIPT}" ]; then
  . "${MM_PRE_COMPARE_SCRIPT}"
fi

# Using -size +0 avoids uselessly checking the empty log files created
# by ${SOURCEDIR}/Makefile and the device entries in ./dev, but does
# check the scripts in ./dev, as we'd like (assuming no devfs of course).
#
for COMPFILE in `find . -type f -size +0`; do

  # First, check to see if the file exists in DESTDIR.  If not, the
  # diff_loop function knows how to handle it.
  #
  if [ ! -e "${DESTDIR}${COMPFILE#.}" ]; then
    case "${AUTO_RUN}" in
      '')
        diff_loop
        ;;
      *)
        case "${AUTO_INSTALL}" in
        '')
          # If this is an auto run, make it official
          echo "   *** ${COMPFILE} will remain for your consideration"
          ;;
        *)
          diff_loop
          ;;
        esac
        ;;
    esac # Auto run test
    continue
  fi

  case "${STRICT}" in
  '' | [Nn][Oo])
    # Compare CVS $Id's first so if the file hasn't been modified
    # local changes will be ignored.
    # If the files have the same $Id, delete the one in temproot so the
    # user will have less to wade through if files are left to merge by hand.
    #
    CVSID1=`grep "[$]${CVS_ID_TAG}:" ${DESTDIR}${COMPFILE#.} 2>/dev/null`
    CVSID2=`grep "[$]${CVS_ID_TAG}:" ${COMPFILE} 2>/dev/null` || CVSID2=none

    case "${CVSID2}" in
    "${CVSID1}")
      echo " *** Temp ${COMPFILE} and installed have the same CVS Id, deleting"
      rm "${COMPFILE}"
      ;;

    *)
      tempfoo=`basename $0`
      TMPFILE1=`mktemp -t ${tempfoo}` || break
      TMPFILE2=`mktemp -t ${tempfoo}` || break
      sed "s/[$]${CVS_ID_TAG}:.*[$]//g" "${DESTDIR}${COMPFILE#.}" > "${TMPFILE1}"
      sed "s/[$]${CVS_ID_TAG}:.*[$]//g" "${COMPFILE}" > "${TMPFILE2}"
      if diff -q ${DIFF_OPTIONS} "${TMPFILE1}" "${TMPFILE2}" > \
        /dev/null 2>&1; then
        echo " *** Temp ${COMPFILE} and installed are the same except CVS Id, replacing"
        if mm_install "${COMPFILE}"; then
          echo "   *** ${COMPFILE} upgraded successfully"
          echo ''
        else
          echo "   *** Problem upgrading ${COMPFILE}, it will remain to merge by hand"
        fi
      fi
      rm -f "${TMPFILE1}" "${TMPFILE2}"
      ;;
    esac
    ;;
  esac

  # If the file is still here either because the $Ids are different, the
  # file doesn't have an $Id, or we're using STRICT mode; look at the diff.
  #
  if [ -f "${COMPFILE}" ]; then

    # Do an absolute diff first to see if the files are actually different.
    # If they're not different, delete the one in temproot.
    #
    if diff -q ${DIFF_OPTIONS} "${DESTDIR}${COMPFILE#.}" "${COMPFILE}" > \
      /dev/null 2>&1; then
      echo " *** Temp ${COMPFILE} and installed are the same, deleting"
      rm "${COMPFILE}"
    else
      # Ok, the files are different, so show the user where they differ.
      # Use user's choice of diff methods; and user's pager if they have one.
      # Use more if not.
      # Use unified diffs by default.  Context diffs give me a headache. :)
      #
      case "${AUTO_RUN}" in
      '')
        # prompt user to install/delete/merge changes
        diff_loop
        ;;
      *)
        # If this is an auto run, make it official
        echo "   *** ${COMPFILE} will remain for your consideration"
        ;;
      esac # Auto run test
    fi # Yes, the files are different
  fi # Yes, the file still remains to be checked
done # This is for the do way up there at the beginning of the comparison

echo ''
echo "*** Comparison complete"

if [ -f "${DESTDIR}${MTREEDB}.new" ]; then
  echo "*** Saving mtree database for future upgrades"
  mv -f ${DESTDIR}${MTREEDB}.new ${DESTDIR}${MTREEDB} 2>/dev/null
fi

echo ''

TEST_FOR_FILES=`find ${TEMPROOT} -type f -size +0 2>/dev/null`
if [ -n "${TEST_FOR_FILES}" ]; then
  echo "*** Files that remain for you to merge by hand:"
  find "${TEMPROOT}" -type f -size +0
  echo ''
fi

case "${AUTO_RUN}" in
'')
  echo -n "Do you wish to delete what is left of ${TEMPROOT}? [no] "
  read DEL_TEMPROOT

  case "${DEL_TEMPROOT}" in
  [yY]*)
    if delete_temproot; then
      echo " *** ${TEMPROOT} has been deleted"
    else
      echo " *** Unable to delete ${TEMPROOT}"
    fi
    ;;
  *)
    echo " *** ${TEMPROOT} will remain"
    ;;
  esac
  ;;
*) ;;
esac

case "${AUTO_INSTALLED_FILES}" in
'') ;;
*)
  case "${AUTO_RUN}" in
  '')
    (
      echo ''
      echo '*** You chose the automatic install option for files that did not'
      echo '    exist on your system.  The following were installed for you:'
      echo "${AUTO_INSTALLED_FILES}"
    ) | ${PAGER}
    ;;
  *)
    echo ''
    echo '*** You chose the automatic install option for files that did not'
    echo '    exist on your system.  The following were installed for you:'
    echo "${AUTO_INSTALLED_FILES}"
    ;;
  esac
  ;;
esac

case "${AUTO_UPGRADED_FILES}" in
'') ;;
*)
  case "${AUTO_RUN}" in
  '')
    (
      echo ''
      echo '*** You chose the automatic upgrade option for files that you did'
      echo '    not alter on your system.  The following were upgraded for you:'
      echo "${AUTO_UPGRADED_FILES}"
    ) | ${PAGER}
    ;;
  *)
    echo ''
    echo '*** You chose the automatic upgrade option for files that you did'
    echo '    not alter on your system.  The following were upgraded for you:'
    echo "${AUTO_UPGRADED_FILES}"
    ;;
  esac
  ;;
esac

run_it_now () {
  case "${AUTO_RUN}" in
  '')
    unset YES_OR_NO
    echo ''
    echo -n '    Would you like to run it now? y or n [n] '
    read YES_OR_NO

    case "${YES_OR_NO}" in
    y)
      echo "    Running ${1}"
      echo ''
      eval "${1}"
      ;;
    ''|n)
      echo ''
      echo "       *** Cancelled"
      echo ''
      echo "    Make sure to run ${1} yourself"
      ;;
    *)
      echo ''
      echo "       *** Sorry, I do not understand your answer (${YES_OR_NO})"
      echo ''
      echo "    Make sure to run ${1} yourself"
    esac
    ;;
  *) ;;
  esac
}

case "${NEED_MAKEDEV}" in
'') ;;
*)
  echo ''
  echo "*** You installed a new ${DESTDIR}/dev/MAKEDEV script, so make sure that you run"
  echo "    'cd ${DESTDIR}/dev && /bin/sh MAKEDEV all' to rebuild your devices"
  run_it_now "cd ${DESTDIR}/dev && /bin/sh MAKEDEV all"
  ;;
esac

case "${NEED_NEWALIASES}" in
'') ;;
*)
  echo ''
  if [ -n "${DESTDIR}" ]; then
    echo "*** You installed a new aliases file into ${DESTDIR}/etc/mail, but"
    echo "    the newaliases command is limited to the directories configured"
    echo "    in sendmail.cf.  Make sure to create your aliases database by"
    echo "    hand when your sendmail configuration is done."
  else
    echo "*** You installed a new aliases file, so make sure that you run"
    echo "    '/usr/bin/newaliases' to rebuild your aliases database"
    run_it_now '/usr/bin/newaliases'
  fi
  ;;
esac

case "${NEED_CAP_MKDB}" in
'') ;;
*)
  echo ''
  echo "*** You installed a login.conf file, so make sure that you run"
  echo "    '/usr/bin/cap_mkdb ${DESTDIR}/etc/login.conf'"
  echo "     to rebuild your login.conf database"
  run_it_now "/usr/bin/cap_mkdb ${DESTDIR}/etc/login.conf"
  ;;
esac

case "${NEED_PWD_MKDB}" in
'') ;;
*)
  echo ''
  echo "*** You installed a new master.passwd file, so make sure that you run"
  if [ -n "${DESTDIR}" ]; then
    echo "    '/usr/sbin/pwd_mkdb -d ${DESTDIR}/etc -p ${DESTDIR}/etc/master.passwd'"
    echo "    to rebuild your password files"
    run_it_now "/usr/sbin/pwd_mkdb -d ${DESTDIR}/etc -p ${DESTDIR}/etc/master.passwd"
  else
    echo "    '/usr/sbin/pwd_mkdb -p /etc/master.passwd'"
    echo "     to rebuild your password files"
    run_it_now '/usr/sbin/pwd_mkdb -p /etc/master.passwd'
  fi
  ;;
esac

echo ''

if [ -r "${MM_EXIT_SCRIPT}" ]; then
  . "${MM_EXIT_SCRIPT}"
fi

case "${COMP_CONFS}" in
'') ;;
*)
  . ${DESTDIR}/etc/defaults/rc.conf

  (echo ''
  echo "*** Comparing conf files: ${rc_conf_files}"

  for CONF_FILE in ${rc_conf_files}; do
    if [ -r "${DESTDIR}${CONF_FILE}" ]; then
      echo ''
      echo "*** From ${DESTDIR}${CONF_FILE}"
      echo "*** From ${DESTDIR}/etc/defaults/rc.conf"

      for RC_CONF_VAR in `grep -i ^[a-z] ${DESTDIR}${CONF_FILE} |
        cut -d '=' -f 1`; do
        echo ''
        grep -w ^${RC_CONF_VAR} ${DESTDIR}${CONF_FILE}
        grep -w ^${RC_CONF_VAR} ${DESTDIR}/etc/defaults/rc.conf ||
          echo ' * No default variable with this name'
      done
    fi
  done) | ${PAGER}
  echo ''
  ;;
esac

case "${PRE_WORLD}" in
'') ;;
*)
  MAKE_CONF="${SOURCEDIR%etc}share/examples/etc/make.conf"

  (echo ''
  echo '*** Comparing make variables'
  echo ''
  echo "*** From ${DESTDIR}/etc/make.conf"
  echo "*** From ${MAKE_CONF}"

  for MAKE_VAR in `grep -i ^[a-z] ${DESTDIR}/etc/make.conf | cut -d '=' -f 1`; do
    echo ''
    grep -w ^${MAKE_VAR} ${DESTDIR}/etc/make.conf
    grep -w ^#${MAKE_VAR} ${MAKE_CONF} ||
      echo ' * No example variable with this name'
  done) | ${PAGER}
  ;;
esac

exit 0

