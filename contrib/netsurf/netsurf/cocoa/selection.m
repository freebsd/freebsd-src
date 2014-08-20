/*
 * Copyright 2011 Sven Weidauer <sven.weidauer@gmail.com>
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

#import <Cocoa/Cocoa.h>

#import "cocoa/BrowserViewController.h"
#import "cocoa/selection.h"

#import "desktop/gui.h"
#import "desktop/browser_private.h"


static NSMutableString *cocoa_clipboard_string;

/**
 * Core asks front end for clipboard contents.
 *
 * \param  buffer  UTF-8 text, allocated by front end, ownership yeilded to core
 * \param  length  Byte length of UTF-8 text in buffer
 */
static void gui_get_clipboard(char **buffer, size_t *length)
{
	NSPasteboard *pb = [NSPasteboard generalPasteboard];
	NSString *string = [pb stringForType: NSStringPboardType];

	*buffer = NULL;
	*length = 0;

	if (string) {
		const char *text = [string UTF8String];
		*length = strlen(text);

		*buffer = malloc(*length);

		if (*buffer != NULL) {
			memcpy(*buffer, text, *length);
		} else {
			*length = 0;
		}
	}
}

/**
 * Core tells front end to put given text in clipboard
 *
 * \param  buffer    UTF-8 text, owned by core
 * \param  length    Byte length of UTF-8 text in buffer
 * \param  styles    Array of styles given to text runs, owned by core, or NULL
 * \param  n_styles  Number of text run styles in array
 */
static void gui_set_clipboard(const char *buffer, size_t length,
		nsclipboard_styles styles[], int n_styles)
{
	/* Empty clipboard string */
	if (nil == cocoa_clipboard_string) {
		cocoa_clipboard_string = [[NSMutableString alloc] init];
	} else {
		[cocoa_clipboard_string setString: @""];
	}

	/* Add text to clipboard string */
	if (nil == cocoa_clipboard_string) return;
	
	[cocoa_clipboard_string appendString: [[[NSString alloc] 
				initWithBytes: buffer
				length: length
				encoding: NSUTF8StringEncoding] 
				autorelease]];
	
	/* Stick it on the pasteboard */
	NSPasteboard *pb = [NSPasteboard generalPasteboard];
	[pb declareTypes: [NSArray arrayWithObject: NSStringPboardType] owner: nil];
	bool result = [pb setString: cocoa_clipboard_string forType: NSStringPboardType];

	if (result) {
		/* Empty clipboard string */
		if (nil == cocoa_clipboard_string) {
			cocoa_clipboard_string = [[NSMutableString alloc] init];
		} else {
			[cocoa_clipboard_string setString: @""];
		}
	}
}

static struct gui_clipboard_table clipboard_table = {
	.get = gui_get_clipboard,
	.set = gui_set_clipboard,
};

struct gui_clipboard_table *cocoa_clipboard_table = &clipboard_table;
