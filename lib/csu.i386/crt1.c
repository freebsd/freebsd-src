typedef void (*func_ptr) (void);

func_ptr __CTOR_LIST__[2];
func_ptr __DTOR_LIST__[2];

/* Run all the global destructors on exit from the program.  */
 
static void
__do_global_dtors ()
{
	unsigned nptrs = (unsigned long) __DTOR_LIST__[0];
	unsigned i;
 
  /* Some systems place the number of pointers
     in the first word of the table.
     On other systems, that word is -1.
     In all cases, the table is null-terminated.  */
 
  /* If the length is not recorded, count up to the null.  */
  if (nptrs == -1)
    for (nptrs = 0; __DTOR_LIST__[nptrs + 1] != 0; nptrs++);
 
  /* GNU LD format.  */
  for (i = nptrs; i >= 1; i--)
    __DTOR_LIST__[i] ();

}

static void
__do_global_ctors ()
{
	func_ptr *p;

	for (p = __CTOR_LIST__ + 1; *p; )
		(*p++)();
	atexit (__do_global_dtors);
}

__init()
{
	static int initialized = 0;
	if (! initialized) {
		initialized = 1;
		__do_global_ctors ();
	}

}
