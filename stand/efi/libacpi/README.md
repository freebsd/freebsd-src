# Google Summer of Code 2025 @ FreeBSD
Mentor: Warner Losh @bsdimp <imp@FreeBSD.org>  
Student: Kayla (Kat) Powell @kpowkitty <kpowkitty@FreeBSD.org>
---------------
## ACPI Initialization in Loader with Lua Bindings
Intel's Advanced Configuration and Power Interface (ACPI) brought power management out of the BIOS
and into the operating system. We can make it even more powerful by creating an interface
to script it. Currently, the scripting language of choice in FreeBSD (rather than POSIX `sh`, `bash`, or `awk`) 
is Lua. Therefore, this project aims to initialize a portion of ACPI in the FreeBSD bootloader, with respect
to its storage, memory, and stdlib constraints, so we can enumerate and evaluate objects on the device trees to
provide an interface to Lua.

## Milestones
[x] `OsdMemory.c` for `amd64` (`arm64` postponed)  
[x] `AcpiInitializeSubsystem`  
[x] `AcpiInitializeTables`  
[x] `AcpiEnableSubsystem` in reduced hardware mode, with events enabled  
[x] `AcpiLoadTables`  
[x] `AcpiWalkNamespace`  
[x] `AcpiEvaluateObject`  
[x] ACPICA initialized in loader for `amd64`    
[ ] lacpi_object.c  (V1)    [ V1 TESTING  ] 
    -- V1: Users can evaluate objects
[ ] lacpi_data.c    (V1)    [ V1 TESTING  ]  
    -- V1: Users can attach, get, detach data from nodes
[ ] lacpi_walk.c    (V2)    [ V1 COMPLETE ]  
    -- V1: Walk and read nodes off the namespace
    -- V2: Namespace format
    -- V3: Strategies for walking the namespace
[ ] Demonstration Lua scripts & man page update  

**Future goals of this project include arm64 compat.**

### For more information, please consult these resources:
[ACPI Lua Bindings FreeBSD project wiki page](https://wiki.freebsd.org/SummerOfCode2025Projects/ACPI%20Initialization%20in%20Loader%20With%20Lua%20Bindings)  
[Write-up](https://kmpow.com/content/gsoc-writeup)

---------------

## Building
This project requires an amd64 UEFI FreeBSD system.

The current working version of this project is found on branch `acpi_init`.

1. Clone the repo
```
$ git clone git@github.com:kpowkitty/freebsd-src.git
$ git checkout acpi_init
```

2. Build the necessary dependencies and install the loader image
```
$ cd stand/efi
# make -j $MAX_JOBS && make install
```

3. Reboot
```
# shutdown -r now
```

## System Recovery
In general cases, to recover your system, you need to mount your drive 
on a live iso and replace your drive's `loader.efi` with the live iso image's `loader.efi`. 
