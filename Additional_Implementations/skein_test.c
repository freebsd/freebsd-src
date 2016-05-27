/***********************************************************************
**
** Test/verification code for the Skein block functions.
**
** Source code author: Doug Whiting, 2008.
**
** This algorithm and source code is released to the public domain.
**
** Testing:
**   - buffering of incremental calls (random cnt steps)
**   - partial input byte handling
**   - output sample hash results (for comparison of ref vs. optimized)
**   - performance
**
***********************************************************************/

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <assert.h>

#include "skein.h"
#include "SHA3api_ref.h"

static const uint_t HASH_BITS[] =    /* list of hash hash lengths to test */
        { 160,224,256,384,512,1024, 256+8,512+8,1024+8,2048+8 };

#define HASH_BITS_CNT   (sizeof(HASH_BITS)/sizeof(HASH_BITS[0]))

/* bits of the verbose flag word */
#define V_KAT_LONG      (1u << 0)
#define V_KAT_SHORT     (1u << 1)
#define V_KAT_NO_TREE   (1u << 2)
#define V_KAT_NO_SEQ    (1u << 3)
#define V_KAT_NO_3FISH  (1u << 4)
#define V_KAT_DO_3FISH  (1u << 5)

/* automatic compiler version number detection */
#if !defined(CompilerVersion)

#if   defined(_MSC_VER) && (_MSC_VER >= 1400)
#define CompilerVersion (900)
#elif defined(_MSC_VER) && (_MSC_VER >= 1200)
#define CompilerVersion (600)
#elif defined(_MSC_VER) && (_MSC_VER >= 1000)
#define CompilerVersion (420)
#elif defined(__GNUC__) && defined(__GNUC_MINOR__) && defined(__GNUC_PATCHLEVEL__)
#define CompilerVersion (100*__GNUC__ + 10*__GNUC_MINOR__ + __GNUC_PATCHLEVEL__)
#elif defined(__BORLANDC__) /* this is in hex */
#define CompilerVersion (100*(__BORLANDC__ >> 8) + 10*((__BORLANDC__ >> 4) & 0xF) + (__BORLANDC__ & 0xF))
#endif

#endif

#if defined(SKEIN_CODE_SIZE) || defined(SKEIN_PERF) 
/* external functions to determine code size (in bytes) */
size_t  Skein_256_Process_Block_CodeSize(void);
size_t  Skein_512_Process_Block_CodeSize(void);
size_t  Skein1024_Process_Block_CodeSize(void);
size_t  Skein_256_API_CodeSize(void);
size_t  Skein_512_API_CodeSize(void);
size_t  Skein1024_API_CodeSize(void);
uint_t  Skein_256_Unroll_Cnt(void);
uint_t  Skein_512_Unroll_Cnt(void);
uint_t  Skein1024_Unroll_Cnt(void);
#elif defined(SKEIN_LOOP)
uint_t  Skein_256_Unroll_Cnt(void) { return (SKEIN_LOOP / 100) % 10; }
uint_t  Skein_512_Unroll_Cnt(void) { return (SKEIN_LOOP /  10) % 10; }
uint_t  Skein1024_Unroll_Cnt(void) { return (SKEIN_LOOP      ) % 10; }
#else
uint_t  Skein_256_Unroll_Cnt(void) { return 0; }
uint_t  Skein_512_Unroll_Cnt(void) { return 0; }
uint_t  Skein1024_Unroll_Cnt(void) { return 0; }
#endif

/* External function to process blkCnt (nonzero) full block(s) of data. */
void    Skein_256_Process_Block(Skein_256_Ctxt_t *ctx,const u08b_t *blkPtr,size_t blkCnt,size_t byteCntAdd);
void    Skein_512_Process_Block(Skein_512_Ctxt_t *ctx,const u08b_t *blkPtr,size_t blkCnt,size_t byteCntAdd);
void    Skein1024_Process_Block(Skein1024_Ctxt_t *ctx,const u08b_t *blkPtr,size_t blkCnt,size_t byteCntAdd);

/********************** debug i/o helper routines **********************/
void FatalError(const char *s,...)
    { /* print out a msg and exit with an error code */
    va_list ap;
    va_start(ap,s);
    vprintf(s,ap);
    va_end(ap);
    printf("\n");
    exit(2);
    }

static uint_t _quiet_   =   0;  /* quiet processing? */
static uint_t verbose   =   0;  /* verbose flag bits */
static uint_t katHash   = ~0u;  /* use as a quick check on KAT results */

void ShowBytes(uint_t cnt,const u08b_t *b)
    { /* formatted output of byte array */
    uint_t i;

    for (i=0;i < cnt;i++)
        {
        if (i %16 ==  0) printf("    ");
        else if (i % 4 == 0) printf(" ");
        printf(" %02X",b[i]);
        katHash = (katHash ^ b[i]) * 0xDEADBEEF;
        katHash = (katHash ^ (katHash >> 23) ^ (katHash >> 17) ^ (katHash >> 9)) * 0xCAFEF00D;
        if (i %16 == 15 || i==cnt-1) printf("\n");
        }
    }

#ifndef SKEIN_DEBUG
uint_t skein_DebugFlag     =   0;     /* dummy flags (if not defined elsewhere) */
#endif

#define SKEIN_DEBUG_SHORT   (SKEIN_DEBUG_HDR | SKEIN_DEBUG_STATE | SKEIN_DEBUG_TWEAK | SKEIN_DEBUG_KEY | SKEIN_DEBUG_INPUT_08 | SKEIN_DEBUG_FINAL)
#define SKEIN_DEBUG_DEFAULT (SKEIN_DEBUG_SHORT)

void Show_Debug(const char *s,...)
    {
    if (skein_DebugFlag)              /* are we showing debug info? */
        {
        va_list ap;
        va_start(ap,s);
        vprintf(s,ap);
        va_end(ap);
        }
    }

/************** Timing routine (for performance measurements) ***********/
/* unfortunately, this is generally assembly code and not very portable */

#if defined(_M_IX86) || defined(__i386) || defined(_i386) || defined(__i386__) || defined(i386) || \
    defined(_X86_)   || defined(__x86_64__) || defined(_M_X64) || defined(__x86_64)
#define _Is_X86_    1
#endif

#if  defined(_Is_X86_) && (!defined(__STRICT_ANSI__)) && (defined(__GNUC__) || !defined(__STDC__)) && \
    (defined(__BORLANDC__) || defined(_MSC_VER) || defined(__MINGW_H) || defined(__GNUC__))
#define HI_RES_CLK_OK         1          /* it's ok to use RDTSC opcode */

#if defined(_MSC_VER) && defined(_M_X64)
#include <intrin.h>
#pragma intrinsic(__rdtsc)
#endif

#endif

uint_32t HiResTime(void)
    {
#if defined(HI_RES_CLK_OK)
    uint_32t x[2];
#if   defined(__BORLANDC__)
#define COMPILER_ID "BCC"
    _asm { push edx };
    __emit__(0x0F,0x31);            /* RDTSC instruction */
    _asm { pop  edx };
    _asm { mov x[0],eax };
#elif defined(_MSC_VER)
#define COMPILER_ID "MSC"
#if defined(_MSC_VER) && defined(_M_X64)
    x[0] = (uint_32t) __rdtsc();
#else
    _asm { push  edx };
    _asm { _emit 0fh }; _asm { _emit 031h };
    _asm { pop   edx };
    _asm { mov x[0],eax };
#endif
#elif defined(__MINGW_H) || defined(__GNUC__)
#define COMPILER_ID "GCC"
    asm volatile("rdtsc" : "=a"(x[0]), "=d"(x[1]));
#else
#error  "HI_RES_CLK_OK -- but no assembler code for this platform (?)"
#endif
    return x[0];
#else
    /* avoid annoying MSVC 9.0 compiler warning #4720 in ANSI mode! */
#if (!defined(_MSC_VER)) || (!defined(__STDC__)) || (_MSC_VER < 1300)
    FatalError("No support for RDTSC on this CPU platform\n");
#endif
    return 0;
#endif /* defined(HI_RES_CLK_OK) */
    }

/******** OS-specific calls for setting priorities and sleeping ******/
#if (defined(_MSC_VER) && (_MSC_VER >= 1300) && !defined(__STRICT_ANSI__) && !defined(__STDC__)) \
    && defined(_M_X64)
#include <Windows.h>
#include <WinBase.h>

#ifdef  SKEIN_FORCE_LOCK_CPU            /* NielsF says this is not a good way to do things */
#define SKEIN_LOCK_CPU_OK (1)
int Lock_CPU(void)
    {   /* lock this process to this CPU for perf timing */
        /*   -- thanks to Brian Gladman for this code    */
    HANDLE ph;
    DWORD_PTR afp;
    DWORD_PTR afs;
    ph = GetCurrentProcess();
    if(GetProcessAffinityMask(ph, &afp, &afs))
        {
        afp &= (((size_t)1u) << GetCurrentProcessorNumber());
        if(!SetProcessAffinityMask(ph, afp))
            return 1;
        }
    else
        {
        return 2;
        }
    return 0;   /* success */
    }
#endif

#define _GOT_OS_SLEEP        (1)
void OS_Sleep(uint_t msec)
    {
    Sleep(msec);
    }

#define _GOT_OS_SET_PRIORITY (1)
int OS_Set_High_Priority(void)
    {
    if(!SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST))
        return 1;
#ifdef SKEIN_LOCK_CPU_OK    
    if (Lock_CPU())
        return 2;
#endif
    return 0;
    }

int OS_Set_Normal_Priority(void)
    {
    if(!SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL))
        return 1;
    return 0;
    }
#endif

#if defined(__linux) || defined(__linux__) || defined(linux) || defined(__gnu_linux__)
#include <unistd.h>
#define _GOT_OS_SLEEP        (1)
void OS_Sleep(uint_t mSec)
    {
    usleep(mSec*1000);
    }
#endif

#ifndef _GOT_OS_SET_PRIORITY
/* dummy routines if nothing is available */
int OS_Set_High_Priority(void)
    {
    return 0;
    }
int OS_Set_Normal_Priority(void)
    {
    return 0;
    }
#endif

#ifndef _GOT_OS_SLEEP
uint_32t OS_Sleep(uint_32t mSec)
    {
    return mSec;    /* avoid compiler warnings */
    }
#endif
   
#ifndef COMPILER_ID
#define COMPILER_ID "(unknown)"
#endif
/********************** use RC4 to generate test data ******************/
/* Note: this works identically on all platforms (big/little-endian)   */
static struct
    {
    uint_t I,J;                         /* RC4 vars */
    u08b_t state[256];
    } prng;

void RandBytes(void *dst,uint_t byteCnt)
    {
    u08b_t a,b;
    u08b_t *d = (u08b_t *) dst;

    for (;byteCnt;byteCnt--,d++)        /* run RC4  */
        {
        prng.I  = (prng.I+1) & 0xFF;
        a       =  prng.state[prng.I];
        prng.J  = (prng.J+a) & 0xFF;
        b       =  prng.state[prng.J];
        prng.state[prng.I] = b;
        prng.state[prng.J] = a;
        *d      =  prng.state[(a+b) & 0xFF];
        }
    }

/* get a pseudo-random 32-bit integer in a portable way */
uint_t Rand32(void)
    {
    uint_t i,n;
    u08b_t tmp[4];

    RandBytes(tmp,sizeof(tmp));

    for (i=n=0;i<sizeof(tmp);i++)
        n = n*256 + tmp[i];
    
    return n;
    }

/* init the (RC4-based) prng */
void Rand_Init(u64b_t seed)
    {
    uint_t i,j;
    u08b_t tmp[512];

    /* init the "key" in an endian-independent fashion */
    for (i=0;i<8;i++)
        tmp[i] = (u08b_t) (seed >> (8*i));

    /* initialize the permutation */
    for (i=0;i<256;i++)
        prng.state[i]=(u08b_t) i;

    /* now run the RC4 key schedule */
    for (i=j=0;i<256;i++)
        {                   
        j = (j + prng.state[i] + tmp[i%8]) & 0xFF;
        tmp[256]      = prng.state[i];
        prng.state[i] = prng.state[j];
        prng.state[j] = tmp[256];
        }
    prng.I = prng.J = 0;  /* init I,J variables for RC4 */
    
    /* discard initial keystream before returning */
    RandBytes(tmp,sizeof(tmp));
    }
    
/***********************************************************************/
/* An AHS-like API that allows explicit setting of block size          */
/*    [i.e., the AHS API selects a block size based solely on the ]    */
/*    [hash result length, while Skein allows independent hash    ]    */
/*    [result size and block size                                 ]    */
/***********************************************************************/

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
/* select the context size and init the context */
int Skein_Init(int blkSize,hashState *state, int hashbitlen)
    {
    switch (blkSize)
        {
        case  256:
            state->statebits = 64*SKEIN_256_STATE_WORDS;
            return Skein_256_Init(&state->u.ctx_256,(size_t) hashbitlen);
        case  512:
            state->statebits = 64*SKEIN_512_STATE_WORDS;
            return Skein_512_Init(&state->u.ctx_512,(size_t) hashbitlen);
        case 1024:
            state->statebits = 64*SKEIN1024_STATE_WORDS;
            return Skein1024_Init(&state->u.ctx1024,(size_t) hashbitlen);
        default:
            return SKEIN_FAIL;
        }
    }

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
/* select the context size and init (extended) the context */
int Skein_InitExt(int blkSize,hashState *state, int hashbitlen,u64b_t treeInfo,const u08b_t *key,size_t keyBytes)
    {
    switch (blkSize)
        {
        case  256:
            state->statebits = 64*SKEIN_256_STATE_WORDS;
            return Skein_256_InitExt(&state->u.ctx_256,(size_t) hashbitlen,treeInfo,key,keyBytes);
        case  512:
            state->statebits = 64*SKEIN_512_STATE_WORDS;
            return Skein_512_InitExt(&state->u.ctx_512,(size_t) hashbitlen,treeInfo,key,keyBytes);
        case 1024:
            state->statebits = 64*SKEIN1024_STATE_WORDS;
            return Skein1024_InitExt(&state->u.ctx1024,(size_t) hashbitlen,treeInfo,key,keyBytes);
        default:
            return SKEIN_FAIL;
        }
    }

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
/* process data to be hashed */
int Skein_Update(hashState *state, const BitSequence *data, DataLength databitlen)
    {
    /* only the final Update() call is allowed do partial bytes, else assert an error */
    Skein_Assert((state->u.h.T[1] & SKEIN_T1_FLAG_BIT_PAD) == 0 || databitlen == 0, FAIL);

    if ((databitlen & 7) == 0)
        {
        switch (state->statebits)
            {
            case  512:  return Skein_512_Update(&state->u.ctx_512,data,databitlen >> 3);
            case  256:  return Skein_256_Update(&state->u.ctx_256,data,databitlen >> 3);
            case 1024:  return Skein1024_Update(&state->u.ctx1024,data,databitlen >> 3);
            default: return SKEIN_FAIL;
            }
        }
    else
        {
        size_t bCnt = (databitlen >> 3) + 1;                  /* number of bytes to handle */
        u08b_t mask,*p;

#if (!defined(_MSC_VER)) || (MSC_VER >= 1200)                 /* MSC v4.2 gives (invalid) warning here!!  */
        Skein_assert(&state->u.h == &state->u.ctx_256.h);     /* sanity checks: allow u.h --> all contexts */
        Skein_assert(&state->u.h == &state->u.ctx_512.h);
        Skein_assert(&state->u.h == &state->u.ctx1024.h);
#endif
        switch (state->statebits)
            {
            case  512: Skein_512_Update(&state->u.ctx_512,data,bCnt);
                       p    = state->u.ctx_512.b;
                       break;
            case  256: Skein_256_Update(&state->u.ctx_256,data,bCnt);
                       p    = state->u.ctx_256.b;
                       break;
            case 1024: Skein1024_Update(&state->u.ctx1024,data,bCnt);
                       p    = state->u.ctx1024.b;
                       break;
            default:   return FAIL;
            }
        Skein_Set_Bit_Pad_Flag(state->u.h);                     /* set tweak flag for the final call */
        /* now "pad" the final partial byte the way NIST likes */
        bCnt = state->u.h.bCnt;         /* get the bCnt value (same location for all block sizes) */
        Skein_assert(bCnt != 0);        /* internal sanity check: there IS a partial byte in the buffer! */
        mask = (u08b_t) (1u << (7 - (databitlen & 7)));         /* partial byte bit mask */
        p[bCnt-1]  = (u08b_t)((p[bCnt-1] & (0-mask)) | mask);   /* apply bit padding on final byte (in the buffer) */
        
        return SUCCESS;
        }
    }

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
/* finalize hash computation and output the result (hashbitlen bits) */
int Skein_Final(hashState *state, BitSequence *hashval)
    {
    switch (state->statebits)
        {
        case  512:  return Skein_512_Final(&state->u.ctx_512,hashval);
        case  256:  return Skein_256_Final(&state->u.ctx_256,hashval);
        case 1024:  return Skein1024_Final(&state->u.ctx1024,hashval);
        default:    return SKEIN_FAIL;
        }
    }

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
/* all-in-one hash function */
int Skein_Hash(int blkSize,int hashbitlen, const BitSequence *data, /* all-in-one call */
                DataLength databitlen,BitSequence *hashval)
    {
    hashState  state;
    int r = Skein_Init(blkSize,&state,hashbitlen);
    if (r == SKEIN_SUCCESS)
        { /* these calls do not fail when called properly */
        r = Skein_Update(&state,data,databitlen);
        Skein_Final(&state,hashval);
        }
    return r;
    }

/***********************************************************************/
/* various self-consistency checks */
uint_t Skein_Test(uint_t blkSize,uint_t maxLen,uint_t hashLen,uint_t nStep,uint_t oneBlk)
    {
    enum        { MAX_BUF=1024 };
    u08b_t      b[MAX_BUF+4],hashVal[2][MAX_BUF+4];
    uint_t      i,j,k,n,bCnt,useAHS,step,bitLen,testCnt=0;
    hashState   s[2];
                
    assert(blkSize > 0 && blkSize <= 1024 && (blkSize % 256) == 0);
    assert((hashLen % 8) == 0);

    if (maxLen  > MAX_BUF*8)     /* keep things reasonably small */
        maxLen  = MAX_BUF*8;
    if (hashLen > MAX_BUF*8)
        hashLen = MAX_BUF*8;
    if (maxLen  == 0)            /* default sizes */
        maxLen  = blkSize*2;
    if (hashLen == 0)
        hashLen = blkSize;

    if (oneBlk)
        {
        if (oneBlk > MAX_BUF*8)
            oneBlk = MAX_BUF*8;
        for (i=0;i<oneBlk/8;i++)
            b[i] = (u08b_t) i;
        if (Skein_Hash(blkSize,hashLen,b,oneBlk,hashVal[0]) != SKEIN_SUCCESS)
            FatalError("Skein_Hash != SUCCESS");
        return 1;
        }

    if (nStep == 0)
        {
        printf("Testing Skein: blkSize = %4d bits. hashLen=%4d bits. maxMsgLen = %4d bits.\n",
               blkSize,hashLen,maxLen);
        nStep = 1;
        }

    n = skein_DebugFlag;
    skein_DebugFlag = 0;        /* turn of debug display for this "fake" AHS call */
    if (Init(&s[0],hashLen) != SUCCESS) /* just see if AHS API supports this <blkSize,hashLen> pair */
        FatalError("AHS_API Init() error!");
    skein_DebugFlag = n;        /* restore debug display status */

    useAHS = (s[0].statebits == blkSize);  /* does this <blkSize,hashLen> pair work via AHS_API? */
    
    bCnt = (maxLen + 7) / 8;    /* convert maxLen to bytes */
    for (n=0;n < bCnt;n+=nStep) /* process all the data lengths (# bytes = n+1)*/
        {
        RandBytes(b,maxLen);    /* get something to hash */
        for (j=8;j>0;j--)       /* j = # bits in final byte */
            {
            testCnt++;
            memset(hashVal,0,sizeof(hashVal));
            Show_Debug("\n*** Single Hash() call (%d bits)\n",8*n+j);
            if (Skein_Hash(blkSize,hashLen,b,8*n+j,hashVal[0]) != SKEIN_SUCCESS)
                FatalError("Skein_Hash != SUCCESS");
            for (k=hashLen/8;k<=MAX_BUF;k++)
                if (hashVal[0][k] != 0)
                    FatalError("Skein hash output overrun!: hashLen = %d bits",hashLen);
            if (useAHS)         /* compare using AHS API, if supported */
                {      
                Show_Debug("\n*** Single AHS API Hash() call\n");
                if (Hash(hashLen,b,8*n+j,hashVal[1]) != SUCCESS)
                    FatalError("Skein_Hash != SUCCESS");
                for (k=hashLen/8;k<=MAX_BUF;k++)
                    if (hashVal[1][k] != 0)
                        FatalError("Skein AHS_API hash output overrun!: hashLen = %d bits",hashLen);
                if (memcmp(hashVal[1],hashVal[0],hashLen/8))
                    FatalError("Skein vs. AHS API miscompare");
                }
            /* now try (randomized) steps thru entire input block */
            for (i=0;i<4;i++) 
                {  
                Show_Debug("\n*** Multiple Update() calls [%s]",(i)?"random steps":"step==1");
                if (i >= 2)
                    {
                    Show_Debug("  [re-use precomputed state]");
                    s[0] = s[1]; 
                    }
                else
                    {
                    k = (i) ? Skein_Init   (blkSize,&s[0],hashLen) :
                              Skein_InitExt(blkSize,&s[0],hashLen,SKEIN_CFG_TREE_INFO_SEQUENTIAL,NULL,0);
                    if (k != SKEIN_SUCCESS)
                        FatalError("Skein_Init != SUCCESS");
                    s[1] = s[0];            /* make a copy for next time */
                    }
                Show_Debug("\n");
                for (k=0;k<n+1;k+=step)     /* step thru with variable sized steps */
                    {/* for i == 0, step one byte at a time. for i>0, randomly */
                    step = (i == 0) ? 1 : 1 + (Rand32() % (n+1-k));     /* # bytes to process */
                    bitLen = (k+step >= n+1) ? 8*(step-1) + j: 8*step;  /* partial final byte handling */
                    if (Skein_Update(&s[0],&b[k],bitLen) != SKEIN_SUCCESS)
                        FatalError("Skein_Update != SUCCESS");
                    }
                if (Skein_Final(&s[0],hashVal[1]) != SKEIN_SUCCESS)
                    FatalError("Skein_Final != SUCCESS");
                for (k=hashLen/8;k<=MAX_BUF;k++)
                    if (hashVal[0][k] != 0)
                        FatalError("Skein hash output overrun!: hashLen = %d bits",hashLen);
                if (memcmp(hashVal[1],hashVal[0],hashLen/8))
                    FatalError("Skein Hash() vs. Update() miscompare!");
                }
            }
        }
    return testCnt;
    }

/* filter out <blkSize,hashBits> pairs in short KAT mode */
uint_t Short_KAT_OK(uint_t blkSize,uint_t hashBits)
    {
    switch (blkSize)
        {
        case  256:
            if (hashBits != 256 && hashBits != 224)
                return 0;
            break;
        case  512:
            if (hashBits != 256 && hashBits != 384 && hashBits != 512)
                return 0;
            break;
        case 1024:
            if (hashBits != 384 && hashBits != 512 && hashBits != 1024)
                return 0;
            break;
        default:
            return 0;
        }
    return 1;
    }

#if SKEIN_TREE_HASH
#define MAX_TREE_MSG_LEN  (1 << 12)
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
/* pad final block, no OUTPUT stage */
int Skein_Final_Pad(hashState *state, BitSequence *hashval)
    {
    switch (state->statebits)
        {
        case  512:  return Skein_512_Final_Pad(&state->u.ctx_512,hashval);
        case  256:  return Skein_256_Final_Pad(&state->u.ctx_256,hashval);
        case 1024:  return Skein1024_Final_Pad(&state->u.ctx1024,hashval);
        default:    return SKEIN_FAIL;
        }
    }
/* just the OUTPUT stage */
int Skein_Output(hashState *state, BitSequence *hashval)
    {
    switch (state->statebits)
        {
        case  512:  return Skein_512_Output(&state->u.ctx_512,hashval);
        case  256:  return Skein_256_Output(&state->u.ctx_256,hashval);
        case 1024:  return Skein1024_Output(&state->u.ctx1024,hashval);
        default:    return SKEIN_FAIL;
        }
    }

/* generate a KAT test for the given data and tree parameters. */
/* This is an "all-in-one" call. It is not intended to represent */
/* how a real multi-processor version would be implemented, but  */
/* the results will be the same */
void Skein_TreeHash
    (uint_t blkSize,uint_t hashBits,const u08b_t *msg,size_t msgBytes,
     uint_t leaf   ,uint_t node    ,uint_t maxLevel  ,u08b_t *hashRes)
    {
    enum      { MAX_HEIGHT = 32 };          /* how deep we can go here */
    uint_t    height;
    uint_t    blkBytes  = blkSize/8;
    uint_t    saveDebug = skein_DebugFlag;
    size_t    n,nodeLen,srcOffs,dstOffs,bCnt;
    u64b_t    treeInfo;
    u08b_t    M[MAX_TREE_MSG_LEN+4];
    hashState G,s;

    assert(node < 256 && leaf < 256 && maxLevel < 256);
    assert(node >  0  && leaf >  0  && maxLevel >  1 );
    assert(blkSize == 256 || blkSize == 512 || blkSize == 1024);
    assert(blkBytes <= sizeof(M));
    assert(msgBytes <= sizeof(M));

    /* precompute the config block result G for multiple uses below */
#ifdef SKEIN_DEBUG
    if (skein_DebugFlag)
        skein_DebugFlag |= SKEIN_DEBUG_CONFIG;
#endif
    treeInfo = SKEIN_CFG_TREE_INFO(leaf,node,maxLevel);
    if (Skein_InitExt(blkSize,&G,hashBits,treeInfo,NULL,0) != SKEIN_SUCCESS)
        FatalError("Skein_InitExt() fails in tree");
    skein_DebugFlag = saveDebug;

    bCnt = msgBytes;
    memcpy(M,msg,bCnt);
    for (height=0;;height++)            /* walk up the tree */
        {
        if (height && (bCnt==blkBytes)) /* are we done (with only one block left)? */
            break;
        if (height+1 == maxLevel)       /* is this the final allowed level? */
            {                           /* if so, do it as one big hash */
            s = G;
            Skein_Set_Tree_Level(s.u.h,height+1);
            Skein_Update   (&s,M,bCnt*8);
            Skein_Final_Pad(&s,M);
            break;
            }
        nodeLen = blkBytes << ((height) ? node : leaf);
        for (srcOffs=dstOffs=0;srcOffs <= bCnt;)
            {
            n = bCnt - srcOffs;         /* number of bytes left at this level */
            if (n > nodeLen)            /* limit to node size */
                n = nodeLen;
            s = G;
            s.u.h.T[0] = srcOffs;       /* nonzero initial offset in tweak! */
            Skein_Set_Tree_Level(s.u.h,height+1);
            Skein_Update   (&s,M+srcOffs,n*8);
            Skein_Final_Pad(&s,M+dstOffs);  /* finish up this node, output intermediate result to M[]*/
            dstOffs+=blkBytes;
            srcOffs+=n;
            if (srcOffs >= bCnt)        /* special logic to handle (msgBytes == 0) case */
                break;
            }
        bCnt = dstOffs;
        }

    /* output the result */
    Skein_Output(&s,hashRes);
    }

/*
** Generate tree-mode hash KAT vectors.
** Note:
**    Tree vectors are different enough from non-tree vectors that it 
**    makes sense to separate this out into a different function, rather 
**    than shoehorn it into the same KAT logic as the other modes.
**/
void Skein_GenKAT_Tree(uint_t blkSize)
    {
    static const struct
        {
        uint_t leaf,node,maxLevel,levels;
        }
        TREE_PARMS[] = { {2,2,2,2}, {1,2,3,2}, {2,1,0xFF,3} };
#define TREE_PARM_CNT (sizeof(TREE_PARMS)/sizeof(TREE_PARMS[0]))

    u08b_t  msg[MAX_TREE_MSG_LEN+4],hashVal[MAX_TREE_MSG_LEN+4];
    uint_t  i,j,k,n,p,q,hashBits,node,leaf,leafBytes,msgBytes,byteCnt,levels,maxLevel;

    assert(blkSize == 256 || blkSize == 512 || blkSize == 1024);
    for (i=0;i<MAX_TREE_MSG_LEN;i+=2)
        {   /* generate "incrementing" tree hash input msg data */
        msg[i  ] = (u08b_t) ((i ^ blkSize) ^ (i >> 16));
        msg[i+1] = (u08b_t) ((i ^ blkSize) >> 8);
        }
    for (k=q=n=0;k < HASH_BITS_CNT;k++)
        {
        hashBits = HASH_BITS[k];
        if (!Short_KAT_OK(blkSize,hashBits))
            continue;
        if ((verbose & V_KAT_SHORT) && (hashBits != blkSize))
            continue;
        for (p=0;p <TREE_PARM_CNT;p++)
            {
            if (p && (verbose & V_KAT_SHORT))
                continue;           /* keep short KATs short */
            if (p && hashBits != blkSize)
                continue;           /* we only need one "non-full" size */

            leaf      = TREE_PARMS[p].leaf;
            node      = TREE_PARMS[p].node;
            maxLevel  = TREE_PARMS[p].maxLevel;
            levels    = TREE_PARMS[p].levels;
            leafBytes = (blkSize/8) << leaf;    /* number of bytes in a "full" leaf */

            for (j=0;j<4;j++)       /* different numbers of leaf results */
                {
                if ((verbose & V_KAT_SHORT) && (j != 3) && (j != 0))
                    continue;
                if (j && (hashBits != blkSize))
                    break;
                switch (j)
                    {
                    case 0: n = 1;                                break;
                    case 1: n = 2;                                break;         
                    case 2: n = (1 << (node * (levels-2)))*3/2;
                            if (n <= 2) continue;                 break;
                    case 3: n = (1 << (node * (levels-1)));       break;
                    }
                byteCnt = n*leafBytes;
                assert(byteCnt > 0);
                if (byteCnt > MAX_TREE_MSG_LEN)
                    continue;
                q = (q+1) % leafBytes;
                msgBytes = byteCnt - q;
                switch (blkSize)
                    {
                    case  256: printf("\n:Skein-256: "); break;
                    case  512: printf("\n:Skein-512: "); break;
                    case 1024: printf("\n:Skein-1024:"); break;
                    }
                printf(" %4d-bit hash, msgLen =%6d bits",hashBits,msgBytes*8);
                printf(". Tree: leaf=%02X, node=%02X, maxLevels=%02X\n",leaf,node,maxLevel);
                printf("\nMessage data:\n");
                if (msgBytes == 0)
                    printf("    (none)\n");
                else
                    ShowBytes(msgBytes,msg);
                
                Skein_TreeHash(blkSize,hashBits,msg,msgBytes,leaf,node,maxLevel,hashVal);
                
                printf("Result:\n");
                ShowBytes((hashBits+7)/8,hashVal);
                printf("--------------------------------\n");
                }
            }
        }
    }
#endif

/*
** Output some KAT values. This output is generally re-directed to a file and
** can be compared across platforms to help validate an implementation on a
** new platform (or compare reference vs. optimized code, for example). The
** file will be provided as part of the Skein submission package to NIST.
**
** When used in conjunction with the debug flag, this will output a VERY long
** result. The verbose flag is used to output even more combinations of
**      <blkSize,hashSize,msgLen>
**
** Note: this function does NOT output the NIST AHS KAT format.
*/
void Skein_ShowKAT(uint_t blkSizeMask)
    {
    enum
        {
        DATA_TYPE_ZERO  = 0,
        DATA_TYPE_INC,
        DATA_TYPE_RAND,
        DATA_TYPE_MAC,
        DATA_TYPE_TREE,
        DATA_TYPE_CNT,

        MAX_BYTES = 3*1024/8
        };
    static const char *TYPE_NAMES[] = { "zero","incrementing","random","random+MAC","tree",NULL };
    static const uint_t  MSG_BITS[] =
                { 0,1,2,3,4,5,6,7,8,9,10,32,64,128,192,
                   256-1, 256, 256+1,  384,
                   512-1, 512, 512+1,  768,
                  1024-1,1024,1024+1,
                  2048-1,2048,2048+1
                };
#define MSG_BITS_CNT (sizeof(MSG_BITS)/sizeof(MSG_BITS[0]))

    uint_t      i,j,k,blkSize,dataType,hashBits,msgBits,keyBytes,blkBytes,keyType;
    u08b_t      data[MAX_BYTES+4],key[MAX_BYTES+4],hashVal[MAX_BYTES+4];
    const char *msgType;
    hashState   s;

    Rand_Init(SKEIN_MK_64(0xDEADBEEF,0)); /* init PRNG with repeatable value */
    katHash = ~0u;
    keyType =  0;

#ifdef SKEIN_DEBUG
    /* first, show some "raw" Threefish + feedforward block calls, with round-by-round debug info if enabled */
    if (skein_DebugFlag && !(verbose & V_KAT_NO_3FISH))
        {
        k = skein_DebugFlag;                                        /* save debug flag value */
        skein_DebugFlag  = THREEFISH_DEBUG_ALL & ~ SKEIN_DEBUG_HDR; /* turn on full debug detail, use Threefish name */
        skein_DebugFlag |= (k & SKEIN_DEBUG_PERMUTE);
#else
    if (verbose & V_KAT_DO_3FISH)                                   /* allow non-SKEIN_DEBUG testing */
        {
#endif
        for (blkSize = 256;blkSize <= 1024; blkSize*=2)
            {
            if (blkSizeMask && (blkSize & blkSizeMask) == 0)
                continue;
            for (dataType=DATA_TYPE_ZERO; dataType <= DATA_TYPE_INC; dataType++)
                {
                switch (dataType)
                    {
                    case DATA_TYPE_ZERO:
                            memset(data,0,sizeof(data));
                            memset(key ,0,sizeof(key));
                            break;
                    case DATA_TYPE_INC:
                            for (i=0;i<MAX_BYTES;i++)
                                {
                                key [i] = (u08b_t)      i ;
                                data[i] = (u08b_t) ~key[i];
                                }
                            break;
                    default:
                        continue;
                    }
#ifdef SKEIN_DEBUG
                switch (blkSize)
                    {
                    case  256: printf("\n:Threefish-256: "); break;
                    case  512: printf("\n:Threefish-512: "); break;
                    case 1024: printf("\n:Threefish-1024:"); break;
                    }
                printf(" encryption + plaintext feedforward (round-by-round):\n");
#endif
                memset(&s,0,sizeof(s));
                s.u.h.hashBitLen = blkSize;
                Skein_Get64_LSB_First(s.u.h.T      ,key,2);               /* init T[] */
                Skein_Get64_LSB_First(s.u.ctx1024.X,key+2*8,blkSize/64);  /* init X[] */
                switch (blkSize)
                    {
                    case  256: Skein_256_Process_Block(&s.u.ctx_256,data,1,0); break;
                    case  512: Skein_512_Process_Block(&s.u.ctx_512,data,1,0); break;
                    case 1024: Skein1024_Process_Block(&s.u.ctx1024,data,1,0); break;
                    }
#ifdef SKEIN_DEBUG
                printf("++++++++++++++++++++++++++++++++++++++\n");
#endif
                }
            }
#ifdef SKEIN_DEBUG
        skein_DebugFlag = k;
#endif
        }

    for (dataType=DATA_TYPE_ZERO; dataType < DATA_TYPE_CNT; dataType++)
        {
        msgType = TYPE_NAMES[dataType];
        switch (dataType)
            {
            case DATA_TYPE_ZERO:
                    memset(data,0,sizeof(data));
                    memset(key ,0,sizeof(key));
                    break;
            case DATA_TYPE_INC:
                    for (i=0;i<MAX_BYTES;i++)
                        {
                        key [i] = (u08b_t)      i ;
                        data[i] = (u08b_t) ~key[i];
                        }
                    break;
            case DATA_TYPE_MAC:
                    RandBytes(key ,sizeof(key ));
            case DATA_TYPE_RAND:
                    RandBytes(data,sizeof(data));
                    break;
            case DATA_TYPE_TREE:
                    if (verbose & V_KAT_NO_TREE)
                        continue;
                    break;
            default:    /* should never get here */
                    FatalError("Invalid data type: %d --> '%s'",dataType,msgType);
                    break;
            }
        
        for (blkSize = 256;blkSize <= 1024; blkSize*=2)
            {
            if (blkSizeMask && (blkSize & blkSizeMask) == 0)
                continue;
            if (dataType == DATA_TYPE_TREE)
                {
#if SKEIN_TREE_HASH
                Skein_GenKAT_Tree(blkSize);
#endif
                continue;
                }
            if (verbose & V_KAT_NO_SEQ)
                continue;
            blkBytes = blkSize/8;
            for (j=0;j <  MSG_BITS_CNT;j++)
            for (k=0;k < HASH_BITS_CNT;k++)
                {
                msgBits  =  MSG_BITS[j];  /* message length   */
                hashBits = HASH_BITS[k];  /* hash result size */
                assert(MAX_BYTES*8 >= hashBits && MAX_BYTES*8 >= msgBits);
                if (msgBits != 1024 && hashBits != blkSize && !(verbose & V_KAT_LONG))
                    continue;   /* keep the output size reasonable, unless verbose */
                if (verbose & V_KAT_SHORT)
                    {           /* -v2 ==> generate "short" KAT set by filtering out most vectors */
                    if (dataType != DATA_TYPE_INC)
                        continue;
                    if (msgBits != 8 && msgBits != blkSize && msgBits != 2*blkSize)
                        continue;
                    if (!Short_KAT_OK(blkSize,hashBits))
                        continue;
                    }
                switch (blkSize)
                    {
                    case  256: printf("\n:Skein-256: "); break;
                    case  512: printf("\n:Skein-512: "); break;
                    case 1024: printf("\n:Skein-1024:"); break;
                    }
                printf(" %4d-bit hash, msgLen =%6d bits",hashBits,msgBits);
                if (!(verbose & V_KAT_SHORT))
                    printf(", data = '%s'",msgType);
                printf("\n\nMessage data:\n");
                if (msgBits == 0)
                    printf("    (none)\n");
                else
                    ShowBytes((msgBits+7)/8,data);
                switch (dataType)
                    {
                    default:                            /* straight hash value */
                        if (Skein_Hash(blkSize,hashBits,data,msgBits,hashVal) != SKEIN_SUCCESS)
                            FatalError("Skein_Hash() error!");
                        break;
                    case DATA_TYPE_MAC:                 /* include some MAC computations in KAT file */
                        switch (keyType++)              /* sequence thru different MAC key lengths */
                            {           
                            case 0: keyBytes = blkBytes/2;   break;
                            case 1: keyBytes = blkBytes;     break;
                            case 2: keyBytes = blkBytes  +1; break;
                            case 3: keyBytes = blkBytes*2+1; break;
                            default:keyBytes = 0;       /* not actually a MAC this time, but use InitExt() */
                                    keyType  = 0;       /* start the cycle again next time */
                            }
                        printf("MAC key = %4d bytes:\n",keyBytes);
                        if (keyBytes)                   /* show MAC key, if any */
                            ShowBytes(keyBytes,key);
                        else
                            printf("    (none)          /* use InitExt() call */\n");

                        if (Skein_InitExt(blkSize,&s,hashBits,SKEIN_CFG_TREE_INFO_SEQUENTIAL,key,keyBytes) != SKEIN_SUCCESS)
                            FatalError("Skein_InitExt() error!");
                        if (Skein_Update(&s,data,msgBits) != SKEIN_SUCCESS)
                            FatalError("Skein_Update() error!");
                        if (Skein_Final(&s,hashVal) != SKEIN_SUCCESS)
                            FatalError("Skein_Final() error!");
                        break;
                    case DATA_TYPE_TREE:
                        assert(0);
                        break;
                    }
                printf("Result:\n");
                ShowBytes((hashBits+7)/8,hashVal);
                printf("--------------------------------\n");
                }
            }
        }
    if (!_quiet_)
        fprintf(stderr,"katHash = %08X\n",katHash ^ 0x150183D2);
    }

/* generate pre-computed IVs for inclusion in Skein C code */
void Skein_GenerateIV(void)
    {
    static const struct
        { uint_t blkSize,hashBits; }
            IV_TAB[] = /* which pairs to precompute */
                { { 256, 128 }, { 256, 160 }, { 256, 224 }, { 256, 256 },
                  { 512, 128 }, { 512, 160 }, { 512, 224 }, { 512, 256 },
                  { 512, 384 }, { 512, 512 },
                  {1024, 384 }, {1024, 512 }, {1024,1024 }
                };
    uint_t       i,j,blkSize,hashBits;
    hashState    state;
    const u64b_t *w;
    const char   *s;

    printf("#ifndef _SKEIN_IV_H_\n"
           "#define _SKEIN_IV_H_\n\n"
           "#include \"skein.h\"    /* get Skein macros and types */\n\n"
           "/*\n"
           "***************** Pre-computed Skein IVs *******************\n"
           "**\n"
           "** NOTE: these values are not \"magic\" constants, but\n"
           "** are generated using the Threefish block function.\n"
           "** They are pre-computed here only for speed; i.e., to\n"
           "** avoid the need for a Threefish call during Init().\n"
           "**\n"
           "** The IV for any fixed hash length may be pre-computed.\n"
           "** Only the most common values are included here.\n"
           "**\n"
           "************************************************************\n"
           "**/\n\n"
           "#define MK_64 SKEIN_MK_64\n\n"
          );
    for (i=0;i < sizeof(IV_TAB)/sizeof(IV_TAB[0]); i++)
        {
        blkSize  = IV_TAB[i].blkSize;
        hashBits = IV_TAB[i].hashBits;
        switch (blkSize)
            {
            case  256:  w = state.u.ctx_256.X;  s = "_256"; break;
            case  512:  w = state.u.ctx_512.X;  s = "_512"; break;
            case 1024:  w = state.u.ctx1024.X;  s = "1024"; break;
            default:    FatalError("Invalid blkSize");
                        continue; /* should never happen, but avoids gcc warning */
            }
        if (Skein_Init(blkSize,&state,hashBits) != SKEIN_SUCCESS)
            FatalError("Error generating IV: blkSize=%d, hashBits=%d",blkSize,hashBits);
        printf("/* blkSize = %4d bits. hashSize = %4d bits */\n",blkSize,hashBits);
        printf("const u64b_t SKEIN%s_IV_%d[] =\n    {\n",s,hashBits);
        for (j=0;j<blkSize/64;j++)
            printf("    MK_64(0x%08X,0x%08X)%s\n",
                   (uint_32t)(w[j] >> 32),(uint_32t)w[j],(j+1 == blkSize/64)?"":",");
        printf("    };\n\n");
        }
    printf("#endif /* _SKEIN_IV_H_ */\n");
    }

/* qsort routine */
int compare_uint_32t(const void *aPtr,const void *bPtr)
    {
    uint_32t a = * ((uint_32t *) aPtr);
    uint_32t b = * ((uint_32t *) bPtr);
    
    if (a > b) return  1;
    if (a < b) return -1;
    return 0;
    }

void ShowCompiler(const char *CVER)
    {
    printf(" //:");
#if defined(SKEIN_XMM)
    printf(" 32-XMM, ");
#else
    printf(" %2u-bit, ",(uint_t)(8*sizeof(size_t)));
#endif
    printf("%s%s",COMPILER_ID,CVER);

    /* do we need to show unroll amount? */
#if defined(SKEIN_USE_ASM) && SKEIN_USE_ASM
    printf(" [asm=");
#define _SC_DO_LOOP_ (1)
#elif defined(SKEIN_LOOP)
    printf(" [ C =");
#define _SC_DO_LOOP_ (1)
#endif

#ifdef  _SC_DO_LOOP_
    printf("%c",(Skein_256_Unroll_Cnt())?'0'+Skein_256_Unroll_Cnt():'.');
    printf("%c",(Skein_512_Unroll_Cnt())?'0'+Skein_512_Unroll_Cnt():'.');
    printf("%c",(Skein1024_Unroll_Cnt())?'0'+Skein1024_Unroll_Cnt():'.');
    printf("]");
#endif
    }

/* measure the speed (in CPU clks/byte) for a Skein implementation */
void Skein_MeasurePerformance(const char *target)
    {
    const uint_t MSG_BYTES[] = {1,2,4,8,10,16,32,64,100,128,256,512,1000,1024,2048,4096,8192,10000,16384,32768,100000,0};
    enum     {  TIMER_SAMPLE_CNT = 13, MAX_BUFFER=1024*100, PERF_TIMEOUT_CLKS = 500000 };
    enum     {  _256 = 256, _512 = 512 };
    uint_32t dt[24][3][TIMER_SAMPLE_CNT],t0,t1;
    uint_32t dtMin = ~0u;
    uint_t   targetSize = 0;
    uint_t   repCnt     = 1;
    uint_t   i,k,n,r,blkSize,msgBytes;
    u08b_t   b[MAX_BUFFER],hashVal[SKEIN1024_BLOCK_BYTES*4];
    hashState s;
#ifdef CompilerVersion
    char     CVER[20];                      /* avoid ANSI compiler warnings for sprintf()! :-(( */
    n          = CompilerVersion;
    CVER[0]    = '_';
    CVER[1]    = 'v';
    CVER[2]    = (char)('0'+((n /100)%10));
    CVER[3]    = '.';
    CVER[4]    = (char)('0'+((n / 10)%10));
    CVER[5]    = (char)('0'+((n /  1)%10));
    CVER[6]    = 0;
#else
#define CVER ""
#endif      
    if (target && target[0])
        {
        targetSize = atoi(target);
        for (i=0;target[i];i++)
            if (target[i] == '.')
                {
                repCnt = atoi(target+i+1);
                break;
                }
        if (repCnt == 0)
            repCnt = 1;
        }

    assert(sizeof(dt)/(3*TIMER_SAMPLE_CNT*sizeof(dt[0][0][0])) >=
           sizeof(MSG_BYTES)/sizeof(MSG_BYTES[0]));
    if (OS_Set_High_Priority())
        printf("Unable to set thread to high priority\n");
    fflush(stdout);                     /* let things calm down */
    OS_Sleep(200);                      /* let things settle down for a bit */
    memset(dt,0,sizeof(dt));
    RandBytes(b,sizeof(b));             /* use random data for testing */
    for (i=0;i<4*TIMER_SAMPLE_CNT;i++)  /* calibrate the overhead for measuring time */
        {
        t0 = HiResTime();
        t1 = HiResTime();
        if (dtMin > t1-t0)              /* keep only the minimum time */
            dtMin = t1-t0;
        }
    for (r=0;r<repCnt;r++)
        {
        /* first take all the data and store it in dt, with no printf() activity */
        for (n=0;n < sizeof(MSG_BYTES)/sizeof(MSG_BYTES[0]);n++)
            {
            msgBytes = MSG_BYTES[n];        /* pick the message size (in bits) */
            if (msgBytes > MAX_BUFFER || msgBytes == 0)
                break;
            if (targetSize && targetSize != msgBytes)
                continue;
            for (k=0;k<3;k++)
                {                           /* cycle thru the different block sizes */
                blkSize=256 << k;
                t0=HiResTime();
                t1=HiResTime();
#define OneTest(BITS)                                           \
                Skein##BITS##_Init  (&s.u.ctx##BITS,BITS);      \
                Skein##BITS##_Update(&s.u.ctx##BITS,b,msgBytes);\
                Skein##BITS##_Final (&s.u.ctx##BITS,hashVal);

                OS_Sleep(0);                        /* yield the time slice to OS */
                for (i=0;i<TIMER_SAMPLE_CNT;i++)
                    {
                    HiResTime();                    /* prime the pump */
                    switch (blkSize)
                        {
                        case  256:
                            OneTest(_256);          /* prime the pump */
                            t0 = HiResTime();
                            OneTest(_256);          /* do it twice for some averaging */
                            OneTest(_256);
                            t1 = HiResTime();
                            break;
                        case  512:
                            OneTest(_512);
                            t0 = HiResTime();
                            OneTest(_512);
                            OneTest(_512);
                            t1 = HiResTime();
                            break;
                        case 1024:
                            OneTest(1024);
                            t0 = HiResTime();
                            OneTest(1024);
                            OneTest(1024);
                            t1 = HiResTime();
                            break;
                        }
                    dt[n][k][i] = ((t1 - t0) - dtMin)/2; /* adjust for HiResTime() overhead */
                    }
                }
            }
        OS_Set_Normal_Priority();

        if (targetSize == 0)
            {
            printf("\nSkein performance, in clks per byte, dtMin = %4d clks.\n",dtMin);
            printf("         [compiled %s,%s  by  '%s%s', %u-bit]\n",__TIME__,__DATE__,COMPILER_ID,CVER,(uint_t)(8*sizeof(size_t)));
            printf("         =================================================================\n");
            printf("         ||                       Skein block size                       |\n");
            printf("         ||--------------------------------------------------------------|\n");
            printf(" Message ||       256 bits     |       512 bits     |      1024 bits     |\n");
            printf(" Length  ||====================|====================|====================|\n");
            printf(" (bytes) ||     min    median  |     min    median  |     min    median  |\n"); 
            printf("=========||====================|====================|====================|\n");
            }

        /* now display the results */
        for (n=0;n < sizeof(MSG_BYTES)/sizeof(MSG_BYTES[0]);n++)
            {
            msgBytes = MSG_BYTES[n];       /* pick the message size (in bits) */
            if (msgBytes > MAX_BUFFER || msgBytes == 0)
                break;
            if (targetSize && targetSize != msgBytes)
                continue;
            printf("%7d_ ||",msgBytes);
            for (k=0;k<3;k++)              /* cycle thru the different Skein block sizes */
                {   /* here with dt[n][k][] full of time differences */
                    /* discard high/low, then show min/median of the rest, in clks/byte */
                qsort(dt[n][k],TIMER_SAMPLE_CNT,sizeof(dt[0][0][0]),compare_uint_32t);
                printf(" %8.2f %8.2f  |",dt[n][k][1]/(double)msgBytes,dt[n][k][TIMER_SAMPLE_CNT/2]/(double)msgBytes);
                }
            ShowCompiler(CVER);
            printf("\n");
            if (targetSize == 0 && target && target[0] && repCnt == 1)
                {   /* show the details */
                for (k=0;k<3;k++)
                    {
                    printf("%4d: ",256 << k);
                    for (i=0;i<TIMER_SAMPLE_CNT;i++)
                        printf("%8d",dt[n][k][i]);
                    printf("\n");
                    }
                }
            }
        }
#if defined(SKEIN_CODE_SIZE) || defined(SKEIN_PERF)
    if (targetSize == 0)
        {
        printf("=========||====================|====================|====================|\n");
        printf("Code Size||                    |                    |                    |\n");
        printf("=========||====================|====================|====================|\n");
        printf("    API  || %12d bytes | %12d bytes | %12d bytes |",
               (int) Skein_256_API_CodeSize(),
               (int) Skein_512_API_CodeSize(),
               (int) Skein1024_API_CodeSize());
        ShowCompiler(CVER);
        printf("\n");
        printf("  Block  || %12d bytes | %12d bytes | %12d bytes |",
               (int) Skein_256_Process_Block_CodeSize(),
               (int) Skein_512_Process_Block_CodeSize(),
               (int) Skein1024_Process_Block_CodeSize());
        ShowCompiler(CVER);
        printf("\n");
        }
#endif
    }

void GiveHelp(void)
    {
    printf("Syntax:  skein_test [options]\n"
           "Options: -bNN  = set Skein block size to NN bits\n"
           "         -lNN  = set max test length  to NN bits\n"
           "         -tNN  = set Skein hash length to NN bits\n"
           "         -sNN  = set initial random seed\n"
           "         -g    = generate precomputed IV values to stdout\n"
           "         -k    = output KAT results to stdout\n"
           "         -p    = output performance (clks/byte)\n"
          );
    exit(2);
    }
                   
int main(int argc,char *argv[])
    {
    int    i,n;
    uint_t testCnt;
    uint_t doKAT   =    0;   /* generate KAT vectors?    */
    uint_t blkSize =    0;   /* Skein state size in bits */
    uint_t maxLen  = 1024;   /* max block size   in bits */
    uint_t hashLen =    0;   /* hash length      in bits (0 --> all) */
    uint_t seed0   = (uint_t) time(NULL); /* randomize based on time */
    uint_t oneBlk  =    0;   /* test block size */

    for (i=1;i<argc;i++)
        {   /* process command-line switches */
        if (argv[i][0] == '-')
            {
            switch(toupper(argv[i][1]))
                {
                case '?': GiveHelp();                         break;
                case 'B': blkSize       |= atoi(argv[i]+2);   break;
                case 'L': maxLen         = atoi(argv[i]+2);   break;
                case 'S': seed0          = atoi(argv[i]+2);   break;
                case 'T': hashLen        = atoi(argv[i]+2);   break;
                case 'K': doKAT          = 1;                 break;
                case 'V': verbose       |= (argv[i][2]) ? atoi(argv[i]+2) : V_KAT_LONG; break;
                case 'G': Skein_GenerateIV();                 return 0;
                case 'P': Skein_MeasurePerformance(argv[i]+2);return 0;
                case 'Q': _quiet_        = 1;                 break;
                case 'D': switch (toupper(argv[i][2]))
                              {
#ifdef SKEIN_DEBUG
                              case  0 : skein_DebugFlag |= SKEIN_DEBUG_DEFAULT; break;
                              case '-': skein_DebugFlag |= SKEIN_DEBUG_SHORT;   break;
                              case '+': skein_DebugFlag |= SKEIN_DEBUG_ALL;     break;
                              case 'P': skein_DebugFlag |= SKEIN_DEBUG_PERMUTE; break;
                              case 'I': skein_DebugFlag |= SKEIN_DEBUG_SHORT |  SKEIN_DEBUG_INJECT; break;
                              case 'C': skein_DebugFlag |= SKEIN_DEBUG_SHORT & ~SKEIN_DEBUG_CONFIG; break;
#endif
                              default : skein_DebugFlag |= atoi(argv[i]+2);     break;
                              }
                          break;
                default:  FatalError("Unsupported command-line option: %s",argv[i]);
                          break;
                }
            }
        else if (argv[i][0] == '?')
            GiveHelp();
        else if (isdigit(argv[i][0]))
            oneBlk = atoi(argv[i]);
        }

    if (blkSize == 0)                     /* default is all block sizes */
        blkSize = 256 | 512 | 1024;
    if (doKAT)
        {
        Skein_ShowKAT(blkSize);
        }
    else
        {
        if (oneBlk == 0)
            printf("Seed0 = %d. Compiler = %s\n",seed0,COMPILER_ID);
        Rand_Init(SKEIN_MK_64(0xDEADBEEF,seed0)); /* init PRNG for test data */

        testCnt=0;
        for (i=256;i<=1024;i*=2)
            {
            if (blkSize & i)
                {
                if (hashLen == 0)              /* use all hash sizes? */
                    {
                    for (n=0;n < HASH_BITS_CNT;n++)
                        testCnt += Skein_Test(i,maxLen,HASH_BITS[n],0,oneBlk);
                    }
                else
                    testCnt += Skein_Test(i,maxLen,hashLen,0,oneBlk);
                }
            }
        if (oneBlk)
            return 0;
        if (testCnt)
            printf("Success: %4d tests\n",testCnt);
        }
    /* do a quick final self-consistentcy check test to make sure nothing is broken */
    skein_DebugFlag = 0;        /* no debug output here */
    for (blkSize = 256;blkSize <= 1024; blkSize*=2)
        {
        Skein_Test(blkSize,16,0,1,0);
        }

    return 0;
    }
