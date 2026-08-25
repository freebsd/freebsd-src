#
# Copyright (c) 2026 Abdelkader Boudih <freebsd@seuros.com>
#
# SPDX-License-Identifier: BSD-2-Clause
#

#include <sys/bus.h>
#include <dev/video/video.h>

INTERFACE video;

CODE {
	static int
	video_default_open(device_t dev __unused)
	{

		return (0);
	}

	static void
	video_default_close(device_t dev __unused)
	{
	}

	static int
	video_default_frmsize(device_t dev __unused,
	    struct video_frmsizeenum *fs __unused)
	{

		return (ENOTTY);
	}

	static int
	video_default_frmival(device_t dev __unused,
	    struct video_frmivalenum *fi __unused)
	{

		return (ENOTTY);
	}

	static int
	video_default_format(device_t dev __unused,
	    struct video_format *fmt __unused)
	{

		return (ENOTTY);
	}

	static int
	video_default_fract(device_t dev __unused,
	    struct video_fract *fr __unused)
	{

		return (ENOTTY);
	}

	static int
	video_default_enum_input(device_t dev __unused, uint32_t index __unused,
	    struct video_input *inp __unused)
	{

		return (ENOTTY);
	}

	static int
	video_default_get_input(device_t dev __unused, uint32_t *idx __unused)
	{

		return (ENOTTY);
	}

	static int
	video_default_set_input(device_t dev __unused, uint32_t idx __unused)
	{

		return (ENOTTY);
	}

	static int
	video_default_query_control(device_t dev __unused,
	    struct video_control_desc *d __unused)
	{

		return (ENOTTY);
	}

	static int
	video_default_get_control(device_t dev __unused,
	    struct video_control *c __unused)
	{

		return (ENOTTY);
	}

	static int
	video_default_set_control(device_t dev __unused,
	    const struct video_control *c __unused)
	{

		return (ENOTTY);
	}
};

METHOD int open {
	device_t	dev;
} DEFAULT video_default_open;

METHOD void close {
	device_t	dev;
} DEFAULT video_default_close;

METHOD int querycap {
	device_t		 dev;
	struct video_caps	*caps;
};

METHOD int enum_format {
	device_t		 dev;
	uint32_t		 index;
	struct video_format	*format;
};

METHOD int enum_framesizes {
	device_t			 dev;
	struct video_frmsizeenum	*fsize;
} DEFAULT video_default_frmsize;

METHOD int enum_frameintervals {
	device_t			 dev;
	struct video_frmivalenum	*fival;
} DEFAULT video_default_frmival;

METHOD int try_format {
	device_t		 dev;
	struct video_format	*format;
} DEFAULT video_default_format;

METHOD int set_format {
	device_t			 dev;
	const struct video_format	*format;
};

METHOD int get_format {
	device_t		 dev;
	struct video_format	*format;
};

METHOD int get_parm {
	device_t		 dev;
	struct video_fract	*fract;
} DEFAULT video_default_fract;

METHOD int set_parm {
	device_t		 dev;
	struct video_fract	*fract;
} DEFAULT video_default_fract;

METHOD int enum_input {
	device_t		 dev;
	uint32_t		 index;
	struct video_input	*input;
} DEFAULT video_default_enum_input;

METHOD int get_input {
	device_t	 dev;
	uint32_t	*index;
} DEFAULT video_default_get_input;

METHOD int set_input {
	device_t	dev;
	uint32_t	index;
} DEFAULT video_default_set_input;

METHOD int query_control {
	device_t			 dev;
	struct video_control_desc	*desc;
} DEFAULT video_default_query_control;

METHOD int get_control {
	device_t		 dev;
	struct video_control	*control;
} DEFAULT video_default_get_control;

METHOD int set_control {
	device_t			 dev;
	const struct video_control	*control;
} DEFAULT video_default_set_control;

# stop_stream must quiesce the provider before returning.
METHOD int start_stream {
	device_t	dev;
};

METHOD void stop_stream {
	device_t	dev;
};
