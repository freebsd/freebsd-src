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

#import "cocoa/ScrollableView.h"

@class LocalHistoryController;

@interface BrowserView : ScrollableView <NSTextInput> {
	struct browser_window *browser;
	
	NSPoint caretPoint;
	CGFloat caretHeight;
	BOOL caretVisible;
	BOOL hasCaret;
	NSTimer *caretTimer;
	
	BOOL isDragging;
	NSPoint dragStart;
	
	BOOL historyVisible;
	LocalHistoryController *history;
	
	NSString *markedText;
}

@property (readwrite, assign, nonatomic) struct browser_window *browser;
@property (readwrite, retain, nonatomic) NSTimer *caretTimer;
@property (readwrite, assign, nonatomic, getter=isHistoryVisible) BOOL historyVisible;

- (void) removeCaret;
- (void) addCaretAt: (NSPoint) point height: (CGFloat) height;

- (void) reformat;
- (void) updateHistory;

@end
