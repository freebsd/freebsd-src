/* This should be replaced by whatever namespace-clean
   version of strerror you have available. */

extern char *strerror();

char *
_IO_strerror(errnum)
     int errnum;
{
  return strerror(errnum);
}
