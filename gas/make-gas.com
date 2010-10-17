$!make-gas.com
$! Set the def dir to proper place for use in batch. Works for interactive to.
$flnm = f$enviroment("PROCEDURE")     ! get current procedure name
$set default 'f$parse(flnm,,,"DEVICE")''f$parse(flnm,,,"DIRECTORY")'
$v = 'f$verify(0)'
$!
$!	Command file to build a GNU assembler on VMS
$!
$!	If you are using a version of GCC that supports global constants
$!	you should remove the define="const=" from the gcc lines.
$!
$!	Caution:  Versions 1.38.1 and earlier had a bug in the handling of
$!	some static constants. If you are using such a version of the
$!	assembler, and you wish to compile without the "const=" hack,
$!	you should first build this version *with* the "const="
$!	definition, and then use that assembler to rebuild it without the
$!	"const=" definition.  Failure to do this will result in an assembler
$!	that will mung floating point constants.
$!
$!	Note: The version of gas shipped on the GCC VMS tapes has been patched
$!	to fix the above mentioned bug.
$!
$	!The gcc-vms driver was modified to use `-1' quite some time ago,
$	!so don't echo this text any more...
$ !write sys$output "If this assembler is going to be used with GCC 1.n, you"
$ !write sys$output "need to modify the driver to supply the -1 switch to gas."
$ !write sys$output "This is required because of a small change in how global"
$ !write sys$output "constant variables are handled.  Failure to include this"
$ !write sys$output "will result in linker warning messages about mismatched
$ !write sys$output "psect attributes."
$!
$ gas_host="vms"
$ arch_indx = 1 + ((f$getsyi("CPU").ge.128).and.1)	! vax==1, alpha==2
$ arch = f$element(arch_indx,"|","|VAX|Alpha|")
$ if arch.eqs."VAX"
$ then
$  cpu_type="vax"
$  obj_format="vms"
$  atof="vax"
$ else
$  cpu_type="alpha"
$  obj_format="evax"
$  atof="ieee"
$ endif
$ emulation="generic"
$!
$	COPY	= "copy/noLog"
$!
$ C_DEFS :="""VMS"""
$! C_DEFS :="""VMS""","""const="""
$ C_INCLUDES	= "/Include=([],[.config],[-.include],[-.include.aout])"
$ C_FLAGS	= "/noVerbose/Debug" + c_includes
$!
$!
$ on error then  goto bail
$ if f$search("[-.libiberty]liberty.olb").eqs.""
$ then	@[-.libiberty]vmsbuild.com
$	write sys$output "Now building gas."
$ endif
$ if "''p1'" .eqs. "LINK" then goto Link
$!
$!  This helps gcc 1.nn find the aout/* files.
$!
$ aout_dev = f$parse(flnm,,,"DEVICE")
$ tmp = aout_dev - ":"
$if f$trnlnm(tmp).nes."" then aout_dev = f$trnlnm(tmp)
$ aout_dir = aout_dev+f$parse(flnm,,,"DIRECTORY")' -
	- "GAS]" + "INCLUDE.AOUT.]" - "]["
$assign 'aout_dir' aout/tran=conc
$ opcode_dir = aout_dev+f$parse(flnm,,,"DIRECTORY")' -
	- "GAS]" + "INCLUDE.OPCODE.]" - "]["
$assign 'opcode_dir' opcode/tran=conc
$!
$ set verify
$!
$ gcc 'c_flags'/Define=('C_DEFS')/Object=[]tc-'cpu_type'.obj [.config]tc-'cpu_type'.c
$ gcc 'c_flags'/Define=('C_DEFS')/Object=[]obj-'obj_format'.obj [.config]obj-'obj_format'.c
$ gcc 'c_flags'/Define=('C_DEFS')/Object=[]atof-'atof'.obj [.config]atof-'atof'.c
$ gcc 'c_flags'/Define=('C_DEFS') app.c
$ gcc 'c_flags'/Define=('C_DEFS') as.c
$ gcc 'c_flags'/Define=('C_DEFS') atof-generic.c
$ gcc 'c_flags'/Define=('C_DEFS') bignum-copy.c
$ gcc 'c_flags'/Define=('C_DEFS') cond.c
$ gcc 'c_flags'/Define=('C_DEFS') depend.c
$ gcc 'c_flags'/Define=('C_DEFS') dwarf2dbg.c
$ gcc 'c_flags'/Define=('C_DEFS') dw2gencfi.c
$ gcc 'c_flags'/Define=('C_DEFS') ehopt.c
$ gcc 'c_flags'/Define=('C_DEFS') expr.c
$ gcc 'c_flags'/Define=('C_DEFS') flonum-konst.c
$ gcc 'c_flags'/Define=('C_DEFS') flonum-copy.c
$ gcc 'c_flags'/Define=('C_DEFS') flonum-mult.c
$ gcc 'c_flags'/Define=('C_DEFS') frags.c
$ gcc 'c_flags'/Define=('C_DEFS') hash.c
$ gcc 'c_flags'/Define=('C_DEFS') input-file.c
$ gcc 'c_flags'/Define=('C_DEFS') input-scrub.c
$ gcc 'c_flags'/Define=('C_DEFS') literal.c
$ gcc 'c_flags'/Define=('C_DEFS') messages.c
$ gcc 'c_flags'/Define=('C_DEFS') output-file.c
$ gcc 'c_flags'/Define=('C_DEFS') read.c
$ gcc 'c_flags'/Define=('C_DEFS') subsegs.c
$ gcc 'c_flags'/Define=('C_DEFS') symbols.c
$ gcc 'c_flags'/Define=('C_DEFS') write.c
$ gcc 'c_flags'/Define=('C_DEFS') listing.c
$ gcc 'c_flags'/Define=('C_DEFS') ecoff.c
$ gcc 'c_flags'/Define=('C_DEFS') stabs.c
$ gcc 'c_flags'/Define=('C_DEFS') sb.c
$ gcc 'c_flags'/Define=('C_DEFS') macro.c
$link:
$!'f$verify(0)'
$ if f$trnlnm("IFILE$").nes."" then  close/noLog ifile$
$ create gcc-as.opt
!
!	Linker options file for GNU assembler
!
$ open/Append ifile$ gcc-as.opt
$ write ifile$ "tc-''cpu_type'.obj"
$ write ifile$ "obj-''obj_format'.obj"
$ write ifile$ "atof-''atof'.obj"
$ COPY sys$input: ifile$:
app.obj,-
as.obj,-
atof-generic.obj,-
bignum-copy.obj,-
cond.obj,-
depend.obj,-
dwarf2dbg.obj,-
dw2gencfi.obj,-
ehopt.obj,-
expr.obj,-
flonum-konst.obj,-
flonum-copy.obj,-
flonum-mult.obj,-
frags.obj,-
hash.obj,-
input-file.obj,-
input-scrub.obj,-
literal.obj,-
messages.obj,-
output-file.obj,-
read.obj,-
subsegs.obj,-
symbols.obj,-
write.obj,-
listing.obj,-
ecoff.obj,-
stabs.obj,-
sb.obj,-
macro.obj,-
[-.libiberty]liberty.olb/Lib
gnu_cc:[000000]gcclib.olb/Lib,sys$library:vaxcrtl.olb/Lib
! Tell linker exactly what psect attributes we want -- match VAXCRTL.
psect_attr=ENVIRON,long,pic,ovr,rel,gbl,noshr,noexe,rd,wrt
$ close ifile$
$ set verify=(Proc,noImag)
$ link/noMap/Exec=gcc-as.exe gcc-as.opt/Opt,version.opt/Opt
$!
$bail: exit $status + 0*f$verify(v)	!'f$verify(0)'
