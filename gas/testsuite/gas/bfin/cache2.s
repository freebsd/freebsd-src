
.EXTERN MY_LABEL2;
.section .text;

//
//12 CACHE CONTROL
//

//PREFETCH [ Preg ] ; /* indexed (a) */
PREFETCH [ P0 ] ;
PREFETCH [ P1 ] ;
PREFETCH [ P2 ] ;
PREFETCH [ P3 ] ;
PREFETCH [ P4 ] ;
PREFETCH [ P5 ] ;
PREFETCH [ SP ] ;
PREFETCH [ FP ] ;

//PREFETCH [ Preg ++ ] ; /* indexed, post increment (a) */
PREFETCH [ P0++ ] ;
PREFETCH [ P1++ ] ;
PREFETCH [ P2++ ] ;
PREFETCH [ P3++ ] ;
PREFETCH [ P4++ ] ;
PREFETCH [ P5++ ] ;
PREFETCH [ SP++ ] ;
PREFETCH [ FP++ ] ;

//FLUSH [ Preg ] ; /* indexed (a) */
FLUSH [ P0 ] ;
FLUSH [ P1 ] ;
FLUSH [ P2 ] ;
FLUSH [ P3 ] ;
FLUSH [ P4 ] ;
FLUSH [ P5 ] ;
FLUSH [ SP ] ;
FLUSH [ FP ] ;
//FLUSH [ Preg ++ ] ; /* indexed, post increment (a) */
FLUSH [ P0++ ] ;
FLUSH [ P1++ ] ;
FLUSH [ P2++ ] ;
FLUSH [ P3++ ] ;
FLUSH [ P4++ ] ;
FLUSH [ P5++ ] ;
FLUSH [ SP++ ] ;
FLUSH [ FP++ ] ;

//FLUSHINV [ Preg ] ; /* indexed (a) */
FLUSHINV [ P0 ] ;
FLUSHINV [ P1 ] ;
FLUSHINV [ P2 ] ;
FLUSHINV [ P3 ] ;
FLUSHINV [ P4 ] ;
FLUSHINV [ P5 ] ;
FLUSHINV [ SP ] ;
FLUSHINV [ FP ] ;

//FLUSHINV [ Preg ++ ] ; /* indexed, post increment (a) */
FLUSHINV [ P0++ ] ;
FLUSHINV [ P1++ ] ;
FLUSHINV [ P2++ ] ;
FLUSHINV [ P3++ ] ;
FLUSHINV [ P4++ ] ;
FLUSHINV [ P5++ ] ;
FLUSHINV [ SP++ ] ;
FLUSHINV [ FP++ ] ;

//IFLUSH [ Preg ] ; /* indexed (a) */
IFLUSH [ P0 ] ;
IFLUSH [ P1 ] ;
IFLUSH [ P2 ] ;
IFLUSH [ P3 ] ;
IFLUSH [ P4 ] ;
IFLUSH [ P5 ] ;
IFLUSH [ SP ] ;
IFLUSH [ FP ] ;

//IFLUSH [ Preg ++ ] ; /* indexed, post increment (a) */
IFLUSH [ P0++ ] ;
IFLUSH [ P1++ ] ;
IFLUSH [ P2++ ] ;
IFLUSH [ P3++ ] ;
IFLUSH [ P4++ ] ;
IFLUSH [ P5++ ] ;
IFLUSH [ SP++ ] ;
IFLUSH [ FP++ ] ;
