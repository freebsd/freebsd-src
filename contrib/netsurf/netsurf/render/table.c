/*
 * Copyright 2005 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2005 Richard Wilson <info@tinct.net>
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

/** \file
 * Table processing and layout (implementation).
 */

#include <assert.h>

#include <dom/dom.h>

#include "css/css.h"
#include "css/utils.h"
#include "render/box.h"
#include "render/table.h"
#include "utils/log.h"
#include "utils/talloc.h"

/* Define to enable verbose table debug */
#undef TABLE_DEBUG

/**
 * Container for border values during table border calculations
 */
struct border {
	enum css_border_style_e style;	/**< border-style */
	enum css_border_color_e color;	/**< border-color type */
	css_color c;			/**< border-color value */
	css_fixed width;		/**< border-width length */
	css_unit unit;			/**< border-width units */
};

static void table_used_left_border_for_cell(struct box *cell);
static void table_used_top_border_for_cell(struct box *cell);
static void table_used_right_border_for_cell(struct box *cell);
static void table_used_bottom_border_for_cell(struct box *cell);
static bool table_border_is_more_eyecatching(const struct border *a,
		box_type a_src, const struct border *b, box_type b_src);
static void table_cell_top_process_table(struct box *table, struct border *a, 
		box_type *a_src);
static bool table_cell_top_process_group(struct box *cell, struct box *group, 
		struct border *a, box_type *a_src);
static bool table_cell_top_process_row(struct box *cell, struct box *row, 
		struct border *a, box_type *a_src);


/**
 * Determine the column width types for a table.
 *
 * \param  table  box of type BOX_TABLE
 * \return  true on success, false on memory exhaustion
 *
 * The table->col array is allocated and type and width are filled in for each
 * column.
 */

bool table_calculate_column_types(struct box *table)
{
	unsigned int i, j;
	struct column *col;
	struct box *row_group, *row, *cell;

	if (table->col)
		/* table->col already constructed, for example frameset table */
		return true;

	table->col = col = talloc_array(table, struct column, table->columns);
	if (!col)
		return false;

	for (i = 0; i != table->columns; i++) {
		col[i].type = COLUMN_WIDTH_UNKNOWN;
		col[i].width = 0;
		col[i].positioned = true;
	}

	/* 1st pass: cells with colspan 1 only */
	for (row_group = table->children; row_group; row_group =row_group->next)
	for (row = row_group->children; row; row = row->next)
	for (cell = row->children; cell; cell = cell->next) {
		enum css_width_e type;
		css_fixed value = 0;
		css_unit unit = CSS_UNIT_PX;

		assert(cell->type == BOX_TABLE_CELL);
		assert(cell->style);

		if (cell->columns != 1)
			continue;
		i = cell->start_column;

		if (css_computed_position(cell->style) != 
				CSS_POSITION_ABSOLUTE &&
				css_computed_position(cell->style) != 
				CSS_POSITION_FIXED) {
			col[i].positioned = false;
	        }

		type = css_computed_width(cell->style, &value, &unit);

		/* fixed width takes priority over any other width type */
		if (col[i].type != COLUMN_WIDTH_FIXED &&
				type == CSS_WIDTH_SET && unit != CSS_UNIT_PCT) {
			col[i].type = COLUMN_WIDTH_FIXED;
			col[i].width = FIXTOINT(nscss_len2px(value, unit, 
					cell->style));
			if (col[i].width < 0)
				col[i].width = 0;
			continue;
		}

		if (col[i].type != COLUMN_WIDTH_UNKNOWN)
			continue;

		if (type == CSS_WIDTH_SET && unit == CSS_UNIT_PCT) {
			col[i].type = COLUMN_WIDTH_PERCENT;
			col[i].width = FIXTOINT(value);
			if (col[i].width < 0)
				col[i].width = 0;
		} else if (type == CSS_WIDTH_AUTO) {
			col[i].type = COLUMN_WIDTH_AUTO;
		}
	}

	/* 2nd pass: cells which span multiple columns */
	for (row_group = table->children; row_group; row_group =row_group->next)
	for (row = row_group->children; row; row = row->next)
	for (cell = row->children; cell; cell = cell->next) {
		unsigned int fixed_columns = 0, percent_columns = 0,
				auto_columns = 0, unknown_columns = 0;
		int fixed_width = 0, percent_width = 0;
		enum css_width_e type;
		css_fixed value = 0;
		css_unit unit = CSS_UNIT_PX;

		if (cell->columns == 1)
			continue;
		i = cell->start_column;

		for (j = i; j < i + cell->columns; j++) {
			col[j].positioned = false;
		}
 
		/* count column types in spanned cells */
		for (j = 0; j != cell->columns; j++) {
			if (col[i + j].type == COLUMN_WIDTH_FIXED) {
				fixed_width += col[i + j].width;
				fixed_columns++;
			} else if (col[i + j].type == COLUMN_WIDTH_PERCENT) {
				percent_width += col[i + j].width;
				percent_columns++;
			} else if (col[i + j].type == COLUMN_WIDTH_AUTO) {
				auto_columns++;
			} else {
				unknown_columns++;
			}
		}

		if (!unknown_columns)
			continue;

		type = css_computed_width(cell->style, &value, &unit);

		/* if cell is fixed width, and all spanned columns are fixed
		 * or unknown width, split extra width among unknown columns */
		if (type == CSS_WIDTH_SET && unit != CSS_UNIT_PCT &&
				fixed_columns + unknown_columns ==
				cell->columns) {
			int width = (FIXTOFLT(nscss_len2px(value, unit, 
					cell->style)) -	fixed_width) / 
					unknown_columns;
			if (width < 0)
				width = 0;
			for (j = 0; j != cell->columns; j++) {
				if (col[i + j].type == COLUMN_WIDTH_UNKNOWN) {
					col[i + j].type = COLUMN_WIDTH_FIXED;
					col[i + j].width = width;
				}
			}
		}

		/* as above for percentage width */
		if (type == CSS_WIDTH_SET && unit == CSS_UNIT_PCT &&
				percent_columns + unknown_columns ==
				cell->columns) {
			int width = (FIXTOFLT(value) -
					percent_width) / unknown_columns;
			if (width < 0)
				width = 0;
			for (j = 0; j != cell->columns; j++) {
				if (col[i + j].type == COLUMN_WIDTH_UNKNOWN) {
					col[i + j].type = COLUMN_WIDTH_PERCENT;
					col[i + j].width = width;
				}
			}
		}
	}

	/* use AUTO if no width type was specified */
	for (i = 0; i != table->columns; i++) {
		if (col[i].type == COLUMN_WIDTH_UNKNOWN)
			col[i].type = COLUMN_WIDTH_AUTO;
	}

#ifdef TABLE_DEBUG
	for (i = 0; i != table->columns; i++)
		LOG(("table %p, column %u: type %s, width %i", table, i,
				((const char *[]) {"UNKNOWN", "FIXED", "AUTO",
				"PERCENT", "RELATIVE"})[col[i].type],
				col[i].width));
#endif

	return true;
}

/**
 * Calculate used values of border-{trbl}-{style,color,width} for table cells.
 *
 * \param cell  Table cell to consider
 *
 * \post \a cell's border array is populated
 */
void table_used_border_for_cell(struct box *cell)
{
	int side;

	assert(cell->type == BOX_TABLE_CELL);

	if (css_computed_border_collapse(cell->style) == 
			CSS_BORDER_COLLAPSE_SEPARATE) {
		css_fixed width = 0;
		css_unit unit = CSS_UNIT_PX;

		/* Left border */
		cell->border[LEFT].style = 
				css_computed_border_left_style(cell->style);
		css_computed_border_left_color(cell->style,
				&cell->border[LEFT].c);
		css_computed_border_left_width(cell->style, &width, &unit);
		cell->border[LEFT].width = 
			FIXTOINT(nscss_len2px(width, unit, cell->style));

		/* Top border */
		cell->border[TOP].style = 
				css_computed_border_top_style(cell->style);
		css_computed_border_top_color(cell->style,
				&cell->border[TOP].c);
		css_computed_border_top_width(cell->style, &width, &unit);
		cell->border[TOP].width = 
			FIXTOINT(nscss_len2px(width, unit, cell->style));

		/* Right border */
		cell->border[RIGHT].style = 
				css_computed_border_right_style(cell->style);
		css_computed_border_right_color(cell->style,
				&cell->border[RIGHT].c);
		css_computed_border_right_width(cell->style, &width, &unit);
		cell->border[RIGHT].width = 
			FIXTOINT(nscss_len2px(width, unit, cell->style));

		/* Bottom border */
		cell->border[BOTTOM].style = 
				css_computed_border_bottom_style(cell->style);
		css_computed_border_bottom_color(cell->style,
				&cell->border[BOTTOM].c);
		css_computed_border_bottom_width(cell->style, &width, &unit);
		cell->border[BOTTOM].width = 
			FIXTOINT(nscss_len2px(width, unit, cell->style));
	} else {
		/* Left border */
		table_used_left_border_for_cell(cell);

		/* Top border */
		table_used_top_border_for_cell(cell);

		/* Right border */
		table_used_right_border_for_cell(cell);

		/* Bottom border */
		table_used_bottom_border_for_cell(cell);
	}

	/* Finally, ensure that any borders configured as 
	 * hidden or none have zero width. (c.f. layout_find_dimensions) */
	for (side = 0; side != 4; side++) {
		if (cell->border[side].style == CSS_BORDER_STYLE_HIDDEN ||
				cell->border[side].style == 
				CSS_BORDER_STYLE_NONE)
			cell->border[side].width = 0;
	}
}

/******************************************************************************
 * Helpers for used border calculations                                       *
 ******************************************************************************/

/**
 * Calculate used values of border-left-{style,color,width}
 *
 * \param cell Table cell to consider
 */
void table_used_left_border_for_cell(struct box *cell)
{
	struct border a, b;
	box_type a_src, b_src;

	/** \todo Need column and column_group, too */

	/* Initialise to computed left border for cell */
	a.style = css_computed_border_left_style(cell->style);
	a.color = css_computed_border_left_color(cell->style, &a.c);
	css_computed_border_left_width(cell->style, &a.width, &a.unit);
	a.width = nscss_len2px(a.width, a.unit, cell->style);
	a.unit = CSS_UNIT_PX;
	a_src = BOX_TABLE_CELL;

	if (cell->prev != NULL || cell->start_column != 0) {
		/* Cell to the left -- consider its right border */
		struct box *prev = NULL;

		if (cell->prev == NULL) {
			struct box *row;

			/* Spanned from a previous row in current row group */
			for (row = cell->parent; row != NULL; row = row->prev) {
				for (prev = row->children; prev != NULL; 
						prev = prev->next) {
					if (prev->start_column + 
							prev->columns == 
							cell->start_column)
						break;
				}

				if (prev != NULL)
					break;
			}

			assert(prev != NULL);
		} else {
			prev = cell->prev;
		}

		b.style = css_computed_border_right_style(prev->style);
		b.color = css_computed_border_right_color(prev->style, &b.c);
		css_computed_border_right_width(prev->style, &b.width, &b.unit);
		b.width = nscss_len2px(b.width, b.unit, prev->style);
		b.unit = CSS_UNIT_PX;
		b_src = BOX_TABLE_CELL;

		if (table_border_is_more_eyecatching(&a, a_src, &b, b_src)) {
			a = b;
			a_src = b_src;
		}
	} else {
		/* First cell in row, so consider rows and row group */
		struct box *row = cell->parent;
		struct box *group = row->parent;
		struct box *table = group->parent;
		unsigned int rows = cell->rows;

		while (rows-- > 0 && row != NULL) {
			/* Spanned rows -- consider their left border */
			b.style = css_computed_border_left_style(row->style);
			b.color = css_computed_border_left_color(
					row->style, &b.c);
			css_computed_border_left_width(
					row->style, &b.width, &b.unit);
			b.width = nscss_len2px(b.width, b.unit, row->style);
			b.unit = CSS_UNIT_PX;
			b_src = BOX_TABLE_ROW;
		
			if (table_border_is_more_eyecatching(&a, a_src, 
					&b, b_src)) {
				a = b;
				a_src = b_src;
			}

			row = row->next;
		}

		/** \todo can cells span row groups? */

		/* Row group -- consider its left border */
		b.style = css_computed_border_left_style(group->style);
		b.color = css_computed_border_left_color(group->style, &b.c);
		css_computed_border_left_width(group->style, &b.width, &b.unit);
		b.width = nscss_len2px(b.width, b.unit, group->style);
		b.unit = CSS_UNIT_PX;
		b_src = BOX_TABLE_ROW_GROUP;
		
		if (table_border_is_more_eyecatching(&a, a_src, &b, b_src)) {
			a = b;
			a_src = b_src;
		}

		/* The table itself -- consider its left border */
		b.style = css_computed_border_left_style(table->style);
		b.color = css_computed_border_left_color(table->style, &b.c);
		css_computed_border_left_width(table->style, &b.width, &b.unit);
		b.width = nscss_len2px(b.width, b.unit, table->style);
		b.unit = CSS_UNIT_PX;
		b_src = BOX_TABLE;
		
		if (table_border_is_more_eyecatching(&a, a_src, &b, b_src)) {
			a = b;
			a_src = b_src;
		}
	}

	/* a now contains the used left border for the cell */
	cell->border[LEFT].style = a.style;
	cell->border[LEFT].c = a.c;
	cell->border[LEFT].width = 
			FIXTOINT(nscss_len2px(a.width, a.unit, cell->style));
}

/**
 * Calculate used values of border-top-{style,color,width}
 *
 * \param cell Table cell to consider
 */
void table_used_top_border_for_cell(struct box *cell)
{
	struct border a, b;
	box_type a_src, b_src;
	struct box *row = cell->parent;
	bool process_group = false;

	/* Initialise to computed top border for cell */
	a.style = css_computed_border_top_style(cell->style);
	css_computed_border_top_color(cell->style, &a.c);
	css_computed_border_top_width(cell->style, &a.width, &a.unit);
	a.width = nscss_len2px(a.width, a.unit, cell->style);
	a.unit = CSS_UNIT_PX;
	a_src = BOX_TABLE_CELL;

	/* Top border of row */
	b.style = css_computed_border_top_style(row->style);
	css_computed_border_top_color(row->style, &b.c);
	css_computed_border_top_width(row->style, &b.width, &b.unit);
	b.width = nscss_len2px(b.width, b.unit, row->style);
	b.unit = CSS_UNIT_PX;
	b_src = BOX_TABLE_ROW;

	if (table_border_is_more_eyecatching(&a, a_src, &b, b_src)) {
		a = b;
		a_src = b_src;
	}

	if (row->prev != NULL) {
		/* Consider row(s) above */
		while (table_cell_top_process_row(cell, row->prev, 
				&a, &a_src) == false) {
			if (row->prev->prev == NULL) {
				/* Consider row group */
				process_group = true;
				break;
			} else {
				row = row->prev;
			}
		}
	} else {
		process_group = true;
	}

	if (process_group) {
		struct box *group = row->parent;

		/* Top border of row group */
		b.style = css_computed_border_top_style(group->style);
		b.color = css_computed_border_top_color(group->style, &b.c);
		css_computed_border_top_width(group->style, &b.width, &b.unit);
		b.width = nscss_len2px(b.width, b.unit, group->style);
		b.unit = CSS_UNIT_PX;
		b_src = BOX_TABLE_ROW_GROUP;

		if (table_border_is_more_eyecatching(&a, a_src, &b, b_src)) {
			a = b;
			a_src = b_src;
		}

		if (group->prev == NULL) {
			/* Top border of table */
			table_cell_top_process_table(group->parent, &a, &a_src);
		} else {
			/* Process previous group(s) */
			while (table_cell_top_process_group(cell, group->prev, 
					&a, &a_src) == false) {
				if (group->prev->prev == NULL) {
					/* Top border of table */
					table_cell_top_process_table(
							group->parent, 
							&a, &a_src);
					break;
				} else {
					group = group->prev;
				}
			}
		}
	}

	/* a now contains the used top border for the cell */
	cell->border[TOP].style = a.style;
	cell->border[TOP].c = a.c;
	cell->border[TOP].width = 
			FIXTOINT(nscss_len2px(a.width, a.unit, cell->style));
}

/**
 * Calculate used values of border-right-{style,color,width}
 *
 * \param cell Table cell to consider
 */
void table_used_right_border_for_cell(struct box *cell)
{
	struct border a, b;
	box_type a_src, b_src;

	/** \todo Need column and column_group, too */

	/* Initialise to computed right border for cell */
	a.style = css_computed_border_right_style(cell->style);
	css_computed_border_right_color(cell->style, &a.c);
	css_computed_border_right_width(cell->style, &a.width, &a.unit);
	a.width = nscss_len2px(a.width, a.unit, cell->style);
	a.unit = CSS_UNIT_PX;
	a_src = BOX_TABLE_CELL;

	if (cell->next != NULL || cell->start_column + cell->columns != 
			cell->parent->parent->parent->columns) {
		/* Cell is not at right edge of table -- no right border */
		a.style = CSS_BORDER_STYLE_NONE;
		a.width = 0;
		a.unit = CSS_UNIT_PX;
	} else {
		/* Last cell in row, so consider rows and row group */
		struct box *row = cell->parent;
		struct box *group = row->parent;
		struct box *table = group->parent;
		unsigned int rows = cell->rows;

		while (rows-- > 0 && row != NULL) {
			/* Spanned rows -- consider their right border */
			b.style = css_computed_border_right_style(row->style);
			b.color = css_computed_border_right_color(
					row->style, &b.c);
			css_computed_border_right_width(
					row->style, &b.width, &b.unit);
			b.width = nscss_len2px(b.width, b.unit, row->style);
			b.unit = CSS_UNIT_PX;
			b_src = BOX_TABLE_ROW;
		
			if (table_border_is_more_eyecatching(&a, a_src, 
					&b, b_src)) {
				a = b;
				a_src = b_src;
			}

			row = row->next;
		}

		/** \todo can cells span row groups? */

		/* Row group -- consider its right border */
		b.style = css_computed_border_right_style(group->style);
		b.color = css_computed_border_right_color(group->style, &b.c);
		css_computed_border_right_width(group->style, 
				&b.width, &b.unit);
		b.width = nscss_len2px(b.width, b.unit, group->style);
		b.unit = CSS_UNIT_PX;
		b_src = BOX_TABLE_ROW_GROUP;
		
		if (table_border_is_more_eyecatching(&a, a_src, &b, b_src)) {
			a = b;
			a_src = b_src;
		}

		/* The table itself -- consider its right border */
		b.style = css_computed_border_right_style(table->style);
		b.color = css_computed_border_right_color(table->style, &b.c);
		css_computed_border_right_width(table->style, 
				&b.width, &b.unit);
		b.width = nscss_len2px(b.width, b.unit, table->style);
		b.unit = CSS_UNIT_PX;
		b_src = BOX_TABLE;
		
		if (table_border_is_more_eyecatching(&a, a_src, &b, b_src)) {
			a = b;
			a_src = b_src;
		}
	}

	/* a now contains the used right border for the cell */
	cell->border[RIGHT].style = a.style;
	cell->border[RIGHT].c = a.c;
	cell->border[RIGHT].width = 
			FIXTOINT(nscss_len2px(a.width, a.unit, cell->style));
}

/**
 * Calculate used values of border-bottom-{style,color,width}
 *
 * \param cell Table cell to consider
 */
void table_used_bottom_border_for_cell(struct box *cell)
{
	struct border a, b;
	box_type a_src, b_src;
	struct box *row = cell->parent;
	unsigned int rows = cell->rows;

	/* Initialise to computed bottom border for cell */
	a.style = css_computed_border_bottom_style(cell->style);
	css_computed_border_bottom_color(cell->style, &a.c);
	css_computed_border_bottom_width(cell->style, &a.width, &a.unit);
	a.width = nscss_len2px(a.width, a.unit, cell->style);
	a.unit = CSS_UNIT_PX;
	a_src = BOX_TABLE_CELL;

	while (rows-- > 0 && row != NULL)
		row = row->next;

	/** \todo Can cells span row groups? */

	if (row != NULL) {
		/* Cell is not at bottom edge of table -- no bottom border */
		a.style = CSS_BORDER_STYLE_NONE;
		a.width = 0;
		a.unit = CSS_UNIT_PX;
	} else {
		/* Cell at bottom of table, so consider row and row group */
		struct box *row = cell->parent;
		struct box *group = row->parent;
		struct box *table = group->parent;

		/* Bottom border of row */
		b.style = css_computed_border_bottom_style(row->style);
		b.color = css_computed_border_bottom_color(row->style, &b.c);
		css_computed_border_bottom_width(row->style, &b.width, &b.unit);
		b.width = nscss_len2px(b.width, b.unit, row->style);
		b.unit = CSS_UNIT_PX;
		b_src = BOX_TABLE_ROW;
		
		if (table_border_is_more_eyecatching(&a, a_src, &b, b_src)) {
			a = b;
			a_src = b_src;
		}

		/* Row group -- consider its bottom border */
		b.style = css_computed_border_bottom_style(group->style);
		b.color = css_computed_border_bottom_color(group->style, &b.c);
		css_computed_border_bottom_width(group->style, 
				&b.width, &b.unit);
		b.width = nscss_len2px(b.width, b.unit, group->style);
		b.unit = CSS_UNIT_PX;
		b_src = BOX_TABLE_ROW_GROUP;
		
		if (table_border_is_more_eyecatching(&a, a_src, &b, b_src)) {
			a = b;
			a_src = b_src;
		}

		/* The table itself -- consider its bottom border */
		b.style = css_computed_border_bottom_style(table->style);
		b.color = css_computed_border_bottom_color(table->style, &b.c);
		css_computed_border_bottom_width(table->style, 
				&b.width, &b.unit);
		b.width = nscss_len2px(b.width, b.unit, table->style);
		b.unit = CSS_UNIT_PX;
		b_src = BOX_TABLE;
		
		if (table_border_is_more_eyecatching(&a, a_src, &b, b_src)) {
			a = b;
		}
	}

	/* a now contains the used bottom border for the cell */
	cell->border[BOTTOM].style = a.style;
	cell->border[BOTTOM].c = a.c;
	cell->border[BOTTOM].width = 
			FIXTOINT(nscss_len2px(a.width, a.unit, cell->style));
}

/**
 * Determine if a border style is more eyecatching than another
 *
 * \param a      Reference border style
 * \param a_src  Source of \a a
 * \param b      Candidate border style
 * \param b_src  Source of \a b
 * \return True if \a b is more eyecatching than \a a
 */
bool table_border_is_more_eyecatching(const struct border *a,
		box_type a_src,	const struct border *b, box_type b_src)
{
	css_fixed awidth, bwidth;
	int impact = 0;

	/* See CSS 2.1 $17.6.2.1 */

	/* 1 + 2 -- hidden beats everything, none beats nothing */
	if (a->style == CSS_BORDER_STYLE_HIDDEN ||
			b->style == CSS_BORDER_STYLE_NONE)
		return false;

	if (b->style == CSS_BORDER_STYLE_HIDDEN ||
			a->style == CSS_BORDER_STYLE_NONE)
		return true;

	/* 3a -- wider borders beat narrow ones */
	/* The widths must be absolute, which will be the case 
	 * if they've come from a computed style. */
	assert(a->unit != CSS_UNIT_EM && a->unit != CSS_UNIT_EX);
	assert(b->unit != CSS_UNIT_EM && b->unit != CSS_UNIT_EX);
	awidth = nscss_len2px(a->width, a->unit, NULL);
	bwidth = nscss_len2px(b->width, b->unit, NULL);

	if (awidth < bwidth)
		return true;
	else if (bwidth < awidth)
		return false;

	/* 3b -- sort by style */
	switch (a->style) {
	case CSS_BORDER_STYLE_DOUBLE: impact++;
	case CSS_BORDER_STYLE_SOLID:  impact++;
	case CSS_BORDER_STYLE_DASHED: impact++;
	case CSS_BORDER_STYLE_DOTTED: impact++;
	case CSS_BORDER_STYLE_RIDGE:  impact++;
	case CSS_BORDER_STYLE_OUTSET: impact++;
	case CSS_BORDER_STYLE_GROOVE: impact++;
	case CSS_BORDER_STYLE_INSET:  impact++;
	default:
		break;
	}

	switch (b->style) {
	case CSS_BORDER_STYLE_DOUBLE: impact--;
	case CSS_BORDER_STYLE_SOLID:  impact--;
	case CSS_BORDER_STYLE_DASHED: impact--;
	case CSS_BORDER_STYLE_DOTTED: impact--;
	case CSS_BORDER_STYLE_RIDGE:  impact--;
	case CSS_BORDER_STYLE_OUTSET: impact--;
	case CSS_BORDER_STYLE_GROOVE: impact--;
	case CSS_BORDER_STYLE_INSET:  impact--;
	default:
		break;
	}

	if (impact < 0)
		return true;
	else if (impact > 0)
		return false;

	/* 4a -- sort by origin */
	impact = 0;

	switch (a_src) {
	case BOX_TABLE_CELL:       impact++;
	case BOX_TABLE_ROW:        impact++;
	case BOX_TABLE_ROW_GROUP:  impact++;
	/** \todo COL/COL_GROUP */
	case BOX_TABLE:            impact++;
	default:
		break;
	}

	switch (b_src) {
	case BOX_TABLE_CELL:       impact--;
	case BOX_TABLE_ROW:        impact--;
	case BOX_TABLE_ROW_GROUP:  impact--;
	/** \todo COL/COL_GROUP */
	case BOX_TABLE:            impact--;
	default:
		break;
	}

	if (impact < 0)
		return true;
	else if (impact > 0)
		return false;

	/* 4b -- furthest left (if direction: ltr) and towards top wins */
	/** \todo Currently assumes b satisifies this */
	return true;
}

/******************************************************************************
 * Helpers for top border collapsing                                          *
 ******************************************************************************/

/**
 * Process a table
 *
 * \param table  Table to process
 * \param a      Current border style for cell
 * \param a_src  Source of \a a
 *
 * \post \a a will be updated with most eyecatching style
 * \post \a a_src will be updated also
 */
void table_cell_top_process_table(struct box *table, struct border *a, 
		box_type *a_src)
{
	struct border b;
	box_type b_src;

	/* Top border of table */
	b.style = css_computed_border_top_style(table->style);
	b.color = css_computed_border_top_color(table->style, &b.c);
	css_computed_border_top_width(table->style, &b.width, &b.unit);
	b.width = nscss_len2px(b.width, b.unit, table->style);
	b.unit = CSS_UNIT_PX;
	b_src = BOX_TABLE;

	if (table_border_is_more_eyecatching(a, *a_src, &b, b_src)) {
		*a = b;
		*a_src = b_src;
	}
}

/**
 * Process a group
 *
 * \param cell   Cell being considered
 * \param group  Group to process
 * \param a      Current border style for cell
 * \param a_src  Source of \a a
 * \return true if group has non-empty rows, false otherwise
 *
 * \post \a a will be updated with most eyecatching style
 * \post \a a_src will be updated also
 */
bool table_cell_top_process_group(struct box *cell, struct box *group,
		struct border *a, box_type *a_src)
{
	struct border b;
	box_type b_src;

	/* Bottom border of group */
	b.style = css_computed_border_bottom_style(group->style);
	b.color = css_computed_border_bottom_color(group->style, &b.c);
	css_computed_border_bottom_width(group->style, &b.width, &b.unit);
	b.width = nscss_len2px(b.width, b.unit, group->style);
	b.unit = CSS_UNIT_PX;
	b_src = BOX_TABLE_ROW_GROUP;

	if (table_border_is_more_eyecatching(a, *a_src, &b, b_src)) {
		*a = b;
		*a_src = b_src;
	}

	if (group->last != NULL) {
		/* Process rows in group, starting with last */
		struct box *row = group->last;

		while (table_cell_top_process_row(cell, row, 
				a, a_src) == false) {
			if (row->prev == NULL) {
				return false;
			} else {
				row = row->prev;
			}
		}
	} else {
		/* Group is empty, so consider its top border */
		b.style = css_computed_border_top_style(group->style);
		b.color = css_computed_border_top_color(group->style, &b.c);
		css_computed_border_top_width(group->style, &b.width, &b.unit);
		b.width = nscss_len2px(b.width, b.unit, group->style);
		b.unit = CSS_UNIT_PX;
		b_src = BOX_TABLE_ROW_GROUP;

		if (table_border_is_more_eyecatching(a, *a_src, &b, b_src)) {
			*a = b;
			*a_src = b_src;
		}

		return false;
	}

	return true;
}

/**
 * Process a row
 *
 * \param cell   Cell being considered
 * \param row    Row to process
 * \param a      Current border style for cell
 * \param a_src  Source of \a a
 * \return true if row has cells, false otherwise
 *
 * \post \a a will be updated with most eyecatching style
 * \post \a a_src will be updated also
 */
bool table_cell_top_process_row(struct box *cell, struct box *row, 
		struct border *a, box_type *a_src)
{
	struct border b;
	box_type b_src;

	/* Bottom border of row */
	b.style = css_computed_border_bottom_style(row->style);
	b.color = css_computed_border_bottom_color(row->style, &b.c);
	css_computed_border_bottom_width(row->style, &b.width, &b.unit);
	b.width = nscss_len2px(b.width, b.unit, row->style);
	b.unit = CSS_UNIT_PX;
	b_src = BOX_TABLE_ROW;

	if (table_border_is_more_eyecatching(a, *a_src, &b, b_src)) {
		*a = b;
		*a_src = b_src;
	}

	if (row->children == NULL) {
		/* Row is empty, so consider its top border */
		b.style = css_computed_border_top_style(row->style);
		b.color = css_computed_border_top_color(row->style, &b.c);
		css_computed_border_top_width(row->style, &b.width, &b.unit);
		b.width = nscss_len2px(b.width, b.unit, row->style);
		b.unit = CSS_UNIT_PX;
		b_src = BOX_TABLE_ROW;

		if (table_border_is_more_eyecatching(a, *a_src, &b, b_src)) {
			*a = b;
			*a_src = b_src;
		}

		return false;
	} else {
		/* Process cells that are directly above the cell being 
		 * considered. They may not be in this row, but in one of the
		 * rows above it in the case where rowspan > 1. */
		struct box *c;
		bool processed = false;

		while (processed == false) {
			for (c = row->children; c != NULL; c = c->next) {
				/* Ignore cells to the left */
				if (c->start_column + c->columns - 1 <
						cell->start_column)
					continue;
				/* Ignore cells to the right */
				if (c->start_column > cell->start_column +
						cell->columns - 1)
					continue;

				/* Flag that we've processed a cell */
				processed = true;

				/* Consider bottom border */
				b.style = css_computed_border_bottom_style(
						c->style);
				b.color = css_computed_border_bottom_color(
						c->style, &b.c);
				css_computed_border_bottom_width(c->style,
						&b.width, &b.unit);
				b.width = nscss_len2px(b.width, b.unit, 
						c->style);
				b.unit = CSS_UNIT_PX;
				b_src = BOX_TABLE_CELL;

				if (table_border_is_more_eyecatching(a, *a_src,
						&b, b_src)) {
					*a = b;
					*a_src = b_src;
				}
			}

			if (processed == false) {
				/* There must be a preceding row */
				assert(row->prev != NULL);

				row = row->prev;
			}
		}
	}

	return true;
}

