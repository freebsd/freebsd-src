/*
 * wpa_gui - ScanResultsItem class
 * Copyright (c) 2015, Adrian Nowicki <adinowicki@gmail.com>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "scanresultsitem.h"

bool ScanResultsItem::operator< (const QTreeWidgetItem &other) const
{
	int sortCol = treeWidget()->sortColumn();
	if (sortCol == 2 || sortCol == 3) {
		return text(sortCol).toInt() < other.text(sortCol).toInt();
	}
	return text(sortCol) < other.text(sortCol);
}
