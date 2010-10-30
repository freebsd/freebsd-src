#include <stdio.h>
#include <dlfcn.h>

int bar = -20;

int 
main (void)
{
  int ret = 0;
  void *handle;
  void (*fcn) (void);

  handle = dlopen("./tmpdir/libdl1.so", RTLD_GLOBAL|RTLD_LAZY);
  if (!handle)
    {
      printf("dlopen ./tmpdir/libdl1.so: %s\n", dlerror ());
      return 1;
    }

  fcn = (void (*)(void)) dlsym(handle, "foo");
  if (!fcn)
    {
      printf("dlsym foo: %s\n", dlerror ());
      ret += 1;
    }
  else
    {
      (*fcn) ();
    }

  dlclose (handle);
  return ret;
}
