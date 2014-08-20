/*
 * Copyright 2011 John-Mark Bell <jmb@netsurf-browser.org>
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

#include <gst/gst.h>

#include "content/content_factory.h"
#include "content/content_protected.h"
#include "image/video.h"

typedef struct nsvideo_content {
	struct content base;

	GstElement *playbin;
	GstElement *appsrc;
} nsvideo_content;

static gboolean nsvideo_bus_call(GstBus *bus, GstMessage *msg, 
		nsvideo_content *video)
{
	switch (GST_MESSAGE_TYPE(msg)) {
	case GST_MESSAGE_ERROR:
		break;
	case GST_MESSAGE_EOS:
		break;
	default:
		break;
	}

	return TRUE;
}

static void nsvideo_need_data_event(GstElement *playbin, guint size,
		nsvideo_content *video)
{
}

static void nsvideo_enough_data_event(GstElement *playbin,
		nsvideo_content *video)
{
}

static void nsvideo_source_event(GObject *object, GObject *orig, 
		GParamSpec *pspec, nsvideo_content *video)
{
	g_object_get(orig, pspec->name, &video->appsrc, NULL);

	g_signal_connect(video->appsrc, "need-data", 
			G_CALLBACK(nsvideo_need_data_event), video);
	g_signal_connect(video->appsrc, "enough-data",
			G_CALLBACK(nsvideo_enough_data_event), video);
}

static nserror nsvideo_create(const content_handler *handler,
		lwc_string *imime_type, const http_parameter *params,
		llcache_handle *llcache,
		const char *fallback_charset, bool quirks,
		struct content **c)
{
	nsvideo_content *video;
	nserror error;
	GstBus *bus;

	video = calloc(1, sizeof(nsvideo_content));
	if (video == NULL)
		return NSERROR_NOMEM;

	error = content__init(&video->base, handler, imime_type, params,
			llcache, fallback_charset, quirks);
	if (error != NSERROR_OK) {
		free(video);
		return error;
	}

	error = llcache_handle_force_stream(llcache);
	if (error != NSERROR_OK) {
		free(video);
		return error;
	}

	video->playbin = gst_element_factory_make("playbin2", NULL);
	if (video->playbin == NULL) {
		free(video);
		return NSERROR_NOMEM;
	}

	bus = gst_pipeline_get_bus(GST_PIPELINE(video->playbin));
	gst_bus_add_watch(bus, (GstBusFunc) nsvideo_bus_call, video);
	gst_object_unref(bus);

	g_object_set(video->playbin, "uri", "appsrc://", NULL);
	g_signal_connect(video->playbin, "deep-notify::source",
			G_CALLBACK(nsvideo_source_event), video);

	/** \todo Create appsink & register with playbin */

	gst_element_set_state(video->playbin, GST_STATE_PLAYING);
	
	*c = (struct content *) video;

	return NSERROR_OK;
}

static bool nsvideo_process_data(struct content *c, const char *data, 
		unsigned int size)
{
	nsvideo_content *video = (nsvideo_content *) c;
	GstBuffer *buffer;
	GstFlowReturn ret;

	buffer = gst_buffer_new();
	GST_BUFFER_DATA(buffer) = (guint8 *) data;
	GST_BUFFER_SIZE(buffer) = (gsize) size;

	/* Send data to appsrc */
	g_signal_emit_by_name(video->appsrc, "push-buffer", buffer, &ret);

	return ret == GST_FLOW_OK;
}

static bool nsvideo_convert(struct content *c)
{
	nsvideo_content *video = (nsvideo_content *) c;
	GstFlowReturn ret;

	/* Tell appsrc we're done */
	g_signal_emit_by_name(video->appsrc, "end-of-stream", &ret);

	/* Appsink will flag DONE on receipt of first frame */

	return ret == GST_FLOW_OK;
}

static void nsvideo_destroy(struct content *c)
{
	nsvideo_content *video = (nsvideo_content *) c;

	gst_element_set_state(video->playbin, GST_STATE_NULL);
	gst_object_unref(video->playbin);
}

static bool nsvideo_redraw(struct content *c, struct content_redraw_data *data,
		const struct rect *clip, const struct redraw_context *ctx)
{
	/** \todo Implement */
	return true;
}

static nserror nsvideo_clone(const struct content *old, struct content **newc)
{
	/** \todo Implement */
	return NSERROR_CLONE_FAILED;
}

static content_type nsvideo_type(void)
{
	/** \todo Lies */
	return CONTENT_IMAGE;
}

static void *nsvideo_get_internal(const struct content *c, void *context)
{
	/** \todo Return pointer to bitmap containing current frame, if any? */
	return NULL;
}

static const content_handler nsvideo_content_handler = {
	.create = nsvideo_create,
	.process_data = nsvideo_process_data,
	.data_complete = nsvideo_convert,
	.destroy = nsvideo_destroy,
	.redraw = nsvideo_redraw,
	.clone = nsvideo_clone,
	.type = nsvideo_type,
	.get_internal = nsvideo_get_internal,
	/* Can't share videos because we stream them */
	.no_share = true
};

static const char *nsvideo_types[] = {
	"video/mp4",
	"video/webm"
};

CONTENT_FACTORY_REGISTER_TYPES(nsvideo, nsvideo_types, 
		nsvideo_content_handler);

