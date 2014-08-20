#include <stdbool.h>

#include "libnsfb.h"
#include "libnsfb_plot.h"
#include "libnsfb_plot_util.h"

#include "nsfb.h"

enum {
        POINT_LEFTOF_REGION = 1,
        POINT_RIGHTOF_REGION = 2,
        POINT_ABOVE_REGION = 4,
        POINT_BELOW_REGION = 8,
};

#define REGION(x,y,cx1,cx2,cy1,cy2) \
    ( ( (y) > (cy2) ? POINT_BELOW_REGION : 0) |                         \
      ( (y) < (cy1) ? POINT_ABOVE_REGION : 0) |                         \
      ( (x) > (cx2) ? POINT_RIGHTOF_REGION : 0) |                       \
      ( (x) < (cx1) ? POINT_LEFTOF_REGION : 0) )

#define SWAP(a, b) do { int t; t=(a); (a)=(b); (b)=t;  } while(0)

/* clip a rectangle with another clipping rectangle.
 *
 * @param clip The rectangle to clip to.
 * @param rect The rectangle to clip. 
 * @return false if the \a rect lies completely outside the \a clip rectangle,
 *         true if some of the \a rect is still visible.
 */
bool 
nsfb_plot_clip(const nsfb_bbox_t * restrict clip, nsfb_bbox_t * restrict rect)
{
        char region1;
        char region2;

	if (rect->x1 < rect->x0) SWAP(rect->x0, rect->x1);

	if (rect->y1 < rect->y0) SWAP(rect->y0, rect->y1);

	region1 = REGION(rect->x0, rect->y0, clip->x0, clip->x1 - 1, clip->y0, clip->y1 - 1);
	region2 = REGION(rect->x1, rect->y1, clip->x0, clip->x1 - 1, clip->y0, clip->y1 - 1);

        /* area lies entirely outside the clipping rectangle */
        if ((region1 | region2) && (region1 & region2))
                return false;

        if (rect->x0 < clip->x0)
                rect->x0 = clip->x0;
        if (rect->x0 > clip->x1)
                rect->x0 = clip->x1;

        if (rect->x1 < clip->x0)
                rect->x1 = clip->x0;
        if (rect->x1 > clip->x1)
                rect->x1 = clip->x1;

        if (rect->y0 < clip->y0)
                rect->y0 = clip->y0;
        if (rect->y0 > clip->y1)
                rect->y0 = clip->y1;

        if (rect->y1 < clip->y0)
                rect->y1 = clip->y0;
        if (rect->y1 > clip->y1)
                rect->y1 = clip->y1;

        return true;
}

bool 
nsfb_plot_clip_ctx(nsfb_t *nsfb, nsfb_bbox_t * restrict rect)
{
    return nsfb_plot_clip(&nsfb->clip, rect);
}

/** Clip a line to a bounding box.
 */
bool nsfb_plot_clip_line(const nsfb_bbox_t *clip, nsfb_bbox_t * restrict line)
{
        char region1;
        char region2;
        region1 = REGION(line->x0, line->y0, clip->x0, clip->x1 - 1, clip->y0, clip->y1 - 1);
        region2 = REGION(line->x1, line->y1, clip->x0, clip->x1 - 1, clip->y0, clip->y1 - 1);

        while (region1 | region2) {
                if (region1 & region2) {
                        /* line lies entirely outside the clipping rectangle */
                        return false;
                }

                if (region1) {
                        /* first point */
                        if (region1 & POINT_BELOW_REGION) {
                                /* divide line at bottom */
                                line->x0 = (line->x0 + (line->x1 - line->x0) *
                                       (clip->y1 - 1 - line->y0) / (line->y1 - line->y0));
                                line->y0 = clip->y1 - 1;
                        } else if (region1 & POINT_ABOVE_REGION) {
                                /* divide line at top */
                                line->x0 = (line->x0 + (line->x1 - line->x0) *
                                       (clip->y0 - line->y0) / (line->y1 - line->y0));
                                line->y0 = clip->y0;
                        } else if (region1 & POINT_RIGHTOF_REGION) {
                                /* divide line at right */
                                line->y0 = (line->y0 + (line->y1 - line->y0) *
                                       (clip->x1  - 1 - line->x0) / (line->x1 - line->x0));
                                line->x0 = clip->x1 - 1;
                        } else if (region1 & POINT_LEFTOF_REGION) {
                                /* divide line at right */
                                line->y0 = (line->y0 + (line->y1 - line->y0) *
                                       (clip->x0 - line->x0) / (line->x1 - line->x0));
                                line->x0 = clip->x0;
                        }

                        region1 = REGION(line->x0, line->y0, clip->x0, clip->x1 - 1, clip->y0, clip->y1 - 1);
                } else {
                        /* second point */
                        if (region2 & POINT_BELOW_REGION) {
                                /* divide line at bottom*/
                                line->x1 = (line->x0 + (line->x1 - line->x0) *
                                       (clip->y1  - 1 - line->y0) / (line->y1 - line->y0));
                                line->y1 = clip->y1 - 1;
                        } else if (region2 & POINT_ABOVE_REGION) {
                                /* divide line at top*/
                                line->x1 = (line->x0 + (line->x1 - line->x0) *
                                       (clip->y0 - line->y0) / (line->y1 - line->y0));
                                line->y1 = clip->y0;
                        } else if (region2 & POINT_RIGHTOF_REGION) {
                                /* divide line at right*/
                                line->y1 = (line->y0 + (line->y1 - line->y0) *
                                       (clip->x1  - 1 - line->x0) / (line->x1 - line->x0));
                                line->x1 = clip->x1 - 1;
                        } else if (region2 & POINT_LEFTOF_REGION) {
                                /* divide line at right*/
                                line->y1 = (line->y0 + (line->y1 - line->y0) *
                                       (clip->x0 - line->x0) / (line->x1 - line->x0));
                                line->x1 = clip->x0;
                        }

                        region2 = REGION(line->x1, line->y1, clip->x0, clip->x1 - 1, clip->y0, clip->y1 - 1);
                }
        }

        return true;
}

bool nsfb_plot_clip_line_ctx(nsfb_t *nsfb, nsfb_bbox_t * restrict line)
{
    return nsfb_plot_clip_line(&nsfb->clip, line);
}

/* documented in libnsfb_plot_util.h */
bool 
nsfb_plot_add_rect(const nsfb_bbox_t *box1, const nsfb_bbox_t *box2, nsfb_bbox_t *result)
{
    /* lower x coordinate */
    if (box1->x0 < box2->x0)
        result->x0 = box1->x0;
    else
        result->x0 = box2->x0;

    /* lower y coordinate */
    if (box1->y0 < box2->y0)
        result->y0 = box1->y0;
    else
        result->y0 = box2->y0;

    /* upper x coordinate */
    if (box1->x1 > box2->x1)
        result->x1 = box1->x1;
    else
        result->x1 = box2->x1;

    /* upper y coordinate */
    if (box1->y1 > box2->y1)
        result->y1 = box1->y1;
    else
        result->y1 = box2->y1;

    return true;
} 

bool nsfb_plot_bbox_intersect(const nsfb_bbox_t *box1, const nsfb_bbox_t *box2)
{
    if (box2->x1 < box1->x0)
        return false;

    if (box2->y1 < box1->y0)
        return false;

    if (box2->x0 > box1->x1)
        return false;

    if (box2->y0 > box1->y1)
        return false;

    return true;
}
