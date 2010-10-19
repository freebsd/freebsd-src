#include "te-linux.h"

/* The EABI requires the use of VFP.  */
#define FPU_DEFAULT FPU_ARCH_VFP_V2
#define EABI_DEFAULT EF_ARM_EABI_VER4
