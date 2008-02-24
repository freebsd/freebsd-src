/*-
 * Copyright (c) 2004 Lukas Ertl
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
 * $FreeBSD: src/sys/geom/vinum/geom_vinum.h,v 1.13 2007/04/12 17:54:35 le Exp $
 */

#ifndef	_GEOM_VINUM_H_
#define	_GEOM_VINUM_H_

#define	ERRBUFSIZ	1024

/* geom_vinum_drive.c */
void	gv_config_new_drive(struct gv_drive *);
void	gv_drive_modify(struct gv_drive *);
void	gv_save_config_all(struct gv_softc *);
void	gv_save_config(struct g_consumer *, struct gv_drive *,
	    struct gv_softc *);

/* geom_vinum_init.c */
void	gv_parityop(struct g_geom *, struct gctl_req *);
void	gv_start_obj(struct g_geom *, struct gctl_req *);

/* geom_vinum_list.c */
void	gv_ld(struct g_geom *, struct gctl_req *, struct sbuf *);
void	gv_lp(struct g_geom *, struct gctl_req *, struct sbuf *);
void	gv_ls(struct g_geom *, struct gctl_req *, struct sbuf *);
void	gv_lv(struct g_geom *, struct gctl_req *, struct sbuf *);
void	gv_list(struct g_geom *, struct gctl_req *);

/* geom_vinum_move.c */
void	gv_move(struct g_geom *, struct gctl_req *);

/* geom_vinum_rename.c */
void	gv_rename(struct g_geom *, struct gctl_req *);

/* geom_vinum_rm.c */
void	gv_remove(struct g_geom *, struct gctl_req *);
int	gv_resetconfig(struct g_geom *, struct gctl_req *);
int	gv_rm_sd(struct gv_softc *sc, struct gctl_req *req,
	    struct gv_sd *s, int flags);

/* geom_vinum_state.c */
int	gv_sdstatemap(struct gv_plex *);
void	gv_setstate(struct g_geom *, struct gctl_req *);
int	gv_set_drive_state(struct gv_drive *, int, int);
int	gv_set_sd_state(struct gv_sd *, int, int);
void	gv_update_sd_state(struct gv_sd *);
void	gv_update_plex_state(struct gv_plex *);
void	gv_update_vol_state(struct gv_volume *);

/* geom_vinum_subr.c */
void	gv_adjust_freespace(struct gv_sd *, off_t);
void	gv_free_sd(struct gv_sd *);
struct g_geom	*find_vinum_geom(void);
struct gv_drive	*gv_find_drive(struct gv_softc *, char *);
struct gv_plex	*gv_find_plex(struct gv_softc *, char *);
struct gv_sd	*gv_find_sd(struct gv_softc *, char *);
struct gv_volume *gv_find_vol(struct gv_softc *, char *);
void	gv_format_config(struct gv_softc *, struct sbuf *, int, char *);
int	gv_is_striped(struct gv_plex *);
int	gv_is_open(struct g_geom *);
void	gv_kill_drive_thread(struct gv_drive *);
void	gv_kill_plex_thread(struct gv_plex *);
void	gv_kill_vol_thread(struct gv_volume *);
int	gv_object_type(struct gv_softc *, char *);
void	gv_parse_config(struct gv_softc *, u_char *, int);
int	gv_sd_to_drive(struct gv_softc *, struct gv_drive *, struct gv_sd *,
	    char *, int);
int	gv_sd_to_plex(struct gv_plex *, struct gv_sd *, int);
void	gv_update_plex_config(struct gv_plex *);
void	gv_update_vol_size(struct gv_volume *, off_t);
off_t	gv_vol_size(struct gv_volume *);
off_t	gv_plex_size(struct gv_plex *);

#endif /* !_GEOM_VINUM_H_ */
