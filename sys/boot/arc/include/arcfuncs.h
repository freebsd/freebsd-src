/*
 * Copyright (c) 1999, Stefan Esser <se@freebsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 *
 */

#ifdef __GNUC__
#define INLINE inline
#else
#define INLINE /**/
#endif

/* System Parameter Block holding ARC and VENDOR function vector addresses */

#define SPBlock ((SPB *)0xffffffff806fe000ul)

/*
 * Convert between 32bit (ARC) and 64bit (Alpha) pointers
 */

static INLINE void*
ptr(arcptr p)
{
   return (void*)(int64_t)p.adr;
}

static INLINE arcptr
toarcptr(void *p)
{
   arcptr p32;
   p32.adr = (int32_t)(int64_t) p;
   return (p32);
}

/*
 * Return entry point for ARC BIOS function "funcno"
 */

static INLINE void *
get_arc_vector(int funcno)
{
   arcptr (*arc_vector)[] = ptr(SPBlock->FirmwareVectorP);
   return ptr((*arc_vector)[funcno -1]);
}

/*
 * Return entry point for VENDOR function "funcno"
 */

static INLINE void *
get_vendor_vector(int funcno)
{
   arcptr (*arc_vector)[] = ptr(SPBlock->PrivateVectorP);
   return ptr((*arc_vector)[funcno -1]);
}

static INLINE int
get_vendor_vector_length(void)
{
   return SPBlock->PrivateVectorLength;
}

/*
 * Macros to create inline wrappers for ARCS BIOS functions
 * 
 * Parameter:
 *	num	function number (starting at 1)
 *	result	result type
 *	fn	function name
 *	param	parameter list (types and formal args)
 *	args	parameter list (formal args only)
 */

#define ARC_FN(num,result,fn,param,args) \
static inline result fn param { \
	typedef result _fn_t param; \
	_fn_t *p_ ## fn = get_arc_vector(num); \
	return p_ ## fn args; \
}

#define VND_FN(num,result,fn,param,args) \
static INLINE result fn param { \
	typedef result _fn_t param; \
	_fn_t *p_ ## fn = get_vendor_vector(num); \
	return p_ ## fn args; \
}

/* function codes as defined in ARC Specification Version 1.2 */

ARC_FN(1, int32_t, Load, 
       (char *Path, u_int32_t TopAddr, u_int32_t *ExecAddr, u_int32_t *LowAddr),
       (Path, TopAddr, ExecAddr, LowAddr))
ARC_FN(2, int32_t, Invoke, 
       (u_int32_t ExecAddr, u_int32_t StackAddr, u_int32_t Argc, char *Argv[], char *Envp[]),
       (ExecAddr, StackAddr, Argc, Argv, Envp))
ARC_FN(3, int32_t, Execute,
       (char *Path, u_int32_t Argc, char *Argv[], char *Envp[]),
       (Path, Argc, Argv, Envp))
ARC_FN(4, void, Halt, (void), ())
ARC_FN(5, void, PowerDown, (void), ())
ARC_FN(6, void, Restart, (void), ())
ARC_FN(7, void, FwReboot, (void), ())
ARC_FN(8, void, EnterInteractiveMode, (void), ())
ARC_FN(10, CONFIGURATION_COMPONENT *, GetPeer,
       (CONFIGURATION_COMPONENT *Current),
       (Current))
ARC_FN(11, CONFIGURATION_COMPONENT *, GetChild,
       (CONFIGURATION_COMPONENT *Current),
       (Current))
ARC_FN(12, CONFIGURATION_COMPONENT *, GetParent,
       (CONFIGURATION_COMPONENT *Current),
       (Current))
ARC_FN(13, CONFIGURATION_COMPONENT *, AddChild,
       (CONFIGURATION_COMPONENT *Current, CONFIGURATION_COMPONENT *Template, 
	void *ConfigurationData),
       (Current, Template, ConfigurationData))
ARC_FN(14, int32_t, DeleteComponent,
       (CONFIGURATION_COMPONENT *ComponentToDelete),
       (ComponentToDelete))
ARC_FN(15, CONFIGURATION_COMPONENT *, GetComponent, (char *Path), (Path))
ARC_FN(16, int32_t, GetConfigurationData,
       (void *ConfigurationData, CONFIGURATION_COMPONENT *Component),
       (ConfigurationData, Component))
ARC_FN(17, int32_t, SaveConfiguration, (void), ())
ARC_FN(18, SYSTEM_ID *, GetSystemId, (void), ())
ARC_FN(19, MEMORY_DESCRIPTOR *, GetMemoryDescriptor, 
       (MEMORY_DESCRIPTOR *Current),
       (Current))
ARC_FN(21, TIME_FIELDS *, GetTime, (void), ())
ARC_FN(22, u_int32_t, GetRelativeTime, (void), ())
ARC_FN(23, int32_t, GetDirectoryEntry,
       (u_int32_t FileId, DIRECTORY_ENTRY *Buffer, u_int32_t Length, u_int32_t *Count),
       (FileId, Buffer, Length, Count))
ARC_FN(24, int32_t, Open, 
       (const char *Path, OPEN_MODE OpenMode, u_int32_t *FileId),
       (Path, OpenMode, FileId))
ARC_FN(25, int32_t, Close, (u_int32_t FileId), (FileId))
ARC_FN(26, int32_t, Read, 
       (u_int32_t FileId, void *Buffer,  u_int32_t N, u_int32_t *Count),
       (FileId, Buffer, N, Count))
ARC_FN(27, int32_t, GetReadStatus, (u_int32_t FileId), (FileId))
ARC_FN(28, int32_t, Write, 
       (u_int32_t FileId, void const *Buffer, u_int32_t N, u_int32_t *Count),
       (FileId, Buffer, N, Count))
ARC_FN(29, int32_t, Seek, 
       (u_int32_t FileId, fpos_t *Position, SEEK_MODE SeekMode),
       (FileId, Position, SeekMode))
ARC_FN(30, int32_t, Mount, 
       (char *Path, MOUNT_OPERATION Operation), 
       (Path, Operation))
ARC_FN(31, char *, GetEnvironmentVariable, (char *Name), (Name))
ARC_FN(32, int32_t, SetEnvironmentVariable, 
       (char *Name, char *Value), 
       (Name, Value))
ARC_FN(33, int32_t, GetFileInformation, 
       (u_int32_t FileId, FILE_INFORMATION *Information),
       (FileId, Information))
ARC_FN(34, int32_t, SetFileInformation,
       (u_int32_t FileId, u_int32_t AttributeFlags, u_int32_t AttributeMask),
       (FileId, AttributeFlags, AttributeMask))
ARC_FN(35, void, FlushAllCaches, (void), ())
ARC_FN(36, int32_t, TestUnicodeCharacter, 
       (u_int32_t FileId, WCHAR UnicodeCharacter),
       (FileId, UnicodeCharacter))
ARC_FN(37, ARC_DISPLAY_STATUS *, GetDisplayStatus, (u_int32_t FileId), (FileId))

/* Vendor specific function codes have not been verified beyond function 4 */

VND_FN(1, void *, AllocatePool, (u_int32_t NumberOfBytes), (NumberOfBytes))
VND_FN(2, void, StallExecution, (u_int32_t Microseconds), (Microseconds))
VND_FN(3, u_int32_t, Print, 
       (char *Format, int32_t Arg1, int32_t Arg2, int32_t Arg3), 
       (Format, Arg1, Arg2, Arg3))
VND_FN(4, void, ReturnExtendedSystemInformation, 
       (EXTENDED_SYSTEM_INFORMATION *SystemInfo),
       (SystemInfo))
