/*
 * Copyright 2005 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2005 John M Bell <jmb202@ecs.soton.ac.uk>
 * Copyright 2004 Kevin Bagust <kevin.bagust@ntlworld.com>
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
 * Box tree normalisation (implementation).
 */

#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include "css/css.h"
#include "css/select.h"
#include "render/box.h"
#include "render/html_internal.h"
#include "render/table.h"
#include "utils/log.h"

/* Define to enable box normalise debug */
#undef BOX_NORMALISE_DEBUG

/**
 * Row spanning information for a cell
 */
struct span_info {
	/** Number of rows this cell spans */
	unsigned int row_span;
	/** Row group of cell */
	struct box *rg;
	/** The cell in this column spans all rows until the end of the table */
	bool auto_row;
};

/**
 * Column record for a table
 */
struct columns {
	/** Current column index */
	unsigned int current_column;
	/** Number of columns in main part of table 1..max columns */
	unsigned int num_columns;
	/** Information about columns in main table, array [0, num_columns) */
	struct span_info *spans;
	/** Number of rows in table */
	unsigned int num_rows;
};


static bool box_normalise_table(struct box *table, html_content *c);
static bool box_normalise_table_spans(struct box *table, 
		struct span_info *spans, html_content *c);
static bool box_normalise_table_row_group(struct box *row_group,
		struct columns *col_info,
		html_content *c);
static bool box_normalise_table_row(struct box *row,
		struct columns *col_info,
		html_content *c);
static bool calculate_table_row(struct columns *col_info,
		unsigned int col_span, unsigned int row_span,
		unsigned int *start_column, struct box *cell);
static bool box_normalise_inline_container(struct box *cont, html_content *c);

/**
 * Ensure the box tree is correctly nested by adding and removing nodes.
 *
 * \param  block     box of type BLOCK, INLINE_BLOCK, or TABLE_CELL
 * \param  box_pool  pool to allocate new boxes in
 * \return  true on success, false on memory exhaustion
 *
 * The tree is modified to satisfy the following:
 * \code
 * parent               permitted child nodes
 * BLOCK, INLINE_BLOCK  BLOCK, INLINE_CONTAINER, TABLE
 * INLINE_CONTAINER     INLINE, INLINE_BLOCK, FLOAT_LEFT, FLOAT_RIGHT, BR, TEXT
 * INLINE, TEXT         none
 * TABLE                at least 1 TABLE_ROW_GROUP
 * TABLE_ROW_GROUP      at least 1 TABLE_ROW
 * TABLE_ROW            at least 1 TABLE_CELL
 * TABLE_CELL           BLOCK, INLINE_CONTAINER, TABLE (same as BLOCK)
 * FLOAT_(LEFT|RIGHT)   exactly 1 BLOCK or TABLE
 * \endcode
 */

bool box_normalise_block(struct box *block, html_content *c)
{
	struct box *child;
	struct box *next_child;
	struct box *table;
	css_computed_style *style;
	nscss_select_ctx ctx;

	assert(block != NULL);

#ifdef BOX_NORMALISE_DEBUG
	LOG(("block %p, block->type %u", block, block->type));
#endif

	assert(block->type == BOX_BLOCK || block->type == BOX_INLINE_BLOCK ||
			block->type == BOX_TABLE_CELL);

	for (child = block->children; child != NULL; child = next_child) {
#ifdef BOX_NORMALISE_DEBUG
		LOG(("child %p, child->type = %d", child, child->type));
#endif

		next_child = child->next;	/* child may be destroyed */

		switch (child->type) {
		case BOX_BLOCK:
			/* ok */
			if (box_normalise_block(child, c) == false)
				return false;
			break;
		case BOX_INLINE_CONTAINER:
			if (box_normalise_inline_container(child, c) == false)
				return false;
			break;
		case BOX_TABLE:
			if (box_normalise_table(child, c) == false)
				return false;
			break;
		case BOX_INLINE:
		case BOX_INLINE_END:
		case BOX_INLINE_BLOCK:
		case BOX_FLOAT_LEFT:
		case BOX_FLOAT_RIGHT:
		case BOX_BR:
		case BOX_TEXT:
			/* should have been wrapped in inline
			   container by convert_xml_to_box() */
			assert(0);
			break;
		case BOX_TABLE_ROW_GROUP:
		case BOX_TABLE_ROW:
		case BOX_TABLE_CELL:
			/* insert implied table */
			assert(block->style != NULL);

			ctx.ctx = c->select_ctx;
			ctx.quirks = (c->quirks == DOM_DOCUMENT_QUIRKS_MODE_FULL);
			ctx.base_url = c->base_url;
			ctx.universal = c->universal;

			style = nscss_get_blank_style(&ctx, block->style);
			if (style == NULL)
				return false;

			table = box_create(NULL, style, true, block->href,
					block->target, NULL, NULL, c->bctx);
			if (table == NULL) {
				css_computed_style_destroy(style);
				return false;
			}
			table->type = BOX_TABLE;

			if (child->prev == NULL)
				block->children = table;
			else
				child->prev->next = table;

			table->prev = child->prev;

			while (child != NULL && (
					child->type == BOX_TABLE_ROW_GROUP ||
					child->type == BOX_TABLE_ROW ||
					child->type == BOX_TABLE_CELL)) {
				box_add_child(table, child);

				next_child = child->next;
				child->next = NULL;
				child = next_child;
			}

			table->last->next = NULL;
			table->next = next_child = child;
			if (table->next != NULL)
				table->next->prev = table;
			else
				block->last = table;
			table->parent = block;

			if (box_normalise_table(table, c) == false)
				return false;
			break;
		default:
			assert(0);
		}
	}

	return true;
}


bool box_normalise_table(struct box *table, html_content * c)
{
	struct box *child;
	struct box *next_child;
	struct box *row_group;
	css_computed_style *style;
	struct columns col_info;
	nscss_select_ctx ctx;

	assert(table != NULL);
	assert(table->type == BOX_TABLE);

#ifdef BOX_NORMALISE_DEBUG
	LOG(("table %p", table));
#endif

	col_info.num_columns = 1;
	col_info.current_column = 0;
	col_info.spans = malloc(2 * sizeof *col_info.spans);
	if (col_info.spans == NULL)
		return false;

	col_info.spans[0].row_span = col_info.spans[1].row_span = 0;
	col_info.spans[0].auto_row = false;
	col_info.spans[1].auto_row = false;
	col_info.num_rows = 0;

	for (child = table->children; child != NULL; child = next_child) {
		next_child = child->next;
		switch (child->type) {
		case BOX_TABLE_ROW_GROUP:
			/* ok */
			if (box_normalise_table_row_group(child,
					&col_info, c) == false) {
				free(col_info.spans);
				return false;
			}
			break;
		case BOX_BLOCK:
		case BOX_INLINE_CONTAINER:
		case BOX_TABLE:
		case BOX_TABLE_ROW:
		case BOX_TABLE_CELL:
			/* insert implied table row group */
			assert(table->style != NULL);

			ctx.ctx = c->select_ctx;
			ctx.quirks = (c->quirks == DOM_DOCUMENT_QUIRKS_MODE_FULL);
			ctx.base_url = c->base_url;
			ctx.universal = c->universal;

			style = nscss_get_blank_style(&ctx, table->style);
			if (style == NULL) {
				free(col_info.spans);
				return false;
			}

			row_group = box_create(NULL, style, true, table->href,
					table->target, NULL, NULL, c->bctx);
			if (row_group == NULL) {
				css_computed_style_destroy(style);
				free(col_info.spans);
				return false;
			}

			row_group->type = BOX_TABLE_ROW_GROUP;

			if (child->prev == NULL)
				table->children = row_group;
			else
				child->prev->next = row_group;

			row_group->prev = child->prev;

			while (child != NULL && (
					child->type == BOX_BLOCK ||
					child->type == BOX_INLINE_CONTAINER ||
					child->type == BOX_TABLE ||
					child->type == BOX_TABLE_ROW ||
					child->type == BOX_TABLE_CELL)) {
				box_add_child(row_group, child);

				next_child = child->next;
				child->next = NULL;
				child = next_child;
			}

			assert(row_group->last != NULL);

			row_group->last->next = NULL;
			row_group->next = next_child = child;
			if (row_group->next != NULL)
				row_group->next->prev = row_group;
			else
				table->last = row_group;
			row_group->parent = table;

			if (box_normalise_table_row_group(row_group,
					&col_info, c) == false) {
				free(col_info.spans);
				return false;
			}
			break;
		case BOX_INLINE:
		case BOX_INLINE_END:
		case BOX_INLINE_BLOCK:
		case BOX_FLOAT_LEFT:
		case BOX_FLOAT_RIGHT:
		case BOX_BR:
		case BOX_TEXT:
			/* should have been wrapped in inline
			   container by convert_xml_to_box() */
			assert(0);
			break;
		default:
			fprintf(stderr, "%i\n", child->type);
			assert(0);
		}
	}

	table->columns = col_info.num_columns;
	table->rows = col_info.num_rows;

	if (table->children == NULL) {
		struct box *row;

#ifdef BOX_NORMALISE_DEBUG
		LOG(("table->children == 0, creating implied row"));
#endif

		assert(table->style != NULL);

		ctx.ctx = c->select_ctx;
		ctx.quirks = (c->quirks == DOM_DOCUMENT_QUIRKS_MODE_FULL);
		ctx.base_url = c->base_url;
		ctx.universal = c->universal;

		style = nscss_get_blank_style(&ctx, table->style);
		if (style == NULL) {
			free(col_info.spans);
			return false;
		}

		row_group = box_create(NULL, style, true, table->href,
				table->target, NULL, NULL, c->bctx);
		if (row_group == NULL) {
			css_computed_style_destroy(style);
			free(col_info.spans);
			return false;
		}
		row_group->type = BOX_TABLE_ROW_GROUP;

		style = nscss_get_blank_style(&ctx, row_group->style);
		if (style == NULL) {
			box_free(row_group);
			free(col_info.spans);
			return false;
		}

		row = box_create(NULL, style, true, row_group->href,
				row_group->target, NULL, NULL, c->bctx);
		if (row == NULL) {
			css_computed_style_destroy(style);
			box_free(row_group);
			free(col_info.spans);
			return false;
		}
		row->type = BOX_TABLE_ROW;

		row->parent = row_group;
		row_group->children = row_group->last = row;

		row_group->parent = table;
		table->children = table->last = row_group;

		table->rows = 1;
	}

	if (box_normalise_table_spans(table, col_info.spans, c) == false) {
		free(col_info.spans);
		return false;
	}

	free(col_info.spans);

	if (table_calculate_column_types(table) == false)
		return false;

#ifdef BOX_NORMALISE_DEBUG
	LOG(("table %p done", table));
#endif

	return true;
}


/**
 * Normalise table cell column/row counts for colspan/rowspan = 0.
 * Additionally, generate empty cells.
 *
 * \param table  Table to process
 * \param spans  Array of length table->columns for use in empty cell detection
 * \param c      Content containing table
 * \return True on success, false on memory exhaustion.
 */

bool box_normalise_table_spans(struct box *table, struct span_info *spans,
		html_content *c)
{
	struct box *table_row_group;
	struct box *table_row;
	struct box *table_cell;
	unsigned int rows_left = table->rows;
	unsigned int group_rows_left;
	unsigned int col;
	nscss_select_ctx ctx;

	/* Clear span data */
	memset(spans, 0, table->columns * sizeof(struct span_info));

	/* Scan table, filling in width and height of table cells with 
	 * colspan = 0 and rowspan = 0. Also generate empty cells */
	for (table_row_group = table->children;
	     table_row_group != NULL;
	     table_row_group = table_row_group->next) {

		group_rows_left = table_row_group->rows;

		for (table_row = table_row_group->children;
		     table_row != NULL;
		     table_row = table_row->next) {

			for (table_cell = table_row->children;
			     table_cell != NULL;
			     table_cell = table_cell->next) {

				/* colspan = 0 -> colspan = 1 */
				if (table_cell->columns == 0) {
					table_cell->columns = 1;
				}

				/* if rowspan is 0 it is expanded to
				 * the number of rows left in the row
				 * group
				 */
				if (table_cell->rows == 0) {
					table_cell->rows = group_rows_left;
				}

				/* limit rowspans within group */
				if (table_cell->rows > group_rows_left) {
					table_cell->rows = group_rows_left;
				}

				/* Record span information */
				for (col = table_cell->start_column;
						col < table_cell->start_column +
						table_cell->columns; col++) {
					spans[col].row_span = table_cell->rows;
				}
			}

			/* Reduce span count of each column */
			for (col = 0; col < table->columns; col++) {
				if (spans[col].row_span == 0) {
					unsigned int start = col;
					css_computed_style *style;
					struct box *cell, *prev;

					/* If it's already zero, then we need 
					 * to generate an empty cell for the 
					 * gap in the row that spans as many 
					 * columns as remain blank. 
					 */
					assert(table_row->style != NULL);

					/* Find width of gap */
					while (col < table->columns &&
							spans[col].row_span == 
							0) {
						col++;
					}

					ctx.ctx = c->select_ctx;
					ctx.quirks = (c->quirks == 
						DOM_DOCUMENT_QUIRKS_MODE_FULL);
					ctx.base_url = c->base_url;
					ctx.universal = c->universal;

					style = nscss_get_blank_style(&ctx, 
							table_row->style);
					if (style == NULL)
						return false;

					cell = box_create(NULL, style, true, 
							table_row->href, 
							table_row->target, 
							NULL, NULL, c->bctx);
					if (cell == NULL) {
						css_computed_style_destroy(
								style);
						return false;
					}
					cell->type = BOX_TABLE_CELL;

					cell->rows = 1;
					cell->columns = col - start;
					cell->start_column = start;

					/* Find place to insert cell */
					for (prev = table_row->children;
							prev != NULL;
							prev = prev->next) {
						if (prev->start_column + 
							prev->columns == 
								start)
							break;
						if (prev->next == NULL)
							break;
					}

					/* Insert it */
					if (prev == NULL) {
						if (table_row->children != NULL)
							table_row->children->
								prev = cell;
						else
							table_row->last = cell;

						cell->next = 
							table_row->children;
						table_row->children = cell;
					} else {
						if (prev->next != NULL)
							prev->next->prev = cell;
						else
							table_row->last = cell;

						cell->next = prev->next;
						prev->next = cell;
						cell->prev = prev;
					}
					cell->parent = table_row;
				} else {
					spans[col].row_span--;
				}
			}

			assert(rows_left > 0);

			rows_left--;
		}

		group_rows_left--;
	}

	return true;
}


bool box_normalise_table_row_group(struct box *row_group,
		struct columns *col_info,
		html_content * c)
{
	struct box *child;
	struct box *next_child;
	struct box *row;
	css_computed_style *style;
	nscss_select_ctx ctx;
	unsigned int group_row_count = 0;

	assert(row_group != 0);
	assert(row_group->type == BOX_TABLE_ROW_GROUP);

#ifdef BOX_NORMALISE_DEBUG
	LOG(("row_group %p", row_group));
#endif

	for (child = row_group->children; child != NULL; child = next_child) {
		next_child = child->next;

		switch (child->type) {
		case BOX_TABLE_ROW:
			/* ok */
			group_row_count++;
			if (box_normalise_table_row(child, col_info,
					c) == false)
				return false;
			break;
		case BOX_BLOCK:
		case BOX_INLINE_CONTAINER:
		case BOX_TABLE:
		case BOX_TABLE_ROW_GROUP:
		case BOX_TABLE_CELL:
			/* insert implied table row */
			assert(row_group->style != NULL);

			ctx.ctx = c->select_ctx;
			ctx.quirks = (c->quirks == DOM_DOCUMENT_QUIRKS_MODE_FULL);
			ctx.base_url = c->base_url;
			ctx.universal = c->universal;

			style = nscss_get_blank_style(&ctx, row_group->style);
			if (style == NULL)
				return false;

			row = box_create(NULL, style, true, row_group->href,
					row_group->target, NULL, NULL, c->bctx);
			if (row == NULL) {
				css_computed_style_destroy(style);
				return false;
			}
			row->type = BOX_TABLE_ROW;

			if (child->prev == NULL)
				row_group->children = row;
			else
				child->prev->next = row;

			row->prev = child->prev;

			while (child != NULL && (
					child->type == BOX_BLOCK ||
					child->type == BOX_INLINE_CONTAINER ||
					child->type == BOX_TABLE ||
					child->type == BOX_TABLE_ROW_GROUP ||
					child->type == BOX_TABLE_CELL)) {
				box_add_child(row, child);

				next_child = child->next;
				child->next = NULL;
				child = next_child;
			}

			assert(row->last != NULL);

			row->last->next = NULL;
			row->next = next_child = child;
			if (row->next != NULL)
				row->next->prev = row;
			else
				row_group->last = row;
			row->parent = row_group;

			group_row_count++;
			if (box_normalise_table_row(row, col_info,
					c) == false)
				return false;
			break;
		case BOX_INLINE:
		case BOX_INLINE_END:
		case BOX_INLINE_BLOCK:
		case BOX_FLOAT_LEFT:
		case BOX_FLOAT_RIGHT:
		case BOX_BR:
		case BOX_TEXT:
			/* should have been wrapped in inline
			   container by convert_xml_to_box() */
			assert(0);
			break;
		default:
			assert(0);
		}
	}

	if (row_group->children == NULL) {
#ifdef BOX_NORMALISE_DEBUG
		LOG(("row_group->children == 0, inserting implied row"));
#endif

		assert(row_group->style != NULL);

		ctx.ctx = c->select_ctx;
		ctx.quirks = (c->quirks == DOM_DOCUMENT_QUIRKS_MODE_FULL);
		ctx.base_url = c->base_url;
		ctx.universal = c->universal;

		style = nscss_get_blank_style(&ctx, row_group->style);
		if (style == NULL) {
			return false;
		}

		row = box_create(NULL, style, true, row_group->href,
				row_group->target, NULL, NULL, c->bctx);
		if (row == NULL) {
			css_computed_style_destroy(style);
			return false;
		}
		row->type = BOX_TABLE_ROW;

		row->parent = row_group;
		row_group->children = row_group->last = row;

		group_row_count = 1;

		/* Keep table's row count in sync */
		col_info->num_rows++;
	}

	row_group->rows = group_row_count;

#ifdef BOX_NORMALISE_DEBUG
	LOG(("row_group %p done", row_group));
#endif

	return true;
}


bool box_normalise_table_row(struct box *row,
		struct columns *col_info,
		html_content * c)
{
	struct box *child;
	struct box *next_child;
	struct box *cell = NULL;
	css_computed_style *style;
	unsigned int i;
	nscss_select_ctx ctx;

	assert(row != NULL);
	assert(row->type == BOX_TABLE_ROW);

#ifdef BOX_NORMALISE_DEBUG
	LOG(("row %p", row));
#endif

	for (child = row->children; child != NULL; child = next_child) {
		next_child = child->next;

		switch (child->type) {
		case BOX_TABLE_CELL:
			/* ok */
			if (box_normalise_block(child, c) == false)
				return false;
			cell = child;
			break;
		case BOX_BLOCK:
		case BOX_INLINE_CONTAINER:
		case BOX_TABLE:
		case BOX_TABLE_ROW_GROUP:
		case BOX_TABLE_ROW:
			/* insert implied table cell */
			assert(row->style != NULL);

			ctx.ctx = c->select_ctx;
			ctx.quirks = (c->quirks == DOM_DOCUMENT_QUIRKS_MODE_FULL);
			ctx.base_url = c->base_url;
			ctx.universal = c->universal;

			style = nscss_get_blank_style(&ctx, row->style);
			if (style == NULL)
				return false;

			cell = box_create(NULL, style, true, row->href,
					row->target, NULL, NULL, c->bctx);
			if (cell == NULL) {
				css_computed_style_destroy(style);
				return false;
			}
			cell->type = BOX_TABLE_CELL;

			if (child->prev == NULL)
				row->children = cell;
			else
				child->prev->next = cell;

			cell->prev = child->prev;

			while (child != NULL && (
					child->type == BOX_BLOCK ||
					child->type == BOX_INLINE_CONTAINER ||
					child->type == BOX_TABLE ||
					child->type == BOX_TABLE_ROW_GROUP ||
					child->type == BOX_TABLE_ROW)) {
				box_add_child(cell, child);

				next_child = child->next;
				child->next = NULL;
				child = next_child;
			}

			assert(cell->last != NULL);

			cell->last->next = NULL;
			cell->next = next_child = child;
			if (cell->next != NULL)
				cell->next->prev = cell;
			else
				row->last = cell;
			cell->parent = row;

			if (box_normalise_block(cell, c) == false)
				return false;
			break;
		case BOX_INLINE:
		case BOX_INLINE_END:
		case BOX_INLINE_BLOCK:
		case BOX_FLOAT_LEFT:
		case BOX_FLOAT_RIGHT:
		case BOX_BR:
		case BOX_TEXT:
			/* should have been wrapped in inline
			   container by convert_xml_to_box() */
			assert(0);
			break;
		default:
			assert(0);
		}

		if (calculate_table_row(col_info, cell->columns, cell->rows,
				&cell->start_column, cell) == false)
			return false;
	}


	/* Update row spanning details for all columns */
	for (i = 0; i < col_info->num_columns; i++) {
		if (col_info->spans[i].row_span != 0 && 
				col_info->spans[i].auto_row == false) {
			/* This cell spans rows, and is not an auto row.
			 * Reduce number of rows left to span */
			col_info->spans[i].row_span--;
		}
	}

	/* Reset current column for next row */
	col_info->current_column = 0;

	/* Increment row counter */
	col_info->num_rows++;

#ifdef BOX_NORMALISE_DEBUG
	LOG(("row %p done", row));
#endif

	return true;
}


/**
 * Compute the column index at which the current cell begins.
 * Additionally, update the column record to reflect row spanning.
 *
 * \param col_info      Column record
 * \param col_span      Number of columns that current cell spans
 * \param row_span      Number of rows that current cell spans
 * \param start_column  Pointer to location to receive column index
 * \param cell		Box for current table cell
 * \return  true on success, false on memory exhaustion
 */

bool calculate_table_row(struct columns *col_info,
		unsigned int col_span, unsigned int row_span,
		unsigned int *start_column, struct box *cell)
{
	unsigned int cell_start_col = col_info->current_column;
	unsigned int cell_end_col;
	unsigned int i;
	struct span_info *spans;
	struct box *rg = cell->parent->parent; /* Cell's row group */

	/* Skip columns with cells spanning from above */
	/* TODO: Need to ignore cells spanning from above that belong to
	 *       different row group.  We don't have that info here. */
	while (col_info->spans[cell_start_col].row_span != 0 &&
			col_info->spans[cell_start_col].rg == rg) {
		cell_start_col++;
	}

	/* Update current column with calculated start */
	col_info->current_column = cell_start_col;

	/* If this cell has a colspan of 0, then assume 1.
	 * No other browser supports colspan=0, anyway. */
	if (col_span == 0)
		col_span = 1;
 
	cell_end_col = cell_start_col + col_span;

	if (col_info->num_columns < cell_end_col) {
		/* It appears that this row has more columns than
		 * the maximum recorded for the table so far. 
		 * Allocate more span records. */
		spans = realloc(col_info->spans,
				sizeof *spans * (cell_end_col + 1));
		if (spans == NULL)
			return false;

		col_info->spans = spans;
		col_info->num_columns = cell_end_col;

		/* Mark new final column as sentinel */
		col_info->spans[cell_end_col].row_span = 0;
		col_info->spans[cell_end_col].auto_row = false;
	}

	/* This cell may span multiple columns. If it also wants to span 
	 * multiple rows, temporarily assume it spans 1 row only. This will
	 * be fixed up in box_normalise_table_spans() */
	for (i = cell_start_col; i < cell_end_col; i++) {
		col_info->spans[i].row_span = (row_span == 0) ? 1 : row_span;
		col_info->spans[i].auto_row = (row_span == 0);
		col_info->spans[i].rg = rg;
	}

	/* Update current column with calculated end. */
	col_info->current_column = cell_end_col;

	*start_column = cell_start_col;

	return true;
}


bool box_normalise_inline_container(struct box *cont, html_content * c)
{
	struct box *child;
	struct box *next_child;

	assert(cont != NULL);
	assert(cont->type == BOX_INLINE_CONTAINER);

#ifdef BOX_NORMALISE_DEBUG
	LOG(("cont %p", cont));
#endif

	for (child = cont->children; child != NULL; child = next_child) {
		next_child = child->next;
		switch (child->type) {
		case BOX_INLINE:
		case BOX_INLINE_END:
		case BOX_BR:
		case BOX_TEXT:
			/* ok */
			break;
		case BOX_INLINE_BLOCK:
			/* ok */
			if (box_normalise_block(child, c) == false)
				return false;
			break;
		case BOX_FLOAT_LEFT:
		case BOX_FLOAT_RIGHT:
			/* ok */
			assert(child->children != NULL);

			switch (child->children->type) {
			case BOX_BLOCK:
				if (box_normalise_block(child->children,
						c) == false)
					return false;
				break;
			case BOX_TABLE:
				if (box_normalise_table(child->children,
						c) == false)
					return false;
				break;
			default:
				assert(0);
			}

			if (child->children == NULL) {
				/* the child has destroyed itself: remove float */
				if (child->prev == NULL)
					child->parent->children = child->next;
				else
					child->prev->next = child->next;
				if (child->next != NULL)
					child->next->prev = child->prev;
				else
					child->parent->last = child->prev;

				box_free(child);
			}
			break;
		case BOX_BLOCK:
		case BOX_INLINE_CONTAINER:
		case BOX_TABLE:
		case BOX_TABLE_ROW_GROUP:
		case BOX_TABLE_ROW:
		case BOX_TABLE_CELL:
		default:
			assert(0);
		}
	}

#ifdef BOX_NORMALISE_DEBUG
	LOG(("cont %p done", cont));
#endif

	return true;
}
