/*
 * Copyright 2013 Ole Loots <ole@monochrom.net>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <inttypes.h>
#include <sys/types.h>
#include <string.h>


#include "assert.h"
#include "cflib.h"

#include "utils/nsoption.h"
#include "content/urldb.h"
#include "desktop/browser.h"
#include "desktop/plotters.h"
#include "desktop/mouse.h"
#include "desktop/treeview.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/utils.h"
#include "atari/gui.h"
#include "atari/plot/plot.h"
#include "atari/misc.h"
#include "atari/gemtk/gemtk.h"
#include "atari/treeview.h"
#include "atari/res/netsurf.rsh"


/**
 * Declare Core Window Callbacks:
 */

void atari_treeview_redraw_request(struct core_window *cw,
								const struct rect *r);
void atari_treeview_update_size(struct core_window *cw, int width, int height);
void atari_treeview_scroll_visible(struct core_window *cw,
								const struct rect *r);
void atari_treeview_get_window_dimensions(struct core_window *cw,
		int *width, int *height);
		// TODO: implement drag status!
void atari_treeview_drag_status(struct core_window *cw,
		core_window_drag_status ds);

static struct core_window_callback_table cw_t = {
	.redraw_request = atari_treeview_redraw_request,
	.update_size = atari_treeview_update_size,
	.scroll_visible = atari_treeview_scroll_visible,
	.get_window_dimensions = atari_treeview_get_window_dimensions,
	.drag_status = atari_treeview_drag_status
};


struct atari_treeview_window {
	struct atari_treeview_window * prev_open;
	struct atari_treeview_window * next_open;
	GUIWIN * window;
	bool disposing;
	bool redraw;
	bool is_open;
	GRECT rdw_area;
	POINT extent;
	POINT click;
	POINT startdrag;
	struct atari_treeview_callbacks *io;
	void * user_data;
};

static struct atari_treeview_window * treeviews_open;

/* native GUI event handlers: */
static void on_mbutton_event(struct core_window *cw, EVMULT_OUT *ev_out,
									short msg[8]);
static void on_keybd_event(struct core_window *cw, EVMULT_OUT *ev_out,
									short msg[8]);
static void on_redraw_event(struct core_window *cw, EVMULT_OUT *ev_out,
									short msg[8]);

/* static utils: */
static void atari_treeview_dump_info(struct atari_treeview_window *tv, char *s);

/**
 * Schedule a redraw of the treeview content
 *
 */
static void atari_treeview_redraw_grect_request(struct core_window *cw,
												GRECT *area)
{
	struct atari_treeview_window *tv = (struct atari_treeview_window *)cw;
	if (cw != NULL) {
		if( tv->redraw == false ){
			tv->redraw = true;
			tv->rdw_area.g_x = area->g_x;
			tv->rdw_area.g_y = area->g_y;
			tv->rdw_area.g_w = area->g_w;
			tv->rdw_area.g_h = area->g_h;
		} else {
			/* merge the redraw area to the new area.: */
			int newx1 = area->g_x+area->g_w;
			int newy1 = area->g_y+area->g_h;
			int oldx1 = tv->rdw_area.g_x + tv->rdw_area.g_w;
			int oldy1 = tv->rdw_area.g_y + tv->rdw_area.g_h;
			tv->rdw_area.g_x = MIN(tv->rdw_area.g_x, area->g_x);
			tv->rdw_area.g_y = MIN(tv->rdw_area.g_y, area->g_y);
			tv->rdw_area.g_w = ( oldx1 > newx1 ) ? oldx1 - tv->rdw_area.g_x : newx1 - tv->rdw_area.g_x;
			tv->rdw_area.g_h = ( oldy1 > newy1 ) ? oldy1 - tv->rdw_area.g_y : newy1 - tv->rdw_area.g_y;
		}
		//dbg_grect("atari_treeview_request_redraw_grect", &tv->rdw_area);
	}
}


void atari_treeview_get_grect(struct core_window *cw, enum treeview_area_e mode,
									GRECT *dest)
{

	struct atari_treeview_window *tv = (struct atari_treeview_window *)cw;

	if (mode == TREEVIEW_AREA_CONTENT) {
		gemtk_wm_get_grect(tv->window, GEMTK_WM_AREA_CONTENT, dest);
	}
	else if (mode == TREEVIEW_AREA_TOOLBAR) {
		gemtk_wm_get_grect(tv->window, GEMTK_WM_AREA_TOOLBAR, dest);
	}
}

GUIWIN * atari_treeview_get_gemtk_window(struct core_window *cw)
{
	struct atari_treeview_window *tv = (struct atari_treeview_window *)cw;
	return(tv->window);
}

static void atari_treeview_dump_info(struct atari_treeview_window *tv,
									char * title)
{
	printf("Treeview Dump (%s)\n", title);
	printf("=================================\n");
	gemtk_wm_dump_window_info(atari_treeview_get_gemtk_window((struct core_window *)tv));
	GEMTK_DBG_GRECT("Redraw Area: \n", &tv->rdw_area)
	dbg_grect("Redraw Area2:      \n", &tv->rdw_area);
	printf("Extent: x: %d, y: %d\n", tv->extent, tv->extent);
}

static bool atari_treeview_is_iconified(struct core_window *cw){

    struct atari_treeview_window *tv = (struct atari_treeview_window *)cw;

    return((gemtk_wm_get_state(tv->window)&GEMTK_WM_STATUS_ICONIFIED) != 0);
}

static void atari_treeview_redraw_icon(struct core_window *cw, GRECT *clip)
{
    struct atari_treeview_window *tv = (struct atari_treeview_window *)cw;
    GRECT visible, work;
    OBJECT * tree = gemtk_obj_get_tree(ICONIFY);
    short aesh = gemtk_wm_get_handle(tv->window);

    gemtk_wm_get_grect(tv->window, GEMTK_WM_AREA_WORK, &work);

    tree->ob_x = work.g_x;
    tree->ob_y = work.g_y;
    tree->ob_width = work.g_w;
    tree->ob_height = work.g_h;

    wind_get_grect(aesh, WF_FIRSTXYWH, &visible);
    while (visible.g_h > 0 && visible.g_w > 0) {

        if (rc_intersect(&work, &visible)) {
            objc_draw(tree, 0, 8, visible.g_x, visible.g_y, visible.g_w,
                        visible.g_h);
        } else {
            //dbg_grect("redraw vis area outside", &visible);
        }

        wind_get_grect(aesh, WF_NEXTXYWH, &visible);
    }
}

void atari_treeview_redraw(struct core_window *cw)
{
	struct atari_treeview_window *tv = (struct atari_treeview_window *)cw;
	short pxy[4];

	if (tv != NULL && tv->is_open) {
		if( tv->redraw && ((plot_get_flags() & PLOT_FLAG_OFFSCREEN) == 0) ) {

			short todo[4];
			GRECT work;
			short handle = gemtk_wm_get_handle(tv->window);
			struct gemtk_wm_scroll_info_s *slid;

			gemtk_wm_get_grect(tv->window, GEMTK_WM_AREA_CONTENT, &work);
			slid = gemtk_wm_get_scroll_info(tv->window);

//			// Debug code: this 3 lines help to inspect the redraw
//			// areas...
//			pxy[0] = work.g_x;
//			pxy[1] = work.g_y;
//			pxy[2] = pxy[0] + work.g_w-1;
//			pxy[3] = pxy[1] + work.g_h-1;
//
//			vsf_color(plot_get_vdi_handle(), 0);
//			v_bar(plot_get_vdi_handle(), (short*)&pxy);
//			evnt_timer(500);

			struct redraw_context ctx = {
				.interactive = true,
				.background_images = true,
				.plot = &atari_plotters
			};
			plot_set_dimensions(work.g_x, work.g_y, work.g_w, work.g_h);
			if (plot_lock() == false)
				return;

			if( wind_get(handle, WF_FIRSTXYWH,
							&todo[0], &todo[1], &todo[2], &todo[3] )!=0 ) {
				while (todo[2] && todo[3]) {

					if(!rc_intersect(&work, (GRECT*)&todo)){
						if (wind_get(handle, WF_NEXTXYWH,
							&todo[0], &todo[1], &todo[2], &todo[3])==0) {
							break;
						}
						continue;
					}
					pxy[0] = todo[0];
					pxy[1] = todo[1];
					pxy[2] = todo[0] + todo[2]-1;
					pxy[3] = todo[1] + todo[3]-1;
					vs_clip(plot_get_vdi_handle(), 1, (short*)&pxy);

					// Debug code: this 3 lines help to inspect the redraw
					// areas...

//					vsf_color(plot_get_vdi_handle(), 3);
//					v_bar(plot_get_vdi_handle(), (short*)&pxy);
//					evnt_timer(500);


					/* convert screen to treeview coords: */
					todo[0] = todo[0] - work.g_x ;//+ slid->x_pos*slid->x_unit_px;
					todo[1] = todo[1] - work.g_y ;//+ slid->y_pos*slid->y_unit_px;
					if( todo[0] < 0 ){
						todo[2] = todo[2] + todo[0];
						todo[0] = 0;
					}
					if( todo[1] < 0 ){
						todo[3] = todo[3] + todo[1];
						todo[1] = 0;
					}

					if (rc_intersect((GRECT *)&tv->rdw_area,(GRECT *)&todo)) {
							struct rect clip;

							clip.x0 = todo[0]+(slid->x_pos*slid->x_unit_px);
							clip.y0 = todo[1]+(slid->y_pos*slid->y_unit_px);
							clip.x1 = clip.x0 + todo[2]+(slid->x_pos*slid->x_unit_px);
							clip.y1 = clip.y0 + todo[3]+(slid->y_pos*slid->y_unit_px);

                            tv->io->draw(cw, -(slid->x_pos*slid->x_unit_px),
										-(slid->y_pos*slid->y_unit_px),
												&clip, &ctx);
					}
					vs_clip(plot_get_vdi_handle(), 0, (short*)&pxy);
					if (wind_get(handle, WF_NEXTXYWH,
							&todo[0], &todo[1], &todo[2], &todo[3])==0) {
						break;
					}
				}
			} else {
				plot_unlock();
				return;
			}
			plot_unlock();
			tv->redraw = false;
			tv->rdw_area.g_x = 65000;
			tv->rdw_area.g_y = 65000;
			tv->rdw_area.g_w = -1;
			tv->rdw_area.g_h = -1;
		} else {
			/* just copy stuff from the offscreen buffer */
		}
	}
}


/**
 * GEMTK (netsurf's GEM toolkit) event sink
 *
*/
static short handle_event(GUIWIN *win, EVMULT_OUT *ev_out, short msg[8])
{
	short retval = 0;
	struct atari_treeview_window *tv = (struct atari_treeview_window *)
											gemtk_wm_get_user_data(win);
	struct core_window *cw = (struct core_window *)tv;

    if( (ev_out->emo_events & MU_MESAG) != 0 ) {
        // handle message
        switch (msg[0]) {

        case WM_REDRAW:
			on_redraw_event(cw, ev_out, msg);
            break;

        default:
            break;
        }
    }
    if( (ev_out->emo_events & MU_KEYBD) != 0 ) {
        on_keybd_event(cw, ev_out, msg);
    }
    if( (ev_out->emo_events & MU_BUTTON) != 0 ) {
        LOG(("Treeview click at: %d,%d\n", ev_out->emo_mouse.p_x,
             ev_out->emo_mouse.p_y));
        on_mbutton_event(cw, ev_out, msg);
    }

    if(tv != NULL && tv->io->gemtk_user_func != NULL){
		tv->io->gemtk_user_func(win, ev_out, msg);
    }

	// TODO: evaluate return values of event handler functions and pass them on:
    return(retval);
}


static void __CDECL on_keybd_event(struct core_window *cw, EVMULT_OUT *ev_out,
									short msg[8])
{
	bool r=false;
	long kstate = 0;
	long kcode = 0;
	long ucs4;
	long ik;
	unsigned short nkc = 0;
	unsigned short nks = 0;
	unsigned char ascii;
	struct atari_treeview_window *tv = (struct atari_treeview_window *)cw;

	kstate = ev_out->emo_kmeta;
	kcode = ev_out->emo_kreturn;
	nkc= gem_to_norm( (short)kstate, (short)kcode );
	ascii = (nkc & 0xFF);
	ik = nkc_to_input_key(nkc, &ucs4);

	if (ik == 0) {
		if (ascii >= 9) {
			tv->io->keypress(cw, ucs4);
		}
	} else {
		tv->io->keypress(cw, ik);
	}
}


static void __CDECL on_redraw_event(struct core_window *cw, EVMULT_OUT *ev_out,
									short msg[8])
{
	GRECT work, clip;
	struct gemtk_wm_scroll_info_s *slid;
	struct atari_treeview_window *tv = (struct atari_treeview_window *)cw;

	if (tv == NULL)
		return;

	gemtk_wm_get_grect(tv->window, GEMTK_WM_AREA_CONTENT, &work);
	//dbg_grect("treeview work: ", &work);

	atari_treeview_get_grect(cw, TREEVIEW_AREA_CONTENT, &work);
	//dbg_grect("treeview work: ", &work);
	slid = gemtk_wm_get_scroll_info(tv->window);

	clip = work;

	/* check if the redraw area intersects with the content area: */
	if ( !rc_intersect( (GRECT*)&msg[4], &clip)) {
		return;
	}

    if (atari_treeview_is_iconified(cw) == true) {
        atari_treeview_redraw_icon(cw, &clip);
        return;
    }

	/* make redraw coords relative to content viewport */
	clip.g_x -= work.g_x;
	clip.g_y -= work.g_y;

	/* normalize the redraw coords: */
	if( clip.g_x < 0 ) {
		clip.g_w = work.g_w + clip.g_x;
		clip.g_x = 0;
	}
	if( clip.g_y < 0 ) {
		clip.g_h = work.g_h + clip.g_y;
		clip.g_y = 0;
	}

	/* Merge redraw coords: */
	if( clip.g_h > 0 && clip.g_w > 0 ) {

		GRECT rdrw_area;

		rdrw_area.g_x = clip.g_x;
		rdrw_area.g_y = clip.g_y;
		rdrw_area.g_w = clip.g_w;
		rdrw_area.g_h = clip.g_h;

		//dbg_grect("treeview on_redraw_event ", &rdrw_area);

		atari_treeview_redraw_grect_request(cw, &rdrw_area);
	}
}

static void __CDECL on_mbutton_event(struct core_window *cw, EVMULT_OUT *ev_out,
									short msg[8])
{
	struct atari_treeview_window *tv = (struct atari_treeview_window *)cw;
	struct gemtk_wm_scroll_info_s *slid;
	GRECT work;
	short mx, my;
	int bms;
	bool ignore=false;
	short cur_rel_x, cur_rel_y, dummy, mbut;

	assert(tv);

	gemtk_wm_get_grect(tv->window, GEMTK_WM_AREA_CONTENT, &work);
	slid = gemtk_wm_get_scroll_info(tv->window);
	mx = ev_out->emo_mouse.p_x;
	my = ev_out->emo_mouse.p_y;

	/* mouse click relative origin: */

	short origin_rel_x = (mx-work.g_x) +
							(slid->x_pos*slid->x_unit_px);
	short origin_rel_y = (my-work.g_y) +
							(slid->y_pos*slid->y_unit_px);

	/* Only pass on events in the content area: */
	if( origin_rel_x >= 0 && origin_rel_y >= 0
		&& mx < work.g_x + work.g_w
		&& my < work.g_y + work.g_h )
	{
		if (ev_out->emo_mclicks == 2) {
			tv->io->mouse_action(cw,
								BROWSER_MOUSE_CLICK_1|BROWSER_MOUSE_DOUBLE_CLICK,
								origin_rel_x, origin_rel_y);
			return;
		}

		graf_mkstate(&cur_rel_x, &cur_rel_y, &mbut, &dummy);
		/* check for click or hold: */
		if( (mbut&1) == 0 ){
			bms = BROWSER_MOUSE_CLICK_1 | BROWSER_MOUSE_PRESS_1;
			if(ev_out->emo_mclicks == 2 ) {
				bms = BROWSER_MOUSE_DOUBLE_CLICK;
			}
			tv->io->mouse_action(cw, bms, origin_rel_x, origin_rel_y);
		} else {
			/* button still pressed */
			short prev_x = origin_rel_x;
			short prev_y = origin_rel_y;

			cur_rel_x = origin_rel_x;
			cur_rel_y = origin_rel_y;

			gem_set_cursor(&gem_cursors.hand);

			tv->startdrag.x = origin_rel_x;
			tv->startdrag.y = origin_rel_y;
			/* First, report mouse press, to trigger entry selection */
			tv->io->mouse_action(cw, BROWSER_MOUSE_CLICK_1 | BROWSER_MOUSE_PRESS_1, cur_rel_x,
									cur_rel_y);
			atari_treeview_redraw(cw);
			tv->io->mouse_action(cw, BROWSER_MOUSE_DRAG_1 | BROWSER_MOUSE_DRAG_ON,
								cur_rel_x, cur_rel_y);
			do{
				if (abs(prev_x-cur_rel_x) > 5 || abs(prev_y-cur_rel_y) > 5) {
					tv->io->mouse_action(cw,
								BROWSER_MOUSE_HOLDING_1 | BROWSER_MOUSE_DRAG_ON,
								cur_rel_x, cur_rel_y);
					prev_x = cur_rel_x;
					prev_y = cur_rel_y;
				}

				if (tv->redraw) {
					// TODO: maybe GUI poll would fit better here?
					// 		 ... is gui_poll re-entrance save?
					atari_treeview_redraw(cw);
				}

				/* sample mouse button state: */
				graf_mkstate(&cur_rel_x, &cur_rel_y, &mbut, &dummy);
				cur_rel_x = (cur_rel_x-work.g_x)+(slid->x_pos*slid->x_unit_px);
				cur_rel_y = (cur_rel_y-work.g_y)+(slid->y_pos*slid->y_unit_px);
			} while( mbut & 1 );

			/* End drag: */
			tv->io->mouse_action(cw, BROWSER_MOUSE_HOVER, cur_rel_x, cur_rel_y);
			gem_set_cursor(&gem_cursors.arrow);
		}
	}
}


struct core_window *
atari_treeview_create(GUIWIN *win, struct atari_treeview_callbacks * callbacks,
					void * user_data, uint32_t flags)
{

	/* allocate the core_window struct: */
	struct atari_treeview_window * tv;
	struct gemtk_wm_scroll_info_s *slid;

	tv = calloc(1, sizeof(struct atari_treeview_window));
	if (tv == NULL) {
		LOG(("calloc failed"));
		warn_user(messages_get_errorcode(NSERROR_NOMEM), 0);
		return NULL;
	}

	/* Store the window ref inside the new treeview: */
	tv->window = win;
	tv->io = callbacks;
	tv->user_data = user_data;

	// Setup gemtk event handler function:
	gemtk_wm_set_event_handler(win, handle_event);

	// bind window user data to treeview ref:
	gemtk_wm_set_user_data(win, (void*)tv);

	// Get acces to the gemtk scroll info struct:
	slid = gemtk_wm_get_scroll_info(tv->window);

	// Setup line and column height/width of the window,
	// each scroll takes the configured steps:
	slid->y_unit_px = 16;
	slid->x_unit_px = 16;

	assert(tv->io);
	assert(tv->io->init_phase2);

	/* Now that the window is configured for treeview content,			*/
	/* call init_phase2 which must create the treeview 					*/
	/* descriptor, and at least setup the the default					*/
	/* event handlers of the treeview:			 						*/
	/* It would be more simple to not pass around the callbacks			*/
	/* but the treeview constructor requires them for initialization...	*/
	nserror err = tv->io->init_phase2((struct core_window *)tv, &cw_t);
	if (err != NSERROR_OK) {
		free(tv);
		tv = NULL;
	}

	return((struct core_window *)tv);
}

void atari_treeview_delete(struct core_window * cw)
{
	struct atari_treeview_window *tv = (struct atari_treeview_window*)cw;

	assert(tv);
	assert(tv->io->finish);

	tv->io->finish(cw);

	free(tv);
}


void atari_treeview_open(struct core_window *cw, GRECT *pos)
{
	struct atari_treeview_window *tv = (struct atari_treeview_window*)cw;
	if (tv->window != NULL && tv->is_open == false) {
		tv->is_open = true;
		wind_open_grect(gemtk_wm_get_handle(tv->window), pos);
		gemtk_wm_link(tv->window);
		if (treeviews_open == NULL) {
			treeviews_open = tv;
			treeviews_open->next_open = NULL;
			treeviews_open->prev_open = NULL;
		} else {
			struct atari_treeview_window * tmp;
			tmp = treeviews_open;
			while(tmp->next_open != NULL){
				tmp = tmp->next_open;
			}
			tmp->next_open = tv;
			tv->prev_open = tmp;
			tv->next_open = NULL;
		}
	}
}

bool atari_treeview_is_open(struct core_window *cw)
{
	struct atari_treeview_window *tv = (struct atari_treeview_window*)cw;
	return(tv->is_open);
}

void atari_treeview_set_user_data(struct core_window * cw,
								void *user_data_ptr)
{
	struct atari_treeview_window *tv = (struct atari_treeview_window*)cw;
	tv->user_data = user_data_ptr;
}

void * atari_treeview_get_user_data(struct core_window * cw)
{
	struct atari_treeview_window *tv = (struct atari_treeview_window*)cw;
	return(tv->user_data);
}

void atari_treeview_close(struct core_window *cw)
{
	struct atari_treeview_window *tv = (struct atari_treeview_window*)cw;
	if (tv->window != NULL) {
		tv->is_open = false;
		wind_close(gemtk_wm_get_handle(tv->window));
		gemtk_wm_unlink(tv->window);
		/* unlink the window: */
		struct atari_treeview_window *tmp = treeviews_open;
		if (tv->prev_open != NULL) {
			tv->prev_open->next_open = tv->next_open;
		} else {
			treeviews_open = tv->next_open;
		}
		if (tv->next_open != NULL) {
			tv->next_open->prev_open = tv->prev_open;
		}
	}
}


/**
 * Core Window Callbacks:
 */

/**
 * Request a redraw of the window
 *
 * \param cw		the core window object
 * \param r		rectangle to redraw
 */
void atari_treeview_redraw_request(struct core_window *cw, const struct rect *r)
{
	GRECT area;
	struct gemtk_wm_scroll_info_s * slid;
	struct atari_treeview_window * tv = (struct atari_treeview_window *)cw;

	RECT_TO_GRECT(r, &area)

	assert(tv);

	slid = gemtk_wm_get_scroll_info(tv->window);

	//dbg_rect("redraw rect request", r);

	// treeview redraw is always full window width:
	area.g_x = 0;
	area.g_w = 8000;
	// but vertical redraw region is clipped:
	area.g_y = r->y0 - (slid->y_pos*slid->y_unit_px);
	area.g_h = r->y1 - r->y0;
	atari_treeview_redraw_grect_request(cw, &area);
}

/**
 * Update the limits of the window
 *
 * \param cw		the core window object
 * \param width		the width in px, or negative if don't care
 * \param height	the height in px, or negative if don't care
 */
void atari_treeview_update_size(struct core_window *cw, int width, int height)
{
	GRECT area;
	struct gemtk_wm_scroll_info_s *slid;
	struct atari_treeview_window *tv = (struct atari_treeview_window *)cw;

	if (tv != NULL) {

		if (tv->disposing)
			return;

		/* Get acces to the gemtk window slider settings: */
		slid = gemtk_wm_get_scroll_info(tv->window);

		/* recalculate and refresh sliders: */
		atari_treeview_get_grect(cw, TREEVIEW_AREA_CONTENT, &area);
		if (width > -1) {
			slid->x_units = (width/slid->x_unit_px);
		} else {
			slid->x_units = 1;
		}

		if (height > -1) {
			slid->y_units = (height/slid->y_unit_px);
		} else {
			slid->y_units = 1;
		}

		tv->extent.x = width;
		tv->extent.y = height;


		/*printf("units content: %d, units viewport: %d\n", (height/slid->y_unit_px),
					(area.g_h/slid->y_unit_px));*/
		gemtk_wm_update_slider(tv->window, GEMTK_WM_VH_SLIDER);
	}
}


/**
 * Scroll the window to make area visible
 *
 * \param cw		the core window object
 * \param r		rectangle to make visible
 */
void atari_treeview_scroll_visible(struct core_window *cw, const struct rect *r)
{
	/* atari frontend doesn't support dragging outside the treeview */
	/* so there is no need to implement this? 						*/
}


/**
 * Get window viewport dimensions
 *
 * \param cw		the core window object
 * \param width		to be set to viewport width in px, if non NULL
 * \param height	to be set to viewport height in px, if non NULL
 */
void atari_treeview_get_window_dimensions(struct core_window *cw,
		int *width, int *height)
{
	if (cw != NULL && (width != NULL || height != NULL)) {
		GRECT work;
		struct atari_treeview_window *tv = (struct atari_treeview_window *)cw;
		atari_treeview_get_grect(cw, TREEVIEW_AREA_CONTENT, &work);
		*width = work.g_w;
		*height = work.g_h;
	}
}


/**
 * Inform corewindow owner of drag status
 *
 * \param cw		the core window object
 * \param ds		the current drag status
 */
void atari_treeview_drag_status(struct core_window *cw,
		core_window_drag_status ds)
{

}

void atari_treeview_flush_redraws(void)
{
	struct atari_treeview_window *tmp;

	tmp = treeviews_open;

	if(tmp){
		while(tmp){
			assert(tmp->is_open);
			if(tmp->redraw){
			    if (atari_treeview_is_iconified((struct core_window *)tmp)) {
                    /* No content redraw for iconified windows */
                    /* because otherwise the icon draw function would */
                    /* have to deal with plot canvas coords */
                    continue;
			    }

                atari_treeview_redraw((struct core_window *)tmp);
			}
			tmp = tmp->next_open;
		}
	}
}

