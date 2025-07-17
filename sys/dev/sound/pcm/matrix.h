/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2007-2009 Ariff Abdullah <ariff@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _SND_MATRIX_H_
#define _SND_MATRIX_H_

#undef SND_MULTICHANNEL
#ifndef SND_OLDSTEREO
#define SND_MULTICHANNEL	1
#endif

/*
 * XXX = unused, but part of the definition (will be used someday, maybe).
 */
#define SND_CHN_T_FL		0	/* Front Left         */
#define SND_CHN_T_FR		1	/* Front Right        */
#define SND_CHN_T_FC		2	/* Front Center       */
#define SND_CHN_T_LF		3	/* Low Frequency      */
#define SND_CHN_T_BL		4	/* Back Left          */
#define SND_CHN_T_BR		5	/* Back Right         */
#define SND_CHN_T_FLC		6	/* Front Left Center  XXX */
#define SND_CHN_T_FRC		7	/* Front Right Center XXX */
#define SND_CHN_T_BC		8	/* Back Center        */
#define SND_CHN_T_SL		9	/* Side Left          */
#define SND_CHN_T_SR		10	/* Side Right         */
#define SND_CHN_T_TC		11	/* Top Center         XXX */
#define SND_CHN_T_TFL		12	/* Top Front Left     XXX */
#define SND_CHN_T_TFC		13	/* Top Front Center   XXX */
#define SND_CHN_T_TFR		14	/* Top Front Right    XXX */
#define SND_CHN_T_TBL		15	/* Top Back Left      XXX */
#define SND_CHN_T_TBC		16	/* Top Back Center    XXX */
#define SND_CHN_T_TBR		17	/* Top Back Right     XXX */
#define SND_CHN_T_MAX		18	/* Maximum channels   */

#define SND_CHN_T_ZERO		(SND_CHN_T_MAX + 1)	/* Zero samples */

#define SND_CHN_T_LABELS	{					\
	 "fl",  "fr",  "fc",  "lf",  "bl",  "br",			\
	"flc", "frc",  "bc",  "sl",  "sr",  "tc",			\
	"tfl", "tfc", "tfr", "tbl", "tbc", "tbr"			\
}

#define SND_CHN_T_NAMES	{						\
	"Front Left", "Front Right", "Front Center",			\
	"Low Frequency Effects",					\
	"Back Left", "Back Right",					\
	"Front Left Center", "Front Right Center",			\
	"Back Center",							\
	"Side Left", "Side Right",					\
	"Top Center",							\
	"Top Front Left", "Top Front Center", "Top Front Right",	\
	"Top Back Left", "Top Back Center", "Top Back Right"		\
}

#define SND_CHN_T_MASK_FL	(1 << SND_CHN_T_FL)
#define SND_CHN_T_MASK_FR	(1 << SND_CHN_T_FR)
#define SND_CHN_T_MASK_FC	(1 << SND_CHN_T_FC)
#define SND_CHN_T_MASK_LF	(1 << SND_CHN_T_LF)
#define SND_CHN_T_MASK_BL	(1 << SND_CHN_T_BL)
#define SND_CHN_T_MASK_BR	(1 << SND_CHN_T_BR)
#define SND_CHN_T_MASK_FLC	(1 << SND_CHN_T_FLC)
#define SND_CHN_T_MASK_FRC	(1 << SND_CHN_T_FRC)
#define SND_CHN_T_MASK_BC	(1 << SND_CHN_T_BC)
#define SND_CHN_T_MASK_SL	(1 << SND_CHN_T_SL)
#define SND_CHN_T_MASK_SR	(1 << SND_CHN_T_SR)
#define SND_CHN_T_MASK_TC	(1 << SND_CHN_T_TC)
#define SND_CHN_T_MASK_TFL	(1 << SND_CHN_T_TFL)
#define SND_CHN_T_MASK_TFC	(1 << SND_CHN_T_TFC)
#define SND_CHN_T_MASK_TFR	(1 << SND_CHN_T_TFR)
#define SND_CHN_T_MASK_TBL	(1 << SND_CHN_T_TBL)
#define SND_CHN_T_MASK_TBC	(1 << SND_CHN_T_TBC)
#define SND_CHN_T_MASK_TBR	(1 << SND_CHN_T_TBR)

#define SND_CHN_LEFT_MASK	(SND_CHN_T_MASK_FL  |			\
				 SND_CHN_T_MASK_BL  |			\
				 SND_CHN_T_MASK_FLC |			\
				 SND_CHN_T_MASK_SL  |			\
				 SND_CHN_T_MASK_TFL |			\
				 SND_CHN_T_MASK_TBL)

#define SND_CHN_RIGHT_MASK	(SND_CHN_T_MASK_FR  |			\
				 SND_CHN_T_MASK_BR  |			\
				 SND_CHN_T_MASK_FRC |			\
				 SND_CHN_T_MASK_SR  |			\
				 SND_CHN_T_MASK_TFR |			\
				 SND_CHN_T_MASK_TBR)

#define SND_CHN_CENTER_MASK	(SND_CHN_T_MASK_FC  |			\
				 SND_CHN_T_MASK_BC  |			\
				 SND_CHN_T_MASK_TC  |			\
				 SND_CHN_T_MASK_TFC |			\
				 SND_CHN_T_MASK_TBC |			\
				 SND_CHN_T_MASK_LF)	/* XXX what?!? */

/*
 * Matrix identity.
 */

/* 1 @ Mono 1.0 */
#define SND_CHN_MATRIX_1_0	0
#define SND_CHN_MATRIX_1	SND_CHN_MATRIX_1_0

/* 2 @ Stereo 2.0 */
#define SND_CHN_MATRIX_2_0	1
#define SND_CHN_MATRIX_2	SND_CHN_MATRIX_2_0	

/* 3 @ 2.1 (lfe), 3.0 (rear center, DEFAULT) */
#define SND_CHN_MATRIX_2_1	2
#define SND_CHN_MATRIX_3_0	3
#define SND_CHN_MATRIX_3	SND_CHN_MATRIX_3_0

/* 4 @ 3.1 (lfe), 4.0 (Quadraphonic, DEFAULT) */
#define SND_CHN_MATRIX_3_1	4
#define SND_CHN_MATRIX_4_0	5
#define SND_CHN_MATRIX_4	SND_CHN_MATRIX_4_0

/* 5 @ 4.1 (lfe), 5.0 (center, DEFAULT) */
#define SND_CHN_MATRIX_4_1	6
#define SND_CHN_MATRIX_5_0	7
#define SND_CHN_MATRIX_5	SND_CHN_MATRIX_5_0

/* 6 @ 5.1 (lfe, DEFAULT), 6.0 (rear center) */
#define SND_CHN_MATRIX_5_1	8
#define SND_CHN_MATRIX_6_0	9
#define SND_CHN_MATRIX_6	SND_CHN_MATRIX_5_1

/* 7 @ 6.1 (lfe, DEFAULT), 7.0 */
#define SND_CHN_MATRIX_6_1	10
#define SND_CHN_MATRIX_7_0	11
#define SND_CHN_MATRIX_7	SND_CHN_MATRIX_6_1

/* 8 @ 7.1 (lfe) */
#define SND_CHN_MATRIX_7_1	12
#define SND_CHN_MATRIX_8	SND_CHN_MATRIX_7_1

#define SND_CHN_MATRIX_MAX	13

#define SND_CHN_MATRIX_BEGIN	SND_CHN_MATRIX_1_0
#define SND_CHN_MATRIX_END	SND_CHN_MATRIX_7_1

/* Custom matrix identity */
#define SND_CHN_MATRIX_DRV		-4	/* driver own identity   */
#define SND_CHN_MATRIX_PCMCHANNEL	-3	/* PCM channel identity  */
#define SND_CHN_MATRIX_MISC		-2	/* misc, custom defined  */
#define SND_CHN_MATRIX_UNKNOWN		-1	/* unknown               */

#define SND_CHN_T_VOL_0DB	SND_CHN_T_MAX
#define SND_CHN_T_VOL_MAX	(SND_CHN_T_VOL_0DB + 1)

#define SND_CHN_T_BEGIN		SND_CHN_T_FL
#define SND_CHN_T_END		SND_CHN_T_TBR
#define SND_CHN_T_STEP		1
#define SND_CHN_MIN		1

#ifdef SND_MULTICHANNEL
#define SND_CHN_MAX		8
#else
#define SND_CHN_MAX		2
#endif

/*
 * Multichannel interleaved volume matrix. Each calculated value relative
 * to master and 0db will be stored in each CLASS + 1 as long as
 * chn_setvolume_matrix() or the equivalent CHN_SETVOLUME() macros is
 * used (see channel.c).
 */
#define SND_VOL_C_MASTER	0
#define SND_VOL_C_PCM		1
#define SND_VOL_C_PCM_VAL	2
#define SND_VOL_C_MAX		3

#define SND_VOL_C_BEGIN		SND_VOL_C_PCM
#define SND_VOL_C_END		SND_VOL_C_PCM
#define SND_VOL_C_STEP		2

#define SND_VOL_C_VAL(x)	((x) + 1)

#define SND_VOL_0DB_MIN		1
#define SND_VOL_0DB_MAX		100

#define SND_VOL_0DB_MASTER	100
#define SND_VOL_0DB_PCM		45

#define SND_VOL_RESOLUTION	8
#define SND_VOL_FLAT		(1 << SND_VOL_RESOLUTION)

#define SND_VOL_CALC_SAMPLE(x, y)	(((x) * (y)) >> SND_VOL_RESOLUTION)

#define SND_VOL_CALC_VAL(x, y, z)					\
			(((((x)[y][z] << SND_VOL_RESOLUTION) /		\
			 (x)[y][SND_CHN_T_VOL_0DB]) *			\
			 (x)[SND_VOL_C_MASTER][z]) /			\
			 (x)[SND_VOL_C_MASTER][SND_CHN_T_VOL_0DB])	\

/*
 * Standard matrix maps:
 *
 * struct pcmchan_matrix {
 * 	.id = Matrix identity (see matrix.h). Custom defined should use
 * 	      one of SND_CHN_MATRIX_MISC (for whatever purposes) or
 * 	      SND_CHN_MATRIX_DRV (hardware driver).
 * 	.channels = Total number of channels, including whatever 'extended'
 * 		    (the X.ext notions, mostly LFE).
 * 	.ext = Total number of extended channels (LFE).
 * 	.map = {
 * 		Sequences of channel type and interleave structure.
 * 		[interleave offset] = {
 * 			.type = channel type (see matrix.h).
 * 			.members = Masks of channels that is acceptable as a
 * 				   member of this channel type.
 * 		},
 * 		[total channels] = {
 * 			.type = Maximum channels marker (SND_CHN_T_MAX).
 * 			.members = 0 (no channels allowed here).
 * 		},
 * 	},
 * 	.mask = Mask of channels that exist in this map.
 * 	.offset = {
 * 		channel offset that directly translate to the above interleave
 * 		offset according to SND_CHN_T_* definitions.
 * 	}
 * };
 *
 * Rule of thumb: Avoid using SND_CHN_T_* that is marked with XXX (matrix.h),
 *                or be prepared for the horror to come.
 *
 */

#define SND_CHN_MATRIX_MAP_1_0	{					\
	.id = SND_CHN_MATRIX_1_0,					\
	.channels = 1,							\
	.ext = 0,							\
	.map = {							\
		/* Mono, center, etc. */				\
		[0] = {							\
			.type = SND_CHN_T_FL,				\
			.members =					\
			    SND_CHN_T_MASK_FL | SND_CHN_T_MASK_FR |	\
			    SND_CHN_T_MASK_FC | SND_CHN_T_MASK_LF |	\
			    SND_CHN_T_MASK_BL | SND_CHN_T_MASK_BR |	\
			    SND_CHN_T_MASK_BC | SND_CHN_T_MASK_SL |	\
			    SND_CHN_T_MASK_SR				\
		},							\
		[1] = {							\
			.type = SND_CHN_T_MAX,				\
			.members = 0					\
		}							\
	},								\
	.mask = SND_CHN_T_MASK_FL | SND_CHN_T_MASK_FR |			\
		SND_CHN_T_MASK_FC,					\
	.offset = {  0,  0,  0,  0,  0,  0, -1, -1,  0,			\
		     0,  0, -1, -1, -1, -1, -1, -1, -1  }		\
}

#define SND_CHN_MATRIX_MAP_2_0	{					\
	.id = SND_CHN_MATRIX_2_0,					\
	.channels = 2,							\
	.ext = 0,							\
	.map = {							\
		/* Left */						\
		[0] = {							\
			.type = SND_CHN_T_FL,				\
			.members =					\
			    SND_CHN_T_MASK_FL | SND_CHN_T_MASK_FC |	\
			    SND_CHN_T_MASK_LF | SND_CHN_T_MASK_BL |	\
			    SND_CHN_T_MASK_BC | SND_CHN_T_MASK_SL	\
		},							\
		/* Right */						\
		[1] = {							\
			.type = SND_CHN_T_FR,				\
			.members =					\
			    SND_CHN_T_MASK_FR | SND_CHN_T_MASK_FC |	\
			    SND_CHN_T_MASK_LF | SND_CHN_T_MASK_BR |	\
			    SND_CHN_T_MASK_BC | SND_CHN_T_MASK_SR	\
		},							\
		[2] = {							\
			.type = SND_CHN_T_MAX,				\
			.members = 0					\
		}							\
	},								\
	.mask = SND_CHN_T_MASK_FL | SND_CHN_T_MASK_FR,			\
	.offset = {  0,  1, -1, -1, -1, -1, -1, -1, -1,			\
		    -1, -1, -1, -1, -1, -1, -1, -1, -1  }		\
}

#define SND_CHN_MATRIX_MAP_2_1	{					\
	.id = SND_CHN_MATRIX_2_1,					\
	.channels = 3,							\
	.ext = 1,							\
	.map = {							\
		/* Left */						\
		[0] = {							\
			.type = SND_CHN_T_FL,				\
			.members =					\
			    SND_CHN_T_MASK_FL | SND_CHN_T_MASK_FC |	\
			    SND_CHN_T_MASK_BL | SND_CHN_T_MASK_BC |	\
			    SND_CHN_T_MASK_SL				\
		},							\
		/* Right */						\
		[1] = {							\
			.type = SND_CHN_T_FR,				\
			.members =					\
			    SND_CHN_T_MASK_FR | SND_CHN_T_MASK_FC |	\
			    SND_CHN_T_MASK_BR | SND_CHN_T_MASK_BC |	\
			    SND_CHN_T_MASK_SR				\
		},							\
		/* LFE */						\
		[2] = {							\
			.type = SND_CHN_T_LF,				\
			.members = SND_CHN_T_MASK_LF			\
		},							\
		[3] = {							\
			.type = SND_CHN_T_MAX,				\
			.members = 0					\
		}							\
	},								\
	.mask = SND_CHN_T_MASK_FL | SND_CHN_T_MASK_FR |			\
		SND_CHN_T_MASK_LF,					\
	.offset = {  0,  1, -1,  2, -1, -1, -1, -1, -1,			\
		    -1, -1, -1, -1, -1, -1, -1, -1, -1  }		\
}

#define SND_CHN_MATRIX_MAP_3_0	{	/* 3 channels default */	\
	.id = SND_CHN_MATRIX_3_0,					\
	.channels = 3,							\
	.ext = 0,							\
	.map = {							\
		/* Left */						\
		[0] = {							\
			.type = SND_CHN_T_FL,				\
			.members =					\
			    SND_CHN_T_MASK_FL | SND_CHN_T_MASK_FC |	\
			    SND_CHN_T_MASK_LF | SND_CHN_T_MASK_SL	\
		},							\
		/* Right */						\
		[1] = {							\
			.type = SND_CHN_T_FR,				\
			.members =					\
			    SND_CHN_T_MASK_FR | SND_CHN_T_MASK_FC |	\
			    SND_CHN_T_MASK_LF | SND_CHN_T_MASK_SR	\
		},							\
		/* Rear Center */					\
		[2] = {							\
			.type = SND_CHN_T_BC,				\
			.members =					\
			    SND_CHN_T_MASK_LF | SND_CHN_T_MASK_BL |	\
			    SND_CHN_T_MASK_BR | SND_CHN_T_MASK_BC |	\
			    SND_CHN_T_MASK_SL | SND_CHN_T_MASK_SR	\
		},							\
		[3] = {							\
			.type = SND_CHN_T_MAX,				\
			.members = 0					\
		}							\
	},								\
	.mask = SND_CHN_T_MASK_FL | SND_CHN_T_MASK_FR |			\
		SND_CHN_T_MASK_BC,					\
	.offset = {  0,  1, -1, -1, -1, -1, -1, -1,  2,			\
		    -1, -1, -1, -1, -1, -1, -1, -1, -1  }		\
}

#define SND_CHN_MATRIX_MAP_3_1	{					\
	.id = SND_CHN_MATRIX_3_1,					\
	.channels = 4,							\
	.ext = 1,							\
	.map = {							\
		/* Left */						\
		[0] = {							\
			.type = SND_CHN_T_FL,				\
			.members =					\
			    SND_CHN_T_MASK_FL | SND_CHN_T_MASK_FC |	\
			    SND_CHN_T_MASK_SL				\
		},							\
		/* Right */						\
		[1] = {							\
			.type = SND_CHN_T_FR,				\
			.members =					\
			    SND_CHN_T_MASK_FR | SND_CHN_T_MASK_FC |	\
			    SND_CHN_T_MASK_SR				\
		},							\
		/* LFE */						\
		[2] = {							\
			.type = SND_CHN_T_LF,				\
			.members = SND_CHN_T_MASK_LF			\
		},							\
		/* Rear Center */					\
		[3] = {							\
			.type = SND_CHN_T_BC,				\
			.members =					\
			    SND_CHN_T_MASK_BL | SND_CHN_T_MASK_BR |	\
			    SND_CHN_T_MASK_BC |				\
			    SND_CHN_T_MASK_SL | SND_CHN_T_MASK_SR	\
		},							\
		[4] = {							\
			.type = SND_CHN_T_MAX,				\
			.members = 0					\
		}							\
	},								\
	.mask = SND_CHN_T_MASK_FL | SND_CHN_T_MASK_FR |			\
		SND_CHN_T_MASK_LF | SND_CHN_T_MASK_BC,			\
	.offset = {  0,  1, -1,  2, -1, -1, -1, -1,  3,			\
		    -1, -1, -1, -1, -1, -1, -1, -1, -1  }		\
}

#define SND_CHN_MATRIX_MAP_4_0	{					\
	.id = SND_CHN_MATRIX_4_0,					\
	.channels = 4,							\
	.ext = 0,							\
	.map = {							\
		/* Left */						\
		[0] = {							\
			.type = SND_CHN_T_FL,				\
			.members =					\
			    SND_CHN_T_MASK_FL | SND_CHN_T_MASK_FC |	\
			    SND_CHN_T_MASK_LF | SND_CHN_T_MASK_SL	\
		},							\
		/* Right */						\
		[1] = {							\
			.type = SND_CHN_T_FR,				\
			.members =					\
			    SND_CHN_T_MASK_FR | SND_CHN_T_MASK_FC |	\
			    SND_CHN_T_MASK_LF | SND_CHN_T_MASK_SR	\
		},							\
		/* Rear Left */						\
		[2] = {							\
			.type = SND_CHN_T_BL,				\
			.members =					\
			    SND_CHN_T_MASK_LF | SND_CHN_T_MASK_BL |	\
			    SND_CHN_T_MASK_BC | SND_CHN_T_MASK_SL	\
		},							\
		/* Rear Right */					\
		[3] = {							\
			.type = SND_CHN_T_BR,				\
			.members =					\
			    SND_CHN_T_MASK_LF | SND_CHN_T_MASK_BR |	\
			    SND_CHN_T_MASK_BC | SND_CHN_T_MASK_SR	\
		},							\
		[4] = {							\
			.type = SND_CHN_T_MAX,				\
			.members = 0					\
		}							\
	},								\
	.mask = SND_CHN_T_MASK_FL | SND_CHN_T_MASK_FR |			\
		SND_CHN_T_MASK_BL | SND_CHN_T_MASK_BR,			\
	.offset = {  0,  1, -1, -1,  2,  3, -1, -1, -1,			\
		    -1, -1, -1, -1, -1, -1, -1, -1, -1  }		\
}

#define SND_CHN_MATRIX_MAP_4_1	{					\
	.id = SND_CHN_MATRIX_4_1,					\
	.channels = 5,							\
	.ext = 1,							\
	.map = {							\
		/* Left */						\
		[0] = {							\
			.type = SND_CHN_T_FL,				\
			.members =					\
			    SND_CHN_T_MASK_FL | SND_CHN_T_MASK_FC |	\
			    SND_CHN_T_MASK_SL				\
		},							\
		/* Right */						\
		[1] = {							\
			.type = SND_CHN_T_FR,				\
			.members =					\
			    SND_CHN_T_MASK_FR | SND_CHN_T_MASK_FC |	\
			    SND_CHN_T_MASK_SR				\
		},							\
		/* Rear Left */						\
		[2] = {							\
			.type = SND_CHN_T_BL,				\
			.members =					\
			    SND_CHN_T_MASK_BL | SND_CHN_T_MASK_BC |	\
			    SND_CHN_T_MASK_SL				\
		},							\
		/* Rear Right */					\
		[3] = {							\
			.type = SND_CHN_T_BR,				\
			.members =					\
			    SND_CHN_T_MASK_BR | SND_CHN_T_MASK_BC |	\
			    SND_CHN_T_MASK_SR				\
		},							\
		/* LFE */						\
		[4] = {							\
			.type = SND_CHN_T_LF,				\
			.members = SND_CHN_T_MASK_LF			\
		},							\
		[5] = {							\
			.type = SND_CHN_T_MAX,				\
			.members = 0					\
		}							\
	},								\
	.mask = SND_CHN_T_MASK_FL | SND_CHN_T_MASK_FR |			\
		SND_CHN_T_MASK_BL | SND_CHN_T_MASK_BR |			\
		SND_CHN_T_MASK_LF,					\
	.offset = {  0,  1, -1,  4,  2,  3, -1, -1, -1,			\
		    -1, -1, -1, -1, -1, -1, -1, -1, -1  }		\
}

#define SND_CHN_MATRIX_MAP_5_0	{	/* 5 channels default */	\
	.id = SND_CHN_MATRIX_5_0,					\
	.channels = 5,							\
	.ext = 0,							\
	.map = {							\
		/* Left */						\
		[0] = {							\
			.type = SND_CHN_T_FL,				\
			.members =					\
			    SND_CHN_T_MASK_FL | SND_CHN_T_MASK_LF |	\
			    SND_CHN_T_MASK_SL				\
		},							\
		/* Right */						\
		[1] = {							\
			.type = SND_CHN_T_FR,				\
			.members =					\
			    SND_CHN_T_MASK_FR | SND_CHN_T_MASK_LF |	\
			    SND_CHN_T_MASK_SR				\
		},							\
		/* Rear Left */						\
		[2] = {							\
			.type = SND_CHN_T_BL,				\
			.members =					\
			    SND_CHN_T_MASK_LF | SND_CHN_T_MASK_BL |	\
			    SND_CHN_T_MASK_BC | SND_CHN_T_MASK_SL	\
		},							\
		/* Rear Right */					\
		[3] = {							\
			.type = SND_CHN_T_BR,				\
			.members =					\
			    SND_CHN_T_MASK_LF | SND_CHN_T_MASK_BR |	\
			    SND_CHN_T_MASK_BC | SND_CHN_T_MASK_SR	\
		},							\
		/* Center */						\
		[4] = {							\
			.type = SND_CHN_T_FC,				\
			.members = SND_CHN_T_MASK_FC			\
		},							\
		[5] = {							\
			.type = SND_CHN_T_MAX,				\
			.members = 0					\
		}							\
	},								\
	.mask = SND_CHN_T_MASK_FL | SND_CHN_T_MASK_FR |			\
		SND_CHN_T_MASK_BL | SND_CHN_T_MASK_BR |			\
		SND_CHN_T_MASK_FC,					\
	.offset = {  0,  1,  4, -1,  2,  3, -1, -1, -1,			\
		    -1, -1, -1, -1, -1, -1, -1, -1, -1  }		\
}

#define SND_CHN_MATRIX_MAP_5_1	{	/* 6 channels default */	\
	.id = SND_CHN_MATRIX_5_1,					\
	.channels = 6,							\
	.ext = 1,							\
	.map = {							\
		/* Left */						\
		[0] = {							\
			.type = SND_CHN_T_FL,				\
			.members =					\
			    SND_CHN_T_MASK_FL | SND_CHN_T_MASK_SL	\
		},							\
		/* Right */						\
		[1] = {							\
			.type = SND_CHN_T_FR,				\
			.members =					\
			    SND_CHN_T_MASK_FR | SND_CHN_T_MASK_SR	\
		},							\
		/* Rear Left */						\
		[2] = {							\
			.type = SND_CHN_T_BL,				\
			.members =					\
			    SND_CHN_T_MASK_BL | SND_CHN_T_MASK_BC |	\
			    SND_CHN_T_MASK_SL				\
		},							\
		/* Rear Right */					\
		[3] = {							\
			.type = SND_CHN_T_BR,				\
			.members =					\
			    SND_CHN_T_MASK_BR | SND_CHN_T_MASK_BC |	\
			    SND_CHN_T_MASK_SR				\
		},							\
		/* Center */						\
		[4] = {							\
			.type = SND_CHN_T_FC,				\
			.members = SND_CHN_T_MASK_FC			\
		},							\
		/* LFE */						\
		[5] = {							\
			.type = SND_CHN_T_LF,				\
			.members = SND_CHN_T_MASK_LF			\
		},							\
		[6] = {							\
			.type = SND_CHN_T_MAX,				\
			.members = 0					\
		}							\
	},								\
	.mask = SND_CHN_T_MASK_FL | SND_CHN_T_MASK_FR |			\
		SND_CHN_T_MASK_BL | SND_CHN_T_MASK_BR |			\
		SND_CHN_T_MASK_FC | SND_CHN_T_MASK_LF,			\
	.offset = {  0,  1,  4,  5,  2,  3, -1, -1, -1,			\
		    -1, -1, -1, -1, -1, -1, -1, -1, -1  }		\
}

#define SND_CHN_MATRIX_MAP_6_0	{					\
	.id = SND_CHN_MATRIX_6_0,					\
	.channels = 6,							\
	.ext = 0,							\
	.map  = {							\
		/* Left */						\
		[0] = {							\
			.type = SND_CHN_T_FL,				\
			.members =					\
			    SND_CHN_T_MASK_FL | SND_CHN_T_MASK_LF |	\
			    SND_CHN_T_MASK_SL				\
		},							\
		/* Right */						\
		[1] = {							\
			.type = SND_CHN_T_FR,				\
			.members =					\
			    SND_CHN_T_MASK_FR | SND_CHN_T_MASK_LF |	\
			    SND_CHN_T_MASK_SR				\
		},							\
		/* Rear Left */						\
		[2] = {							\
			.type = SND_CHN_T_BL,				\
			.members =					\
			    SND_CHN_T_MASK_BL | SND_CHN_T_MASK_LF |	\
			    SND_CHN_T_MASK_SL				\
		},							\
		/* Rear Right */					\
		[3] = {							\
			.type = SND_CHN_T_BR,				\
			.members =					\
			    SND_CHN_T_MASK_BR | SND_CHN_T_MASK_LF |	\
			    SND_CHN_T_MASK_SR				\
		},							\
		/* Center */						\
		[4] = {							\
			.type = SND_CHN_T_FC,				\
			.members = SND_CHN_T_MASK_FC			\
		},							\
		/* Rear Center */					\
		[5] = {							\
			.type = SND_CHN_T_BC,				\
			.members = SND_CHN_T_MASK_BC			\
		},							\
		[6] = {							\
			.type = SND_CHN_T_MAX,				\
			.members = 0					\
		}							\
	},								\
	.mask = SND_CHN_T_MASK_FL | SND_CHN_T_MASK_FR |			\
		SND_CHN_T_MASK_BL | SND_CHN_T_MASK_BR |			\
		SND_CHN_T_MASK_FC | SND_CHN_T_MASK_BC,			\
	.offset = {  0,  1,  4, -1,  2,  3, -1, -1,  5,			\
		    -1, -1, -1, -1, -1, -1, -1, -1, -1  }		\
}

#define SND_CHN_MATRIX_MAP_6_1	{					\
	.id = SND_CHN_MATRIX_6_1,					\
	.channels = 7,							\
	.ext = 1,							\
	.map = {							\
		/* Left */						\
		[0] = {							\
			.type = SND_CHN_T_FL,				\
			.members =					\
			    SND_CHN_T_MASK_FL | SND_CHN_T_MASK_SL	\
		},							\
		/* Right */						\
		[1] = {							\
			.type = SND_CHN_T_FR,				\
			.members =					\
			    SND_CHN_T_MASK_FR | SND_CHN_T_MASK_SR	\
		},							\
		/* Rear Left */						\
		[2] = {							\
			.type = SND_CHN_T_BL,				\
			.members =					\
			    SND_CHN_T_MASK_BL | SND_CHN_T_MASK_SL	\
		},							\
		/* Rear Right */					\
		[3] = {							\
			.type = SND_CHN_T_BR,				\
			.members =					\
			    SND_CHN_T_MASK_BR | SND_CHN_T_MASK_SR	\
		},							\
		/* Center */						\
		[4] = {							\
			.type = SND_CHN_T_FC,				\
			.members = SND_CHN_T_MASK_FC			\
		},							\
		/* LFE */						\
		[5] = {							\
			.type = SND_CHN_T_LF,				\
			.members = SND_CHN_T_MASK_LF			\
		},							\
		/* Rear Center */					\
		[6] = {							\
			.type = SND_CHN_T_BC,				\
			.members = SND_CHN_T_MASK_BC			\
		},							\
		[7] = {							\
			.type = SND_CHN_T_MAX,				\
			.members = 0					\
		}							\
	},								\
	.mask = SND_CHN_T_MASK_FL | SND_CHN_T_MASK_FR |			\
		SND_CHN_T_MASK_BL | SND_CHN_T_MASK_BR |			\
		SND_CHN_T_MASK_FC | SND_CHN_T_MASK_LF |			\
		SND_CHN_T_MASK_BC,					\
	.offset = {  0,  1,  4,  5,  2,  3, -1, -1,  6,			\
		    -1, -1, -1, -1, -1, -1, -1, -1, -1  }		\
}

#define SND_CHN_MATRIX_MAP_7_0	{					\
	.id = SND_CHN_MATRIX_7_0,					\
	.channels = 7,							\
	.ext = 0,							\
	.map = {							\
		/* Left */						\
		[0] = {							\
			.type = SND_CHN_T_FL,				\
			.members =					\
			    SND_CHN_T_MASK_FL | SND_CHN_T_MASK_LF	\
		},							\
		/* Right */						\
		[1] = {							\
			.type = SND_CHN_T_FR,				\
			.members =					\
			    SND_CHN_T_MASK_FR | SND_CHN_T_MASK_LF	\
		},							\
		/* Rear Left */						\
		[2] = {							\
			.type = SND_CHN_T_BL,				\
			.members =					\
			    SND_CHN_T_MASK_BL | SND_CHN_T_MASK_BC |	\
			    SND_CHN_T_MASK_LF				\
		},							\
		/* Rear Right */					\
		[3] = {							\
			.type = SND_CHN_T_BR,				\
			.members =					\
			    SND_CHN_T_MASK_BR | SND_CHN_T_MASK_BC |	\
			    SND_CHN_T_MASK_LF				\
		},							\
		/* Center */						\
		[4] = {							\
			.type = SND_CHN_T_FC,				\
			.members =					\
			    SND_CHN_T_MASK_FC | SND_CHN_T_MASK_LF	\
		},							\
		/* Side Left */						\
		[5] = {							\
			.type = SND_CHN_T_SL,				\
			.members =					\
			    SND_CHN_T_MASK_SL | SND_CHN_T_MASK_LF	\
		},							\
		/* Side Right */					\
		[6] = {							\
			.type = SND_CHN_T_SR,				\
			.members =					\
			    SND_CHN_T_MASK_SR | SND_CHN_T_MASK_LF	\
		},							\
		[7] = {							\
			.type = SND_CHN_T_MAX,				\
			.members = 0					\
		}							\
	},								\
	.mask = SND_CHN_T_MASK_FL | SND_CHN_T_MASK_FR |			\
		SND_CHN_T_MASK_BL | SND_CHN_T_MASK_BR |			\
		SND_CHN_T_MASK_FC |					\
		SND_CHN_T_MASK_SL | SND_CHN_T_MASK_SR,			\
	.offset = {  0,  1,  4, -1,  2,  3, -1, -1, -1,			\
		     5,  6, -1, -1, -1, -1, -1, -1, -1  }		\
}

#define SND_CHN_MATRIX_MAP_7_1	{					\
	.id = SND_CHN_MATRIX_7_1,					\
	.channels = 8,							\
	.ext = 1,							\
	.map = {							\
		/* Left */						\
		[0] = {							\
			.type = SND_CHN_T_FL,				\
			.members = SND_CHN_T_MASK_FL			\
		},							\
		/* Right */						\
		[1] = {							\
			.type = SND_CHN_T_FR,				\
			.members = SND_CHN_T_MASK_FR			\
		},							\
		/* Rear Left */						\
		[2] = {							\
			.type = SND_CHN_T_BL,				\
			.members =					\
			    SND_CHN_T_MASK_BL | SND_CHN_T_MASK_BC	\
		},							\
		/* Rear Right */					\
		[3] = {							\
			.type = SND_CHN_T_BR,				\
			.members =					\
			    SND_CHN_T_MASK_BR | SND_CHN_T_MASK_BC	\
		},							\
		/* Center */						\
		[4] = {							\
			.type = SND_CHN_T_FC,				\
			.members = SND_CHN_T_MASK_FC			\
		},							\
		/* LFE */						\
		[5] = {							\
			.type = SND_CHN_T_LF,				\
			.members = SND_CHN_T_MASK_LF			\
		},							\
		/* Side Left */						\
		[6] = {							\
			.type = SND_CHN_T_SL,				\
			.members = SND_CHN_T_MASK_SL			\
		},							\
		/* Side Right */					\
		[7] = {							\
			.type = SND_CHN_T_SR,				\
			.members = SND_CHN_T_MASK_SR			\
		},							\
		[8] = {							\
			.type = SND_CHN_T_MAX,				\
			.members = 0					\
		}							\
	},								\
	.mask = SND_CHN_T_MASK_FL | SND_CHN_T_MASK_FR |			\
		SND_CHN_T_MASK_BL | SND_CHN_T_MASK_BR |			\
		SND_CHN_T_MASK_FC | SND_CHN_T_MASK_LF |			\
		SND_CHN_T_MASK_SL | SND_CHN_T_MASK_SR,			\
	.offset = {  0,  1,  4,  5,  2,  3, -1, -1, -1,			\
		     6,  7, -1, -1, -1, -1, -1, -1, -1  }		\
}

#endif	/* !_SND_MATRIX_H_ */
