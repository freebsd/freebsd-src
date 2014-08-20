//
//  PSMTabBarController.h
//  PSMTabBarControl
//
//  Created by Kent Sutherland on 11/24/06.
//  Copyright 2006 Kent Sutherland. All rights reserved.
//

#import <Cocoa/Cocoa.h>

@class PSMTabBarControl, PSMTabBarCell;

@interface PSMTabBarController : NSObject
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 1060
	<NSMenuDelegate>
#endif
{
	PSMTabBarControl						*_control;
	NSMutableArray						*_cellTrackingRects;
	NSMutableArray						*_closeButtonTrackingRects;
	NSMutableArray						*_cellFrames;
	NSRect									_addButtonRect;
	NSMenu									*_overflowMenu;
}

- (id)initWithTabBarControl:(PSMTabBarControl *)control;

- (NSRect)addButtonRect;
- (NSMenu *)overflowMenu;
- (NSRect)cellTrackingRectAtIndex:(NSUInteger)index;
- (NSRect)closeButtonTrackingRectAtIndex:(NSUInteger)index;
- (NSRect)cellFrameAtIndex:(NSUInteger)index;

- (void)setSelectedCell:(PSMTabBarCell *)cell;

- (void)layoutCells;

@end
