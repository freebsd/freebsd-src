Changes in version 0.4
======================

Released on 2013/12/07.

* Cope with the lack of AM_PROG_AR in configure.ac, which first
  appeared in Automake 1.11.2.  Fixes a problem in Ubuntu 10.04
  LTS, which appears stuck in 1.11.1.

* Stopped shipping an Atffile.  The only supported way to run the tests
  is via Kyua.

Interface changes:

* Issue 5: New methods added to the state class: open_all.

* Removed default parameter values from all state methods and all
  standalone operations.  It is often unclear what the default value is
  given that it depends on the specific Lua operation.  Being explicit
  on the caller side is clearer.

* Modified operations do_file and do_string to support passing a number
  of arguments to the loaded chunks and an error handler to the backing
  pcall call.


Changes in version 0.3
======================

Released on 2013/06/14.

* Issue 1: Added support for Lua 5.2 while maintaining support for Lua
  5.1.  Applications using Lutok can be modified to use the new
  interface in this new version and thus support both Lua releases.
  However, because of incompatible changes to the Lua API, this release
  of Lutok is incompatible with previous releases as well.

* Issue 3: Tweaked configure to look for Lua using the pkg-config names
  lua-5.2 and lua-5.1.  These are the names used by FreeBSD.

Interface changes:

* New global constants: registry_index.

* New methods added to the state class: get_global_table.

* Removed global constants: globals_index.


Changes in version 0.2
======================

Released on 2012/05/30.

* New global constants: globals_index.

* New methods added to the state class: get_metafield, get_metatable,
  insert, push_value, raw_get and raw_set.

* Acknowledged that Lua 5.2 is currently not supported.


Changes in version 0.1
======================

Released on 2012/01/29.

* This is the first public release of the Lutok package.
