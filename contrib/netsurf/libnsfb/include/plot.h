

/** Clears plotting area to a flat colour (if needed)
 */
typedef bool (nsfb_plotfn_clg_t)(nsfb_t *nsfb, nsfb_colour_t c);

/** Plots a rectangle outline. The line can be solid, dotted or
 *		  dashed. Top left corner at (x0,y0) and rectangle has given
 *		  width and height.
 */
typedef	bool (nsfb_plotfn_rectangle_t)(nsfb_t *nsfb, nsfb_bbox_t *rect, int line_width, nsfb_colour_t c, bool dotted, bool dashed);

/** Plots a line using a given pen.
 */
typedef bool (nsfb_plotfn_line_t)(nsfb_t *nsfb, int linec, nsfb_bbox_t *line, nsfb_plot_pen_t *pen);

/** Plots a filled polygon with straight lines between points.
 *		  The lines around the edge of the ploygon are not plotted. The
 *		  polygon is filled with the non-zero winding rule.
 */
typedef	bool (nsfb_plotfn_polygon_t)(nsfb_t *nsfb, const int *p, unsigned int n, nsfb_colour_t fill);

/** Plots a filled rectangle. Top left corner at (x0,y0), bottom
 *		  right corner at (x1,y1). Note: (x0,y0) is inside filled area,
 *		  but (x1,y1) is below and to the right. See diagram below.
 */
typedef	bool (nsfb_plotfn_fill_t)(nsfb_t *nsfb, nsfb_bbox_t *rect, nsfb_colour_t c);

/** Clipping operations.
 */
typedef	bool (nsfb_plotfn_clip_t)(nsfb_t *nsfb, nsfb_bbox_t *clip);

/** Plots an arc, around (x,y), from anticlockwise from angle1 to
 *		  angle2. Angles are measured anticlockwise from horizontal, in
 *		  degrees.
 */
typedef	bool (nsfb_plotfn_arc_t)(nsfb_t *nsfb, int x, int y, int radius, int angle1, int angle2, nsfb_colour_t c);

/** Plots a point.
 *
 * Plot a single alpha blended pixel.
 */
typedef	bool (nsfb_plotfn_point_t)(nsfb_t *nsfb, int x, int y, nsfb_colour_t c);

/** Plot an ellipse.
 *
 * plot an ellipse outline, note if teh bounding box is square this will plot a
 * circle.
 */
typedef	bool (nsfb_plotfn_ellipse_t)(nsfb_t *nsfb, nsfb_bbox_t *ellipse, nsfb_colour_t c);

/** Plot a filled ellipse.
 *
 * plot a filled ellipse, note if the bounding box is square this will plot a
 * circle.
 */
typedef	bool (nsfb_plotfn_ellipse_fill_t)(nsfb_t *nsfb, nsfb_bbox_t *ellipse, nsfb_colour_t c);


/** Plot bitmap
 */
typedef bool (nsfb_plotfn_bitmap_t)(nsfb_t *nsfb, const nsfb_bbox_t *loc, const nsfb_colour_t *pixel, int bmp_width, int bmp_height, int bmp_stride, bool alpha);

/** Plot tiled bitmap
 */
typedef bool (nsfb_plotfn_bitmap_tiles_t)(nsfb_t *nsfb, const nsfb_bbox_t *loc, int tiles_x, int tiles_y, const nsfb_colour_t *pixel, int bmp_width, int bmp_height, int bmp_stride, bool alpha);


/** Copy an area of screen 
 *
 * Copy an area of the display.
 */
typedef bool (nsfb_plotfn_copy_t)(nsfb_t *nsfb, nsfb_bbox_t *srcbox, nsfb_bbox_t *dstbox);


/** Plot an 8 bit per pixel glyph.
 */
typedef bool (nsfb_plotfn_glyph8_t)(nsfb_t *nsfb, nsfb_bbox_t *loc, const uint8_t *pixel, int pitch, nsfb_colour_t c);


/** Plot an 1 bit per pixel glyph.
 */
typedef bool (nsfb_plotfn_glyph1_t)(nsfb_t *nsfb, nsfb_bbox_t *loc, const uint8_t *pixel, int pitch, nsfb_colour_t c);

/** Read rectangle of screen into buffer
 */
typedef	bool (nsfb_plotfn_readrect_t)(nsfb_t *nsfb, nsfb_bbox_t *rect, nsfb_colour_t *buffer);

/** Plot quadratic bezier spline
 */
typedef bool (nsfb_plotfn_quadratic_bezier_t)(nsfb_t *nsfb, nsfb_bbox_t *curve, nsfb_point_t *ctrla, nsfb_plot_pen_t *pen);

/** Plot cubic bezier spline
 */
typedef bool (nsfb_plotfn_cubic_bezier_t)(nsfb_t *nsfb, nsfb_bbox_t *curve, nsfb_point_t *ctrla, nsfb_point_t *ctrlb, nsfb_plot_pen_t *pen);

typedef bool (nsfb_plotfn_polylines_t)(nsfb_t *nsfb, int pointc, const nsfb_point_t *points, nsfb_plot_pen_t *pen);

/** plot path */
typedef bool (nsfb_plotfn_path_t)(nsfb_t *nsfb, int pathc, nsfb_plot_pathop_t *pathop, nsfb_plot_pen_t *pen);

/** plotter function table. */
typedef struct nsfb_plotter_fns_s {
    nsfb_plotfn_clg_t *clg;
    nsfb_plotfn_rectangle_t *rectangle;
    nsfb_plotfn_line_t *line;
    nsfb_plotfn_polygon_t *polygon;
    nsfb_plotfn_fill_t *fill;
    nsfb_plotfn_clip_t *get_clip;
    nsfb_plotfn_clip_t *set_clip;
    nsfb_plotfn_ellipse_t *ellipse;
    nsfb_plotfn_ellipse_fill_t *ellipse_fill;
    nsfb_plotfn_arc_t *arc;
    nsfb_plotfn_bitmap_t *bitmap;
    nsfb_plotfn_bitmap_tiles_t *bitmap_tiles;
    nsfb_plotfn_point_t *point;
    nsfb_plotfn_copy_t *copy;
    nsfb_plotfn_glyph8_t *glyph8;
    nsfb_plotfn_glyph1_t *glyph1;
    nsfb_plotfn_readrect_t *readrect;
    nsfb_plotfn_quadratic_bezier_t *quadratic;
    nsfb_plotfn_cubic_bezier_t *cubic;
    nsfb_plotfn_path_t *path;
    nsfb_plotfn_polylines_t *polylines;
} nsfb_plotter_fns_t;


bool select_plotters(nsfb_t *nsfb);

