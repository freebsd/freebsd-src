$!
$! This file sets things up to build gas on a VMS system to generate object
$! files for a VMS system.  We do not use the configure script, since we
$! do not have /bin/sh to execute it.
$!
$! If you are running this file, then obviously the host is vax-dec-vms.
$!
$gas_host="vms"
$!
$cpu_type="vax"
$emulation="generic"
$obj_format="vms"
$atof="vax"
$!
$! host specific information
$call link host.h	[.config]ho-'gas_host'.h
$!
$! Target specific information
$call link targ-cpu.c	[.config]tc-'cpu_type'.c
$call link targ-cpu.h	[.config]tc-'cpu_type'.h
$call link targ-env.h	[.config]te-'emulation'.h
$!
$! Code to handle the object file format.
$call link obj-format.h	[.config]obj-'obj_format'.h
$call link obj-format.c	[.config]obj-'obj_format'.c
$!
$! Code to handle floating point.
$call link atof-targ.c	[.config]atof-'atof'.c
$!
$!
$! Create the file version.opt, which helps identify the executalbe.
$!
$search version.c version_string,"="/match=and/output=t.tmp
$open ifile$ t.tmp
$read ifile$ line
$close ifile$
$delete/nolog t.tmp;
$ijk=f$locate("""",line)+1
$line=f$extract(ijk,f$length(line)-ijk,line)
$ijk=f$locate("""",line)
$line=f$extract(0,ijk,line)
$ijk=f$locate("\n",line)
$line=f$extract(0,ijk,line)
$!
$i=0
$loop:
$elm=f$element(i," ",line)
$if elm.eqs."" then goto no_ident
$if (elm.les."9").and.(elm.ges."0") then goto write_ident
$i=i+1
$goto loop
$!
$no_ident:
$elm="?.??"
$!
$!
$write_ident:
$open ifile$ version.opt/write
$write ifile$ "ident="+""""+elm+""""
$close ifile$
$!
$ !
$ if f$search("config.status") .nes. "" then delete config.status.*
$ open/write file config.status
$ write file "Links are now set up for use with a vax running VMS."
$ close file
$ type config.status
$exit
$!
$!
$link:
$subroutine
$if f$search(p1).nes."" then delete/nolog 'p1';
$copy 'p2' 'p1'
$write sys$output "Linked ''p2' to ''p1'."
$endsubroutine
