dnl ######################################################################
dnl AC_HOST_MACROS: define HOST_CPU, HOST_VENDOR, and HOST_OS
AC_DEFUN(AMU_HOST_MACROS,
[
# these are defined already by the macro 'CANONICAL_HOST'
  AC_MSG_CHECKING([host cpu])
  AC_DEFINE_UNQUOTED(HOST_CPU, "$host_cpu")
  AC_MSG_RESULT($host_cpu)

  AC_MSG_CHECKING([vendor])
  AC_DEFINE_UNQUOTED(HOST_VENDOR, "$host_vendor")
  AC_MSG_RESULT($host_vendor)

  AC_MSG_CHECKING([host full OS name and version])
  # normalize some host OS names
  case ${host_os} in
	# linux is linux is linux, regardless of RMS.
	linux-gnu* | lignux* )	host_os=linux ;;
  esac
  AC_DEFINE_UNQUOTED(HOST_OS, "$host_os")
  AC_MSG_RESULT($host_os)

# break host_os into host_os_name and host_os_version
  AC_MSG_CHECKING([host OS name])
  host_os_name=`echo $host_os | sed 's/\..*//g'`
  # normalize some OS names
  case ${host_os_name} in
	# linux is linux is linux, regardless of RMS.
	linux-gnu* | lignux* )	host_os_name=linux ;;
  esac
  AC_DEFINE_UNQUOTED(HOST_OS_NAME, "$host_os_name")
  AC_MSG_RESULT($host_os_name)

# parse out the OS version of the host
  AC_MSG_CHECKING([host OS version])
  host_os_version=`echo $host_os | sed 's/^[[^0-9]]*//g'`
  if test -z "$host_os_version"
  then
	host_os_version=`(uname -r) 2>/dev/null` || host_os_version=unknown
  fi
  case ${host_os_version} in
	# fixes for some OS versions (solaris used to be here)
	* ) # do nothing for now
	;;
  esac
  AC_DEFINE_UNQUOTED(HOST_OS_VERSION, "$host_os_version")
  AC_MSG_RESULT($host_os_version)

# figure out host architecture (different than CPU)
  AC_MSG_CHECKING([host OS architecture])
  host_arch=`(uname -m) 2>/dev/null` || host_arch=unknown
  # normalize some names
  case ${host_arch} in
	sun4* )	host_arch=sun4 ;;
	sun3x )	host_arch=sun3 ;;
	sun )	host_arch=`(arch) 2>/dev/null` || host_arch=unknown ;;
	i?86 )	host_arch=i386 ;; # all x86 should show up as i386
  esac
  AC_DEFINE_UNQUOTED(HOST_ARCH, "$host_arch")
  AC_MSG_RESULT($host_arch)

# figure out host name
  AC_MSG_CHECKING([host name])
  host_name=`(hostname || uname -n) 2>/dev/null` || host_name=unknown
  AC_DEFINE_UNQUOTED(HOST_NAME, "$host_name")
  AC_MSG_RESULT($host_name)

# figure out user name
  AC_MSG_CHECKING([user name])
  if test -n "$USER"
  then
    user_name="$USER"
  else
    if test -n "$LOGNAME"
    then
      user_name="$LOGNAME"
    else
      user_name=`(whoami) 2>/dev/null` || user_name=unknown
    fi
  fi
  AC_DEFINE_UNQUOTED(USER_NAME, "$user_name")
  AC_MSG_RESULT($user_name)

# figure out configuration date
  AC_MSG_CHECKING([configuration date])
  config_date=`(date) 2>/dev/null` || config_date=unknown_date
  AC_DEFINE_UNQUOTED(CONFIG_DATE, "$config_date")
  AC_MSG_RESULT($config_date)

])
dnl ======================================================================
