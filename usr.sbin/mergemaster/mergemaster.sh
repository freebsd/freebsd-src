#!/bin/sh

# mergemaster

# Compare files created by /usr/src/etc/Makefile (or the directory
# the user specifies) with the currently installed copies.

# Copyright 1998-2001 Douglas Barton
# DougB@FreeBSD.org

# $FreeBSD$

PATH=/bin:/usr/bin:/usr/sbin

display_usage () {
  VERSION_NUMBER=`grep "[$]FreeBSD:" $0 | cut -d ' ' -f 4`
  echo "mergemaster version ${VERSION_NUMBER}"
  echo 'Usage: mergemaster [-scrvahi] [-m /path]'
  echo '         [-t /path] [-d] [-u N] [-w N] [-D /path]'
  echo "Options:"
  echo "  -s  Strict comparison (diff every pair of files)"
  echo "  -c  Use context diff instead of unified diff"
  echo "  -r  Re-run on a previously cleaned directory (skip temproot creation)"
  echo "  -v  Be more verbose about the process, include additional checks"
  echo "  -a  Leave all files that differ to merge by hand"
  echo "  -h  Display more complete help"
  echo '  -i  Automatically install files that do not exist in destination directory'
  echo "  -m /path/directory  Specify location of source to do the make in"
  echo "  -t /path/directory  Specify temp root directory"
  echo "  -d  Add date and time to directory name (e.g., /var/tmp/temproot.`date +%m%d.%H.%M`)"
  echo "  -u N  Specify a numeric umask"
  echo "  -w N  Specify a screen width in columns to sdiff"
  echo '  -D /path/directory  Specify the destination directory to install files to'
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
      if [ "${HANDLE_COMPFILE}" = "v" -o "${HANDLE_COMPFILE}" = "V" ]; then
        (
          echo ''
          echo "  *** Displaying differences between ${COMPFILE} and installed version:"
          echo ''
          diff "${DIFF_FLAG}" "${DESTDIR}${COMPFILE#.}" "${COMPFILE}"
        ) | ${PAGER}
        echo ''
      fi
    else
      echo ''
      echo "  *** There is no installed version of ${COMPFILE}"
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
      echo "  Use 'm' to merge the old and new versions"
      echo "  Use 'v' to view to differences between the old and new versions again"
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

# Set the default path for the temporary root environment
#
TEMPROOT='/var/tmp/temproot'

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
while getopts ":ascrvhim:t:du:w:D:" COMMAND_LINE_ARGUMENT ; do
  case "${COMMAND_LINE_ARGUMENT}" in
  s)
    STRICT=yes
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

# Define what CVS $Id tag to look for to aid portability.
#
CVS_ID_TAG=FreeBSD

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
          rm -rf "${TEMPROOT}"
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
    echo " *** Press [Enter] or [Return] key to continue"
    read ANY_KEY
    unset ANY_KEY
    ;;
  esac

  { cd ${SOURCEDIR} &&
    case "${DESTDIR}" in
    '') ;;
    *)
      make DESTDIR=${DESTDIR} distrib-dirs
      ;;
    esac
    make DESTDIR=${TEMPROOT} distrib-dirs &&
    make DESTDIR=${TEMPROOT} -DNO_MAKEDEV_RUN distribution;} ||
  { echo '';
    echo "  *** FATAL ERROR: Cannot 'cd' to ${SOURCEDIR} and install files to";
    echo "      the temproot environment";
    echo '';
    exit 1;}

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
    echo ' *** Press [Enter] or [Return] key to continue'
    read ANY_KEY
    unset ANY_KEY
    diff -qr ${DESTDIR}/etc ${TEMPROOT}/etc | grep "^Only in /etc" | ${PAGER}
    echo ''
    echo ' *** Press [Enter] or [Return] key to continue'
    read ANY_KEY
    unset ANY_KEY
    ;;
  esac

  # We really don't want to have to deal with these files, since
  # master.passwd is the real file that should be compared, then
  # the user should run pwd_mkdb if necessary.
  #
  rm ${TEMPROOT}/etc/spwd.db ${TEMPROOT}/etc/passwd ${TEMPROOT}/etc/pwd.db

  # Avoid comparing the motd if the user specifies it in .mergemasterrc
  case "${IGNORE_MOTD}" in
  '') ;;
  *) rm ${TEMPROOT}/etc/motd
     ;;
  esac

  # Avoid trying to update MAKEDEV if /dev is on a devfs
  if /sbin/sysctl vfs.devfs.generation > /dev/null 2>&1 ; then
    rm ${TEMPROOT}/dev/MAKEDEV ${TEMPROOT}/dev/MAKEDEV.local
  fi

  ;; # End of the "RERUN" test
esac

# Get ready to start comparing files

# Check umask if not specified on the command line,
# and we are not doing an autorun
#
if [ -z "${NEW_UMASK}" -a -z "${AUTO_RUN}" ]; then
  USER_UMASK=`umask`
  case "${USER_UMASK}" in
  0022) ;;
  *)
    echo ''
    echo " *** Your umask is currently set to ${USER_UMASK}.  By default, this script"
    echo "     installs all files with the same user, group and modes that"
    echo "     they are created with by ${SOURCEDIR}/Makefile, compared to"
    echo "     a umask of 022.  This umask allows world read permission when"
    echo "     the file's default permissions have it."
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

# Warn users who still have ${DESTDIR}/etc/sysconfig
#
if [ -e "${DESTDIR}/etc/sysconfig" ]; then
  echo ''
  echo " *** There is a sysconfig file on this system in ${DESTDIR}/etc/."
  echo ''
  echo '     Starting with FreeBSD version 2.2.2 those settings moved from'
  echo '     /etc/sysconfig to /etc/rc.conf.  If you are upgrading an older'
  echo '     system make sure that you transfer your settings by hand from'
  echo '     sysconfig to rc.conf and install the rc.conf file.  If you'
  echo '     have already made this transition, you should consider'
  echo '     renaming or deleting the sysconfig file.'
  echo ''
  case "${AUTO_RUN}" in
  '')
    echo -n "Continue with the merge process? [yes] "
    read CONT_OR_NOT

    case "${CONT_OR_NOT}" in
    [nN]*)
      exit 0
      ;;
    *)
      echo "   *** Continuing"
      echo ''
      ;;
    esac
    ;;
  *) ;;
  esac
fi

# Use the umask/mode information to install the files
# Create directories as needed
#
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
    DIR_MODE=`perl -e 'printf "%04o\n", (((stat("$ARGV[0]"))[2] & 07777) &~ \
      oct("$ARGV[1]"))' "${TEMPROOT}/${INSTALL_DIR}" "${CONFIRMED_UMASK}"`
    install -d -o root -g wheel -m "${DIR_MODE}" "${DESTDIR}${INSTALL_DIR}"
  fi

  FILE_MODE=`perl -e 'printf "%04o\n", (((stat("$ARGV[0]"))[2] & 07777) &~ \
      oct("$ARGV[1]"))' "${1}" "${CONFIRMED_UMASK}"`

  if [ ! -x "${1}" ]; then
    case "${1#.}" in
    /etc/mail/aliases)
      NEED_NEWALIASES=yes
      ;;
    /etc/login.conf)
      NEED_CAP_MKDB=yes
      ;;
    /etc/master.passwd)
      install -m 600 "${1}" "${DESTDIR}${INSTALL_DIR}"
      NEED_PWD_MKDB=yes
      DONT_INSTALL=yes
      ;;
    /.cshrc | /.profile)
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
      install -m "${FILE_MODE}" "${1}" "${DESTDIR}${INSTALL_DIR}"
      ;;
    *)
      unset DONT_INSTALL
      ;;
    esac
  else
    case "${1#.}" in
    /dev/MAKEDEV)
      NEED_MAKEDEV=yes
      ;;
    esac
    install -m "${FILE_MODE}" "${1}" "${DESTDIR}${INSTALL_DIR}"
  fi
  return $?
}

echo ''
echo "*** Beginning comparison"
echo ''

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
    diff_loop
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
    CVSID2=`grep "[$]${CVS_ID_TAG}:" ${COMPFILE} 2>/dev/null`

    case "${CVSID2}" in
    "${CVSID1}")
      echo " *** Temp ${COMPFILE} and installed have the same CVS Id, deleting"
      rm "${COMPFILE}"
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
    if diff -q "${DESTDIR}${COMPFILE#.}" "${COMPFILE}" > /dev/null 2>&1; then
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
echo ''

TEST_FOR_FILES=`find ${TEMPROOT} -type f -size +0 2>/dev/null`
if [ -n "${TEST_FOR_FILES}" ]; then
  echo "*** Files that remain for you to merge by hand:"
  find "${TEMPROOT}" -type f -size +0
fi

case "${AUTO_RUN}" in
'')
  echo ''
  echo -n "Do you wish to delete what is left of ${TEMPROOT}? [no] "
  read DEL_TEMPROOT

  case "${DEL_TEMPROOT}" in
  [yY]*)
    if rm -rf "${TEMPROOT}"; then
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

run_it_now () {
  case "${AUTO_RUN}" in
  '')
    unset YES_OR_NO
    echo ''
    echo -n '    Would you like to run it now? [y or n] '
    read YES_OR_NO

    echo ''

    case "${YES_OR_NO}" in
    y)
      echo "      Running ${1}"
      eval "${1}"
      ;;
    *)
      echo "      Make sure to run ${1} yourself"
      ;;
    esac
    ;;
  *) ;;
  esac
}

case "${NEED_MAKEDEV}" in
'') ;;
*)
  echo ''
  echo "*** You installed a new /dev/MAKEDEV script, so make sure that you run"
  echo "    'cd /dev && /bin/sh MAKEDEV all' to rebuild your devices"
  run_it_now 'cd /dev && /bin/sh MAKEDEV all'
  ;;
esac

case "${NEED_NEWALIASES}" in
'') ;;
*)
  echo ''
  echo "*** You installed a new aliases file, so make sure that you run"
  echo "    '/usr/bin/newaliases' to rebuild your aliases database"
  run_it_now '/usr/bin/newaliases'
  ;;
esac

case "${NEED_CAP_MKDB}" in
'') ;;
*)
  echo ''
  echo "*** You installed a login.conf file, so make sure that you run"
  echo "    '/usr/bin/cap_mkdb /etc/login.conf' to rebuild your login.conf database"
  run_it_now '/usr/bin/cap_mkdb /etc/login.conf'
  ;;
esac

case "${NEED_PWD_MKDB}" in
'') ;;
*)
  echo ''
  echo "*** You installed a new master.passwd file, so make sure that you run"
  echo "    '/usr/sbin/pwd_mkdb -p /etc/master.passwd' to rebuild your password files"
  run_it_now '/usr/sbin/pwd_mkdb -p /etc/master.passwd'
  ;;
esac

echo ''

if [ -r "${MM_EXIT_SCRIPT}" ]; then
  . "${MM_EXIT_SCRIPT}"
fi

exit 0

