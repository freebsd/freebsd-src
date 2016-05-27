#include <stdio.h>
#include "skein.h"

#define   SKEIN_CODE_SIZE (1)       /* instantiate code size routines */
#define   SKEIN_LOOP    (111)       /* unroll only 8 rounds */
#define   SKEIN_USE_ASM (512+1024)  /* what to exclude here */
#include "skein.c"
#include "skein_block.c"

/* for code size limitations, make "dummy" versions of unused block functions */
#if SKEIN_USE_ASM & 256
void Skein_256_Process_Block(Skein_256_Ctxt_t *ctx,const u08b_t *blkPtr,size_t blkCnt,size_t byteCntAdd) { }
#endif
#if SKEIN_USE_ASM & 512
void Skein_512_Process_Block(Skein_512_Ctxt_t *ctx,const u08b_t *blkPtr,size_t blkCnt,size_t byteCntAdd) { }
#endif
#if SKEIN_USE_ASM & 1024
void Skein1024_Process_Block(Skein1024_Ctxt_t *ctx,const u08b_t *blkPtr,size_t blkCnt,size_t byteCntAdd) { }
#endif

const u08b_t msg[1] = 
  {
  0
  };

int main(int argc,char *argv[])
    {
    u08b_t hash[1024/8];
	u08b_t i,x;
    static size_t aBytes,bBytes,uCount;

#if !(SKEIN_USE_ASM & 256)
    Skein_256_Ctxt_t ctx;

    aBytes = 2*Skein_256_API_CodeSize();
	bBytes = 2*Skein_256_Process_Block_CodeSize();
	uCount =   Skein_256_Unroll_Cnt();

    Skein_256_Init  (&ctx,256);
	Skein_256_Update(&ctx,msg,sizeof(msg));
	Skein_256_Final (&ctx,hash);

    Skein_256_Process_Block(&ctx,msg,1,256);
#endif

#if !(SKEIN_USE_ASM & 512)
    Skein_512_Ctxt_t ctx;

    aBytes = 2*Skein_512_API_CodeSize();
	bBytes = 2*Skein_512_Process_Block_CodeSize();
	uCount =   Skein_512_Unroll_Cnt();

    Skein_512_Init  (&ctx,512);
	Skein_512_Update(&ctx,msg,sizeof(msg));
	Skein_512_Final (&ctx,hash);

    Skein_512_Process_Block(&ctx,msg,1,512);
#endif

#if !(SKEIN_USE_ASM & 1024)
    Skein1024_Ctxt_t ctx;

    aBytes = 2*Skein1024_API_CodeSize();
	bBytes = 2*Skein1024_Process_Block_CodeSize();
	uCount =   Skein1024_Unroll_Cnt();

    Skein1024_Init  (&ctx,1024);
	Skein1024_Update(&ctx,msg,sizeof(msg));
	Skein1024_Final (&ctx,hash);

    Skein1024_Process_Block(&ctx,msg,1,1024);
#endif
    printf("API size = %4d bytes. Block size = %4d bytes. Unroll=%d\n",
	          aBytes,bBytes,uCount);
    for (i=x=0;i<5;i++)
	    printf("hash[%d] = %02X [%02X]\n",i,hash[i],x ^= hash[i]);
    }
