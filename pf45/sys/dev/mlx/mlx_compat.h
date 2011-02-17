/*-
 * Copyright (c) 2000, 2001 Michael Smith
 * Copyright (c) 2000 BSDi
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
 *
 * $FreeBSD$
 */
/*
 * Portability and compatibility interfaces.
 */

#if __FreeBSD_version < 500003
# include <machine/clock.h>
# define INTR_ENTROPY			0

# include <sys/buf.h>			/* old buf style */
typedef struct buf			mlx_bio;
typedef struct buf_queue_head		mlx_bioq;
# define MLX_BIO_QINIT(bq)		bufq_init(&bq);
# define MLX_BIO_QINSERT(bq, bp)	bufq_insert_tail(&bq, bp)
# define MLX_BIO_QFIRST(bq)		bufq_first(&bq)
# define MLX_BIO_QREMOVE(bq, bp)	bufq_remove(&bq, bp)
# define MLX_BIO_IS_READ(bp)		((bp)->b_flags & B_READ)
# define MLX_BIO_DATA(bp)		(bp)->b_data
# define MLX_BIO_LENGTH(bp)		(bp)->b_bcount
# define MLX_BIO_LBA(bp)		(bp)->b_pblkno
# define MLX_BIO_SOFTC(bp)		(bp)->b_dev->si_drv1
# define MLX_BIO_UNIT(bp)		*(int *)((bp)->b_dev->si_drv2)
# define MLX_BIO_SET_ERROR(bp, err)	do { (bp)->b_error = err; (bp)->b_flags |= B_ERROR;} while(0)
# define MLX_BIO_HAS_ERROR(bp)		((bp)->b_flags & B_ERROR)
# define MLX_BIO_RESID(bp)		(bp)->b_resid
# define MLX_BIO_DONE(bp)		biodone(bp)
# define MLX_BIO_STATS_START(bp)	devstat_start_transaction(&((struct mlxd_softc *)MLX_BIO_SOFTC(bp))->mlxd_stats)
# define MLX_BIO_STATS_END(bp)		devstat_end_transaction_buf(&((struct mlxd_softc *)MLX_BIO_SOFTC(bp))->mlxd_stats, bp)
#else
# include <sys/bio.h>
typedef struct bio			mlx_bio;
typedef struct bio_queue_head		mlx_bioq;
# define MLX_BIO_QINIT(bq)		bioq_init(&bq);
# define MLX_BIO_QINSERT(bq, bp)	bioq_insert_tail(&bq, bp)
# define MLX_BIO_QFIRST(bq)		bioq_first(&bq)
# define MLX_BIO_QREMOVE(bq, bp)	bioq_remove(&bq, bp)
# define MLX_BIO_IS_READ(bp)		((bp)->bio_cmd == BIO_READ)
# define MLX_BIO_DATA(bp)		(bp)->bio_data
# define MLX_BIO_LENGTH(bp)		(bp)->bio_bcount
# define MLX_BIO_LBA(bp)		(bp)->bio_pblkno
# define MLX_BIO_SOFTC(bp)		(bp)->bio_disk->d_drv1
# define MLX_BIO_UNIT(bp)		(bp)->bio_disk->d_unit
# define MLX_BIO_SET_ERROR(bp, err)	do { (bp)->bio_error = err; (bp)->bio_flags |= BIO_ERROR;} while(0)
# define MLX_BIO_HAS_ERROR(bp)		((bp)->bio_flags & BIO_ERROR)
# define MLX_BIO_RESID(bp)		(bp)->bio_resid
# define MLX_BIO_DONE(bp)		biodone(bp)	/* XXX nice to integrate bio_finish here */
# define MLX_BIO_STATS_START(bp)
# define MLX_BIO_STATS_END(bp)
#endif

