/*
 * test relax
 * mv <-> mv!   : for mv! : register number must be in 0-15
 * mv <-> mhfl! : for mhfl! : rD must be in 16-31, rS must be in 0-15
 * mv <-> mlfh! : for mhfl! : rD must be in 0-15, rS must be in 16-31

 * Author: ligang
 */

/* This block test mv -> mv! */
.align 4

  mv  r0, r15      #32b -> 16b
  mv! r0, r15

  mv  r15, r15     #32b -> 16b
  mv! r15, r15

  mv  r3, r5       #32b -> 16b
  mv  r3, r5       #32b -> 16b

  mv! r6, r7
  mv  r6, r7       #32b -> 16b

  mv  r8, r10      #No transform
  mv  r21, r23

/* This block test mv! -> mv */
.align 4

  mv! r0, r15      #16b -> 32b      
  mv  r23, r27

  mv! r2, r8       #No transform      
  mv! r2, r8       #No transform

  mv! r2, r8       #No transform      
  mv  r2, r8       

/* This block test mv -> mhfl! */
.align 4

  mv    r31, r0        #32b -> 16b
  mhfl! r31, r0

  mv    r16, r15       #32b -> 16b
  mv!   r16, r15

  mv    r23, r5        #32b -> 16b
  mv    r23, r5        #32b -> 16b

  mhfl! r26, r7
  mv    r26, r7        #32b -> 16b

  mv    r28, r10       #No transform
  mv    r21, r23

/* This block test mhfl! -> mv */
.align 4

  mhfl! r31, r0       #16b -> 32b      
  mv    r23, r27

  mhfl! r22, r8       #No transform      
  mhfl! r22, r8       #No transform

  mhfl! r23, r15      #No transform      
  mv    r23, r15       

/* This block test mv -> mlfh! */
.align 4

  mv    r0, r31        #32b -> 16b
  mlfh! r0, r31

  mv    r15, r16       #32b -> 16b
  mv!   r15, r16

  mv    r5, r23        #32b -> 16b
  mv    r5, r23        #32b -> 16b

  mlfh! r7, r26
  mv    r7, r26        #32b -> 16b

  mv    r10, r28       #No transform
  mv    r21, r23

/* This block test mhfl! -> mv */
.align 4

  mlfh! r0, r31       #16b -> 32b      
  mv    r23, r27

  mlfh! r8, r22       #No transform      
  mlfh! r8, r22       #No transform

  mlfh! r15, r23      #No transform      
  mv    r15, r23       
