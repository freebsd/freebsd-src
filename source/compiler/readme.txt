/*
 * Miscellaneous instructions for building and using the iASL compiler.
 */
Last update 9 December 2013.


1) Generating iASL from source
------------------------------

Generation of the ASL compiler from source code requires these items:

    1) The ACPICA source code tree.
    2) An ANSI C compiler.
    3) The Flex (or Lex) lexical analyzer generator.
    4) The Bison (or Yacc) parser generator.

There are three major ACPICA source code components that are required to
generate the compiler (Basically, the entire ACPICA source tree should
be installed):

    1) The ASL compiler source.
    2) The ACPICA Core Subsystem source. In particular, the Namespace
        Manager component is used to create an internal ACPI namespace
        and symbol table, and the AML Interpreter is used to evaluate
        constant expressions.
    3) The "common" source directory that is used for all ACPI components.


1a) Notes for Linux/Unix generation
-----------------------------------

iASL has been generated with these versions of Flex/Bison:

    flex:  Version 2.5.32
    bison: Version 2.6.2

Other required packages:

    make
    gcc C compiler
    m4 (macro processor required by bison)

On Linux/Unix systems, the following commands will build the compiler:

    cd acpica (or cd acpica/generate/unix)
    make clean
    make iasl


1b) Notes for Windows generation
--------------------------------

On Windows, the Visual Studio 2008 project file appears in this directory:

    generate/msvc9/AcpiComponents.sln

The Windows versions of GNU Flex/Bison must be installed, and they must
be installed in a directory that contains no embedded spaces in the
pathname. They cannot be installed in the default "c:\Program Files"
directory. This is a bug in Bison. The default Windows project file for
iASL assumes that these tools are installed at this location:

    c:\GnuWin32

Once the tools are installed, ensure that this path is added to the
default system $Path environment variable:

    c:\GnuWin32\bin

Goto: ControlPanel/System/AdvancedSystemSettings/EnvironmentVariables

Important: Now Windows must be rebooted to make the system aware of
the updated $Path. Otherwise, Bison will not be able to find the M4
interpreter library and will fail.

iASL has been generated with these versions of Flex/Bison for Windows:

    Flex for Windows:  V2.5.4a
    Bison for Windows: V2.4.1

Flex is available at:  http://gnuwin32.sourceforge.net/packages/flex.htm
Bison is available at: http://gnuwin32.sourceforge.net/packages/bison.htm



2) Integration as a custom tool for Visual Studio
-------------------------------------------------

This procedure adds the iASL compiler as a custom tool that can be used
to compile ASL source files. The output is sent to the VC output 
window.

a) Select Tools->Customize.

b) Select the "Tools" tab.

c) Scroll down to the bottom of the "Menu Contents" window. There you
   will see an empty rectangle. Click in the rectangle to enter a 
   name for this tool.

d) Type "iASL Compiler" in the box and hit enter. You can now edit
   the other fields for this new custom tool.

e) Enter the following into the fields:

   Command:             C:\Acpi\iasl.exe
   Arguments:           -vi "$(FilePath)"
   Initial Directory    "$(FileDir)"
   Use Output Window    <Check this option>

   "Command" must be the path to wherever you copied the compiler.
   "-vi" instructs the compiler to produce messages appropriate for VC.
   Quotes around FilePath and FileDir enable spaces in filenames.

f) Select "Close".

These steps will add the compiler to the tools menu as a custom tool.
By enabling "Use Output Window", you can click on error messages in
the output window and the source file and source line will be
automatically displayed by VC. Also, you can use F4 to step through
the messages and the corresponding source line(s).



3) Integrating iASL into a Visual Studio ASL project build
----------------------------------------------------------

This procedure creates a project that compiles ASL files to AML.

a) Create a new, empty project and add your .ASL files to the project

b) For all ASL files in the project, specify a custom build (under
Project/Settings/CustomBuild with the following settings (or similar):

Commands:
    c:\acpi\libraries\iasl.exe -vs -vi "$(InputPath)"

Output:
    $(InputDir)\$(InputPath).aml
