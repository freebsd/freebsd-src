# $FreeBSD$

ARCH_SRCS = e_acos.S e_asin.S e_atan2.S e_exp.S e_fmod.S e_log.S e_log10.S \
	    e_remainder.S e_scalb.S e_sqrt.S s_atan.S s_ceil.S s_copysign.S \
	    s_cos.S s_finite.S s_floor.S s_llrint.S s_logb.S s_lrint.S \
	    s_rint.S s_scalbn.S s_significand.S s_sin.S s_tan.S

# float counterparts
ARCH_SRCS+= e_atan2f.S e_log10f.S e_logf.S e_remainderf.S e_scalbf.S \
	    e_sqrtf.S s_ceilf.S s_copysignf.S s_cosf.S s_floorf.S s_logbf.S \
	    s_rintf.S s_scalbnf.S s_significandf.S s_sinf.S s_tanf.S
