dnl check if ld supports --version-script
dnl
AC_DEFUN([rk_VERSIONSCRIPT],[
AC_CACHE_CHECK(for ld --version-script, rk_cv_version_script,[
  rk_cv_version_script=no

  cat > conftest.map <<EOF
HEIM_GSS_V1 {
        global: gss*;
};
HEIM_GSS_V1_1 {
        global: gss_init_creds;
} HEIM_GSS_V1;
EOF
cat > conftest.c <<EOF
int gss_init_creds(int foo) { return 0; }
EOF

  if AC_TRY_COMMAND([${CC-cc} -c $CFLAGS -fPIC conftest.c])  && 
     AC_TRY_COMMAND([${CC-cc} -shared -Wl,--version-script,conftest.map $CFLAGS $LDFLAGS -o libconftestlib.so conftest.o]);
  then
    rk_cv_version_script=yes
  fi
rm -rf conftest* libconftest* .libs
])

if test $rk_cv_version_script = yes ; then
  doversioning=yes
  LDFLAGS_VERSION_SCRIPT="-Wl,--version-script,"
else
  doversioning=no
  LDFLAGS_VERSION_SCRIPT=
fi
AC_SUBST(VERSIONING)

AM_CONDITIONAL(versionscript,test $doversioning = yes)
AC_SUBST(LDFLAGS_VERSION_SCRIPT)

])