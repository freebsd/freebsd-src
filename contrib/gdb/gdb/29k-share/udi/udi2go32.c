/*

Interface from UDI calls in 32-bit mode to go32 in 16-bit mode. 
Communication is done through a single interrupt vector, which passes
data through two linear buffers. 

Call:
	AH  = 0xfe
	AL  = UDI function number
	ECX = IN length
	ESI = pointer to IN buffer
	EDI = pointer to OUT buffer

Return:
	EAX = return value of UDI function

Vector:
	0x21

*/
#ifdef __GO32__

#include <stdlib.h>
#include "udiproc.h"
#include "udisoc.h"

char dfe_errmsg[500];

static char in_buffer[4096];
static char out_buffer[4096];
static char *in_ptr;
static char *out_ptr;

#define IN_INIT()		in_ptr = in_buffer
#define IN_VAL(t,v)		*((t *)in_ptr)++ = v
#define IN_DATA(ptr, cnt)	memcpy(in_ptr, ptr, cnt), in_ptr += cnt

#define OUT_INIT()		out_ptr = out_buffer
#define OUT_VAL(t)		(*((t *)out_ptr)++)
#define OUT_DATA(ptr, cnt)	memcpy(ptr, out_ptr, cnt), out_ptr += cnt

static int DO_CALL(int function)
{
  asm("pushl %esi");
  asm("pushl %edi");
  asm("movb %0, %%al" : : "g" (function));
  asm("movl _in_ptr, %ecx");
  asm("movl $_in_buffer, %esi");
  asm("subl %esi, %ecx");
  asm("movl $_out_buffer, %edi");
  asm("movb $0xfe, %ah");
  asm("int $0x21");
  asm("popl %edi");
  asm("popl %esi");
}

/*----------------------------------------------------------------------*/

#ifdef TEST_UDI
int main()
{
  int r;
  long p2;
  short p1;
  IN_INIT();
  IN_VAL(long, 11111111);
  IN_VAL(short, 2222);
  IN_DATA("Hello, world\n", 17);

  r = DO_CALL(42);

  OUT_INIT();
  p1 = OUT_VAL(short);
  p2 = OUT_VAL(long);
  printf("main: p1=%d p2=%d rv=%d\n", p1, p2, r);
  return r;
}
#endif

/*----------------------------------------------------------------------*/

unsupported(char *s)
{
  printf("unsupported UDI host call %s\n", s);
  abort();
}

UDIError UDIConnect (
  char		*Configuration,		/* In */
  UDISessionId	*Session		/* Out */
  )
{
  int r;
  out_buffer[0] = 0; /* DJ - test */
  IN_INIT();
  IN_DATA(Configuration, strlen(Configuration)+1);
  
  r = DO_CALL(UDIConnect_c);

  OUT_INIT();  
  *Session = OUT_VAL(UDISessionId);
  return r;
}

UDIError UDIDisconnect (
  UDISessionId	Session,		/* In */
  UDIBool	Terminate		/* In */
  )
{
  int r;
  IN_INIT();
  IN_VAL(UDISessionId, Session);
  IN_VAL(UDIBool, Terminate);
  
  return DO_CALL(UDIDisconnect_c);
}

UDIError UDISetCurrentConnection (
  UDISessionId	Session			/* In */
  )
{
  IN_INIT();
  IN_VAL(UDISessionId, Session);
  
  return DO_CALL(UDISetCurrentConnection_c);
}

UDIError UDICapabilities (
  UDIUInt32	*TIPId,			/* Out */
  UDIUInt32	*TargetId,		/* Out */
  UDIUInt32	DFEId,			/* In */
  UDIUInt32	DFE,			/* In */
  UDIUInt32	*TIP,			/* Out */
  UDIUInt32	*DFEIPCId,		/* Out */
  UDIUInt32	*TIPIPCId,		/* Out */
  char		*TIPString		/* Out */
  )
{
  int r;
  IN_INIT();
  IN_VAL(UDIUInt32, DFEId);
  IN_VAL(UDIUInt32, DFE);
  r = DO_CALL(UDICapabilities_c);
  OUT_INIT();
  *TIPId = OUT_VAL(UDIUInt32);
  *TargetId = OUT_VAL(UDIUInt32);
  *TIP = OUT_VAL(UDIUInt32);
  *DFEIPCId = OUT_VAL(UDIUInt32);
  *TIPIPCId = OUT_VAL(UDIUInt32);
  strcpy(TIPString, out_ptr);
  return r;
}

UDIError UDIEnumerateTIPs (
  UDIInt	(*UDIETCallback)	/* In */
    ( char *Configuration )	/* In to callback() */
  )
{
  UDIETCallback("montip.exe");
}

UDIError UDIGetErrorMsg (
  UDIError	ErrorCode,		/* In */
  UDISizeT	MsgSize,		/* In */
  char		*Msg,			/* Out */
  UDISizeT	*CountDone		/* Out */
  )
{
  int r;
  if (MsgSize > 4000)
    MsgSize = 4000;
  IN_INIT();
  IN_VAL(UDIError, ErrorCode);
  IN_VAL(UDISizeT, MsgSize);
  
  r = DO_CALL(UDIGetErrorMsg_c);
  
  OUT_INIT();
  *CountDone = OUT_VAL(UDISizeT);
  OUT_DATA(Msg, *CountDone);
  return r;
}

UDIError UDIGetTargetConfig (
  UDIMemoryRange KnownMemory[],		/* Out */
  UDIInt	*NumberOfRanges,	/* In/Out */
  UDIUInt32	ChipVersions[],		/* Out */
  UDIInt	*NumberOfChips		/* In/Out */
  )
{
  int r, i;
  int nr = *NumberOfRanges;
  int nc = *NumberOfChips;
  IN_INIT();
  IN_VAL(UDIInt, *NumberOfRanges);
  IN_VAL(UDIInt, *NumberOfChips);
  r = DO_CALL(UDIGetTargetConfig_c);
  if (r == UDIErrorIncomplete)
    return r;
  OUT_INIT();
  *NumberOfRanges = OUT_VAL(UDIInt);
  *NumberOfChips = OUT_VAL(UDIInt);
  for (i=0; i<nr; i++)
  {
    KnownMemory[i].Space = OUT_VAL(short);
    KnownMemory[i].Offset = OUT_VAL(CPUOffset);
    KnownMemory[i].Size = OUT_VAL(CPUSizeT);
  }
  for (i=0; i<nc; i++)
  {
    ChipVersions[i] = OUT_VAL(UDIUInt32);
  }
  return r;
}

UDIError UDICreateProcess (
  UDIPId	*PId			/* Out */
  )
{
  int r = DO_CALL(UDICreateProcess_c);

  OUT_INIT();
  *PId = OUT_VAL(UDIPId);

  return r;
}

UDIError UDISetCurrentProcess (
  UDIPId	PId			/* In */
  )
{
  IN_INIT();
  IN_VAL(UDIPId, PId);

  return DO_CALL(UDISetCurrentProcess_c);
}

UDIError UDIDestroyProcess (
  UDIPId	PId			/* In */
  )
{
  IN_INIT();
  IN_VAL(UDIPId, PId);

  return DO_CALL(UDIDestroyProcess_c);
}

UDIError UDIInitializeProcess (
  UDIMemoryRange ProcessMemory[],	/* In */
  UDIInt	NumberOfRanges,		/* In */
  UDIResource	EntryPoint,		/* In */
  CPUSizeT	StackSizes[],		/* In */
  UDIInt	NumberOfStacks,		/* In */
  char		*ArgString		/* In */
  )
{
  int i, r;
  IN_INIT();
  IN_VAL(UDIInt, NumberOfRanges);
  for (i=0; i<NumberOfRanges; i++)
  {
    IN_VAL(short, ProcessMemory[i].Space);
    IN_VAL(CPUOffset, ProcessMemory[i].Offset);
    IN_VAL(CPUSizeT, ProcessMemory[i].Size);
  }
  IN_VAL(short, EntryPoint.Space);
  IN_VAL(CPUOffset, EntryPoint.Offset);
  IN_VAL(UDIInt, NumberOfStacks);
  for (i=0; i<NumberOfStacks; i++)
    IN_VAL(CPUSizeT, StackSizes[i]);
  IN_DATA(ArgString, strlen(ArgString)+1);

  return DO_CALL(UDIInitializeProcess_c);
}

UDIError UDIRead (
  UDIResource	From,			/* In */
  UDIHostMemPtr	To,			/* Out */
  UDICount	Count,			/* In */
  UDISizeT	Size,			/* In */
  UDICount	*CountDone,		/* Out */
  UDIBool	HostEndian		/* In */
  )
{
  int cleft = Count, cthis, dthis;
  int cdone = 0, r, bsize=2048/Size;
  
  while (cleft)
  {
    cthis = (cleft<bsize) ? cleft : bsize;
    IN_INIT();
    IN_VAL(short, From.Space);
    IN_VAL(CPUOffset, From.Offset);
    IN_VAL(UDICount, cthis);
    IN_VAL(UDISizeT, Size);
    IN_VAL(UDIBool, HostEndian);

    r = DO_CALL(UDIRead_c);

    OUT_INIT();
    dthis = OUT_VAL(UDICount);
    OUT_DATA(To, dthis*Size);
    cdone += dthis;
    To += dthis*Size;

    if (r != UDINoError)
    {
      *CountDone = cdone;
      return r;
    }
    cleft -= cthis;
  }
  *CountDone = cdone;
  return UDINoError;
}

UDIError UDIWrite (
  UDIHostMemPtr	From,			/* In */
  UDIResource	To,			/* In */
  UDICount	Count,			/* In */
  UDISizeT	Size,			/* In */
  UDICount	*CountDone,		/* Out */
  UDIBool	HostEndian		/* In */
  )
{
  int cleft = Count, cthis, dthis;
  int cdone = 0, r, bsize=2048/Size;
  
  while (cleft)
  {
    cthis = (cleft<bsize) ? cleft : bsize;
    IN_INIT();
    IN_VAL(short, To.Space);
    IN_VAL(CPUOffset, To.Offset);
    IN_VAL(UDICount, cthis);
    IN_VAL(UDISizeT, Size);
    IN_VAL(UDIBool, HostEndian);
    IN_DATA(From, cthis*Size);
    From += cthis*Size;
    
    r = DO_CALL(UDIWrite_c);

    OUT_INIT();
    cdone += OUT_VAL(UDICount);

    if (r != UDINoError)
    {
      *CountDone = cdone;
      return r;
    }
    cleft -= cthis;
  }
  *CountDone = cdone;
  return UDINoError;
}

UDIError UDICopy (
  UDIResource	From,			/* In */
  UDIResource	To,			/* In */
  UDICount	Count,			/* In */
  UDISizeT	Size,			/* In */
  UDICount	*CountDone,		/* Out */
  UDIBool	Direction		/* In */
  )
{
  int r;
  IN_INIT();
  IN_VAL(short, From.Space);
  IN_VAL(CPUOffset, From.Offset);
  IN_VAL(short, To.Space);
  IN_VAL(CPUOffset, To.Offset);
  IN_VAL(UDICount, Count);
  IN_VAL(UDISizeT, Size);
  IN_VAL(UDIBool, Direction);
  
  r = DO_CALL(UDICopy_c);
  
  OUT_INIT();
  *CountDone = OUT_VAL(UDICount);
  
  return r;
}

UDIError UDIExecute (
  void
  )
{
  return DO_CALL(UDIExecute_c);
}

UDIError UDIStep (
  UDIUInt32	Steps,			/* In */
  UDIStepType   StepType,		/* In */
  UDIRange      Range			/* In */
  )
{
  IN_INIT();
  IN_VAL(UDIUInt32, Steps);
  IN_VAL(UDIStepType, StepType);
  IN_VAL(UDIRange, Range);
  
  return DO_CALL(UDIStep_c);
}

UDIVoid UDIStop (
  void
  )
{
  DO_CALL(UDIStop_c);
}

UDIError UDIWait (
  UDIInt32	MaxTime,		/* In */
  UDIPId	*PId,			/* Out */
  UDIUInt32	*StopReason		/* Out */
  )
{
  int r;
  IN_INIT();
  IN_VAL(UDIInt32, MaxTime);
  r = DO_CALL(UDIWait_c);
  OUT_INIT();
  *PId = OUT_VAL(UDIPId);
  *StopReason = OUT_VAL(UDIUInt32);
  return r;
}

UDIError UDISetBreakpoint (
  UDIResource	Addr,			/* In */
  UDIInt32	PassCount,		/* In */
  UDIBreakType	Type,			/* In */
  UDIBreakId	*BreakId		/* Out */
  )
{
  int r;
  IN_INIT();
  IN_VAL(short, Addr.Space);
  IN_VAL(CPUOffset, Addr.Offset);
  IN_VAL(UDIInt32, PassCount);
  IN_VAL(UDIBreakType, Type);
  
  r = DO_CALL(UDISetBreakpoint_c);
  
  OUT_INIT();
  *BreakId = OUT_VAL(UDIBreakId);
  return r;
}

UDIError UDIQueryBreakpoint (
  UDIBreakId	BreakId,		/* In */
  UDIResource	*Addr,			/* Out */
  UDIInt32	*PassCount,		/* Out */
  UDIBreakType	*Type,			/* Out */
  UDIInt32	*CurrentCount		/* Out */
  )
{
  int r;
  IN_INIT();
  IN_VAL(UDIBreakId, BreakId);
  
  r = DO_CALL(UDIQueryBreakpoint_c);
  
  OUT_INIT();
  Addr->Space = OUT_VAL(short);
  Addr->Offset = OUT_VAL(CPUOffset);
  *PassCount = OUT_VAL(UDIInt32);
  *Type = OUT_VAL(UDIBreakType);
  *CurrentCount = OUT_VAL(UDIInt32);
  
  return r;
}

UDIError UDIClearBreakpoint (
  UDIBreakId	BreakId			/* In */
  )
{
  IN_INIT();
  IN_VAL(UDIBreakId, BreakId);
  
  return DO_CALL(UDIClearBreakpoint_c);
}

UDIError UDIGetStdout (
  UDIHostMemPtr	Buf,			/* Out */
  UDISizeT	BufSize,		/* In */
  UDISizeT	*CountDone		/* Out */
  )
{
  int r;
  IN_INIT();
  if (BufSize > 4000)
    BufSize = 4000;
  IN_VAL(UDISizeT,BufSize);
  r = DO_CALL(UDIGetStdout_c);
  OUT_INIT();
  *CountDone = OUT_VAL(UDISizeT);
  if (*CountDone <= BufSize)
    OUT_DATA(Buf, *CountDone);
  return r;
}

UDIError UDIGetStderr (
  UDIHostMemPtr	Buf,			/* Out */
  UDISizeT	BufSize,		/* In */
  UDISizeT	*CountDone		/* Out */
  )
{
  int r;
  IN_INIT();
  if (BufSize > 4000)
    BufSize = 4000;
  IN_VAL(UDISizeT,BufSize);
  r = DO_CALL(UDIGetStderr_c);
  OUT_INIT();
  *CountDone = OUT_VAL(UDISizeT);
  OUT_DATA(Buf, *CountDone);
  return r;
}

UDIError UDIPutStdin (
  UDIHostMemPtr	Buf,			/* In */
  UDISizeT	Count,			/* In */
  UDISizeT	*CountDone		/* Out */
  )
{
  int r;
  IN_INIT();
  if (Count > 4000)
    Count = 4000;
  IN_VAL(UDISizeT,Count);
  IN_DATA(Buf, Count);
  r = DO_CALL(UDIPutStdin_c);
  OUT_INIT();
  *CountDone = OUT_VAL(UDISizeT);
  return r;
}

UDIError UDIStdinMode (
  UDIMode	*Mode			/* Out */
  )
{
  int r;
  IN_INIT();
  r = DO_CALL(UDIStdinMode_c);
  OUT_INIT();
  *Mode = OUT_VAL(UDIMode);
  return r;
}

UDIError UDIPutTrans (
  UDIHostMemPtr	Buf,			/* In */
  UDISizeT	Count,			/* In */
  UDISizeT	*CountDone		/* Out */
  )
{
  int r;
  IN_INIT();
  if (Count > 4000)
    Count = 4000;
  IN_VAL(UDISizeT,Count);
  IN_DATA(Buf, Count);
  r = DO_CALL(UDIPutTrans_c);
  OUT_INIT();
  *CountDone = OUT_VAL(UDISizeT);
  return r;
}

UDIError UDIGetTrans (
  UDIHostMemPtr	Buf,			/* Out */
  UDISizeT	BufSize,		/* In */
  UDISizeT	*CountDone		/* Out */
  )
{
  int r;
  IN_INIT();
  if (BufSize > 4000)
    BufSize = 4000;
  IN_VAL(UDISizeT,BufSize);
  r = DO_CALL(UDIGetTrans_c);
  OUT_INIT();
  *CountDone = OUT_VAL(UDISizeT);
  OUT_DATA(Buf, *CountDone);
  return r;
}

UDIError UDITransMode (
  UDIMode	*Mode			/* Out */
  )
{
  int r;
  IN_INIT();
  r = DO_CALL(UDITransMode_c);
  OUT_INIT();
  *Mode = OUT_VAL(UDIMode);
  return r;
}

#define DFEIPCIdCompany 0x0001	/* Company ID AMD */
#define DFEIPCIdProduct 0x1	/* Product ID 0 */
#define DFEIPCIdVersion 0x125	/* 1.2.5 */

unsigned UDIGetDFEIPCId ()
{
    return((((UDIUInt32)DFEIPCIdCompany) << 16) |(DFEIPCIdProduct << 12) | DFEIPCIdVersion);
}

#endif /* __GO32__ */
