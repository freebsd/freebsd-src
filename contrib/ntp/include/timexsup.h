/*
 * timexsup.h - 'struct timex' support functions
 *
 * Written by Juergen Perlinger (perlinger@ntp.org) for the NTP project.
 * The contents of 'html/copyright.html' apply.
 */
#ifndef TIMEXSUP_H
#define TIMEXSUP_H


/* convert a 'long' time value (in usec or nsec) into seconds, expressed
 * as a 'double'. If 'STA_NANO' is not defined, this will always convert
 * from usec. ('STA_NANO' is Linux specific at the time of this
 * writing.)
 *
 * If 'STA_NANO' is defined, it will be checked in 'status' to decide
 * which time base (usec or nsec) applies for this conversion.
 */
extern double dbl_from_var_long(long lval, int status);

/* convert a 'long' time value in usec into seconds, expressed as
 * 'double'.  This function is there for pure symmetry right now -- it
 * just casts and scales without any additional bells and whistles.
 */ 
extern double dbl_from_usec_long(long lval);

/* If MOD_NANO is defined, set the MOD_NANO bit in '*modes' and
 * calculate the time stamp in nsec; otherwise, calculate the result in
 * usec.
 *
 * Applies proper bounds checks and saturation on LONG_MAX/LONG_MIN to
 * avoid undefined behaviour.
 */
extern long var_long_from_dbl(double dval, unsigned int *modes);

/* convert a 'double' time value (in seconds) into usec with proper
 * bounds check and range clamp.
 */
extern long usec_long_from_dbl(double dval);

#endif
/* -*- that's all folks -*- */
