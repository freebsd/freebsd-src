dnl ######################################################################
dnl Do we have a GNUish getopt
AC_DEFUN(AMU_CHECK_GNU_GETOPT,
[
AC_CACHE_CHECK([for GNU getopt], ac_cv_sys_gnu_getopt, [
AC_TRY_RUN([
#include <stdio.h>
#include <unistd.h>
int main()
{
   int argc = 3;
   char *argv[] = { "actest", "arg", "-x", NULL };
   int c;
   FILE* rf;
   int isGNU = 0;

   rf = fopen("conftestresult", "w");
   if (rf == NULL) exit(1);

   while ( (c = getopt(argc, argv, "x")) != -1 ) {
       switch ( c ) {
          case 'x':
	     isGNU=1;
             break;
          default:
             exit(1);
       }
   }
   fprintf(rf, isGNU ? "yes" : "no");
   exit(0);
}
],[
ac_cv_sys_gnu_getopt="`cat conftestresult`"
],[
AC_MSG_ERROR(could not test for getopt())
])
])
if test "$ac_cv_sys_gnu_getopt" = "yes"
then
    AC_DEFINE(HAVE_GNU_GETOPT)
fi
])
