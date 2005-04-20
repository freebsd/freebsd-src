RocketRAID 182x Driver for FreeBSD
Copyright (C) 2003-2004 HighPoint Technologies, Inc. All rights reserved.

#############################################################################
Revision History:

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

2. Build the driver
---------------------
  1) Install kernel source package and building tools. 
  
  2) Extract the driver files to somewhere.

  3) Run "make" to build the driver.

     
3. Using the driver
---------------------
  1) Copy the driver module to /modules/ (FreeBSD 4.x) or /boot/kernel/
     (FreeBSD 5.x).
   	
  2) The driver can't be loaded by kldload command on a running system.
     Please load the driver during system booting stage. e.g:

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
        ok autoboot
  
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
