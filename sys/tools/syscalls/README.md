# System call creation library

## Usage
`main.lua` generates all files.
Files are associated with their respective modules, and modules can be run as 
standalone scripts to generate specific files.

### Examples
**All files:**
`# /usr/libexec/flua /usr/src/sys/tools/syscalls/main.lua /usr/src/sys/kern/syscalls.master`
<br>
**syscalls.h:**
`# /usr/libexec/flua /usr/src/sys/tools/syscalls/modules/syscalls.h /usr/src/sys/kern/syscalls.master`

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
    * `syscalls.lua` - Generates `syscalls.c`.
    * `syscall_h.lua` - Generates `syscall.h`.
    * `syscall_mk.lua` - Generates `syscall.mk`.
    * `init_sysent.lua` - Generates `init_sysent.c`.
    * `systrace_args.lua` - Generates `systrace_args.c`.
    * `sysproto_h.lua` - Generates `sysproto.h`.

  * `tools`
    * `util.lua` - Contains utility functions.
    * `generator.lua` (was `bsdio.lua`) - Handles file generation for the library.
