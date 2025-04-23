# StackCheckLib Overview

## Table of Contents

- [StackCheckLib](#stackchecklib)
  - [Table of Contents](#table-of-contents)
  - [Introduction and Library Instances](#introduction-and-library-instances)
    - [StackCheckLib](#stackchecklib)
    - [DynamicStackCookieEntryPointLib](#dynamicstackcookieentrypointlib)
    - [StackCheckLibNull](#stackchecklibnull)
  - [How Failures are Handled](#how-failures-are-handled)
  - [Debugging Stack Cookie Check Failures](#debugging-stack-cookie-check-failures)
  - [Usage](#usage)

## Introduction and Library Instances

`StackCheckLib` contains the required functionality for initializing the stack cookie
value (based on a randomly generated value during build time), checking the value,
and triggering an interrupt when a mismatch occurs. The stack cookie is a random value
placed on the stack between the stack variables and the return address so that
continuously writing past the stack variables will cause the stack cookie to be
overwritten. Before the function returns, the stack cookie value will be checked and
if there is a mismatch then `StackCheckLib` handles the failure.

Because UEFI doesn't use the C runtime libraries provided by MSVC, the stack
check code is written in assembly within this library. GCC and Clang compilers
have built-in support for stack cookie checking, so this library only handles failures.

The stack cookie value is initialized at compile time via updates to the AutoGen process.
Each module will define `STACK_COOKIE_VALUE` which is used for the module stack cookie
value.

The entry point libraries under `MdePkg/DynamicStackCookieEntryPointLib/` update the stack
cookie value at runtime for improved security, but there are cases where the stack cookie
global cannot be written to such as in execute-in-place (XIP) modules and during the
temporary RAM phase of the boot process. It is always preferable to use
one of the dynamic stack cookie entry points when possible.

### StackCheckLib

`StackCheckLib` provides the stack cookie checking functionality per architecture and
toolchain. The currently supported pairs are IA32{GCC,MSVC}, X64{GCC, MSVC},
ARM{GCC, MSVC}, and AARCH64{GCC, MSVC}. `StackCheckLib` is agnostic as to
whether the stack cookie was updated during build time or run time; it simply
checks the cookie in the MSVC case and in both GCC and MSVC responds to stack
cookie checking failures.

To add support for other architectures/toolchains, additional assembly files
should be added to `StackCheckLib.inf` and scoped to that architecture/toolchain.

Note: Stack cookie failures generate exceptions and SEC and PEI_CORE may not have
exception handlers registered. In order to safely use stack cookie checking in
these phases, a platform should implement exception handlers because unhandled
exceptions may lead to a hung system state. If a platform does not implement
exception handlers in SEC and PEI_CORE, it is recommended to use `StackCheckLibNull`
for these phases, except for development purposes.

### DynamicStackCookieEntryPointLib

Each EntryPoint lib under `MdePkg/DynamicStackCookieEntryPointLib/` is an instance of
that module type entry point lib which updates the stack cookie value for the module at
runtime. This is the preferred method for stack cookie initialization as it provides
improved security. The stack cookie value is initialized at runtime in `_ModuleEntryPoint`
by calling `rdrand` on x86 and `RNDR` on AARCH64. If the random number generator is not
supported on that platform or otherwise returns an error, then the value will still have
the build-time randomized value to fall back on.

Typically, dynamic cookies cannot be used for SEC, PEI_CORE, and PEIM modules, due to
the lack of the ability to write to globals for many architectures. If a given platform
can write to globals during these phases, it is recommended to use the provided dynamic
stack cookie entry point lib for those types. Note that SEC does not have a universal
entry point, so there is no dynamic stack cookie entry point lib there.

The dynamic stack cookie entry point lib is used in place of the standard entry point lib,
e.g. for UefiDriverEntryPoint to have dynamic stack cookies, a platform would remove
MdePkg/Library/UefiDriverEntryPoint/UefiDriverEntryPoint.inf from its DSC and instead
include MdePkg/Library/DynamicStackCookieEntryPointLib/UefiDriverEntryPoint.inf.

See the Usage section for other ways of including these libraries.

Note: Standalone MM Core support for dynamic cookies for AARCH64 is currently not
supported, due to the unique entry point mechanism there. This support will be
added at a future date.

### StackCheckLibNull

`StackCheckLibNull` is an instance of `StackCheckLib` which does not perform any stack
cookie checks. This is useful for modules which will fail if stack cookie checks are
inserted. Of course, this is not recommended for production code outside of
SEC and PEI_CORE.

## How Failures are Handled

When a stack cookie check fails, the `StackCheckLib` library will first call into a hook
function `StackCheckFailureHook()` which only has a NULL implementation in edk2.
The NULL implementation will simply print the failure address and return, but a platform
can implement their own instance of this library which can perform additional actions
before the system triggers an interrupt.

After `StackCheckFailureHook()` returns, the library will trigger an interrupt with
PcdStackCookieExceptionVector.

- On IA32 and X64 platforms, PcdStackCookieExceptionVector is used as an index into the
Interrupt Descriptor Table.
- On ARM platforms, a software interrupt (`SWI`) is called with the value of
PcdStackCookieExceptionVector. The value can be retrieved by the handler by reading
bits [7:0] of the instruction opcode which will allow the handler to determine if the
interrupt was triggered by the stack cookie check. Reference:
[Arm A64 Instruction Set Architecture Version 2024-3](https://developer.arm.com/documentation/ddi0597/2024-03/Base-Instructions/SVC--Supervisor-Call-?lang=en)
- On AARCH64 platforms, a supervisor call (`SVC`) is called with the value
of PcdStackCookieExceptionVector. This value can similarly be retrieved by the
handler to determine if the interrupt was triggered by the stack cookie check. Reference:
[Arm A64 Instruction Set Architecture Version 2024-3](https://developer.arm.com/documentation/ddi0602/2024-03/Base-Instructions/SVC--Supervisor-Call-?lang=en)

## Debugging Stack Cookie Check Failures

Tracking down the origin of stack cookie failures can be difficult. Programmers may attempt
printf debugging to determine which function has an overflow only to find that the failure
disappears on the next boot. This curiosity is usually due to the black-box heuristic used
by compilers to determine where to put stack cookie checks or compiler optimization features
removing the failing check. The address where the failed stack cookie check occurred will
be printed using DebugLib. If .map files are available, the address combined with the image
offset can be used to determine the function which failed.

GNU-based compilers have the `-fstack-protector-all` flag to force stack cookie checks on
all functions which could create a more consistent environment for debugging assuming an
earlier failure doesn't mask the targeted one and the flash space can accommodate the
increased size.

The Visual Studio (MSVC) toolchain has the ability to generate `.cod` files during compilation
which interleave C and the generated assembly code. These files will contain the stack cookie
checks and are useful for determining where the checks are placed. To generate these files,
append `/FAcs` to the build options for each target module. The easiest way to do this is to
update the tools_def file so the `<TARGET>_<TOOLCHAIN>_<ARCH>_CC_FLAGS` includes `/FAcs`.

## Usage

edk2 updated the tools_def to add `/GS` to VS2022 and VS2019 IA32/X64 builds and
`-fstack-protector` to GCC builds. This will cause stack cookie references to be inserted
throughout the code. Every module should have a `StackCheckLib` instance linked to satisfy
these references. So every module doesn't need to add `StackCheckLib` to the LibraryClasses
section of the INF file, `StackCheckLib` is added as a dependency for each entry point lib.
This means that custom entry point libs need to have StackCheckLib added as a dependency.
The only exception to this is MSVC built host-based unit tests as they will be
compiled with the runtime libraries which already contain the stack cookie definitions
and will collide with `StackCheckLib`. A `StackCheckLibHostApplication.inf` is linked
by `UnitTestFrameworkPkg/UnitTestFrameworkPkgHost.dsc.inc` that provides the stack
cookie functions for GCC HOST_APPLICATIONS but not for MSVC HOST_APPLICATIONS.

### Default Stack Check Library Configuration

`MdePkg/MdeLibs.dsc.inc` links `StackCheckLibNull` and `StackCheckFailureLibNull` for all
types.

As stated above, all HOST_APPLICATIONS will link against a HOST_APPLICATION specific
implementation provided in `UnitTestFrameworkPkg/UnitTestFrameworkPkgHost.dsc.inc`.

### Custom Stack Check Library Configuration

In order to use a different instance of StackCheckLib than `MdeLibs.dsc.inc` provides, a DSC
should add one of the following:

#### Static Stack Check Cookie Configuration

```inf
[Defines]
  DEFINE CUSTOM_STACK_CHECK_LIB = STATIC
```

This will cause `MdeLibs.dsc.inc` to not link `StackCheckLibNull` and instead link
`StackCheckLib` to perform stack cookie checking on the static stack cookies, but not update
any of the stack cookies at runtime.

Because edk2 does not implement exception handling for `SEC` and `PEI_CORE`, `MdeLibs.dsc.inc`
uses `StackCheckLibNull` for these phases always. If a platform wishes to use `StackCheckLib`
for these phases, it should override this in its DSC, e.g.:

```inf
[LibraryClasses.common.SEC, LibraryClasses.common.PEI_CORE]
  StackCheckLib|MdePkg/Library/StackCheckLib/StackCheckLib.inf
```

It is recommended that a platform only do this for debugging or if they have implemented
exception handlers for these phases.

#### Dynamic Stack Cookie Configuration

```inf
[Defines]
  DEFINE CUSTOM_STACK_CHECK_LIB = DYNAMIC
```

This will cause `MdeLibs.dsc.inc` to not link `StackCheckLibNull` and instead link
`StackCheckLib` to perform stack cookie checking. It will also link the dynamic
stack cookie updating versions of `DxeCoreEntryPoint`, `StandaloneMmDriverEntryPoint`,
`UefiApplicationEntryPoint`, and `UefiDriverEntryPoint`.

Because edk2 does not implement exception handling for `SEC` and `PEI_CORE`,
`MdeLibs.dsc.inc` uses `StackCheckLibNull` for these phases always. If a
platform wishes to use `StackCheckLib` for these phases, it can enable static
stack cookie checking, as documented in the previous section.  Due to the fact
that writable global variables are not supported in the `SEC` or `PEI` phases
of execution, dynamic stack cookie checking is not supported here.

It is recommended that a platform only do this for debugging or if they have implemented
exception handlers for these phases.

Note: `StandaloneMmCoreEntryPoint` is recommended to use the dynamic stack cookie if
possible, but as it is not supported on AARCH64 today, it is not included in MdeLibs.dsc.inc.
Platforms should include this separately, e.g.:

```inf
[LibraryClasses.X64]
  StandaloneMmCoreEntryPoint|MdePkg/Library/DynamicStackCookieEntryPointLib/StandaloneMmCoreEntryPoint.inf
```

Platforms then must remove any references to these entry point libs in their DSC, so that
the `MdeLibs.dsc.inc` versions are chosen. Alternatively, for better DSC readability,
a platform can directly reference the dynamic stack cookie entry points.

### Disable Stack Check Library

If a platform would like to disable stack cookies (say for debugging purposes),
they can add the following to their DSC:

```inf
[BuildOptions]
  MSVC:*_*_*_CC_FLAGS = /GS-
  GCC:*_*_*_CC_FLAGS = -fno-stack-protector
```

The same build options can be put in a module's INF to only disable stack cookies
for that module.

Alternatively, a module can have the stack cookies inserted but checking disabled
by including the following in a DSC:

```inf
SomePkg/SomeDirectory/SomeModule.inf {
  <LibraryClasses>
    StackCheckLib|MdePkg/Library/StackCheckLibNull/StackCheckLibNull.inf
}
```

It is not recommended to disable stack cookie checking in production scenarios.
