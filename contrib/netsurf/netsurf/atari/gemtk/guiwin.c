/*
 * Copyright 2012 Ole Loots <ole@monochrom.net>
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

#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

#include <gem.h>
#include <gemx.h>
#include <cflib.h>

#include "gemtk.h"
#include "vaproto.h"

//#define DEBUG_PRINT(x)		printf x
#define DEBUG_PRINT(x)

struct gemtk_window_s {

	/** The AES handle of the window */
    short handle;

    /** the generic event handler function for events passed to the client */
    gemtk_wm_event_handler_f handler_func;

    /** The toolbar redraw function, if any */
    gemtk_wm_redraw_f toolbar_redraw_func;

    /** window configuration */
    uint32_t flags;

    /** window state */
    uint32_t state;

    /** AES Tree used as toolbar */
    OBJECT *toolbar;

    /** Current edit object selected in the toolbar, if any. */
    short toolbar_edit_obj;

    /** Current selected object in the toolbar, if any. */
    short toolbar_focus_obj;

    /** Describes the start of the toolbar tree (usually 0) */
    short toolbar_idx;

    /** depending on the flag GEMTK_WM_FLAG_HAS_VTOOLBAR this defines the toolbar
		height or the toolbar width (GEMTK_WM_FLAG_HAS_VTOOLBAR means width).
	*/
    short toolbar_size;

    /** AES Object tree to be used for windowed dialogs. */
    OBJECT *form;

	/** Current form edit object, if any. */
    short form_edit_obj;

    /** Current form focus object, if any */
    short form_focus_obj;

    /** Describes the start of the form tree */
    short form_idx;

    /** Scroll state */
    struct gemtk_wm_scroll_info_s scroll_info;

    /** Arbitary data set by the user */
    void *user_data;

    /** linked list items */
    struct gemtk_window_s *next, *prev;
};

static GUIWIN * winlist;
static VdiHdl v_vdi_h = -1;
static short work_out[57];

static void move_rect(GUIWIN * win, GRECT *rect, int dx, int dy)
{
    INT16 xy[ 8];
    long dum = 0L;
    GRECT g;

    VdiHdl vh = gemtk_wm_get_vdi_handle(win);

    while(!wind_update(BEG_UPDATE));
    graf_mouse(M_OFF, 0L);

    /* get intersection with screen area */
    wind_get_grect(0, WF_CURRXYWH, &g);
    if(!rc_intersect(&g, rect)){
		goto error;
    }
    xy[0] = rect->g_x;
    xy[1] = rect->g_y;
    xy[2] = xy[0] + rect->g_w-1;
    xy[3] = xy[1] + rect->g_h-1;
    xy[4] = xy[0] + dx;
    xy[5] = xy[1] + dy;
    xy[6] = xy[2] + dx;
    xy[7] = xy[3] + dy;
    vro_cpyfm(vh, S_ONLY, xy, (MFDB *)&dum, (MFDB *)&dum);

error:
    graf_mouse(M_ON, 0L);
    wind_update(END_UPDATE);
}

/**
* Handles common events.
* returns 0 when the event was not handled, 1 otherwise.
*/
static short preproc_wm(GUIWIN * gw, EVMULT_OUT *ev_out, short msg[8])
{
    GRECT g, g_ro, g2;
    short retval = 1;
    int val = 1, old_val;
    struct gemtk_wm_scroll_info_s *slid;

    switch(msg[0]) {

    case WM_HSLID:
        gemtk_wm_get_grect(gw, GEMTK_WM_AREA_CONTENT, &g);
        wind_set(gw->handle, WF_HSLIDE, msg[4], 0, 0, 0);
        slid = gemtk_wm_get_scroll_info(gw);
        val = (float)(slid->x_units-(g.g_w/slid->x_unit_px))/1000*(float)msg[4];
        if(val != slid->x_pos) {
            if (val < slid->x_pos) {
                val = -(MAX(0, slid->x_pos-val));
            } else {
                val = val-slid->x_pos;
            }
            gemtk_wm_scroll(gw, GEMTK_WM_HSLIDER, val, false);
        }
        break;

    case WM_VSLID:
        gemtk_wm_get_grect(gw, GEMTK_WM_AREA_CONTENT, &g);
        wind_set(gw->handle, WF_VSLIDE, msg[4], 0, 0, 0);
        slid = gemtk_wm_get_scroll_info(gw);
        val = (float)(slid->y_units-(g.g_h/slid->y_unit_px))/1000*(float)msg[4];
        if(val != slid->y_pos) {
            if (val < slid->y_pos) {
                val = -(slid->y_pos - val);
            } else {
                val = val -slid->y_pos;
            }
            gemtk_wm_scroll(gw, GEMTK_WM_VSLIDER, val, false);
        }
        break;

    case WM_ARROWED:
        if((gw->flags & GEMTK_WM_FLAG_CUSTOM_SCROLLING) == 0) {

            slid = gemtk_wm_get_scroll_info(gw);
            gemtk_wm_get_grect(gw, GEMTK_WM_AREA_CONTENT, &g);
            g_ro = g;

            switch(msg[4]) {

            case WA_UPPAGE:
                /* scroll page up */
                gemtk_wm_scroll(gw, GEMTK_WM_VSLIDER, -(g.g_h/slid->y_unit_px),
                              true);
                break;

            case WA_UPLINE:
                /* scroll line up */
                gemtk_wm_scroll(gw, GEMTK_WM_VSLIDER, -1, true);
                break;

            case WA_DNPAGE:
                /* scroll page down */
                gemtk_wm_scroll(gw, GEMTK_WM_VSLIDER, g.g_h/slid->y_unit_px,
                              true);
                break;

            case WA_DNLINE:
                /* scroll line down */
                gemtk_wm_scroll(gw, GEMTK_WM_VSLIDER, +1, true);
                break;

            case WA_LFPAGE:
                /* scroll page left */
                gemtk_wm_scroll(gw, GEMTK_WM_HSLIDER, -(g.g_w/slid->x_unit_px),
                              true);
                break;

            case WA_LFLINE:
                /* scroll line left */
                gemtk_wm_scroll(gw, GEMTK_WM_HSLIDER, -1,
                              true);
                break;

            case WA_RTPAGE:
                /* scroll page right */
                gemtk_wm_scroll(gw, GEMTK_WM_HSLIDER, (g.g_w/slid->x_unit_px),
                              true);
                break;

            case WA_RTLINE:
                /* scroll line right */
                gemtk_wm_scroll(gw, GEMTK_WM_HSLIDER, 1,
                              true);
                break;

            default:
                break;
            }
        }
        break;

    case WM_TOPPED:
        wind_set(gw->handle, WF_TOP, 1, 0, 0, 0);
        break;

    case WM_MOVED:
        wind_get_grect(gw->handle, WF_CURRXYWH, &g);
        wind_set(gw->handle, WF_CURRXYWH, msg[4], msg[5], g.g_w, g.g_h);

        if (gw->form) {

			gemtk_wm_get_grect(gw, GEMTK_WM_AREA_CONTENT, &g);
			slid = gemtk_wm_get_scroll_info(gw);

			gw->form[gw->form_idx].ob_x = g.g_x -
					(slid->x_pos * slid->x_unit_px);

			gw->form[gw->form_idx].ob_y = g.g_y -
					(slid->y_pos * slid->y_unit_px);
        }

        break;

    case WM_SIZED:
    case WM_REPOSED:
        wind_get_grect(gw->handle, WF_FULLXYWH, &g2);
        wind_get_grect(gw->handle, WF_CURRXYWH, &g);
        g.g_w = MIN(msg[6], g2.g_w);
        g.g_h = MIN(msg[7], g2.g_h);
        if(g2.g_w != g.g_w || g2.g_h != g.g_h) {
            wind_set(gw->handle, WF_CURRXYWH, g.g_x, g.g_y, g.g_w, g.g_h);
            if((gw->flags & GEMTK_WM_FLAG_CUSTOM_SCROLLING) == 0) {
                if(gemtk_wm_update_slider(gw, GEMTK_WM_VH_SLIDER)) {
                    gemtk_wm_exec_redraw(gw, NULL);
                }
            }
        }


        break;

    case WM_FULLED:
        wind_get_grect(DESKTOP_HANDLE, WF_WORKXYWH, &g);
        wind_get_grect(gw->handle, WF_CURRXYWH, &g2);
        if(g.g_w == g2.g_w && g.g_h == g2.g_h) {
            wind_get_grect(gw->handle, WF_PREVXYWH, &g);
        }
        wind_set_grect(gw->handle, WF_CURRXYWH, &g);
        if((gw->flags & GEMTK_WM_FLAG_CUSTOM_SCROLLING) == 0) {
            if(gemtk_wm_update_slider(gw, GEMTK_WM_VH_SLIDER)) {
                gemtk_wm_exec_redraw(gw, NULL);
            }
        }
        break;

    case WM_ICONIFY:
        wind_set(gw->handle, WF_ICONIFY, msg[4], msg[5], msg[6], msg[7]);
        gw->state |= GEMTK_WM_STATUS_ICONIFIED;
        break;

    case WM_UNICONIFY:
        wind_set(gw->handle, WF_UNICONIFY, msg[4], msg[5], msg[6], msg[7]);
        gw->state &= ~(GEMTK_WM_STATUS_ICONIFIED);
        break;

    case WM_SHADED:
        gw->state |= GEMTK_WM_STATUS_SHADED;
        break;

    case WM_UNSHADED:
        gw->state &= ~(GEMTK_WM_STATUS_SHADED);
        break;

    case WM_REDRAW:
        if ((gw->flags & GEMTK_WM_FLAG_CUSTOM_TOOLBAR) == 0
			&& (gw->toolbar != NULL)) {
            g.g_x = msg[4];
            g.g_y = msg[5];
            g.g_w = msg[6];
            g.g_h = msg[7];
            if((gw->state & GEMTK_WM_STATUS_ICONIFIED) == 0){
                gemtk_wm_toolbar_redraw(gw, WM_REDRAW, &g);
            }
        }
        if (gw->form != NULL) {
			g.g_x = msg[4];
            g.g_y = msg[5];
            g.g_w = msg[6];
            g.g_h = msg[7];
			gemtk_wm_form_redraw(gw, &g);
        }
        break;

    default:
        retval = 0;
        break;

    }

    return(retval);
}

/**
* Preprocess mouse events
*/
static short preproc_mu_button(GUIWIN * gw, EVMULT_OUT *ev_out, short msg[8])
{
    short retval = 0, obj_idx = 0;

    DEBUG_PRINT(("preproc_mu_button\n"));

    // toolbar handling:
    if ((gw->flags & GEMTK_WM_FLAG_CUSTOM_TOOLBAR) == 0
            && gw->toolbar != NULL) {

        GRECT tb_area;

        gemtk_wm_get_grect(gw, GEMTK_WM_AREA_TOOLBAR, &tb_area);

        if (POINT_WITHIN(ev_out->emo_mouse.p_x,
                         ev_out->emo_mouse.p_y, tb_area)) {

            gw->toolbar[gw->toolbar_idx].ob_x = tb_area.g_x;
            gw->toolbar[gw->toolbar_idx].ob_y = tb_area.g_y;
			obj_idx = objc_find(gw->toolbar,
                                      gw->toolbar_idx, 8,
                                      ev_out->emo_mouse.p_x,
                                      ev_out->emo_mouse.p_y);

			gw->toolbar_focus_obj = obj_idx;

            DEBUG_PRINT(("Toolbar index: %d\n", obj_idx));
            if (obj_idx > -1
				&& (gw->toolbar[obj_idx].ob_state & OS_DISABLED)== 0
					&& ((gw->flags & GEMTK_WM_FLAG_CUSTOM_TOOLBAR) == 0)) {

                uint16_t type = (gw->toolbar[obj_idx].ob_type & 0xFF);
                uint16_t nextobj;

                DEBUG_PRINT(("toolbar item type: %d, toolbar_edit_obj: %d\n",
							type, gw->toolbar_edit_obj));
                // Report mouse click to the tree:
                retval = form_wbutton(gw->toolbar, gw->toolbar_focus_obj,
                                ev_out->emo_mclicks, &nextobj,
                                gw->handle);
				if (nextobj == obj_idx
					&& (type == G_FTEXT || type == G_FBOXTEXT))  {
						gw->toolbar_edit_obj = obj_idx;
				}
				else {
					gw->toolbar_edit_obj = -1;
				}
            }

			// send WM_TOOLBAR message
            short oldevents = ev_out->emo_events;
            short msg_out[8] = {WM_TOOLBAR, gl_apid,
                                0, gw->handle,
                                obj_idx, ev_out->emo_mclicks,
                                ev_out->emo_kmeta, ev_out->emo_mbutton
                               };
            ev_out->emo_events = MU_MESAG;
            // notify the window about toolbar click:
            gw->handler_func(gw, ev_out, msg_out);
            ev_out->emo_events = oldevents;
            retval = 1;
        } else {
			if (gw->toolbar_edit_obj != -1) {
				gw->toolbar_edit_obj = -1;
			}
        }
    }

    if (gw->form != NULL) {

        GRECT content_area;
        struct gemtk_wm_scroll_info_s *slid;

        DEBUG_PRINT(("preproc_mu_button: handling form click.\n"));

        gemtk_wm_get_grect(gw, GEMTK_WM_AREA_CONTENT, &content_area);

        if (POINT_WITHIN(ev_out->emo_mouse.p_x,
                         ev_out->emo_mouse.p_y, content_area)) {

            slid = gemtk_wm_get_scroll_info(gw);

			// adjust form position (considering window and scroll position):
            gw->form[gw->form_idx].ob_x = content_area.g_x -
                                          (slid->x_pos * slid->x_unit_px);
            gw->form[gw->form_idx].ob_y = content_area.g_y -
                                          (slid->y_pos * slid->y_unit_px);

            obj_idx = objc_find(gw->form, gw->form_idx, 8,
                                           ev_out->emo_mouse.p_x, ev_out->emo_mouse.p_y);
			gw->form_focus_obj = obj_idx;
			DEBUG_PRINT(("Window Form click, obj: %d\n", gw->form_focus_obj));
            if (obj_idx > -1
                    && (gw->form[obj_idx].ob_state & OS_DISABLED)== 0) {

                uint16_t type = (gw->form[obj_idx].ob_type & 0xFF);
                uint16_t nextobj;

                DEBUG_PRINT(("type: %d\n", type));

				retval = form_wbutton(gw->form, gw->form_focus_obj,
                                ev_out->emo_mclicks, &nextobj,
                                gw->handle);

				if (nextobj == obj_idx
					&& (type == G_FTEXT || type == G_FBOXTEXT))  {
						gw->form_edit_obj = obj_idx;
				}
				else {
					gw->form_edit_obj = -1;
				}

                short oldevents = ev_out->emo_events;
                short msg_out[8] = { GEMTK_WM_WM_FORM_CLICK, gl_apid,
										0, gw->handle,
										gw->form_focus_obj, ev_out->emo_mclicks,
										ev_out->emo_kmeta, 0
									};
				ev_out->emo_events = MU_MESAG;
				// notify the window about form click:
				gw->handler_func(gw, ev_out, msg_out);
				ev_out->emo_events = oldevents;
				retval = 1;
				evnt_timer(150);
            }
        }
        else {
			gw->form_edit_obj = -1;

        }
    }

    return(retval);
}

/**
* Preprocess keyboard events (for forms/toolbars)
*/
static short preproc_mu_keybd(GUIWIN * gw, EVMULT_OUT *ev_out, short msg[8])
{
	short retval = 0;

    if ((gw->toolbar != NULL) && (gw->toolbar_edit_obj > -1)) {

        short next_edit_obj = gw->toolbar_edit_obj;
        short next_char = -1;
        short edit_idx;
        short r;

		DEBUG_PRINT(("%s, gw: %p, toolbar_edit_obj: %d\n", __FUNCTION__, gw,
				gw->toolbar_edit_obj));

        r = form_wkeybd(gw->toolbar, gw->toolbar_edit_obj, next_edit_obj,
                       ev_out->emo_kreturn,
                       &next_edit_obj, &next_char, gw->handle);

        if (next_edit_obj != gw->toolbar_edit_obj) {
			gemtk_wm_set_toolbar_edit_obj(gw, next_edit_obj,
											ev_out->emo_kreturn);
        } else {
            if (next_char > 13) {
                r = objc_wedit(gw->toolbar, gw->toolbar_edit_obj,
                              ev_out->emo_kreturn, &edit_idx,
                              EDCHAR, gw->handle);
            }
        }
        //retval = 1;
        /*gemtk_wm_send_msg(gw, GEMTK_WM_WM_FORM_KEY, gw->toolbar_edit_obj,
						ev_out->emo_kreturn, 0, 0);*/
    }

    if((gw->form != NULL) && (gw->form_edit_obj > -1) ) {

        short next_edit_obj = gw->form_edit_obj;
        short next_char = -1;
        short edit_idx;
        short r;

        r = form_wkeybd(gw->form, gw->form_edit_obj, next_edit_obj,
                       ev_out->emo_kreturn,
                       &next_edit_obj, &next_char, gw->handle);

        if (next_edit_obj != gw->form_edit_obj) {

			if(gw->form_edit_obj != -1) {
				objc_wedit(gw->form, gw->form_edit_obj,
                      ev_out->emo_kreturn, &edit_idx,
                      EDEND, gw->handle);
			}

            gw->form_edit_obj = next_edit_obj;

            objc_wedit(gw->form, gw->form_edit_obj,
                      ev_out->emo_kreturn, &edit_idx,
                      EDINIT, gw->handle);
        } else {
            if(next_char > 13)
                r = objc_wedit(gw->form, gw->form_edit_obj,
                              ev_out->emo_kreturn, &edit_idx,
                              EDCHAR, gw->handle);
        }
    }
    return(retval);
}

/**
* Default toolbar redraw function
*/
static void std_toolbar_redraw(GUIWIN *gw, uint16_t msg, GRECT *clip)
{
	GRECT g, tb_area;

    gemtk_wm_get_grect(gw, GEMTK_WM_AREA_TOOLBAR, &tb_area);

	assert(gw->toolbar);
	assert(gw->toolbar_idx >= 0);

	// Update object position:
	gw->toolbar[gw->toolbar_idx].ob_x = tb_area.g_x;
	gw->toolbar[gw->toolbar_idx].ob_y = tb_area.g_y;
	gw->toolbar[gw->toolbar_idx].ob_width = tb_area.g_w;
	gw->toolbar[gw->toolbar_idx].ob_height = tb_area.g_h;

	wind_get_grect(gw->handle, WF_FIRSTXYWH, &g);
	while (g.g_h > 0 || g.g_w > 0) {
		if(rc_intersect(clip, &g)) {
			objc_draw(gw->toolbar, gw->toolbar_idx, 8, g.g_x, g.g_y,
					g.g_w, g.g_h);

		}
		wind_get_grect(gw->handle, WF_NEXTXYWH, &g);
	}
}

/**
* Event Dispatcher function. The guiwin API doesn't own an event loop,
* so you have to inform it for every event that you want it to handle.
*/
short gemtk_wm_dispatch_event(EVMULT_IN *ev_in, EVMULT_OUT *ev_out, short msg[8])
{
    GUIWIN *dest;
    short retval = 0;
    bool handler_called = false;

    if( (ev_out->emo_events & MU_MESAG) != 0 ) {
        DEBUG_PRINT(("gemtk_wm_handle_event_multi_fast: %d (%x)\n", msg[0],
					msg[0]));
        switch (msg[0]) {
        case WM_REDRAW:
        case WM_CLOSED:
        case WM_TOPPED:
        case WM_ARROWED:
        case WM_HSLID:
        case WM_VSLID:
        case WM_FULLED:
        case WM_SIZED:
        case WM_REPOSED:
        case WM_MOVED:
        case WM_NEWTOP:
        case WM_UNTOPPED:
        case WM_ONTOP:
        case WM_BOTTOM:
        case WM_ICONIFY:
        case WM_UNICONIFY:
        case WM_ALLICONIFY:
        case WM_TOOLBAR:
        case AP_DRAGDROP:
        case AP_TERM:
        case AP_TFAIL:
            dest = gemtk_wm_find(msg[3]);
            if (dest) {
                DEBUG_PRINT(("Found WM_ dest: %p (%d), flags: %d, cb: %p\n",
							dest, dest->handle, dest->flags,
							dest->handler_func));
                if (dest->flags&GEMTK_WM_FLAG_PREPROC_WM) {
                    retval = preproc_wm(dest, ev_out, msg);
                    if(((retval == 0)||(dest->flags&GEMTK_WM_FLAG_RECV_PREPROC_WM))) {
                        retval = dest->handler_func(dest, ev_out, msg);
                        handler_called = true;
                    }
                } else {
                    if (dest->handler_func) {
                        retval = dest->handler_func(dest, ev_out, msg);
                        handler_called = true;
                    }
                }

            }
            break;
// TODO: check code with Thing! Desktop
/*
    We receive VA_PROTOSTATUS but AV_START doesn't seem to cause
    an TeraDesk response. Check if something happens with Thing!
    Desktop.
/*
/*
        case VA_PROTOSTATUS:
        case VA_VIEWED:
        case AV_STARTED:
            gemtk_av_dispatch(msg);
            break;
*/
        }
    } else {

        short h_aes;
        h_aes = wind_find(ev_out->emo_mouse.p_x, ev_out->emo_mouse.p_y);
        if(h_aes > 0 && (ev_out->emo_events != MU_TIMER))  {

            dest = gemtk_wm_find(h_aes);

            if (dest == NULL || dest->handler_func == NULL)
                return(0);

			DEBUG_PRINT(("Found Event receiver GUIWIN: %p (%d), flags: %d, cb: %p\n",
							dest, dest->handle, dest->flags, dest->handler_func));

            if ((ev_out->emo_events & MU_BUTTON) != 0) {
				DEBUG_PRINT(("gemtk_wm_handle_event_multi_fast: MU_BUTTON -> "
								"%d / %d\n", ev_out->emo_mouse.p_x,
								ev_out->emo_mouse.p_y));
                retval = preproc_mu_button(dest, ev_out, msg);
                if(retval != 0) {
                    handler_called = true;
                }
            }

            if ((ev_out->emo_events & MU_KEYBD)) {
				DEBUG_PRINT(("gemtk_wm_handle_event_multi_fast: MU_KEYBD -> %x\n",
								ev_out->emo_kreturn));
                retval = preproc_mu_keybd(dest, ev_out, msg);
                if(retval != 0) {
                    handler_called = true;
                }
            }

            if (handler_called==false) {
                retval = dest->handler_func(dest, ev_out, msg);
            }
        }
    }

    return(retval);
}

/**
*	Initialises the guiwin API
*/
short gemtk_wm_init(void)
{
    if(v_vdi_h == -1) {
        short dummy;
        short work_in[12] = {Getrez()+2,1,1,1,1,1,1,1,1,1,2,1};
        v_vdi_h=graf_handle(&dummy, &dummy, &dummy, &dummy);
        v_opnvwk(work_in, &v_vdi_h, work_out);
    }
    return(0);
}

void gemtk_wm_exit(void)
{
    v_clsvwk(v_vdi_h);
}

/**
* Adds and AES handle to the guiwin list and creates and GUIWIN management
* structure.
*
* \param handle The AES handle
* \param flags Creation flags, configures how the AES window is handled
* \param cb event handler function for that window
*/
GUIWIN * gemtk_wm_add(short handle, uint32_t flags, gemtk_wm_event_handler_f cb)
{

    GUIWIN *win = calloc(1, sizeof(GUIWIN));

    assert(win!=NULL);
    DEBUG_PRINT(("gemtk_wm_add: %d, %p, cb: %p\n", handle, win, cb));

    win->handle = handle;
    win->handler_func = cb;
    win->flags = flags;
	gemtk_wm_link(win);

    DEBUG_PRINT(("Added guiwin: %p, tb: %p\n", win, win->toolbar));
    return(win);
}

/**
* Returns an GUIWIN* for AES handle, when that AES window is managed by gemtk_wm
*/
GUIWIN *gemtk_wm_find(short handle)
{
    GUIWIN *g;
    DEBUG_PRINT(("guiwin search handle: %d\n", handle));
    for (g = winlist; g != NULL; g=g->next) {
        if(g->handle == handle) {
            DEBUG_PRINT(("guiwin found handle: %p\n", g));
            return(g);
        }
    }
    return(NULL);
}

void gemtk_wm_dump_window_info(GUIWIN *win)
{



	char title[255];
	GRECT work_area;
	GRECT curr_area;
	GRECT gemtk_work_area;
	GRECT gemtk_toolbar_area;
	GRECT gemtk_free_area;
	short handle;
	struct gemtk_wm_scroll_info_s *slid;

	handle = gemtk_wm_get_handle(win);

	assert(handle);

	gemtk_wind_get_str(handle, WF_NAME, title, 255);
	wind_get_grect(handle, WF_WORKXYWH, &work_area);
	wind_get_grect(handle, WF_CURRXYWH, &curr_area);
	gemtk_wm_get_grect(win, GEMTK_WM_AREA_CONTENT, &gemtk_free_area);
	gemtk_wm_get_grect(win, GEMTK_WM_AREA_WORK, &gemtk_work_area);
	gemtk_wm_get_grect(win, GEMTK_WM_AREA_TOOLBAR, &gemtk_toolbar_area);
	slid = gemtk_wm_get_scroll_info(win);

	printf ("GEMTK Window:       %p (AES handle: %d)\n", win, win->handle);
	printf ("Title:              %s\n", title);
    GEMTK_DBG_GRECT ("WF_WORKXYWH:          \n", &work_area)
    GEMTK_DBG_GRECT ("WF_CURRXYWH:          \n", &curr_area)
    GEMTK_DBG_GRECT ("GEMTK_WM_AREA_CONTENT:\n", &gemtk_free_area)
    GEMTK_DBG_GRECT ("GEMTK_WM_AREA_WORK:\n",    &gemtk_work_area)
    GEMTK_DBG_GRECT ("GEMTK_WM_AREA_TOOLBAR:\n", &gemtk_toolbar_area)
    printf ("Slider X pos:       %d\n", slid->x_pos);
    printf ("Slider Y pos:       %d\n", slid->y_pos);
    printf ("Slider X units:     %d\n", slid->x_unit_px);
    printf ("Slider Y units:     %d\n", slid->y_unit_px);


#undef DBG_GRECT
};

/**
* Check's if the pointer is managed by the guiwin API.
*/
GUIWIN *gemtk_wm_validate_ptr(GUIWIN *win)
{
    GUIWIN *g;
    for( g = winlist; g != NULL; g=g->next ) {
        DEBUG_PRINT(("guiwin gemtk_wm_validate_ptr check: %p\n", g));
        if(g == win) {
            DEBUG_PRINT(("gemtk_wm_validate_ptr valid: %p\n", g));
            return(g);
        }
    }
    return(NULL);
}

/**
* Add the GUIWIN to the list of handled windows.
*/
GUIWIN *gemtk_wm_link(GUIWIN *win)
{
	/* Make sure the window is not linked: */
	GUIWIN *win_val = gemtk_wm_validate_ptr(win);
	if(win_val){
		DEBUG_PRINT(("GUIWIN %p is already linked!\n", win));
		return(NULL);
	}

	if (winlist == NULL) {
        winlist = win;
        win->next = NULL;
        win->prev = NULL;
    } else {
        GUIWIN *tmp = winlist;
        while( tmp->next != NULL ) {
            tmp = tmp->next;
        }
        tmp->next = win;
        win->prev = tmp;
        win->next = NULL;
    }
    return(win);
}

/**
* Remove the GUIWIN from the list of handled windows.
*/
GUIWIN *gemtk_wm_unlink(GUIWIN *win)
{
	GUIWIN * win_val;

	/* Make sure the window is linked: */
	win_val = gemtk_wm_validate_ptr(win);
    if (win_val == NULL){
    	DEBUG_PRINT(("GUIWIN %p is not linked!\n", win));
		return(NULL);
    }


    /* unlink the window: */
    if(win->prev != NULL ) {
        win->prev->next = win->next;
    } else {
        winlist = win->next;
    }
    if (win->next != NULL) {
        win->next->prev = win->prev;
    }
    return(win);
}

/**
* Remove an GUIWIN from the list of managed windows and free the GUIWIN.
* Call this when the AES window is closed or deleted.
*/
short gemtk_wm_remove(GUIWIN *win)
{
	gemtk_wm_unlink(win);
    DEBUG_PRINT(("guiwin free: %p\n", win));
    free(win);
    return(0);
}

/** Calculate & get a well known area within the GUIWIN.
* \param win The GUIWIN ptr.
* \param mode Specifies the area to retrieve.
* \param dest The calculated rectangle.
*/
void gemtk_wm_get_grect(GUIWIN *win, enum guwin_area_e mode, GRECT *dest)
{

    assert(win != NULL);

    wind_get_grect(win->handle, WF_WORKXYWH, dest);

    if (mode == GEMTK_WM_AREA_CONTENT) {
        GRECT tb_area;
        gemtk_wm_get_grect(win, GEMTK_WM_AREA_TOOLBAR, &tb_area);
        if (win->flags & GEMTK_WM_FLAG_HAS_VTOOLBAR) {
            dest->g_x += tb_area.g_w;
            dest->g_w -= tb_area.g_w;
        }
        else {
			dest->g_y += tb_area.g_h;
            dest->g_h -= tb_area.g_h;
        }
    } else if (mode == GEMTK_WM_AREA_TOOLBAR) {
    	if (win->toolbar) {
			if (win->flags & GEMTK_WM_FLAG_HAS_VTOOLBAR) {
				dest->g_w = win->toolbar_size;
			} else {
				dest->g_h = win->toolbar_size;
			}
    	}
    	else {
			dest->g_w = 0;
			dest->g_h = 0;
    	}
    }
}


/**
*	Scroll the content area (GEMTK_WM_AREA_CONTENT) in the specified dimension
*	(GEMTK_WM_VSLIDER, GEMTK_WM_HSLIDER)
*	\param win The GUIWIN
* 	\param orientation GEMTK_WM_VSLIDER or GEMTK_WM_HSLIDER
* 	\param units the amout to scroll (pass negative values to scroll up)
*	\param refresh Sliders will be updated when this flag is set
*/
void gemtk_wm_scroll(GUIWIN *win, short orientation, int units, bool refresh)
{
    struct gemtk_wm_scroll_info_s *slid = gemtk_wm_get_scroll_info(win);
    int oldpos = 0, newpos = 0, vis_units=0, pix = 0;
    int abs_pix = 0;
    GRECT *redraw=NULL, g, g_ro;

    gemtk_wm_get_grect(win, GEMTK_WM_AREA_CONTENT, &g);
    g_ro = g;

    if (orientation == GEMTK_WM_VSLIDER) {
        pix = units*slid->y_unit_px;
        abs_pix = abs(pix);
        oldpos = slid->y_pos;
        vis_units = g.g_h/slid->y_unit_px;
        newpos = slid->y_pos = MIN(slid->y_units-vis_units,
                                   MAX(0, slid->y_pos+units));
        if(newpos < 0) {
            newpos = slid->y_pos = 0;
        }
        if(oldpos == newpos)
            return;

        if (units>=vis_units || gemtk_wm_has_intersection(win, &g_ro)) {
            // send complete redraw
            redraw = &g_ro;
        } else {
            // only adjust ypos when scrolling down:
            if(pix < 0 ) {
                // blit screen area:
                g.g_h -= abs_pix;
                move_rect(win, &g, 0, abs_pix);
                g.g_y = g_ro.g_y;
                g.g_h = abs_pix;
                redraw = &g;
            } else {
                // blit screen area:
                g.g_y += abs_pix;
                g.g_h -= abs_pix;
                move_rect(win, &g, 0, -abs_pix);
                g.g_y = g_ro.g_y + g_ro.g_h - abs_pix;
                g.g_h = abs_pix;
                redraw = &g;
            }
        }
    } else {
        pix = units*slid->x_unit_px;
        abs_pix = abs(pix);
        oldpos = slid->x_pos;
        vis_units = g.g_w/slid->x_unit_px;
        newpos = slid->x_pos = MIN(slid->x_units-vis_units,
                                   MAX(0, slid->x_pos+units));

        if(newpos < 0) {
            newpos = slid->x_pos = 0;
        }

        if(oldpos == newpos)
            return;
        if (units>=vis_units || gemtk_wm_has_intersection(win, &g_ro)) {
            // send complete redraw
            redraw = &g_ro;
        } else {
            // only adjust ypos when scrolling down:
            if(pix < 0 ) {
                // blit screen area:
                g.g_w -= abs_pix;
                move_rect(win, &g, abs_pix, 0);
                g.g_x = g_ro.g_x;
                g.g_w = abs_pix;
                redraw = &g;
            } else {
                // blit screen area:
                g.g_x += abs_pix;
                g.g_w -= abs_pix;
                move_rect(win, &g, -abs_pix, 0);
                g.g_x = g_ro.g_x + g_ro.g_w - abs_pix;
                g.g_w = abs_pix;
                redraw = &g;
            }
        }
    }

    if (refresh) {
        gemtk_wm_update_slider(win, orientation);
    }

    if ((redraw != NULL) && (redraw->g_h > 0)) {
        gemtk_wm_exec_redraw(win, redraw);
    }
}

/**
* Refresh the sliders of the window.
* \param win the GUIWIN
* \param mode bitmask, valid bits: GEMTK_WM_VSLIDER, GEMTK_WM_HSLIDER
*/
bool gemtk_wm_update_slider(GUIWIN *win, short mode)
{
    GRECT viewport;
    struct gemtk_wm_scroll_info_s * slid;
    unsigned long size, pos;
    int old_x, old_y;

    short handle = gemtk_wm_get_handle(win);
    gemtk_wm_get_grect(win, GEMTK_WM_AREA_CONTENT, &viewport);
    slid = gemtk_wm_get_scroll_info(win);

    old_x = slid->x_pos;
    old_y = slid->y_pos;

    // TODO: check if the window has sliders of that direction...?

    if((mode & GEMTK_WM_VSLIDER) && (slid->y_unit_px > 0)) {
        if ( slid->y_units < (long)viewport.g_h/slid->y_unit_px) {
            size = 1000L;
        } else
            size = MAX( 50L, (unsigned long)viewport.g_h*1000L/
                        (unsigned long)(slid->y_unit_px*slid->y_units));
        wind_set(handle, WF_VSLSIZE, (int)size, 0, 0, 0);

        if (slid->y_units > (long)viewport.g_h/slid->y_unit_px) {
            pos = (unsigned long)slid->y_pos *1000L/
                  (unsigned long)(slid->y_units-viewport.g_h/slid->y_unit_px);
            wind_set(handle, WF_VSLIDE, (int)pos, 0, 0, 0);
        } else if (slid->y_pos) {
            slid->y_pos = 0;
            wind_set(handle, WF_VSLIDE, 0, 0, 0, 0);
        }
    }
    if((mode & GEMTK_WM_HSLIDER) && (slid->x_unit_px > 0)) {
        if ( slid->x_units < (long)viewport.g_w/slid->x_unit_px)
            size = 1000L;
        else
            size = MAX( 50L, (unsigned long)viewport.g_w*1000L/
                        (unsigned long)(slid->x_unit_px*slid->x_units));
        wind_set(handle, WF_HSLSIZE, (int)size, 0, 0, 0);

        if( slid->x_units > (long)viewport.g_w/slid->x_unit_px) {
            pos = (unsigned long)slid->x_pos*1000L/
                  (unsigned long)(slid->x_units-viewport.g_w/slid->x_unit_px);
            wind_set(handle, WF_HSLIDE, (int)pos, 0, 0, 0);
        } else if (slid->x_pos) {
            slid->x_pos = 0;
            wind_set(handle, WF_HSLIDE, 0, 0, 0, 0);
        }
    }

    if(old_x != slid->x_pos || old_y != slid->y_pos) {
        return(true);
    }
    return(false);
}

/**
* Return the AES handle for the GUIWIN.
*/
short gemtk_wm_get_handle(GUIWIN *win)
{
    return(win->handle);
}

/**
* Return the VDI handle for an GUIWIN.
*/
VdiHdl gemtk_wm_get_vdi_handle(GUIWIN *win)
{
    return(v_vdi_h);
}

/**
* Returns the state bitmask of the window
*/
uint32_t gemtk_wm_get_state(GUIWIN *win)
{
    return(win->state);
}

/**
* Set and new event handler function.
*/
void gemtk_wm_set_event_handler(GUIWIN *win,gemtk_wm_event_handler_f cb)
{
    win->handler_func = cb;
}

/**
* Configure the window so that it shows an toolbar or at least reserves
* an area to draw an toolbar.
* \param win The GUIWIN
* \param toolbar the AES form
* \param idx index within the toolbar tree (0 in most cases...)
* \param flags optional configuration flags
*/
//TODO: document flags
void gemtk_wm_set_toolbar(GUIWIN *win, OBJECT *toolbar, short idx, uint32_t flags)
{
    win->toolbar = toolbar;
    win->toolbar_idx = idx;
    win->toolbar_edit_obj = -1;
    if (flags & GEMTK_WM_FLAG_HAS_VTOOLBAR) {
        win->flags |= GEMTK_WM_FLAG_HAS_VTOOLBAR;
        win->toolbar_size = win->toolbar[idx].ob_width;
    }
    else {
		win->toolbar_size = win->toolbar[idx].ob_height;
    }
	gemtk_wm_set_toolbar_redraw_func(win, std_toolbar_redraw);
}

/**  Update width/height of the toolbar region
* \param win the GUIWIN
* \param s the width or height, depending on the flag GEMTK_WM_FLAG_HAS_VTOOLBAR
*/
void gemtk_wm_set_toolbar_size(GUIWIN *win, uint16_t s)
{
	win->toolbar_size = s;
}

short gemtk_wm_get_toolbar_edit_obj(GUIWIN *win)
{
	return(win->toolbar_edit_obj);
}

/** Set the current active edit object */
void gemtk_wm_set_toolbar_edit_obj(GUIWIN *win, uint16_t obj, short kreturn)
{
	short edit_idx;

	DEBUG_PRINT(("%s, win: %p, obj: %d, kret: %d\n", __FUNCTION__,
				win, obj, kreturn));

	if (obj != win->toolbar_edit_obj) {

		DEBUG_PRINT(("%s, current edit obj: %d\n", __FUNCTION__,
					win->toolbar_edit_obj));

		if(win->toolbar_edit_obj != -1) {
			objc_wedit(win->toolbar, win->toolbar_edit_obj, kreturn, &edit_idx,
						EDEND, win->handle);
		}

		win->toolbar_edit_obj = obj;

		objc_wedit(win->toolbar, win->toolbar_edit_obj, kreturn, &edit_idx,
					EDINIT, win->handle);
	} else {
		DEBUG_PRINT(("%s, nothing to do!\n", __FUNCTION__));
	}
}

/** Set an custom toolbar redraw function which is called instead of
*	default drawing routine.
* \param win the GUIWIN
* \param func the custom redraw function
*/
void gemtk_wm_set_toolbar_redraw_func(GUIWIN *win, gemtk_wm_redraw_f func)
{
	win->toolbar_redraw_func = func;
}

/**
* Attach an arbitary pointer to the GUIWIN
*/
void gemtk_wm_set_user_data(GUIWIN *win, void *data)
{
    win->user_data = data;
}

/**
* Retrieve the user_data pointer attached to the GUIWIN.
*/
void *gemtk_wm_get_user_data(GUIWIN *win)
{
    return(win->user_data);
}

/** Get the scroll management structure for a GUIWIN
*/
struct gemtk_wm_scroll_info_s *gemtk_wm_get_scroll_info(GUIWIN *win) {
    return(&win->scroll_info);
}

/**
*	Get the amount of content dimensions within the window
* 	which is calculated by using the scroll_info attached to the GUIWIN.
*/
void gemtk_wm_set_scroll_grid(GUIWIN * win, short x, short y)
{
    struct gemtk_wm_scroll_info_s *slid = gemtk_wm_get_scroll_info(win);

    assert(slid != NULL);

    slid->y_unit_px = x;
    slid->x_unit_px = y;
}


/** Set the size of the content measured in content units
* \param win the GUIWIN
* \param x horizontal size
* \param y vertical size
*/
void gemtk_wm_set_content_units(GUIWIN * win, short x, short y)
{
    struct gemtk_wm_scroll_info_s *slid = gemtk_wm_get_scroll_info(win);

    assert(slid != NULL);

    slid->x_units = x;
    slid->y_units = y;
}

/** Send an Message to a GUIWIN using AES message pipe
* \param win the GUIWIN which shall receive the message
* \param msg_type the WM_ message definition
* \param a the 4th parameter to appl_write
* \param b the 5th parameter to appl_write
* \param c the 6th parameter to appl_write
* \param d the 7th parameter to appl_write
*/
void gemtk_wm_send_msg(GUIWIN *win, short msg_type, short a, short b, short c,
                     short d)
{
    short msg[8];

    msg[0] = msg_type;
    msg[1] = gl_apid;
    msg[2] = 0;
    msg[3] = (win != NULL) ? win->handle : 0;
    msg[4] = a;
    msg[5] = b;
    msg[6] = c;
    msg[7] = d;

    appl_write(gl_apid, 16, &msg);
}

/** Directly execute an Message to a GUIWIN using the internal dispatcher function.
*   This only works for managed windows which have the,
	GEMTK_WM_FLAG_PREPROC_WM flag set.
	This call does not send any AES messages.
* \param win the GUIWIN which shall receive the message
* \param msg_type the WM_ message definition
* \param a the 4th parameter to appl_write
* \param b the 5th parameter to appl_write
* \param c the 6th parameter to appl_write
* \param d the 7th parameter to appl_write
*/
short gemtk_wm_exec_msg(GUIWIN *win, short msg_type, short a, short b, short c,
                     short d)
{
    short msg[8], retval;
    GRECT work;

    EVMULT_OUT event_out;
    EVMULT_IN event_in = {
        .emi_flags = MU_MESAG | MU_TIMER | MU_KEYBD | MU_BUTTON,
        .emi_bclicks = 258,
        .emi_bmask = 3,
        .emi_bstate = 0,
        .emi_m1leave = MO_ENTER,
        .emi_m1 = {0,0,0,0},
        .emi_m2leave = 0,
        .emi_m2 = {0,0,0,0},
        .emi_tlow = 0,
        .emi_thigh = 0
    };

    msg[0] = msg_type;
    msg[1] = gl_apid;
    msg[2] = 0;
    msg[3] = win->handle;
    msg[4] = a;
    msg[5] = b;
    msg[6] = c;
    msg[7] = d;

    event_out.emo_events = MU_MESAG;
    retval = preproc_wm(win, &event_out, msg);
    if (retval == 0 || (win->flags & GEMTK_WM_FLAG_PREPROC_WM) != 0){
		retval = win->handler_func(win, &event_out, msg);
    }

    return(retval);
}

void gemtk_wm_exec_redraw(GUIWIN *win, GRECT *area)
{
    GRECT work;

    if (area == NULL) {
        gemtk_wm_get_grect(win, GEMTK_WM_AREA_WORK, &work);
        if (work.g_w < 1 || work.g_h < 1) {
            if (win->toolbar != NULL) {
                gemtk_wm_get_grect(win, GEMTK_WM_AREA_TOOLBAR, &work);
                if (work.g_w < 1 || work.g_h < 1) {
                    return;
                }
            }
        }
        area = &work;
    }

    gemtk_wm_exec_msg(win, WM_REDRAW, area->g_x, area->g_y, area->g_w,
		area->g_h);
}

/** Attach an AES FORM to the GUIWIN, similar feature like the toolbar
*/
void gemtk_wm_set_form(GUIWIN *win, OBJECT *tree, short index)
{
	DEBUG_PRINT(("Setting form %p (%d) for window %p\n", tree, index, win));
    win->form = tree;
    win->form_edit_obj = -1;
    win->form_focus_obj = -1;
    win->form_idx = index;
}

/** Checks if a GUIWIN is overlapped by other windows.
*/
bool gemtk_wm_has_intersection(GUIWIN *win, GRECT *work)
{
    GRECT area, mywork;
    bool retval = true;

    if (work == NULL) {
        gemtk_wm_get_grect(win, GEMTK_WM_AREA_CONTENT, &mywork);
        work = &mywork;
    }

    wind_get_grect(win->handle, WF_FIRSTXYWH, &area);
    while (area.g_w && area.g_w) {
        //GRECT * ptr = &area;
        if (RC_WITHIN(work, &area)) {
            retval = false;
        }
        wind_get_grect(win->handle, WF_NEXTXYWH, &area);
    }

    return(retval);
}

/** Execute an toolbar redraw
* \param msg specifies the AES message which initiated the redraw, or 0 when
*				the function was called without AES message context.
*/
void gemtk_wm_toolbar_redraw(GUIWIN *gw, uint16_t msg, GRECT *clip)
{
    GRECT tb_area, tb_area_ro, g;

    gemtk_wm_get_grect(gw, GEMTK_WM_AREA_TOOLBAR, &tb_area_ro);

    if(clip == NULL) {
        clip = &tb_area_ro;
    }

    tb_area = tb_area_ro;

    if (rc_intersect(clip, &tb_area)) {
		gw->toolbar_redraw_func(gw, msg, &tb_area);
    }
}

/** Execute FORM redraw
*/
void gemtk_wm_form_redraw(GUIWIN *gw, GRECT *clip)
{
    GRECT area, area_ro, g;
	int scroll_px_x, scroll_px_y;
	struct gemtk_wm_scroll_info_s *slid;
	//int new_x, new_y, old_x, old_y;
	short edit_idx;

	DEBUG_PRINT(("gemtk_wm_form_redraw\n"));

	// calculate form coordinates, include scrolling:
    gemtk_wm_get_grect(gw, GEMTK_WM_AREA_CONTENT, &area_ro);
	slid = gemtk_wm_get_scroll_info(gw);

	// Update form position:
	gw->form[gw->form_idx].ob_x = area_ro.g_x - (slid->x_pos * slid->x_unit_px);
	gw->form[gw->form_idx].ob_y = area_ro.g_y - (slid->y_pos * slid->y_unit_px);

    if(clip == NULL) {
        clip = &area_ro;
    }

    area = area_ro;

	/* Walk the AES rectangle list and redraw the visible areas of the window:*/
    if(rc_intersect(clip, &area)) {

        wind_get_grect(gw->handle, WF_FIRSTXYWH, &g);
        while (g.g_h > 0 || g.g_w > 0) {
            if(rc_intersect(&area, &g)) {
                objc_draw(gw->form, gw->form_idx, 8, g.g_x, g.g_y,
                          g.g_w, g.g_h);

            }
            wind_get_grect(gw->handle, WF_NEXTXYWH, &g);
        }
    }
}


/** Fill the content area with white color
*/
void gemtk_wm_clear(GUIWIN *win)
{
    GRECT area, g;
    short pxy[4];
    VdiHdl vh;

    vh = gemtk_wm_get_vdi_handle(win);

    if(win->state & GEMTK_WM_STATUS_ICONIFIED) {
        // also clear the toolbar area when iconified:
        gemtk_wm_get_grect(win, GEMTK_WM_AREA_WORK, &area);
    } else {
        gemtk_wm_get_grect(win, GEMTK_WM_AREA_CONTENT, &area);
    }

    vsf_interior(vh, FIS_SOLID);
    vsf_color(vh, 0);
    vswr_mode(vh, MD_REPLACE);
    wind_get_grect(win->handle, WF_FIRSTXYWH, &g);
    while (g.g_h > 0 || g.g_w > 0) {
        if(rc_intersect(&area, &g)) {
            pxy[0] = g.g_x;
            pxy[1] = g.g_y;
            pxy[2] = g.g_x+g.g_w-1;
            pxy[3] = g.g_y+g.g_h-1;
            v_bar(vh, pxy);
        }
        wind_get_grect(win->handle, WF_NEXTXYWH, &g);
    }
}
