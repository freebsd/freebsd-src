--------------------------------------------------------------------------------
  Build Instructions for Cocoa NetSurf                         13 January 2011
--------------------------------------------------------------------------------

  This document provides instructions for building the Cocoa version of NetSurf
  and provides guidance on obtaining NetSurf's build dependencies.

  Cocoa NetSurf has been tested on Mac OS X 10.6 on Intel and on Mac OS X 10.5
  on ppc.


  Building NetSurf
==================

  After installing the dependencies NetSurf can be built either using the Xcode
  project file 'cocoa/NetSurf.xcodeproj' or on the command line using the
  Makefile:

     $ make TARGET=cocoa

  In both cases the actual build process is controlled by the Makefile.

  Obtaining NetSurf's build dependencies
========================================

  Many of NetSurf's dependencies are packaged on various operating systems.
  The remainder must be installed manually.  Currently, some of the libraries
  developed as part of the NetSurf project have not had official releases.
  Hopefully they will soon be released with downloadable tarballs and packaged
  in common distros.  For now, you'll have to make do with Git checkouts.

  Package installation
----------------------

  For building the other NetSurf libraries and for configuring NetSurf the
  "pkg-config" tool is required. It can be installed either via fink, macports
  or homebrew or from source.

  OpenSSL, LibPNG, curl, iconv and zlib are provided by Mac OS X.

  The curl library provided by Mac OS X 10.6 causes a crash while fetching
  https resources, so you should install version 7.21.4 (or newer) of libcurl
  if you are running on 10.6.

  LibJPEG and LibMNG can be installed from source or using one of the mentioned
  package managers. 


  The NetSurf project's libraries
---------------------------------

  The NetSurf project has developed several libraries which are required by
  the browser. These are:

  LibParserUtils  --  Parser building utility functions
  LibWapcaplet    --  String internment
  Hubbub          --  HTML5 compliant HTML parser
  LibCSS          --  CSS parser and selection engine
  LibNSGIF        --  GIF format image decoder
  LibNSBMP        --  BMP and ICO format image decoder
  LibROSprite     --  RISC OS Sprite format image decoder

  To fetch each of these libraries, run the appropriate commands from the
  Docs/LIBRARIES file.

      $ make
      $ sudo make install

  This command builds the libraries only for the active architecture. To build
  universal binaries use those commands:

      $ make UNIVERSAL="i386 x86_64 ppc ppc64"
      $ sudo make install

  If you are building NetSurf for using it on only one computer this is not 
  necessary, but if you want to distribute your binary you should build
  universal binaries.  You can also leave some of the platform names out, if
  you don't require them.

  | Note: We advise enabling iconv() support in libparserutils, which vastly
  |       increases the number of supported character sets.  To do this,
  |       create a file called Makefile.config.override in the libparserutils
  |       directory, containing the following line:
  |
  |           CFLAGS += -DWITH_ICONV_FILTER
  |
  |       For more information, consult the libparserutils README file.
