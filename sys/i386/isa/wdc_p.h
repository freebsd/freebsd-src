/*
 *
 * Copyright (c) 1996 Wolfgang Helbig <helbig@ba-stuttgart.de>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Absolutely no warranty of function or purpose is made by the author.
 * 4. Modifications may be freely made to this file if the above conditions
 *    are met.
 *
 *	$Id$
 */

#define Q_CMD640B	0x00000001 /* CMD640B quirk: serialize IDE channels */

void wdc_pci(int quirks);
