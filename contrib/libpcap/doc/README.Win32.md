Building libpcap on Windows with Visual Studio
==============================================

Unlike the UN*Xes on which libpcap can capture network traffic, Windows
has no network traffic capture mechanism that libpcap can use.
Therefore, libpcap requires a driver, and a library to access the
driver, provided by the Npcap or WinPcap projects.

Those projects include versions of libpcap built to use that driver and
library; these instructions are for people who want to build libpcap
source releases, or libpcap from the Git repository, as a replacement
for the version provided with Npcap or WinPcap.

Npcap and WinPcap SDK
---------------------

In order to build libpcap, you will need to download Npcap and its
software development kit (SDK) or WinPcap and its software development
kit.

Npcap is currently being developed and maintained, and offers many
additional capabilities that WinPcap does not.

WinPcap is no longer being developed or maintained; it should be used
only if there is some other requirement to use it rather than Npcap,
such as a requirement to support versions of Windows earlier than
Windows Vista, which is the earliest version supported by Npcap.

Npcap and its SDK can be downloaded from its home page:

  https://npcap.com

The SDK is a ZIP archive; create a folder on your C: drive, e.g.
C:\npcap-sdk, and put the contents of the ZIP archive into that folder.

The WinPcap installer can be downloaded from

  https://www.winpcap.org/install/default.htm

and the WinPcap Developer's Kit can be downloaded from

  https://www.winpcap.org/devel.htm

Required build tools
--------------------

The Developer's Kit is a ZIP archive; it contains a folder named
WpdPack, which you should place on your C: drive, e.g. C:\WpdPack.

Building libpcap on Windows requires Visual Studio 2015 or later.  The
Community Edition of Visual Studio can be downloaded at no cost from

  https://visualstudio.microsoft.com

Additional tools are also required.  Chocolatey is a package manager for
Windows with which those tools, and other tools, can be installed; it
can be downloaded from

  https://chocolatey.org

It is a command-line tool; a GUI tool, Chocolatey GUI, is provided as a
Chocolatey package, which can be installed from the command line:

	choco install chocolateygui

For convenience, the "choco install" command can be run with the "-y"
flag, forcing it to automatically answer all questions asked of the user
with "yes":

	choco install -y chocolateygui

The required tools are:

### CMake ###

libpcap does not provide supported project files for Visual Studio
(there are currently unsupported project files provided, but we do not
guarantee that they will work or that we will continue to provide them).
It does provide files for CMake, which is a cross-platform tool that
runs on UN*Xes and on Windows and that can generate project files for
UN*X Make, the Ninja build system, and Visual Studio, among other build
systems.

Visual Studio 2015 does not provide CMake; an installer can be
downloaded from

  https://cmake.org/download/

When you run the installer, you should choose to add CMake to the system
PATH for all users and to create the desktop icon.

CMake can also be installed as the Chocolatey package "cmake":

	choco install -y cmake

Visual Studio 2017 and later provide CMake, so you will not need to
install CMake if you have installed Visual Studio 2017 or later.  They
include built-in support for CMake-based projects:

  https://devblogs.microsoft.com/cppblog/cmake-support-in-visual-studio/

For Visual Studio 2017, make sure "Visual C++ tools for CMake" is
installed; for Visual Studio 2019, make sure "C++ CMake tools for
Windows" is installed.

### winflexbison ###

libpcap uses the Flex lexical-analyzer generator and the Bison or
Berkeley YACC parser generators to generate the parser for filter
expressions.  Windows versions of Flex and Bison can be downloaded from

  https://sourceforge.net/projects/winflexbison/

The downloaded file is a ZIP archive; create a folder on your C: drive,
e.g. C:\Program Files\winflexbison, and put the contents of the ZIP
archive into that folder.  Then add that folder to the system PATH
environment variable.

Git
---

An optional tool, required only if you will be building from a Git
repository rather than from a release source tarball, is Git.  Git is
provided as an optional installation component, "Git for Windows", with
Visual Studio 2017 and later.

Building from the Visual Studio GUI
-----------------------------------

### Visual Studio 2017 ###

Open the folder containing the libpcap source with Open > Folder.
Visual Studio will run CMake; however, you will need to indicate where
the Npcap or WinPcap SDK is installed.

To do this, go to Project > "Change CMake Settings" > pcap and:

Choose which configuration type to build, if you don't want the default
Debug build.

In the CMakeSettings.json tab, change cmakeCommandArgs to include

	-DPacket_ROOT={path-to-sdk}

where {path-to-sdk} is the path of the directory containing the Npcap or
WinPcap SDK.  Note that backslashes in the path must be specified as two
backslashes.

Save the configuration changes with File > "Save CMakeSettings.json" or
with control-S.

Visual Studio will then re-run CMake.  If that completes without errors,
you can build with CMake > "Build All".

### Visual Studio 2019 ###

Open the folder containing the libpcap source with Open > Folder.
Visual Studio will run CMake; however, you will need to indicate where
the Npcap or WinPcap SDK is installed.

To do this, go to Project > "CMake Settings for pcap" and:

Choose which configuration type to build, if you don't want the default
Debug build.

Scroll down to "Cmake variables and cache", scroll through the list
looking for the entry for Packet_ROOT, and either type in the path of
the directory containing the Npcap or WinPcap SDK or use the "Browse..."
button to browse for that directory.

Save the configuration changes with File > "Save CMakeSettings.json" or
with control-S.

Visual Studio will then re-run CMake.  If that completes without errors,
you can build with Build > "Build All".

Building from the command line
------------------------------

Start the appropriate Native Tools command line prompt.

Change to the directory into which you want to build libpcap, possibly
after creating it first.  One choice is to create it as a subdirectory
of the libpcap source directory.

Run the command

	cmake "-DPacket_ROOT={path-to-sdk}" {path-to-libpcap-source}

where {path-to-sdk} is the path of the directory containing the Npcap or
WinPcap SDK and {path-to-libpcap-source} is the pathname of the
top-level source directory for libpcap.

Run the command

	msbuild/m pcap.sln

to compile libpcap.
