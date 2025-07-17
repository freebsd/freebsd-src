# System call creation library
Parses `syscalls.master` and packages information into objects with methods.
Modules reproduce the previously monolithic file auto-generation.

We generally assume that this script will be run by flua, however we've
carefully crafted modules for it that mimic interfaces provided by modules
available in ports.  Currently, this script is compatible with lua from
ports along with the compatible luafilesystem and lua-posix modules.

## Usage
`main.lua` generates all files.
Files are associated with their respective modules, and modules can be run as
standalone scripts to generate specific files.

### Examples
**All files:**
`# /usr/libexec/flua /usr/src/sys/tools/syscalls/main.lua /usr/src/sys/kern/syscalls.master`
<br>
**syscalls.h:**
`# /usr/libexec/flua /usr/src/sys/tools/syscalls/scripts/syscalls.h /usr/src/sys/kern/syscalls.master`

## Organization
* `root`
  * `main.lua` - Main entry point that calls all scripts.
  * `config.lua` - Contains the global configuration table and associated
                   configuration functions.

  * `core` (Core Classes)
    * `syscall.lua` - Packages each system call entry from `syscalls.master`
                      into a system call object.
    * `scarg.lua` - Packages each argument for the system call into an argument
                    object.
    * `scret.lua` - An object for the return value of the system call.
    * `freebsd-syscall.lua` - Contains the master system call table after
                              processing.

  * `scripts`
    * `init_sysent.lua` - Generates `init_sysent.c`.
    * `libsys_h.lua` - Generates `lib/libsys/_libsys.h`.
    * `syscall_h.lua` - Generates `syscall.h`.
    * `syscall_mk.lua` - Generates `syscall.mk`.
    * `syscalls.lua` - Generates `syscalls.c`.
    * `syscalls_map.lua` - Generates `lib/libsys/syscalls.map`.
    * `sysproto_h.lua` - Generates `sysproto.h`.
    * `systrace_args.lua` - Generates `systrace_args.c`.

  * `tools`
    * `util.lua` - Contains utility functions.
    * `generator.lua` - Handles file generation for the library.
