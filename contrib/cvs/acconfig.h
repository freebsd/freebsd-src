/* Define if you have MIT Kerberos version 4 available.  */
#undef HAVE_KERBEROS

/* Define if you have GSSAPI with MIT Kerberos version 5 available.  */
#undef HAVE_GSSAPI

/* Define if GSS_C_NT_HOSTBASED_SERVICE is defined in the gssapi.h
   header file.  Only relevant when using GSSAPI.  */
#undef HAVE_GSS_C_NT_HOSTBASED_SERVICE

/* Define if you want CVS to be able to be a remote repository client.  */
#undef CLIENT_SUPPORT

/* Define if you want CVS to be able to serve repositories to remote
   clients.  */
#undef SERVER_SUPPORT

/* Define if you want to use the password authenticated server.  */
#undef AUTH_SERVER_SUPPORT

/* Define if you want encryption support.  */
#undef ENCRYPTION

/* Define if you have the connect function.  */
#undef HAVE_CONNECT

/* Define if this system supports chown(), link(), and friends.  */
#undef PRESERVE_PERMISSIONS_SUPPORT

/* Define if you have memchr (always for CVS).  */
#undef HAVE_MEMCHR

/* Define if you have strchr (always for CVS).  */
#undef HAVE_STRCHR

/* Define if utime requires write access to the file (true on Windows,
   but not Unix).  */
#undef UTIME_EXPECTS_WRITABLE

/* Define if setmode is required when writing binary data to stdout.  */
#undef USE_SETMODE_STDOUT

/* Define if the diff library should use setmode for binary files.
   FIXME: Why two different macros for setmode?  */
#undef HAVE_SETMODE

/* Define if you have the crypt function.  */
#undef HAVE_CRYPT

/* Define if you have the getspnam function.  */
#undef HAVE_GETSPNAM

/* Define to force lib/regex.c to use malloc instead of alloca.  */
#undef REGEX_MALLOC

/* Define to force lib/regex.c to define re_comp et al.  */
#undef _REGEX_RE_COMP
