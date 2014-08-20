#include <libcss/computed.h>
#include <libcss/properties.h>
#include <libcss/types.h>

static size_t dump_css_fixed(css_fixed f, char *ptr, size_t len)
{
#define ABS(x) (uint32_t)((x) < 0 ? -(x) : (x))
	uint32_t uintpart = FIXTOINT(ABS(f));
	/* + 500 to ensure round to nearest (division will truncate) */
	uint32_t fracpart = ((ABS(f) & 0x3ff) * 1000 + 500) / (1 << 10);
#undef ABS
	size_t flen = 0;
	char tmp[20];
	size_t tlen = 0;
	char *buf = ptr;

	if (len == 0)
		return 0;

	if (f < 0) {
		buf[0] = '-';
		buf++;
		len--;
	}

	do {
		tmp[tlen] = "0123456789"[uintpart % 10];
		tlen++;

		uintpart /= 10;
	} while (tlen < 20 && uintpart != 0);

	while (tlen > 0 && len > 0) {
		buf[0] = tmp[--tlen];
		buf++;
		len--;
	}

	if (len > 0) {
		buf[0] = '.';
		buf++;
		len--;
	}

	do {
		tmp[tlen] = "0123456789"[fracpart % 10];
		tlen++;

		fracpart /= 10;
	} while (tlen < 20 && fracpart != 0);

	while (tlen > 0 && len > 0) {
		buf[0] = tmp[--tlen];
		buf++;
		flen++;
		len--;
	}

	while (flen < 3 && len > 0) {
		buf[0] = '0';
		buf++;
		flen++;
		len--;
	}

	if (len > 0)
		buf[0] = '\0';

	return buf - ptr;
}
static size_t dump_css_number(css_fixed val, char *ptr, size_t len)
{
	if (INTTOFIX(FIXTOINT(val)) == val)
		return snprintf(ptr, len, "%d", FIXTOINT(val));
	else
		return dump_css_fixed(val, ptr, len);
}

static size_t dump_css_unit(css_fixed val, css_unit unit, char *ptr, size_t len)
{
	size_t ret = dump_css_number(val, ptr, len);

	switch (unit) {
	case CSS_UNIT_PX:
		ret += snprintf(ptr + ret, len - ret, "px");
		break;
	case CSS_UNIT_EX:
		ret += snprintf(ptr + ret, len - ret, "ex");
		break;
	case CSS_UNIT_EM:
		ret += snprintf(ptr + ret, len - ret, "em");
		break;
	case CSS_UNIT_IN:
		ret += snprintf(ptr + ret, len - ret, "in");
		break;
	case CSS_UNIT_CM:
		ret += snprintf(ptr + ret, len - ret, "cm");
		break;
	case CSS_UNIT_MM:
		ret += snprintf(ptr + ret, len - ret, "mm");
		break;
	case CSS_UNIT_PT:
		ret += snprintf(ptr + ret, len - ret, "pt");
		break;
	case CSS_UNIT_PC:
		ret += snprintf(ptr + ret, len - ret, "pc");
		break;
	case CSS_UNIT_PCT:
		ret += snprintf(ptr + ret, len - ret, "%%");
		break;
	case CSS_UNIT_DEG:
		ret += snprintf(ptr + ret, len - ret, "deg");
		break;
	case CSS_UNIT_GRAD:
		ret += snprintf(ptr + ret, len - ret, "grad");
		break;
	case CSS_UNIT_RAD:
		ret += snprintf(ptr + ret, len - ret, "rad");
		break;
	case CSS_UNIT_MS:
		ret += snprintf(ptr + ret, len - ret, "ms");
		break;
	case CSS_UNIT_S:
		ret += snprintf(ptr + ret, len - ret, "s");
		break;
	case CSS_UNIT_HZ:
		ret += snprintf(ptr + ret, len - ret, "Hz");
		break;
	case CSS_UNIT_KHZ:
		ret += snprintf(ptr + ret, len - ret, "kHz");
		break;
	}

	return ret;
}


static void dump_computed_style(const css_computed_style *style, char *buf,
		size_t *len)
{
	char *ptr = buf;
	size_t wrote = 0;
	uint8_t val;
	css_color color = 0;
	lwc_string *url = NULL;
	css_fixed len1 = 0, len2 = 0;
	css_unit unit1 = CSS_UNIT_PX, unit2 = CSS_UNIT_PX;
	css_computed_clip_rect rect = { 0, 0, 0, 0, CSS_UNIT_PX, CSS_UNIT_PX,
					CSS_UNIT_PX, CSS_UNIT_PX, true, true,
					true, true };
	const css_computed_content_item *content = NULL;
	const css_computed_counter *counter = NULL;
	lwc_string **string_list = NULL;
	int32_t zindex = 0;

	/* background-attachment */
	val = css_computed_background_attachment(style);
	switch (val) {
	case CSS_BACKGROUND_ATTACHMENT_INHERIT:
		wrote = snprintf(ptr, *len, "background-attachment: inherit\n");
		break;
	case CSS_BACKGROUND_ATTACHMENT_FIXED:
		wrote = snprintf(ptr, *len, "background-attachment: fixed\n");
		break;
	case CSS_BACKGROUND_ATTACHMENT_SCROLL:
		wrote = snprintf(ptr, *len, "background-attachment: scroll\n");
		break;
	default:
		wrote = 0;
		break;
	}
	ptr += wrote;
	*len -= wrote;

	/* background-color */
	val = css_computed_background_color(style, &color);
	switch (val) {
	case CSS_BACKGROUND_COLOR_INHERIT:
		wrote = snprintf(ptr, *len, "background-color: inherit\n");
		break;
	case CSS_BACKGROUND_COLOR_COLOR:
		wrote = snprintf(ptr, *len, "background-color: #%08x\n", color);
		break;
	default:
		wrote = 0;
		break;
	}
	ptr += wrote;
	*len -= wrote;

	/* background-image */
	val = css_computed_background_image(style, &url);
        if (val == CSS_BACKGROUND_IMAGE_INHERIT) {
                wrote = snprintf(ptr, *len, "background-image: inherit\n");
	} else if (val == CSS_BACKGROUND_IMAGE_IMAGE && url != NULL) {
		wrote = snprintf(ptr, *len, "background-image: url('%.*s')\n",
				(int) lwc_string_length(url), 
				lwc_string_data(url));
	} else if (val == CSS_BACKGROUND_IMAGE_NONE) {
		wrote = snprintf(ptr, *len, "background-image: none\n");
	} else {
		wrote = 0;
	}
	ptr += wrote;
	*len -= wrote;

	/* background-position */
	val = css_computed_background_position(style, &len1, &unit1,
			&len2, &unit2);
	if (val == CSS_BACKGROUND_POSITION_INHERIT) {
                wrote = snprintf(ptr, *len, "background-position: inherit\n");
                ptr += wrote;
                *len -= wrote;
        } else if (val == CSS_BACKGROUND_POSITION_SET) {
		wrote = snprintf(ptr, *len, "background-position: ");
		ptr += wrote;
		*len -= wrote;

		wrote = dump_css_unit(len1, unit1, ptr, *len);
		ptr += wrote;
		*len -= wrote;

		wrote = snprintf(ptr, *len, " ");
		ptr += wrote;
		*len -= wrote;

		wrote = dump_css_unit(len2, unit2, ptr, *len);
		ptr += wrote;
		*len -= wrote;

		wrote = snprintf(ptr, *len, "\n");
		ptr += wrote;
		*len -= wrote;
	}

	/* background-repeat */
	val = css_computed_background_repeat(style);
	switch (val) {
	case CSS_BACKGROUND_REPEAT_INHERIT:
		wrote = snprintf(ptr, *len, "background-repeat: inherit\n");
		break;
	case CSS_BACKGROUND_REPEAT_REPEAT_X:
		wrote = snprintf(ptr, *len, "background-repeat: repeat-x\n");
		break;
	case CSS_BACKGROUND_REPEAT_REPEAT_Y:
		wrote = snprintf(ptr, *len, "background-repeat: repeat-y\n");
		break;
	case CSS_BACKGROUND_REPEAT_REPEAT:
		wrote = snprintf(ptr, *len, "background-repeat: repeat\n");
		break;
	case CSS_BACKGROUND_REPEAT_NO_REPEAT:
		wrote = snprintf(ptr, *len, "background-repeat: no-repeat\n");
		break;
	default:
		wrote = 0;
		break;
	}
	ptr += wrote;
	*len -= wrote;

	/* border-collapse */
	val = css_computed_border_collapse(style);
	switch (val) {
	case CSS_BORDER_COLLAPSE_INHERIT:
		wrote = snprintf(ptr, *len, "border-collapse: inherit\n");
		break;
	case CSS_BORDER_COLLAPSE_SEPARATE:
		wrote = snprintf(ptr, *len, "border-collapse: separate\n");
		break;
	case CSS_BORDER_COLLAPSE_COLLAPSE:
		wrote = snprintf(ptr, *len, "border-collapse: collapse\n");
		break;
	default:
		wrote = 0;
		break;
	}
	ptr += wrote;
	*len -= wrote;

	/* border-spacing */
	val = css_computed_border_spacing(style, &len1, &unit1, &len2, &unit2);
        if (val == CSS_BORDER_SPACING_INHERIT) {
                wrote = snprintf(ptr, *len, "border-spacing: inherit\n");
                ptr += wrote;
                *len -= wrote;
	} else if (val == CSS_BORDER_SPACING_SET) {
		wrote = snprintf(ptr, *len, "border-spacing: ");
		ptr += wrote;
		*len -= wrote;

		wrote = dump_css_unit(len1, unit1, ptr, *len);
		ptr += wrote;
		*len -= wrote;

		wrote = snprintf(ptr, *len, " ");
		ptr += wrote;
		*len -= wrote;

		wrote = dump_css_unit(len2, unit2, ptr, *len);
		ptr += wrote;
		*len -= wrote;

		wrote = snprintf(ptr, *len, "\n");
		ptr += wrote;
		*len -= wrote;
	}

	/* border-top-color */
	val = css_computed_border_top_color(style, &color);
	switch (val) {
	case CSS_BORDER_COLOR_INHERIT:
		wrote = snprintf(ptr, *len, "border-top-color: inherit\n");
		break;
	case CSS_BORDER_COLOR_CURRENT_COLOR:
		wrote = snprintf(ptr, *len, "border-top-color: currentColor\n");
		break;
	case CSS_BORDER_COLOR_COLOR:
		wrote = snprintf(ptr, *len, "border-top-color: #%08x\n", color);
		break;
	default:
		wrote = 0;
		break;
	}
	ptr += wrote;
	*len -= wrote;

	/* border-right-color */
	val = css_computed_border_right_color(style, &color);
	switch (val) {
	case CSS_BORDER_COLOR_INHERIT:
		wrote = snprintf(ptr, *len, "border-right-color: inherit\n");
		break;
	case CSS_BORDER_COLOR_CURRENT_COLOR:
		wrote = snprintf(ptr, *len, "border-right-color: currentColor\n");
		break;
	case CSS_BORDER_COLOR_COLOR:
		wrote = snprintf(ptr, *len,
				"border-right-color: #%08x\n", color);
		break;
	default:
		wrote = 0;
		break;
	}
	ptr += wrote;
	*len -= wrote;

	/* border-bottom-color */
	val = css_computed_border_bottom_color(style, &color);
	switch (val) {
	case CSS_BORDER_COLOR_INHERIT:
		wrote = snprintf(ptr, *len, "border-bottom-color: inherit\n");
		break;
	case CSS_BORDER_COLOR_CURRENT_COLOR:
		wrote = snprintf(ptr, *len, "border-bottom-color: currentColor\n");
		break;
	case CSS_BORDER_COLOR_COLOR:
		wrote = snprintf(ptr, *len,
				"border-bottom-color: #%08x\n", color);
		break;
	default:
		wrote = 0;
		break;
	}
	ptr += wrote;
	*len -= wrote;

	/* border-left-color */
	val = css_computed_border_left_color(style, &color);
	switch (val) {
	case CSS_BORDER_COLOR_INHERIT:
		wrote = snprintf(ptr, *len, "border-left-color: inherit\n");
		break;
	case CSS_BORDER_COLOR_CURRENT_COLOR:
		wrote = snprintf(ptr, *len, "border-left-color: currentColor\n");
		break;
	case CSS_BORDER_COLOR_COLOR:
		wrote = snprintf(ptr, *len,
				"border-left-color: #%08x\n", color);
		break;
	default:
		wrote = 0;
		break;
	}
	ptr += wrote;
	*len -= wrote;

	/* border-top-style */
	val = css_computed_border_top_style(style);
	switch (val) {
	case CSS_BORDER_STYLE_INHERIT:
		wrote = snprintf(ptr, *len, "border-top-style: inherit\n");
		break;
	case CSS_BORDER_STYLE_NONE:
		wrote = snprintf(ptr, *len, "border-top-style: none\n");
		break;
	case CSS_BORDER_STYLE_HIDDEN:
		wrote = snprintf(ptr, *len, "border-top-style: hidden\n");
		break;
	case CSS_BORDER_STYLE_DOTTED:
		wrote = snprintf(ptr, *len, "border-top-style: dotted\n");
		break;
	case CSS_BORDER_STYLE_DASHED:
		wrote = snprintf(ptr, *len, "border-top-style: dashed\n");
		break;
	case CSS_BORDER_STYLE_SOLID:
		wrote = snprintf(ptr, *len, "border-top-style: solid\n");
		break;
	case CSS_BORDER_STYLE_DOUBLE:
		wrote = snprintf(ptr, *len, "border-top-style: double\n");
		break;
	case CSS_BORDER_STYLE_GROOVE:
		wrote = snprintf(ptr, *len, "border-top-style: groove\n");
		break;
	case CSS_BORDER_STYLE_RIDGE:
		wrote = snprintf(ptr, *len, "border-top-style: ridge\n");
		break;
	case CSS_BORDER_STYLE_INSET:
		wrote = snprintf(ptr, *len, "border-top-style: inset\n");
		break;
	case CSS_BORDER_STYLE_OUTSET:
		wrote = snprintf(ptr, *len, "border-top-style: outset\n");
		break;
	default:
		wrote = 0;
		break;
	}
	ptr += wrote;
	*len -= wrote;

	/* border-right-style */
	val = css_computed_border_right_style(style);
	switch (val) {
	case CSS_BORDER_STYLE_INHERIT:
		wrote = snprintf(ptr, *len, "border-right-style: inherit\n");
		break;
	case CSS_BORDER_STYLE_NONE:
		wrote = snprintf(ptr, *len, "border-right-style: none\n");
		break;
	case CSS_BORDER_STYLE_HIDDEN:
		wrote = snprintf(ptr, *len, "border-right-style: hidden\n");
		break;
	case CSS_BORDER_STYLE_DOTTED:
		wrote = snprintf(ptr, *len, "border-right-style: dotted\n");
		break;
	case CSS_BORDER_STYLE_DASHED:
		wrote = snprintf(ptr, *len, "border-right-style: dashed\n");
		break;
	case CSS_BORDER_STYLE_SOLID:
		wrote = snprintf(ptr, *len, "border-right-style: solid\n");
		break;
	case CSS_BORDER_STYLE_DOUBLE:
		wrote = snprintf(ptr, *len, "border-right-style: double\n");
		break;
	case CSS_BORDER_STYLE_GROOVE:
		wrote = snprintf(ptr, *len, "border-right-style: groove\n");
		break;
	case CSS_BORDER_STYLE_RIDGE:
		wrote = snprintf(ptr, *len, "border-right-style: ridge\n");
		break;
	case CSS_BORDER_STYLE_INSET:
		wrote = snprintf(ptr, *len, "border-right-style: inset\n");
		break;
	case CSS_BORDER_STYLE_OUTSET:
		wrote = snprintf(ptr, *len, "border-right-style: outset\n");
		break;
	default:
		wrote = 0;
		break;
	}
	ptr += wrote;
	*len -= wrote;

	/* border-bottom-style */
	val = css_computed_border_bottom_style(style);
	switch (val) {
	case CSS_BORDER_STYLE_INHERIT:
		wrote = snprintf(ptr, *len, "border-bottom-style: inherit\n");
		break;
	case CSS_BORDER_STYLE_NONE:
		wrote = snprintf(ptr, *len, "border-bottom-style: none\n");
		break;
	case CSS_BORDER_STYLE_HIDDEN:
		wrote = snprintf(ptr, *len, "border-bottom-style: hidden\n");
		break;
	case CSS_BORDER_STYLE_DOTTED:
		wrote = snprintf(ptr, *len, "border-bottom-style: dotted\n");
		break;
	case CSS_BORDER_STYLE_DASHED:
		wrote = snprintf(ptr, *len, "border-bottom-style: dashed\n");
		break;
	case CSS_BORDER_STYLE_SOLID:
		wrote = snprintf(ptr, *len, "border-bottom-style: solid\n");
		break;
	case CSS_BORDER_STYLE_DOUBLE:
		wrote = snprintf(ptr, *len, "border-bottom-style: double\n");
		break;
	case CSS_BORDER_STYLE_GROOVE:
		wrote = snprintf(ptr, *len, "border-bottom-style: groove\n");
		break;
	case CSS_BORDER_STYLE_RIDGE:
		wrote = snprintf(ptr, *len, "border-bottom-style: ridge\n");
		break;
	case CSS_BORDER_STYLE_INSET:
		wrote = snprintf(ptr, *len, "border-bottom-style: inset\n");
		break;
	case CSS_BORDER_STYLE_OUTSET:
		wrote = snprintf(ptr, *len, "border-bottom-style: outset\n");
		break;
	default:
		wrote = 0;
		break;
	}
	ptr += wrote;
	*len -= wrote;

	/* border-left-style */
	val = css_computed_border_left_style(style);
	switch (val) {
	case CSS_BORDER_STYLE_INHERIT:
		wrote = snprintf(ptr, *len, "border-left-style: inherit\n");
		break;
	case CSS_BORDER_STYLE_NONE:
		wrote = snprintf(ptr, *len, "border-left-style: none\n");
		break;
	case CSS_BORDER_STYLE_HIDDEN:
		wrote = snprintf(ptr, *len, "border-left-style: hidden\n");
		break;
	case CSS_BORDER_STYLE_DOTTED:
		wrote = snprintf(ptr, *len, "border-left-style: dotted\n");
		break;
	case CSS_BORDER_STYLE_DASHED:
		wrote = snprintf(ptr, *len, "border-left-style: dashed\n");
		break;
	case CSS_BORDER_STYLE_SOLID:
		wrote = snprintf(ptr, *len, "border-left-style: solid\n");
		break;
	case CSS_BORDER_STYLE_DOUBLE:
		wrote = snprintf(ptr, *len, "border-left-style: double\n");
		break;
	case CSS_BORDER_STYLE_GROOVE:
		wrote = snprintf(ptr, *len, "border-left-style: groove\n");
		break;
	case CSS_BORDER_STYLE_RIDGE:
		wrote = snprintf(ptr, *len, "border-left-style: ridge\n");
		break;
	case CSS_BORDER_STYLE_INSET:
		wrote = snprintf(ptr, *len, "border-left-style: inset\n");
		break;
	case CSS_BORDER_STYLE_OUTSET:
		wrote = snprintf(ptr, *len, "border-left-style: outset\n");
		break;
	default:
		wrote = 0;
		break;
	}
	ptr += wrote;
	*len -= wrote;

	/* border-top-width */
	val = css_computed_border_top_width(style, &len1, &unit1);
	switch (val) {
	case CSS_BORDER_WIDTH_INHERIT:
		wrote = snprintf(ptr, *len, "border-top-width: inherit\n");
		break;
	case CSS_BORDER_WIDTH_THIN:
		wrote = snprintf(ptr, *len, "border-top-width: thin\n");
		break;
	case CSS_BORDER_WIDTH_MEDIUM:
		wrote = snprintf(ptr, *len, "border-top-width: medium\n");
		break;
	case CSS_BORDER_WIDTH_THICK:
		wrote = snprintf(ptr, *len, "border-top-width: thick\n");
		break;
	case CSS_BORDER_WIDTH_WIDTH:
		wrote = snprintf(ptr, *len, "border-top-width: ");
		ptr += wrote;
		*len -= wrote;

		wrote = dump_css_unit(len1, unit1, ptr, *len);
		ptr += wrote;
		*len -= wrote;

		wrote = snprintf(ptr, *len, "\n");
		break;
	default:
		wrote = 0;
		break;
	}
	ptr += wrote;
	*len -= wrote;

	/* border-right-width */
	val = css_computed_border_right_width(style, &len1, &unit1);
	switch (val) {
	case CSS_BORDER_WIDTH_INHERIT:
		wrote = snprintf(ptr, *len, "border-right-width: inherit\n");
		break;
	case CSS_BORDER_WIDTH_THIN:
		wrote = snprintf(ptr, *len, "border-right-width: thin\n");
		break;
	case CSS_BORDER_WIDTH_MEDIUM:
		wrote = snprintf(ptr, *len, "border-right-width: medium\n");
		break;
	case CSS_BORDER_WIDTH_THICK:
		wrote = snprintf(ptr, *len, "border-right-width: thick\n");
		break;
	case CSS_BORDER_WIDTH_WIDTH:
		wrote = snprintf(ptr, *len, "border-right-width: ");
		ptr += wrote;
		*len -= wrote;

		wrote = dump_css_unit(len1, unit1, ptr, *len);
		ptr += wrote;
		*len -= wrote;

		wrote = snprintf(ptr, *len, "\n");
		break;
	default:
		wrote = 0;
		break;
	}
	ptr += wrote;
	*len -= wrote;

	/* border-bottom-width */
	val = css_computed_border_bottom_width(style, &len1, &unit1);
	switch (val) {
	case CSS_BORDER_WIDTH_INHERIT:
		wrote = snprintf(ptr, *len, "border-bottom-width: inherit\n");
		break;
	case CSS_BORDER_WIDTH_THIN:
		wrote = snprintf(ptr, *len, "border-bottom-width: thin\n");
		break;
	case CSS_BORDER_WIDTH_MEDIUM:
		wrote = snprintf(ptr, *len, "border-bottom-width: medium\n");
		break;
	case CSS_BORDER_WIDTH_THICK:
		wrote = snprintf(ptr, *len, "border-bottom-width: thick\n");
		break;
	case CSS_BORDER_WIDTH_WIDTH:
		wrote = snprintf(ptr, *len, "border-bottom-width: ");
		ptr += wrote;
		*len -= wrote;

		wrote = dump_css_unit(len1, unit1, ptr, *len);
		ptr += wrote;
		*len -= wrote;

		wrote = snprintf(ptr, *len, "\n");
		break;
	default:
		wrote = 0;
		break;
	}
	ptr += wrote;
	*len -= wrote;

	/* border-left-width */
	val = css_computed_border_left_width(style, &len1, &unit1);
	switch (val) {
	case CSS_BORDER_WIDTH_INHERIT:
		wrote = snprintf(ptr, *len, "border-left-width: inherit\n");
		break;
	case CSS_BORDER_WIDTH_THIN:
		wrote = snprintf(ptr, *len, "border-left-width: thin\n");
		break;
	case CSS_BORDER_WIDTH_MEDIUM:
		wrote = snprintf(ptr, *len, "border-left-width: medium\n");
		break;
	case CSS_BORDER_WIDTH_THICK:
		wrote = snprintf(ptr, *len, "border-left-width: thick\n");
		break;
	case CSS_BORDER_WIDTH_WIDTH:
		wrote = snprintf(ptr, *len, "border-left-width: ");
		ptr += wrote;
		*len -= wrote;

		wrote = dump_css_unit(len1, unit1, ptr, *len);
		ptr += wrote;
		*len -= wrote;

		wrote = snprintf(ptr, *len, "\n");
		break;
	default:
		wrote = 0;
		break;
	}
	ptr += wrote;
	*len -= wrote;

	/* bottom */
	val = css_computed_bottom(style, &len1, &unit1);
	switch (val) {
	case CSS_BOTTOM_INHERIT:
		wrote = snprintf(ptr, *len, "bottom: inherit\n");
		break;
	case CSS_BOTTOM_AUTO:
		wrote = snprintf(ptr, *len, "bottom: auto\n");
		break;
	case CSS_BOTTOM_SET:
		wrote = snprintf(ptr, *len, "bottom: ");
		ptr += wrote;
		*len -= wrote;

		wrote = dump_css_unit(len1, unit1, ptr, *len);
		ptr += wrote;
		*len -= wrote;

		wrote = snprintf(ptr, *len, "\n");
		break;
	default:
		wrote = 0;
		break;
	}
	ptr += wrote;
	*len -= wrote;

	/* caption-side */
	val = css_computed_caption_side(style);
	switch (val) {
	case CSS_CAPTION_SIDE_INHERIT:
		wrote = snprintf(ptr, *len, "caption-side: inherit\n");
		break;
	case CSS_CAPTION_SIDE_TOP:
		wrote = snprintf(ptr, *len, "caption-side: top\n");
		break;
	case CSS_CAPTION_SIDE_BOTTOM:
		wrote = snprintf(ptr, *len, "caption-side: bottom\n");
		break;
	default:
		wrote = 0;
		break;
	}
	ptr += wrote;
	*len -= wrote;

	/* clear */
	val = css_computed_clear(style);
	switch (val) {
	case CSS_CLEAR_INHERIT:
		wrote = snprintf(ptr, *len, "clear: inherit\n");
		break;
	case CSS_CLEAR_NONE:
		wrote = snprintf(ptr, *len, "clear: none\n");
		break;
	case CSS_CLEAR_LEFT:
		wrote = snprintf(ptr, *len, "clear: left\n");
		break;
	case CSS_CLEAR_RIGHT:
		wrote = snprintf(ptr, *len, "clear: right\n");
		break;
	case CSS_CLEAR_BOTH:
		wrote = snprintf(ptr, *len, "clear: both\n");
		break;
	default:
		wrote = 0;
		break;
	}
	ptr += wrote;
	*len -= wrote;

	/* clip */
	val = css_computed_clip(style, &rect);
	switch (val) {
	case CSS_CLIP_INHERIT:
		wrote = snprintf(ptr, *len, "clip: inherit\n");
		break;
	case CSS_CLIP_AUTO:
		wrote = snprintf(ptr, *len, "clip: auto\n");
		break;
	case CSS_CLIP_RECT:
		wrote = snprintf(ptr, *len, "clip: rect( ");
		ptr += wrote;
		*len -= wrote;

		if (rect.top_auto)
			wrote = snprintf(ptr, *len, "auto");
		else
			wrote = dump_css_unit(rect.top, rect.tunit,
					ptr, *len);
		wrote += snprintf(ptr + wrote, *len - wrote, ", ");
		ptr += wrote;
		*len -= wrote;

		if (rect.right_auto)
			wrote = snprintf(ptr, *len, "auto");
		else
			wrote = dump_css_unit(rect.right, rect.runit,
					ptr, *len);
		wrote += snprintf(ptr + wrote, *len - wrote, ", ");
		ptr += wrote;
		*len -= wrote;

		if (rect.bottom_auto)
			wrote = snprintf(ptr, *len, "auto");
		else
			wrote = dump_css_unit(rect.bottom, rect.bunit,
					ptr, *len);
		wrote += snprintf(ptr + wrote, *len - wrote, ", ");
		ptr += wrote;
		*len -= wrote;

		if (rect.left_auto)
			wrote = snprintf(ptr, *len, "auto");
		else
			wrote = dump_css_unit(rect.left, rect.lunit,
					ptr, *len);
		wrote += snprintf(ptr + wrote, *len - wrote, ")\n");
		break;
	default:
		wrote = 0;
		break;
	}
	ptr += wrote;
	*len -= wrote;

	/* color */
	val = css_computed_color(style, &color);
        if (val == CSS_COLOR_INHERIT) {
                wrote = snprintf(ptr, *len, "color: inherit\n");
	} else if (val == CSS_COLOR_COLOR) {
		wrote = snprintf(ptr, *len, "color: #%08x\n", color);
	}
        ptr += wrote;
        *len -= wrote;

	/* content */
	val = css_computed_content(style, &content);
	switch (val) {
	case CSS_CONTENT_INHERIT:
		wrote = snprintf(ptr, *len, "content: inherit\n");
		break;
	case CSS_CONTENT_NONE:
		wrote = snprintf(ptr, *len, "content: none\n");
		break;
	case CSS_CONTENT_NORMAL:
		wrote = snprintf(ptr, *len, "content: normal\n");
		break;
	case CSS_CONTENT_SET:
		wrote = snprintf(ptr, *len, "content:");
		ptr += wrote;
		*len -= wrote;

		while (content->type != CSS_COMPUTED_CONTENT_NONE) {
			wrote = snprintf(ptr, *len, " ");

			switch (content->type) {
			case CSS_COMPUTED_CONTENT_STRING:
				wrote += snprintf(ptr + wrote, 
						*len - wrote,
						"\"%.*s\"",
						(int) lwc_string_length(
						content->data.string),
						lwc_string_data(
						content->data.string));
				break;
			case CSS_COMPUTED_CONTENT_URI:
				wrote += snprintf(ptr + wrote, 
						*len - wrote,
						"uri(\"%.*s\")",
						(int) lwc_string_length(
						content->data.uri),
						lwc_string_data(
						content->data.uri));
				break;
			case CSS_COMPUTED_CONTENT_COUNTER:
				wrote += snprintf(ptr + wrote, 
						*len - wrote,
						"counter(%.*s)",
						(int) lwc_string_length(
						content->data.counter.name),
						lwc_string_data(
						content->data.counter.name));
				break;
			case CSS_COMPUTED_CONTENT_COUNTERS:
				wrote += snprintf(ptr + wrote, 
						*len - wrote,
						"counters(%.*s, "
							"\"%.*s\")",
						(int) lwc_string_length(
						content->data.counters.name),
						lwc_string_data(
						content->data.counters.name),
						(int) lwc_string_length(
						content->data.counters.sep),
						lwc_string_data(
						content->data.counters.sep));
				break;
			case CSS_COMPUTED_CONTENT_ATTR:
				wrote += snprintf(ptr + wrote, 
						*len - wrote,
						"attr(%.*s)",
						(int) lwc_string_length(
						content->data.attr),
						lwc_string_data(
						content->data.attr));
				break;
			case CSS_COMPUTED_CONTENT_OPEN_QUOTE:
				wrote += snprintf(ptr + wrote,
						*len - wrote,
						"open-quote");
				break;
			case CSS_COMPUTED_CONTENT_CLOSE_QUOTE:
				wrote += snprintf(ptr + wrote,
						*len - wrote,
						"close-quote");
				break;
			case CSS_COMPUTED_CONTENT_NO_OPEN_QUOTE:
				wrote += snprintf(ptr + wrote,
						*len - wrote,
						"no-open-quote");
				break;
			case CSS_COMPUTED_CONTENT_NO_CLOSE_QUOTE:
				wrote += snprintf(ptr + wrote,
						*len - wrote,
						"no-close-quote");
				break;
			}

			ptr += wrote;
			*len -= wrote;

			content++;
		}

		wrote = snprintf(ptr, *len, "\n");
		break;
	default:
		wrote = 0;
		break;
	}
	ptr += wrote;
	*len -= wrote;

	/* counter-increment */
	val = css_computed_counter_increment(style, &counter);
        if (val == CSS_COUNTER_INCREMENT_INHERIT) {
                wrote = snprintf(ptr, *len, "counter-increment: inherit\n");
        } else if (counter == NULL) {
		wrote = snprintf(ptr, *len, "counter-increment: none\n");
	} else {
		wrote = snprintf(ptr, *len, "counter-increment:");
		ptr += wrote;
		*len -= wrote;

		while (counter->name != NULL) {
			wrote = snprintf(ptr, *len, " %.*s ",
					(int) lwc_string_length(counter->name),
					lwc_string_data(counter->name));
			ptr += wrote;
			*len -= wrote;

			wrote = dump_css_fixed(counter->value, ptr, *len);
			ptr += wrote;
			*len -= wrote;

			counter++;
		}

		wrote = snprintf(ptr, *len, "\n");
	}
	ptr += wrote;
	*len -= wrote;

	/* counter-reset */
	val = css_computed_counter_reset(style, &counter);
        if (val == CSS_COUNTER_RESET_INHERIT) {
                wrote = snprintf(ptr, *len, "counter-reset: inherit\n");
	} else if (counter == NULL) {
		wrote = snprintf(ptr, *len, "counter-reset: none\n");
	} else {
		wrote = snprintf(ptr, *len, "counter-reset:");
		ptr += wrote;
		*len -= wrote;

		while (counter->name != NULL) {
			wrote = snprintf(ptr, *len, " %.*s ",
					(int) lwc_string_length(counter->name),
					lwc_string_data(counter->name));
			ptr += wrote;
			*len -= wrote;

			wrote = dump_css_fixed(counter->value, ptr, *len);
			ptr += wrote;
			*len -= wrote;

			counter++;
		}

		wrote = snprintf(ptr, *len, "\n");
	}
	ptr += wrote;
	*len -= wrote;

	/* cursor */
	val = css_computed_cursor(style, &string_list);
	wrote = snprintf(ptr, *len, "cursor:");
	ptr += wrote;
	*len -= wrote;

	if (string_list != NULL) {
		while (*string_list != NULL) {
			wrote = snprintf(ptr, *len, " url('%.*s')",
					(int) lwc_string_length(*string_list),
					lwc_string_data(*string_list));
			ptr += wrote;
			*len -= wrote;

			string_list++;
		}
	}
	switch (val) {
	case CSS_CURSOR_INHERIT:
		wrote = snprintf(ptr, *len, " inherit\n");
		break;
	case CSS_CURSOR_AUTO:
		wrote = snprintf(ptr, *len, " auto\n");
		break;
	case CSS_CURSOR_CROSSHAIR:
		wrote = snprintf(ptr, *len, " crosshair\n");
		break;
	case CSS_CURSOR_DEFAULT:
		wrote = snprintf(ptr, *len, " default\n");
		break;
	case CSS_CURSOR_POINTER:
		wrote = snprintf(ptr, *len, " pointer\n");
		break;
	case CSS_CURSOR_MOVE:
		wrote = snprintf(ptr, *len, " move\n");
		break;
	case CSS_CURSOR_E_RESIZE:
		wrote = snprintf(ptr, *len, " e-resize\n");
		break;
	case CSS_CURSOR_NE_RESIZE:
		wrote = snprintf(ptr, *len, " ne-resize\n");
		break;
	case CSS_CURSOR_NW_RESIZE:
		wrote = snprintf(ptr, *len, " nw-resize\n");
		break;
	case CSS_CURSOR_N_RESIZE:
		wrote = snprintf(ptr, *len, " n-resize\n");
		break;
	case CSS_CURSOR_SE_RESIZE:
		wrote = snprintf(ptr, *len, " se-resize\n");
		break;
	case CSS_CURSOR_SW_RESIZE:
		wrote = snprintf(ptr, *len, " sw-resize\n");
		break;
	case CSS_CURSOR_S_RESIZE:
		wrote = snprintf(ptr, *len, " s-resize\n");
		break;
	case CSS_CURSOR_W_RESIZE:
		wrote = snprintf(ptr, *len, " w-resize\n");
		break;
	case CSS_CURSOR_TEXT:
		wrote = snprintf(ptr, *len, " text\n");
		break;
	case CSS_CURSOR_WAIT:
		wrote = snprintf(ptr, *len, " wait\n");
		break;
	case CSS_CURSOR_HELP:
		wrote = snprintf(ptr, *len, " help\n");
		break;
	case CSS_CURSOR_PROGRESS:
		wrote = snprintf(ptr, *len, " progress\n");
		break;
	default:
		wrote = 0;
		break;
	}
	ptr += wrote;
	*len -= wrote;

	/* direction */
	val = css_computed_direction(style);
	switch (val) {
	case CSS_DIRECTION_INHERIT:
		wrote = snprintf(ptr, *len, "direction: inherit\n");
		break;
	case CSS_DIRECTION_LTR:
		wrote = snprintf(ptr, *len, "direction: ltr\n");
		break;
	case CSS_DIRECTION_RTL:
		wrote = snprintf(ptr, *len, "direction: rtl\n");
		break;
	default:
		wrote = 0;
		break;
	}
	ptr += wrote;
	*len -= wrote;

	/* display */
	val = css_computed_display_static(style);
	switch (val) {
	case CSS_DISPLAY_INHERIT:
		wrote = snprintf(ptr, *len, "display: inherit\n");
		break;
	case CSS_DISPLAY_INLINE:
		wrote = snprintf(ptr, *len, "display: inline\n");
		break;
	case CSS_DISPLAY_BLOCK:
		wrote = snprintf(ptr, *len, "display: block\n");
		break;
	case CSS_DISPLAY_LIST_ITEM:
		wrote = snprintf(ptr, *len, "display: list-item\n");
		break;
	case CSS_DISPLAY_RUN_IN:
		wrote = snprintf(ptr, *len, "display: run-in\n");
		break;
	case CSS_DISPLAY_INLINE_BLOCK:
		wrote = snprintf(ptr, *len, "display: inline-block\n");
		break;
	case CSS_DISPLAY_TABLE:
		wrote = snprintf(ptr, *len, "display: table\n");
		break;
	case CSS_DISPLAY_INLINE_TABLE:
		wrote = snprintf(ptr, *len, "display: inline-table\n");
		break;
	case CSS_DISPLAY_TABLE_ROW_GROUP:
		wrote = snprintf(ptr, *len, "display: table-row-group\n");
		break;
	case CSS_DISPLAY_TABLE_HEADER_GROUP:
		wrote = snprintf(ptr, *len, "display: table-header-group\n");
		break;
	case CSS_DISPLAY_TABLE_FOOTER_GROUP:
		wrote = snprintf(ptr, *len, "display: table-footer-group\n");
		break;
	case CSS_DISPLAY_TABLE_ROW:
		wrote = snprintf(ptr, *len, "display: table-row\n");
		break;
	case CSS_DISPLAY_TABLE_COLUMN_GROUP:
		wrote = snprintf(ptr, *len, "display: table-column-group\n");
		break;
	case CSS_DISPLAY_TABLE_COLUMN:
		wrote = snprintf(ptr, *len, "display: table-column\n");
		break;
	case CSS_DISPLAY_TABLE_CELL:
		wrote = snprintf(ptr, *len, "display: table-cell\n");
		break;
	case CSS_DISPLAY_TABLE_CAPTION:
		wrote = snprintf(ptr, *len, "display: table-caption\n");
		break;
	case CSS_DISPLAY_NONE:
		wrote = snprintf(ptr, *len, "display: none\n");
		break;
	default:
		wrote = 0;
		break;
	}
	ptr += wrote;
	*len -= wrote;

	/* empty-cells */
	val = css_computed_empty_cells(style);
	switch (val) {
	case CSS_EMPTY_CELLS_INHERIT:
		wrote = snprintf(ptr, *len, "empty-cells: inherit\n");
		break;
	case CSS_EMPTY_CELLS_SHOW:
		wrote = snprintf(ptr, *len, "empty-cells: show\n");
		break;
	case CSS_EMPTY_CELLS_HIDE:
		wrote = snprintf(ptr, *len, "empty-cells: hide\n");
		break;
	default:
		wrote = 0;
		break;
	}
	ptr += wrote;
	*len -= wrote;

	/* float */
	val = css_computed_float(style);
	switch (val) {
	case CSS_FLOAT_INHERIT:
		wrote = snprintf(ptr, *len, "float: inherit\n");
		break;
	case CSS_FLOAT_LEFT:
		wrote = snprintf(ptr, *len, "float: left\n");
		break;
	case CSS_FLOAT_RIGHT:
		wrote = snprintf(ptr, *len, "float: right\n");
		break;
	case CSS_FLOAT_NONE:
		wrote = snprintf(ptr, *len, "float: none\n");
		break;
	default:
		wrote = 0;
		break;
	}
	ptr += wrote;
	*len -= wrote;

	/* font-family */
	val = css_computed_font_family(style, &string_list);
	if (val == CSS_FONT_FAMILY_INHERIT) {
                wrote = snprintf(ptr, *len, "font-family: inherit\n");
                ptr += wrote;
                *len -= wrote;
        } else {
		wrote = snprintf(ptr, *len, "font-family:");
		ptr += wrote;
		*len -= wrote;

		if (string_list != NULL) {
			while (*string_list != NULL) {
				wrote = snprintf(ptr, *len, " \"%.*s\"",
					(int) lwc_string_length(*string_list),
					lwc_string_data(*string_list));
				ptr += wrote;
				*len -= wrote;

				string_list++;
			}
		}
		switch (val) {
		case CSS_FONT_FAMILY_SERIF:
			wrote = snprintf(ptr, *len, " serif\n");
			break;
		case CSS_FONT_FAMILY_SANS_SERIF:
			wrote = snprintf(ptr, *len, " sans-serif\n");
			break;
		case CSS_FONT_FAMILY_CURSIVE:
			wrote = snprintf(ptr, *len, " cursive\n");
			break;
		case CSS_FONT_FAMILY_FANTASY:
			wrote = snprintf(ptr, *len, " fantasy\n");
			break;
		case CSS_FONT_FAMILY_MONOSPACE:
			wrote = snprintf(ptr, *len, " monospace\n");
			break;
		}
		ptr += wrote;
		*len -= wrote;
	}

	/* font-size */
	val = css_computed_font_size(style, &len1, &unit1);
	switch (val) {
	case CSS_FONT_SIZE_INHERIT:
		wrote = snprintf(ptr, *len, "font-size: inherit\n");
		break;
	case CSS_FONT_SIZE_XX_SMALL:
		wrote = snprintf(ptr, *len, "font-size: xx-small\n");
		break;
	case CSS_FONT_SIZE_X_SMALL:
		wrote = snprintf(ptr, *len, "font-size: x-small\n");
		break;
	case CSS_FONT_SIZE_SMALL:
		wrote = snprintf(ptr, *len, "font-size: small\n");
		break;
	case CSS_FONT_SIZE_MEDIUM:
		wrote = snprintf(ptr, *len, "font-size: medium\n");
		break;
	case CSS_FONT_SIZE_LARGE:
		wrote = snprintf(ptr, *len, "font-size: large\n");
		break;
	case CSS_FONT_SIZE_X_LARGE:
		wrote = snprintf(ptr, *len, "font-size: x-large\n");
		break;
	case CSS_FONT_SIZE_XX_LARGE:
		wrote = snprintf(ptr, *len, "font-size: xx-large\n");
		break;
	case CSS_FONT_SIZE_LARGER:
		wrote = snprintf(ptr, *len, "font-size: larger\n");
		break;
	case CSS_FONT_SIZE_SMALLER:
		wrote = snprintf(ptr, *len, "font-size: smaller\n");
		break;
	case CSS_FONT_SIZE_DIMENSION:
		wrote = snprintf(ptr, *len, "font-size: ");
		ptr += wrote;
		*len -= wrote;

		wrote = dump_css_unit(len1, unit1, ptr, *len);
		ptr += wrote;
		*len -= wrote;

		wrote = snprintf(ptr, *len, "\n");
		break;
	default:
		wrote = 0;
		break;
	}
	ptr += wrote;
	*len -= wrote;

	/* font-style */
	val = css_computed_font_style(style);
	switch (val) {
	case CSS_FONT_STYLE_INHERIT:
		wrote = snprintf(ptr, *len, "font-style: inherit\n");
		break;
	case CSS_FONT_STYLE_NORMAL:
		wrote = snprintf(ptr, *len, "font-style: normal\n");
		break;
	case CSS_FONT_STYLE_ITALIC:
		wrote = snprintf(ptr, *len, "font-style: italic\n");
		break;
	case CSS_FONT_STYLE_OBLIQUE:
		wrote = snprintf(ptr, *len, "font-style: oblique\n");
		break;
	default:
		wrote = 0;
		break;
	}
	ptr += wrote;
	*len -= wrote;

	/* font-variant */
	val = css_computed_font_variant(style);
	switch (val) {
	case CSS_FONT_VARIANT_INHERIT:
		wrote = snprintf(ptr, *len, "font-variant: inherit\n");
		break;
	case CSS_FONT_VARIANT_NORMAL:
		wrote = snprintf(ptr, *len, "font-variant: normal\n");
		break;
	case CSS_FONT_VARIANT_SMALL_CAPS:
		wrote = snprintf(ptr, *len, "font-variant: small-caps\n");
		break;
	default:
		wrote = 0;
		break;
	}
	ptr += wrote;
	*len -= wrote;

	/* font-weight */
	val = css_computed_font_weight(style);
	switch (val) {
	case CSS_FONT_WEIGHT_INHERIT:
		wrote = snprintf(ptr, *len, "font-weight: inherit\n");
		break;
	case CSS_FONT_WEIGHT_NORMAL:
		wrote = snprintf(ptr, *len, "font-weight: normal\n");
		break;
	case CSS_FONT_WEIGHT_BOLD:
		wrote = snprintf(ptr, *len, "font-weight: bold\n");
		break;
	case CSS_FONT_WEIGHT_BOLDER:
		wrote = snprintf(ptr, *len, "font-weight: bolder\n");
		break;
	case CSS_FONT_WEIGHT_LIGHTER:
		wrote = snprintf(ptr, *len, "font-weight: lighter\n");
		break;
	case CSS_FONT_WEIGHT_100:
		wrote = snprintf(ptr, *len, "font-weight: 100\n");
		break;
	case CSS_FONT_WEIGHT_200:
		wrote = snprintf(ptr, *len, "font-weight: 200\n");
		break;
	case CSS_FONT_WEIGHT_300:
		wrote = snprintf(ptr, *len, "font-weight: 300\n");
		break;
	case CSS_FONT_WEIGHT_400:
		wrote = snprintf(ptr, *len, "font-weight: 400\n");
		break;
	case CSS_FONT_WEIGHT_500:
		wrote = snprintf(ptr, *len, "font-weight: 500\n");
		break;
	case CSS_FONT_WEIGHT_600:
		wrote = snprintf(ptr, *len, "font-weight: 600\n");
		break;
	case CSS_FONT_WEIGHT_700:
		wrote = snprintf(ptr, *len, "font-weight: 700\n");
		break;
	case CSS_FONT_WEIGHT_800:
		wrote = snprintf(ptr, *len, "font-weight: 800\n");
		break;
	case CSS_FONT_WEIGHT_900:
		wrote = snprintf(ptr, *len, "font-weight: 900\n");
		break;
	default:
		wrote = 0;
		break;
	}
	ptr += wrote;
	*len -= wrote;

	/* height */
	val = css_computed_height(style, &len1, &unit1);
	switch (val) {
	case CSS_HEIGHT_INHERIT:
		wrote = snprintf(ptr, *len, "height: inherit\n");
		break;
	case CSS_HEIGHT_AUTO:
		wrote = snprintf(ptr, *len, "height: auto\n");
		break;
	case CSS_HEIGHT_SET:
		wrote = snprintf(ptr, *len, "height: ");
		ptr += wrote;
		*len -= wrote;

		wrote = dump_css_unit(len1, unit1, ptr, *len);
		ptr += wrote;
		*len -= wrote;

		wrote = snprintf(ptr, *len, "\n");
		break;
	default:
		wrote = 0;
		break;
	}
	ptr += wrote;
	*len -= wrote;

	/* left */
	val = css_computed_left(style, &len1, &unit1);
	switch (val) {
	case CSS_LEFT_INHERIT:
		wrote = snprintf(ptr, *len, "left: inherit\n");
		break;
	case CSS_LEFT_AUTO:
		wrote = snprintf(ptr, *len, "left: auto\n");
		break;
	case CSS_LEFT_SET:
		wrote = snprintf(ptr, *len, "left: ");
		ptr += wrote;
		*len -= wrote;

		wrote = dump_css_unit(len1, unit1, ptr, *len);
		ptr += wrote;
		*len -= wrote;

		wrote = snprintf(ptr, *len, "\n");
		break;
	default:
		wrote = 0;
		break;
	}
	ptr += wrote;
	*len -= wrote;

	/* letter-spacing */
	val = css_computed_letter_spacing(style, &len1, &unit1);
	switch (val) {
	case CSS_LETTER_SPACING_INHERIT:
		wrote = snprintf(ptr, *len, "letter-spacing: inherit\n");
		break;
	case CSS_LETTER_SPACING_NORMAL:
		wrote = snprintf(ptr, *len, "letter-spacing: normal\n");
		break;
	case CSS_LETTER_SPACING_SET:
		wrote = snprintf(ptr, *len, "letter-spacing: ");
		ptr += wrote;
		*len -= wrote;

		wrote = dump_css_unit(len1, unit1, ptr, *len);
		ptr += wrote;
		*len -= wrote;

		wrote = snprintf(ptr, *len, "\n");
		break;
	default:
		wrote = 0;
		break;
	}
	ptr += wrote;
	*len -= wrote;

	/* line-height */
	val = css_computed_line_height(style, &len1, &unit1);
	switch (val) {
	case CSS_LINE_HEIGHT_INHERIT:
		wrote = snprintf(ptr, *len, "line-height: inherit\n");
		break;
	case CSS_LINE_HEIGHT_NORMAL:
		wrote = snprintf(ptr, *len, "line-height: normal\n");
		break;
	case CSS_LINE_HEIGHT_NUMBER:
		wrote = snprintf(ptr, *len, "line-height: ");
		ptr += wrote;
		*len -= wrote;

		wrote = dump_css_fixed(len1, ptr, *len);
		ptr += wrote;
		*len -= wrote;

		wrote = snprintf(ptr, *len, "\n");
		break;
	case CSS_LINE_HEIGHT_DIMENSION:
		wrote = snprintf(ptr, *len, "line-height: ");
		ptr += wrote;
		*len -= wrote;

		wrote = dump_css_unit(len1, unit1, ptr, *len);
		ptr += wrote;
		*len -= wrote;

		wrote = snprintf(ptr, *len, "\n");
		break;
	default:
		wrote = 0;
		break;
	}
	ptr += wrote;
	*len -= wrote;

	/* list-style-image */
	val = css_computed_list_style_image(style, &url);
        if (val == CSS_LIST_STYLE_IMAGE_INHERIT) {
                wrote = snprintf(ptr, *len, "list-style-image: inherit\n");
	} else if (url != NULL) {
		wrote = snprintf(ptr, *len, "list-style-image: url('%.*s')\n",
				(int) lwc_string_length(url), 
				lwc_string_data(url));
	} else if (val == CSS_LIST_STYLE_IMAGE_NONE) {
		wrote = snprintf(ptr, *len, "list-style-image: none\n");
	} else {
		wrote = 0;
	}
	ptr += wrote;
	*len -= wrote;

	/* list-style-position */
	val = css_computed_list_style_position(style);
	switch (val) {
	case CSS_LIST_STYLE_POSITION_INHERIT:
		wrote = snprintf(ptr, *len, "list-style-position: inherit\n");
		break;
	case CSS_LIST_STYLE_POSITION_INSIDE:
		wrote = snprintf(ptr, *len, "list-style-position: inside\n");
		break;
	case CSS_LIST_STYLE_POSITION_OUTSIDE:
		wrote = snprintf(ptr, *len, "list-style-position: outside\n");
		break;
	default:
		wrote = 0;
		break;
	}
	ptr += wrote;
	*len -= wrote;

	/* list-style-type */
	val = css_computed_list_style_type(style);
	switch (val) {
	case CSS_LIST_STYLE_TYPE_INHERIT:
		wrote = snprintf(ptr, *len, "list-style-type: inherit\n");
		break;
	case CSS_LIST_STYLE_TYPE_DISC:
		wrote = snprintf(ptr, *len, "list-style-type: disc\n");
		break;
	case CSS_LIST_STYLE_TYPE_CIRCLE:
		wrote = snprintf(ptr, *len, "list-style-type: circle\n");
		break;
	case CSS_LIST_STYLE_TYPE_SQUARE:
		wrote = snprintf(ptr, *len, "list-style-type: square\n");
		break;
	case CSS_LIST_STYLE_TYPE_DECIMAL:
		wrote = snprintf(ptr, *len, "list-style-type: decimal\n");
		break;
	case CSS_LIST_STYLE_TYPE_DECIMAL_LEADING_ZERO:
		wrote = snprintf(ptr, *len, 
				"list-style-type: decimal-leading-zero\n");
		break;
	case CSS_LIST_STYLE_TYPE_LOWER_ROMAN:
		wrote = snprintf(ptr, *len, "list-style-type: lower-roman\n");
		break;
	case CSS_LIST_STYLE_TYPE_UPPER_ROMAN:
		wrote = snprintf(ptr, *len, "list-style-type: upper-roman\n");
		break;
	case CSS_LIST_STYLE_TYPE_LOWER_GREEK:
		wrote = snprintf(ptr, *len, "list-style-type: lower-greek\n");
		break;
	case CSS_LIST_STYLE_TYPE_LOWER_LATIN:
		wrote = snprintf(ptr, *len, "list-style-type: lower-latin\n");
		break;
	case CSS_LIST_STYLE_TYPE_UPPER_LATIN:
		wrote = snprintf(ptr, *len, "list-style-type: upper-latin\n");
		break;
	case CSS_LIST_STYLE_TYPE_ARMENIAN:
		wrote = snprintf(ptr, *len, "list-style-type: armenian\n");
		break;
	case CSS_LIST_STYLE_TYPE_GEORGIAN:
		wrote = snprintf(ptr, *len, "list-style-type: georgian\n");
		break;
	case CSS_LIST_STYLE_TYPE_LOWER_ALPHA:
		wrote = snprintf(ptr, *len, "list-style-type: lower-alpha\n");
		break;
	case CSS_LIST_STYLE_TYPE_UPPER_ALPHA:
		wrote = snprintf(ptr, *len, "list-style-type: upper-alpha\n");
		break;
	case CSS_LIST_STYLE_TYPE_NONE:
		wrote = snprintf(ptr, *len, "list-style-type: none\n");
		break;
	default:
		wrote = 0;
		break;
	}
	ptr += wrote;
	*len -= wrote;

	/* margin-top */
	val = css_computed_margin_top(style, &len1, &unit1);
	switch (val) {
	case CSS_MARGIN_INHERIT:
		wrote = snprintf(ptr, *len, "margin-top: inherit\n");
		break;
	case CSS_MARGIN_AUTO:
		wrote = snprintf(ptr, *len, "margin-top: auto\n");
		break;
	case CSS_MARGIN_SET:
		wrote = snprintf(ptr, *len, "margin-top: ");
		ptr += wrote;
		*len -= wrote;

		wrote = dump_css_unit(len1, unit1, ptr, *len);
		ptr += wrote;
		*len -= wrote;

		wrote = snprintf(ptr, *len, "\n");
		break;
	default:
		wrote = 0;
		break;
	}
	ptr += wrote;
	*len -= wrote;

	/* margin-right */
	val = css_computed_margin_right(style, &len1, &unit1);
	switch (val) {
	case CSS_MARGIN_INHERIT:
		wrote = snprintf(ptr, *len, "margin-right: inherit\n");
		break;
	case CSS_MARGIN_AUTO:
		wrote = snprintf(ptr, *len, "margin-right: auto\n");
		break;
	case CSS_MARGIN_SET:
		wrote = snprintf(ptr, *len, "margin-right: ");
		ptr += wrote;
		*len -= wrote;

		wrote = dump_css_unit(len1, unit1, ptr, *len);
		ptr += wrote;
		*len -= wrote;

		wrote = snprintf(ptr, *len, "\n");
		break;
	default:
		wrote = 0;
		break;
	}
	ptr += wrote;
	*len -= wrote;

	/* margin-bottom */
	val = css_computed_margin_bottom(style, &len1, &unit1);
	switch (val) {
	case CSS_MARGIN_INHERIT:
		wrote = snprintf(ptr, *len, "margin-bottom: inherit\n");
		break;
	case CSS_MARGIN_AUTO:
		wrote = snprintf(ptr, *len, "margin-bottom: auto\n");
		break;
	case CSS_MARGIN_SET:
		wrote = snprintf(ptr, *len, "margin-bottom: ");
		ptr += wrote;
		*len -= wrote;

		wrote = dump_css_unit(len1, unit1, ptr, *len);
		ptr += wrote;
		*len -= wrote;

		wrote = snprintf(ptr, *len, "\n");
		break;
	default:
		wrote = 0;
		break;
	}
	ptr += wrote;
	*len -= wrote;

	/* margin-left */
	val = css_computed_margin_left(style, &len1, &unit1);
	switch (val) {
	case CSS_MARGIN_INHERIT:
		wrote = snprintf(ptr, *len, "margin-left: inherit\n");
		break;
	case CSS_MARGIN_AUTO:
		wrote = snprintf(ptr, *len, "margin-left: auto\n");
		break;
	case CSS_MARGIN_SET:
		wrote = snprintf(ptr, *len, "margin-left: ");
		ptr += wrote;
		*len -= wrote;

		wrote = dump_css_unit(len1, unit1, ptr, *len);
		ptr += wrote;
		*len -= wrote;

		wrote = snprintf(ptr, *len, "\n");
		break;
	default:
		wrote = 0;
		break;
	}
	ptr += wrote;
	*len -= wrote;

	/* max-height */
	val = css_computed_max_height(style, &len1, &unit1);
	switch (val) {
	case CSS_MAX_HEIGHT_INHERIT:
		wrote = snprintf(ptr, *len, "max-height: inherit\n");
		break;
	case CSS_MAX_HEIGHT_NONE:
		wrote = snprintf(ptr, *len, "max-height: none\n");
		break;
	case CSS_MAX_HEIGHT_SET:
		wrote = snprintf(ptr, *len, "max-height: ");
		ptr += wrote;
		*len -= wrote;

		wrote = dump_css_unit(len1, unit1, ptr, *len);
		ptr += wrote;
		*len -= wrote;

		wrote = snprintf(ptr, *len, "\n");
		break;
	default:
		wrote = 0;
		break;
	}
	ptr += wrote;
	*len -= wrote;

	/* max-width */
	val = css_computed_max_width(style, &len1, &unit1);
	switch (val) {
	case CSS_MAX_WIDTH_INHERIT:
		wrote = snprintf(ptr, *len, "max-width: inherit\n");
		break;
	case CSS_MAX_WIDTH_NONE:
		wrote = snprintf(ptr, *len, "max-width: none\n");
		break;
	case CSS_MAX_WIDTH_SET:
		wrote = snprintf(ptr, *len, "max-width: ");
		ptr += wrote;
		*len -= wrote;

		wrote = dump_css_unit(len1, unit1, ptr, *len);
		ptr += wrote;
		*len -= wrote;

		wrote = snprintf(ptr, *len, "\n");
		break;
	default:
		wrote = 0;
		break;
	}
	ptr += wrote;
	*len -= wrote;

	/* min-height */
	val = css_computed_min_height(style, &len1, &unit1);
	switch (val) {
	case CSS_MIN_HEIGHT_INHERIT:
		wrote = snprintf(ptr, *len, "min-height: inherit\n");
                break;
	case CSS_MIN_HEIGHT_SET:
		wrote = snprintf(ptr, *len, "min-height: ");
		ptr += wrote;
		*len -= wrote;

		wrote = dump_css_unit(len1, unit1, ptr, *len);
		ptr += wrote;
		*len -= wrote;

		wrote = snprintf(ptr, *len, "\n");
		break;
	default:
		wrote = 0;
		break;
	}
	ptr += wrote;
	*len -= wrote;

	/* min-width */
	val = css_computed_min_width(style, &len1, &unit1);
	switch (val) {
        case CSS_MIN_WIDTH_INHERIT:
                wrote = snprintf(ptr, *len, "min-width: inherit\n");
                break;
	case CSS_MIN_WIDTH_SET:
		wrote = snprintf(ptr, *len, "min-width: ");
		ptr += wrote;
		*len -= wrote;

		wrote = dump_css_unit(len1, unit1, ptr, *len);
		ptr += wrote;
		*len -= wrote;

		wrote = snprintf(ptr, *len, "\n");
		break;
	default:
		wrote = 0;
		break;
	}
	ptr += wrote;
	*len -= wrote;

	/* opacity */
	val = css_computed_opacity(style, &len1);
	switch (val) {
        case CSS_OPACITY_INHERIT:
                wrote = snprintf(ptr, *len, "opacity: inherit\n");
                break;
	case CSS_OPACITY_SET:
		wrote = snprintf(ptr, *len, "opacity: ");
		ptr += wrote;
		*len -= wrote;

		wrote = dump_css_fixed(len1, ptr, *len);
		ptr += wrote;
		*len -= wrote;

		wrote = snprintf(ptr, *len, "\n");
		break;
	default:
		wrote = 0;
		break;
	}
	ptr += wrote;
	*len -= wrote;

	/* outline-color */
	val = css_computed_outline_color(style, &color);
	switch (val) {
	case CSS_OUTLINE_COLOR_INHERIT:
		wrote = snprintf(ptr, *len, "outline-color: inherit\n");
		break;
	case CSS_OUTLINE_COLOR_INVERT:
		wrote = snprintf(ptr, *len, "outline-color: invert\n");
		break;
	case CSS_OUTLINE_COLOR_COLOR:
		wrote = snprintf(ptr, *len, "outline-color: #%08x\n", color);
		break;
	default:
		wrote = 0;
		break;
	}
	ptr += wrote;
	*len -= wrote;

	/* outline-style */
	val = css_computed_outline_style(style);
	switch (val) {
	case CSS_OUTLINE_STYLE_INHERIT:
		wrote = snprintf(ptr, *len, "outline-style: inherit\n");
		break;
	case CSS_OUTLINE_STYLE_NONE:
		wrote = snprintf(ptr, *len, "outline-style: none\n");
		break;
	case CSS_OUTLINE_STYLE_DOTTED:
		wrote = snprintf(ptr, *len, "outline-style: dotted\n");
		break;
	case CSS_OUTLINE_STYLE_DASHED:
		wrote = snprintf(ptr, *len, "outline-style: dashed\n");
		break;
	case CSS_OUTLINE_STYLE_SOLID:
		wrote = snprintf(ptr, *len, "outline-style: solid\n");
		break;
	case CSS_OUTLINE_STYLE_DOUBLE:
		wrote = snprintf(ptr, *len, "outline-style: double\n");
		break;
	case CSS_OUTLINE_STYLE_GROOVE:
		wrote = snprintf(ptr, *len, "outline-style: groove\n");
		break;
	case CSS_OUTLINE_STYLE_RIDGE:
		wrote = snprintf(ptr, *len, "outline-style: ridge\n");
		break;
	case CSS_OUTLINE_STYLE_INSET:
		wrote = snprintf(ptr, *len, "outline-style: inset\n");
		break;
	case CSS_OUTLINE_STYLE_OUTSET:
		wrote = snprintf(ptr, *len, "outline-style: outset\n");
		break;
	default:
		wrote = 0;
		break;
	}
	ptr += wrote;
	*len -= wrote;

	/* outline-width */
	val = css_computed_outline_width(style, &len1, &unit1);
	switch (val) {
	case CSS_OUTLINE_WIDTH_INHERIT:
		wrote = snprintf(ptr, *len, "outline-width: inherit\n");
		break;
	case CSS_OUTLINE_WIDTH_THIN:
		wrote = snprintf(ptr, *len, "outline-width: thin\n");
		break;
	case CSS_OUTLINE_WIDTH_MEDIUM:
		wrote = snprintf(ptr, *len, "outline-width: medium\n");
		break;
	case CSS_OUTLINE_WIDTH_THICK:
		wrote = snprintf(ptr, *len, "outline-width: thick\n");
		break;
	case CSS_OUTLINE_WIDTH_WIDTH:
		wrote = snprintf(ptr, *len, "outline-width: ");
		ptr += wrote;
		*len -= wrote;

		wrote = dump_css_unit(len1, unit1, ptr, *len);
		ptr += wrote;
		*len -= wrote;

		wrote = snprintf(ptr, *len, "\n");
		break;
	default:
		wrote = 0;
		break;
	}
	ptr += wrote;
	*len -= wrote;

	/* overflow */
	val = css_computed_overflow(style);
	switch (val) {
	case CSS_OVERFLOW_INHERIT:
		wrote = snprintf(ptr, *len, "overflow: inherit\n");
		break;
	case CSS_OVERFLOW_VISIBLE:
		wrote = snprintf(ptr, *len, "overflow: visible\n");
		break;
	case CSS_OVERFLOW_HIDDEN:
		wrote = snprintf(ptr, *len, "overflow: hidden\n");
		break;
	case CSS_OVERFLOW_SCROLL:
		wrote = snprintf(ptr, *len, "overflow: scroll\n");
		break;
	case CSS_OVERFLOW_AUTO:
		wrote = snprintf(ptr, *len, "overflow: auto\n");
		break;
	default:
		wrote = 0;
		break;
	}
	ptr += wrote;
	*len -= wrote;

	/* padding-top */
	val = css_computed_padding_top(style, &len1, &unit1);
	switch (val) {
        case CSS_PADDING_INHERIT:
                wrote = snprintf(ptr, *len, "padding-top: inherit\n");
                break;
	case CSS_PADDING_SET:
		wrote = snprintf(ptr, *len, "padding-top: ");
		ptr += wrote;
		*len -= wrote;

		wrote = dump_css_unit(len1, unit1, ptr, *len);
		ptr += wrote;
		*len -= wrote;

		wrote = snprintf(ptr, *len, "\n");
		break;
	default:
		wrote = 0;
		break;
	}
	ptr += wrote;
	*len -= wrote;

	/* padding-right */
	val = css_computed_padding_right(style, &len1, &unit1);
	switch (val) {
        case CSS_PADDING_INHERIT:
                wrote = snprintf(ptr, *len, "padding-right: inherit\n");
                break;
	case CSS_PADDING_SET:
		wrote = snprintf(ptr, *len, "padding-right: ");
		ptr += wrote;
		*len -= wrote;

		wrote = dump_css_unit(len1, unit1, ptr, *len);
		ptr += wrote;
		*len -= wrote;

		wrote = snprintf(ptr, *len, "\n");
		break;
	default:
		wrote = 0;
		break;
	}
	ptr += wrote;
	*len -= wrote;

	/* padding-bottom */
	val = css_computed_padding_bottom(style, &len1, &unit1);
	switch (val) {
        case CSS_PADDING_INHERIT:
                wrote = snprintf(ptr, *len, "padding-bottom: inherit\n");
                break;
	case CSS_PADDING_SET:
		wrote = snprintf(ptr, *len, "padding-bottom: ");
		ptr += wrote;
		*len -= wrote;

		wrote = dump_css_unit(len1, unit1, ptr, *len);
		ptr += wrote;
		*len -= wrote;

		wrote = snprintf(ptr, *len, "\n");
		break;
	default:
		wrote = 0;
		break;
	}
	ptr += wrote;
	*len -= wrote;

	/* padding-left */
	val = css_computed_padding_left(style, &len1, &unit1);
	switch (val) {
        case CSS_PADDING_INHERIT:
                wrote = snprintf(ptr, *len, "padding-left: inherit\n");
                break;
	case CSS_PADDING_SET:
		wrote = snprintf(ptr, *len, "padding-left: ");
		ptr += wrote;
		*len -= wrote;

		wrote = dump_css_unit(len1, unit1, ptr, *len);
		ptr += wrote;
		*len -= wrote;

		wrote = snprintf(ptr, *len, "\n");
		break;
	default:
		wrote = 0;
		break;
	}
	ptr += wrote;
	*len -= wrote;

	/* position */
	val = css_computed_position(style);
	switch (val) {
	case CSS_POSITION_INHERIT:
		wrote = snprintf(ptr, *len, "position: inherit\n");
		break;
	case CSS_POSITION_STATIC:
		wrote = snprintf(ptr, *len, "position: static\n");
		break;
	case CSS_POSITION_RELATIVE:
		wrote = snprintf(ptr, *len, "position: relative\n");
		break;
	case CSS_POSITION_ABSOLUTE:
		wrote = snprintf(ptr, *len, "position: absolute\n");
		break;
	case CSS_POSITION_FIXED:
		wrote = snprintf(ptr, *len, "position: fixed\n");
		break;
	default:
		wrote = 0;
		break;
	}
	ptr += wrote;
	*len -= wrote;

	/* quotes */
	val = css_computed_quotes(style, &string_list);
	if (val == CSS_QUOTES_STRING && string_list != NULL) {
		wrote = snprintf(ptr, *len, "quotes:");
		ptr += wrote;
		*len -= wrote;

		while (*string_list != NULL) {
			wrote = snprintf(ptr, *len, " \"%.*s\"",
				(int) lwc_string_length(*string_list),
				lwc_string_data(*string_list));
			ptr += wrote;
			*len -= wrote;

			string_list++;
		}

		wrote = snprintf(ptr, *len, "\n");
	} else {
		switch (val) {
		case CSS_QUOTES_INHERIT:
			wrote = snprintf(ptr, *len, "quotes: inherit\n");
			break;
		case CSS_QUOTES_NONE:
			wrote = snprintf(ptr, *len, "quotes: none\n");
			break;
		default:
			wrote = 0;
			break;
		}
	}
	ptr += wrote;
	*len -= wrote;

	/* right */
	val = css_computed_right(style, &len1, &unit1);
	switch (val) {
	case CSS_RIGHT_INHERIT:
		wrote = snprintf(ptr, *len, "right: inherit\n");
		break;
	case CSS_RIGHT_AUTO:
		wrote = snprintf(ptr, *len, "right: auto\n");
		break;
	case CSS_RIGHT_SET:
		wrote = snprintf(ptr, *len, "right: ");
		ptr += wrote;
		*len -= wrote;

		wrote = dump_css_unit(len1, unit1, ptr, *len);
		ptr += wrote;
		*len -= wrote;

		wrote = snprintf(ptr, *len, "\n");
		break;
	default:
		wrote = 0;
		break;
	}
	ptr += wrote;
	*len -= wrote;

	/* table-layout */
	val = css_computed_table_layout(style);
	switch (val) {
	case CSS_TABLE_LAYOUT_INHERIT:
		wrote = snprintf(ptr, *len, "table-layout: inherit\n");
		break;
	case CSS_TABLE_LAYOUT_AUTO:
		wrote = snprintf(ptr, *len, "table-layout: auto\n");
		break;
	case CSS_TABLE_LAYOUT_FIXED:
		wrote = snprintf(ptr, *len, "table-layout: fixed\n");
		break;
	default:
		wrote = 0;
		break;
	}
	ptr += wrote;
	*len -= wrote;

	/* text-align */
	val = css_computed_text_align(style);
	switch (val) {
	case CSS_TEXT_ALIGN_INHERIT:
		wrote = snprintf(ptr, *len, "text-align: inherit\n");
		break;
	case CSS_TEXT_ALIGN_LEFT:
		wrote = snprintf(ptr, *len, "text-align: left\n");
		break;
	case CSS_TEXT_ALIGN_RIGHT:
		wrote = snprintf(ptr, *len, "text-align: right\n");
		break;
	case CSS_TEXT_ALIGN_CENTER:
		wrote = snprintf(ptr, *len, "text-align: center\n");
		break;
	case CSS_TEXT_ALIGN_JUSTIFY:
		wrote = snprintf(ptr, *len, "text-align: justify\n");
		break;
	case CSS_TEXT_ALIGN_DEFAULT:
		wrote = snprintf(ptr, *len, "text-align: default\n");
		break;
	case CSS_TEXT_ALIGN_LIBCSS_LEFT:
		wrote = snprintf(ptr, *len, "text-align: -libcss-left\n");
		break;
	case CSS_TEXT_ALIGN_LIBCSS_CENTER:
		wrote = snprintf(ptr, *len, "text-align: -libcss-center\n");
		break;
	case CSS_TEXT_ALIGN_LIBCSS_RIGHT:
		wrote = snprintf(ptr, *len, "text-align: -libcss-right\n");
		break;
	default:
		wrote = 0;
		break;
	}
	ptr += wrote;
	*len -= wrote;

	/* text-decoration */
	val = css_computed_text_decoration(style);
        if (val == CSS_TEXT_DECORATION_INHERIT) {
                wrote = snprintf(ptr, *len, "text-decoration: inherit\n");
		ptr += wrote;
		*len -= wrote;
	} else if (val == CSS_TEXT_DECORATION_NONE) {
		wrote = snprintf(ptr, *len, "text-decoration: none\n");
		ptr += wrote;
		*len -= wrote;
	} else {
		wrote = snprintf(ptr, *len, "text-decoration:");
		ptr += wrote;
		*len -= wrote;

		if (val & CSS_TEXT_DECORATION_BLINK) {
			wrote = snprintf(ptr, *len, " blink");
			ptr += wrote;
			*len -= wrote;
		}
		if (val & CSS_TEXT_DECORATION_LINE_THROUGH) {
			wrote = snprintf(ptr, *len, " line-through");
			ptr += wrote;
			*len -= wrote;
		}
		if (val & CSS_TEXT_DECORATION_OVERLINE) {
			wrote = snprintf(ptr, *len, " overline");
			ptr += wrote;
			*len -= wrote;
		}
		if (val & CSS_TEXT_DECORATION_UNDERLINE) {
			wrote = snprintf(ptr, *len, " underline");
			ptr += wrote;
			*len -= wrote;
		}

		wrote = snprintf(ptr, *len, "\n");
		ptr += wrote;
		*len -= wrote;
	}

	/* text-indent */
	val = css_computed_text_indent(style, &len1, &unit1);
	switch (val) {
        case CSS_TEXT_INDENT_INHERIT:
                wrote = snprintf(ptr, *len, "text-indent: inherit\n");
                break;
	case CSS_TEXT_INDENT_SET:
		wrote = snprintf(ptr, *len, "text-indent: ");
		ptr += wrote;
		*len -= wrote;

		wrote = dump_css_unit(len1, unit1, ptr, *len);
		ptr += wrote;
		*len -= wrote;

		wrote = snprintf(ptr, *len, "\n");
		break;
	default:
		wrote = 0;
		break;
	}
	ptr += wrote;
	*len -= wrote;

	/* text-transform */
	val = css_computed_text_transform(style);
	switch (val) {
	case CSS_TEXT_TRANSFORM_INHERIT:
		wrote = snprintf(ptr, *len, "text-transform: inherit\n");
		break;
	case CSS_TEXT_TRANSFORM_CAPITALIZE:
		wrote = snprintf(ptr, *len, "text-transform: capitalize\n");
		break;
	case CSS_TEXT_TRANSFORM_UPPERCASE:
		wrote = snprintf(ptr, *len, "text-transform: uppercase\n");
		break;
	case CSS_TEXT_TRANSFORM_LOWERCASE:
		wrote = snprintf(ptr, *len, "text-transform: lowercase\n");
		break;
	case CSS_TEXT_TRANSFORM_NONE:
		wrote = snprintf(ptr, *len, "text-transform: none\n");
		break;
	default:
		wrote = 0;
		break;
	}
	ptr += wrote;
	*len -= wrote;

	/* top */
	val = css_computed_top(style, &len1, &unit1);
	switch (val) {
	case CSS_TOP_INHERIT:
		wrote = snprintf(ptr, *len, "top: inherit\n");
		break;
	case CSS_TOP_AUTO:
		wrote = snprintf(ptr, *len, "top: auto\n");
		break;
	case CSS_TOP_SET:
		wrote = snprintf(ptr, *len, "top: ");
		ptr += wrote;
		*len -= wrote;

		wrote = dump_css_unit(len1, unit1, ptr, *len);
		ptr += wrote;
		*len -= wrote;

		wrote = snprintf(ptr, *len, "\n");
		break;
	default:
		wrote = 0;
		break;
	}
	ptr += wrote;
	*len -= wrote;

	/* unicode-bidi */
	val = css_computed_unicode_bidi(style);
	switch (val) {
	case CSS_UNICODE_BIDI_INHERIT:
		wrote = snprintf(ptr, *len, "unicode-bidi: inherit\n");
		break;
	case CSS_UNICODE_BIDI_NORMAL:
		wrote = snprintf(ptr, *len, "unicode-bidi: normal\n");
		break;
	case CSS_UNICODE_BIDI_EMBED:
		wrote = snprintf(ptr, *len, "unicode-bidi: embed\n");
		break;
	case CSS_UNICODE_BIDI_BIDI_OVERRIDE:
		wrote = snprintf(ptr, *len, "unicode-bidi: bidi-override\n");
		break;
	default:
		wrote = 0;
		break;
	}
	ptr += wrote;
	*len -= wrote;

	/* vertical-align */
	val = css_computed_vertical_align(style, &len1, &unit1);
	switch (val) {
	case CSS_VERTICAL_ALIGN_INHERIT:
		wrote = snprintf(ptr, *len, "vertical-align: inherit\n");
		break;
	case CSS_VERTICAL_ALIGN_BASELINE:
		wrote = snprintf(ptr, *len, "vertical-align: baseline\n");
		break;
	case CSS_VERTICAL_ALIGN_SUB:
		wrote = snprintf(ptr, *len, "vertical-align: sub\n");
		break;
	case CSS_VERTICAL_ALIGN_SUPER:
		wrote = snprintf(ptr, *len, "vertical-align: super\n");
		break;
	case CSS_VERTICAL_ALIGN_TOP:
		wrote = snprintf(ptr, *len, "vertical-align: top\n");
		break;
	case CSS_VERTICAL_ALIGN_TEXT_TOP:
		wrote = snprintf(ptr, *len, "vertical-align: text-top\n");
		break;
	case CSS_VERTICAL_ALIGN_MIDDLE:
		wrote = snprintf(ptr, *len, "vertical-align: middle\n");
		break;
	case CSS_VERTICAL_ALIGN_BOTTOM:
		wrote = snprintf(ptr, *len, "vertical-align: bottom\n");
		break;
	case CSS_VERTICAL_ALIGN_TEXT_BOTTOM:
		wrote = snprintf(ptr, *len, "vertical-align: text-bottom\n");
		break;
	case CSS_VERTICAL_ALIGN_SET:
		wrote = snprintf(ptr, *len, "vertical-align: ");
		ptr += wrote;
		*len -= wrote;

		wrote = dump_css_unit(len1, unit1, ptr, *len);
		ptr += wrote;
		*len -= wrote;

		wrote = snprintf(ptr, *len, "\n");
		break;
	default:
		wrote = 0;
		break;
	}
	ptr += wrote;
	*len -= wrote;

	/* visibility */
	val = css_computed_visibility(style);
	switch (val) {
	case CSS_VISIBILITY_INHERIT:
		wrote = snprintf(ptr, *len, "visibility: inherit\n");
		break;
	case CSS_VISIBILITY_VISIBLE:
		wrote = snprintf(ptr, *len, "visibility: visible\n");
		break;
	case CSS_VISIBILITY_HIDDEN:
		wrote = snprintf(ptr, *len, "visibility: hidden\n");
		break;
	case CSS_VISIBILITY_COLLAPSE:
		wrote = snprintf(ptr, *len, "visibility: collapse\n");
		break;
	default:
		wrote = 0;
		break;
	}
	ptr += wrote;
	*len -= wrote;

	/* white-space */
	val = css_computed_white_space(style);
	switch (val) {
	case CSS_WHITE_SPACE_INHERIT:
		wrote = snprintf(ptr, *len, "white-space: inherit\n");
		break;
	case CSS_WHITE_SPACE_NORMAL:
		wrote = snprintf(ptr, *len, "white-space: normal\n");
		break;
	case CSS_WHITE_SPACE_PRE:
		wrote = snprintf(ptr, *len, "white-space: pre\n");
		break;
	case CSS_WHITE_SPACE_NOWRAP:
		wrote = snprintf(ptr, *len, "white-space: nowrap\n");
		break;
	case CSS_WHITE_SPACE_PRE_WRAP:
		wrote = snprintf(ptr, *len, "white-space: pre-wrap\n");
		break;
	case CSS_WHITE_SPACE_PRE_LINE:
		wrote = snprintf(ptr, *len, "white-space: pre-line\n");
		break;
	default:
		wrote = 0;
		break;
	}
	ptr += wrote;
	*len -= wrote;

	/* width */
	val = css_computed_width(style, &len1, &unit1);
	switch (val) {
	case CSS_WIDTH_INHERIT:
		wrote = snprintf(ptr, *len, "width: inherit\n");
		break;
	case CSS_WIDTH_AUTO:
		wrote = snprintf(ptr, *len, "width: auto\n");
		break;
	case CSS_WIDTH_SET:
		wrote = snprintf(ptr, *len, "width: ");
		ptr += wrote;
		*len -= wrote;

		wrote = dump_css_unit(len1, unit1, ptr, *len);
		ptr += wrote;
		*len -= wrote;

		wrote = snprintf(ptr, *len, "\n");
		break;
	default:
		wrote = 0;
		break;
	}
	ptr += wrote;
	*len -= wrote;

	/* word-spacing */
	val = css_computed_word_spacing(style, &len1, &unit1);
	switch (val) {
	case CSS_WORD_SPACING_INHERIT:
		wrote = snprintf(ptr, *len, "word-spacing: inherit\n");
		break;
	case CSS_WORD_SPACING_NORMAL:
		wrote = snprintf(ptr, *len, "word-spacing: normal\n");
		break;
	case CSS_WORD_SPACING_SET:
		wrote = snprintf(ptr, *len, "word-spacing: ");
		ptr += wrote;
		*len -= wrote;

		wrote = dump_css_unit(len1, unit1, ptr, *len);
		ptr += wrote;
		*len -= wrote;

		wrote = snprintf(ptr, *len, "\n");
		break;
	default:
		wrote = 0;
		break;
	}
	ptr += wrote;
	*len -= wrote;

	/* writing-mode */
	val = css_computed_writing_mode(style);
	switch (val) {
	case CSS_WRITING_MODE_INHERIT:
		wrote = snprintf(ptr, *len, "writing-mode: inherit\n");
		break;
	case CSS_WRITING_MODE_HORIZONTAL_TB:
		wrote = snprintf(ptr, *len, "writing-mode: horizontal-tb\n");
		break;
	case CSS_WRITING_MODE_VERTICAL_RL:
		wrote = snprintf(ptr, *len, "writing-mode: vertical-rl\n");
		break;
	case CSS_WRITING_MODE_VERTICAL_LR:
		wrote = snprintf(ptr, *len, "writing-mode: vertical-lr\n");
		break;
	default:
		wrote = 0;
		break;
	}
	ptr += wrote;
	*len -= wrote;

	/* z-index */
	val = css_computed_z_index(style, &zindex);
	switch (val) {
	case CSS_Z_INDEX_INHERIT:
		wrote = snprintf(ptr, *len, "z-index: inherit\n");
		break;
	case CSS_Z_INDEX_AUTO:
		wrote = snprintf(ptr, *len, "z-index: auto\n");
		break;
	case CSS_Z_INDEX_SET:
		wrote = snprintf(ptr, *len, "z-index: %d\n", zindex);
		break;
	default:
		wrote = 0;
		break;
	}
	ptr += wrote;
	*len -= wrote;
}

