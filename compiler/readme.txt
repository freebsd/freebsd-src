
Instructions for integrating iASL compiler into MS VC++ 6.0 environment.

Part 1.  Integration as a custom tool

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


Part 2.  Integration into a project build

This procedure creates a project that compiles ASL files to AML.

a) Create a new, empty project and add your .ASL files to the project

b) For all ASL files in the project, specify a custom build (under
Project/Settings/CustomBuild with the following settings (or similar):

Commands:
c:\acpi\libraries\iasl.exe -vs -vi "$(InputPath)"

Output:
$(InputDir)\$(InputPath).aml



Compiler Generation From Source

Generation of the ASL compiler from source code requires these items:


Required Tools
1) The Flex (or Lex) lexical analyzer generator.
2) The Bison (or Yacc) parser generator.
3) An ANSI C compiler.


Required Source Code.

There are three major source code components that are required to 
generate the compiler:

1) The ASL compiler source.
2) The ACPI CA Core Subsystem source.  In particular, the Namespace Manager
     component is used to create an internal ACPI namespace and symbol table,
     and the AML Interpreter is used to evaluate constant expressions.
3) The Common source for all ACPI components.







