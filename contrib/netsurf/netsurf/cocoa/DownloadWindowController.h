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

struct gui_download_table *cocoa_download_table;

@interface DownloadWindowController : NSWindowController {
	struct download_context *context;
	unsigned long totalSize;
	unsigned long receivedSize;
	
	NSURL *url;
	NSString *mimeType;
	NSURL *saveURL;
	NSFileHandle *outputFile;
	NSMutableData *savedData;
	NSDate *startDate;
	
	BOOL canClose;
	BOOL shouldClose;
}

@property (readwrite, copy, nonatomic) NSURL *URL;
@property (readwrite, copy, nonatomic) NSString *MIMEType;
@property (readwrite, assign, nonatomic) unsigned long totalSize;
@property (readwrite, copy, nonatomic) NSURL *saveURL;
@property (readwrite, assign, nonatomic) unsigned long receivedSize;

@property (readonly, nonatomic) NSString *fileName;
@property (readonly, nonatomic) NSImage *icon;
@property (readonly, nonatomic) NSString *statusText;

- initWithContext: (struct download_context *)ctx;

- (void) abort;

@end

