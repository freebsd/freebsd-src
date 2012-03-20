
Instructions for integrating iASL compiler into MS VC++ environment.

1a) Integration as a custom tool
-------------------------------

This procedure adds the iASL compiler as a custom tool that can be used
to compile ASL source files.  The output is sent to the VC output 
window.

a) Select Tools->Customize.

b) Select the "Tools" tab.

c) Scroll down to the bottom of the "Menu Contents" window.  There you
   will see an empty rectangle.  Click in the rectangle to enter a 
   name for this tool.

d) Type "iASL Compiler" in the box and hit enter.  You can now edit
   the other fields for this new custom tool.

e) Enter the following into the fields:

   Command:             C:\Acpi\iasl.exe
   Arguments:           -e "$(FilePath)"
   Initial Directory    "$(FileDir)"
   Use Output Window    <Check this option>

   "Command" must be the path to wherever you copied the compiler.
   "-e" instructs the compiler to produce messages appropriate for VC.
   Quotes around FilePath and FileDir enable spaces in filenames.

f) Select "Close".

These steps will add the compiler to the tools menu as a custom tool.
By enabling "Use Output Window", you can click on error messages in
the output window and the source file and source line will be
automatically displayed by VC.  Also, you can use F4 to step through
the messages and the corresponding source line(s).


1b) Integration into a project build
------------------------------------

This procedure creates a project that compiles ASL files to AML.

a) Create a new, empty project and add your .ASL files to the project

b) For all ASL files in the project, specify a custom build (under
Project/Settings/CustomBuild with the following settings (or similar):

Commands:
c:\acpi\libraries\iasl.exe -vs -vi "$(InputPath)"

Output:
$(InputDir)\$(InputPath).aml



2) Compiler Generation From Source
-------------------------------

Generation of the ASL compiler from source code requires these items:


2a) Required Tools
--------------

1) The Flex (or Lex) lexical analyzer generator.
2) The Bison (or Yacc) parser generator.
3) An ANSI C compiler.


Windows GNU Flex and GNU Bison Notes:

GNU Flex/Bison must be installed in a directory that has no embedded
spaces in the name. They cannot be installed in the default
c:\"Program Files" directory. This is a bug in Bison. The default
Windows project file for iASL assumes that these tools are
installed at c:\GnuWin32.

When installed, ensure that c:\GnuWin32\bin is added to the default
system $PATH environment variable.

iASL has been generated with these versions on Windows:

    Flex for Windows:  V2.5.4
    Bison for Windows: V2.4.1


Flex is available at:  http://gnuwin32.sourceforge.net/packages/flex.htm
Bison is available at: http://gnuwin32.sourceforge.net/packages/bison.htm


2b) Required Source Code
--------------------

There are three major source code components that are required to 
generate the compiler:

1) The ASL compiler source.
2) The ACPICA Core Subsystem source.  In particular, the Namespace Manager
     component is used to create an internal ACPI namespace and symbol table,
     and the AML Interpreter is used to evaluate constant expressions.
3) The Common source for all ACPI components.







