RocketRAID 182x Driver for FreeBSD
Copyright (C) 2004-2005 HighPoint Technologies, Inc. All rights reserved.
$FreeBSD: src/sys/dev/hptmv/readme.txt,v 1.3.6.1 2008/11/25 02:59:29 kensmith Exp $

#############################################################################
Revision History:

   v1.12 2005-6-10
         Fix over 4G memory support on amd64.
         Fix disk flush problem.

   v1.1  2004-9-23
         Fix activity LED problem.
         Cleanup diagnostic code.

   v1.01 2004-5-24
         First source code release

#############################################################################

1. Overview
---------------------
  This package contains FreeBSD driver source code for HighPoint RocketRAID 
  182x SATA controller.

  NO WARRANTY

  THE DRIVER SOURCE CODE HIGHPOINT PROVIDED IS FREE OF CHARGE, AND THERE IS
  NO WARRANTY FOR THE PROGRAM. THERE ARE NO RESTRICTIONS ON THE USE OF THIS
  FREE SOURCE CODE. HIGHPOINT DOES NOT PROVIDE ANY TECHNICAL SUPPORT IF THE
  CODE HAS BEEN CHANGED FROM ORIGINAL SOURCE CODE.

  LIMITATION OF LIABILITY

  IN NO EVENT WILL HIGHPOINT BE LIABLE FOR DIRECT, INDIRECT, SPECIAL,
  INCIDENTAL, OR CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF OR
  INABILITY TO USE THIS PRODUCT OR DOCUMENTATION, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGES. IN PARTICULAR, HIGHPOINT SHALL NOT HAVE
  LIABILITY FOR ANY HARDWARE, SOFTWARE, OR DATA STORED USED WITH THE
  PRODUCT, INCLUDING THE COSTS OF REPAIRING, REPLACING, OR RECOVERING
  SUCH HARDWARE, OR DATA.


2. Rebuild the kernel with RR182x support
--------------------------------------------

  1) Install kernel source package and building tools. 
  
  2) Extract the driver files under the kernel source tree:

     # cd /usr/src/sys/
     # tar xvzf /your/path/to/rr182x-opensource-v1.12-bsd.tgz

  3) Update the kernel configuration file to include the HighPoint source.
     Assume the configure file is GENERIC, and new kernel configure file is 
     MYKERNEL:

     # cd i386/conf          (or amd64/conf for AMD64)
     # cp GENERIC MYKERNEL

  4) Edit MYKERNEL, and add the following line under "RAID controllers 
     interfaced to the SCSI subsystem":

          device  hptmv   #HighPoint RocketRAID 182x

  5) For i386 system, edit /usr/src/sys/conf/files.i386 and append the lines
     shown below:

          hptmvraid.o optional    hptmv  \
              dependency  "$S/dev/hptmv/i386-elf.raid.o.uu" \
              compile-with    "uudecode < $S/dev/hptmv/i386-elf.raid.o.uu" \
              no-implicit-rule

          dev/hptmv/gui_lib.c     optional        hptmv
          dev/hptmv/hptproc.c     optional        hptmv
          dev/hptmv/ioctl.c       optional        hptmv
          dev/hptmv/entry.c       optional        hptmv
          dev/hptmv/mv.c          optional        hptmv

     For amd64 system, edit /usr/src/sys/conf/files.amd64 and append the lines
     shown below:

          hptmvraid.o optional    hptmv  \
              dependency  "$S/dev/hptmv/amd64-elf.raid.o.uu" \
              compile-with    "uudecode < $S/dev/hptmv/amd64-elf.raid.o.uu" \
              no-implicit-rule

          dev/hptmv/gui_lib.c     optional        hptmv
          dev/hptmv/hptproc.c     optional        hptmv
          dev/hptmv/ioctl.c       optional        hptmv
          dev/hptmv/entry.c       optional        hptmv
          dev/hptmv/mv.c          optional        hptmv

     Note FreeBSD 5.3/5.4 i386 already have a built-in RR182x driver, you should
     replace the old configuration lines with the lines listed above.


  6) Rebuild and install the kernel:

     a) for FreeBSD 5.x-i386:
     
        # cd /usr/src/sys/i386/conf/
        # /usr/sbin/config MYKERNEL
        # cd ../compile/MYKERNEL/
        # make depend
        # make 
        # make install

     b) for FreeBSD 5.x-amd64:

        # cd /usr/src/sys/amd64/conf/
        # /usr/sbin/config MYKERNEL
        # cd ../compile/MYKERNEL/
        # make depend
        # make 
        # make install

     c) for FreeBSD 4.x:
     
        # cd /usr/src/sys/i386/conf/
        # /usr/sbin/config MYKERNEL
        # cd ../../compile/MYKERNEL/
        # make depend
        # make 
        # make install

    If the driver was previously configured as an auto-loaded module by
    /boot/defaults/loader.conf, please remove the entry hptmv_load="YES"
    from loader.conf to prevent the driver from being loaded twice.
    
  7) Reboot from the new kernel.


3. Build/Load the driver as a kernel module
------------------------------------------------

  1) Install kernel source package and building tools. 
  
  2) Extract the driver files under the kernel source tree:
    
     # cd /usr/src/sys/
     # tar xvzf /your/path/to/rr182x-opensource-v1.12-bsd.tgz


  4) Build the driver module:
    
     # cd modules/hptmv
     # make

  5) Copy the driver module to the kernel module directory

     For FreeBSD 4.x:
     
     # cp hptmv.ko /modules/

     For FreeBSD 5.x:
    
     # cp hptmv.ko /boot/kernel/

  6) Reboot and load the driver under loader prompt. e.g:

        BTX loader 1.00  BTX version is 1.01
        Console: internal video/keyboard
        BIOS driver A: is disk0
        BIOS driver C: is disk2
        BIOS 636kB/74512kB available memory
        
        FreeBSD/i386 bootstrap loader, Revision 0.8
        (mailto:jkh@narf.osd.bsdi.com, Sat Apr 21 08:46:19 GMT 2001)
        Loading /boot/defaults/loader.conf
        /kernel text=0x24f1db data=0x3007ec+0x2062c -
        
        Hit [Enter] to boot immediagely, or any other key for command prompt.
        Booting [kernel] in 9 seconds¡­
        
         <-- press SPACE key here 
        Type '?' for a list of commands, 'help' for more detailed help.
        ok load hptmv
        /modules/hptmv.ko text=0xf571 data=0x2c8+0x254
        ok boot
        
     For FreeBSD 5.x, you can select 6 on the boot menu to get a loader prompt.
  
  7) You can add a below line into /boot/defaults/loader.conf to load the
     driver automatically:
    
           hptmv_load="YES"
    
     Please refer to the installation guide in HighPoint FreeBSD driver release 
     package for more information.
     

#############################################################################
Technical support and service

  If you have questions about installing or using your HighPoint product,
  check the user's guide or readme file first, and you will find answers to
  most of your questions here. If you need further assistance, please
  contact us. We offer the following support and information services:

  1)  The HighPoint Web Site provides information on software upgrades,
      answers to common questions, and other topics. The Web Site is
      available from Internet 24 hours a day, 7 days a week, at
      http://www.highpoint-tech.com.

  2)  For technical support, send e-mail to support@highpoint-tech.com

  NOTE: Before you send an e-mail, please visit our Web Site
        (http://www.highpoint-tech.com) to check if there is a new or 
        updated device driver for your system.
