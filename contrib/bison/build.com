$! Set the def dir to proper place for use in batch. Works for interactive too.
$flnm = f$enviroment("PROCEDURE")     ! get current procedure name
$set default 'f$parse(flnm,,,"DEVICE")''f$parse(flnm,,,"DIRECTORY")'
$!
$! This command procedure compiles and links BISON for VMS.
$! BISON has been tested with VAXC version 2.3 and VMS version 4.5
$! and on VMS 4.5 with GCC 1.12.
$!
$! Bj|rn Larsen			blarsen@ifi.uio.no
$! With some contributions by Gabor Karsai, 
$!  KARSAIG1%VUENGVAX.BITNET@jade.berkeley.edu
$! All merged and cleaned by RMS.
$!
$! Adapted for both VAX-11 "C" and VMS/GCC compilation by
$! David L. Kashtan		kashtan.iu.ai.sri.com
$!
$! First we try to sense which C compiler we have available.  Sensing logic
$! borrowed from Emacs.
$!
$set noon		!do not bomb if an error occurs.
$assign nla0: sys$output
$assign nla0: sys$error  !so we do not get an error message about this.
$cc nla0:compiler_check.c
$if $status.eq.%x38090 then goto try_gcc
$ CC :== CC
$ cc_options:="/NOLIST/define=(""index=strchr"",""rindex=strrchr"")"
$ extra_linker_files:="VMSHLP,"
$goto have_compiler
$!
$try_gcc:
$gcc nla0:compiler_check.c
$if $status.eq.%x38090 then goto whoops
$ CC :== GCC
$ cc_options:="/DEBUG"
$ extra_linker_files:="GNU_CC:[000000]GCCLIB/LIB,"
$goto have_compiler
$!
$whoops:
$write sys$output "You must have a C compiler to build BISON.  Sorry."
$deassign sys$output
$deassign sys$error
$exit %x38090
$!
$!
$have_compiler:
$deassign sys$output
$deassign sys$error
$set on
$if f$search("compiler_check.obj").nes."" then dele/nolog compiler_check.obj;
$write sys$output "Building BISON with the ''cc' compiler."
$!
$!	Do the compilation (compiler type is all set up)
$!
$ Compile:
$ if "''p1'" .eqs. "LINK" then goto Link
$ 'CC' 'cc_options' files.c
$ 'CC' 'cc_options' LR0.C
$ 'CC' 'cc_options' ALLOCATE.C
$ 'CC' 'cc_options' CLOSURE.C
$ 'CC' 'cc_options' CONFLICTS.C
$ 'CC' 'cc_options' DERIVES.C
$ 'CC' 'cc_options' VMSGETARGS.C
$ 'CC' 'cc_options' GRAM.C
$ 'CC' 'cc_options' LALR.C
$ 'CC' 'cc_options' LEX.C
$ 'CC' 'cc_options' MAIN.C
$ 'CC' 'cc_options' NULLABLE.C
$ 'CC' 'cc_options' OUTPUT.C
$ 'CC' 'cc_options' PRINT.C
$ 'CC' 'cc_options' READER.C
$ 'CC' 'cc_options' REDUCE.C
$ 'CC' 'cc_options' SYMTAB.C
$ 'CC' 'cc_options' WARSHALL.C
$ 'CC' 'cc_options' VERSION.C
$ if "''CC'" .eqs. "CC" then macro vmshlp.mar
$ Link:
$ link/exec=bison main,LR0,allocate,closure,conflicts,derives,files,-
vmsgetargs,gram,lalr,lex,nullable,output,print,reader,reduce,symtab,warshall,-
version,'extra_linker_files'sys$library:vaxcrtl/lib
$!
$! Generate bison.hlp (for online help).
$!
$runoff bison.rnh
