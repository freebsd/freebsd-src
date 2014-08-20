//
//  PSMTabStyle.h
//  PSMTabBarControl
//
//  Created by John Pannell on 2/17/06.
//  Copyright 2006 Positive Spin Media. All rights reserved.
//

/*
   Protocol to be observed by all style delegate objects.  These objects handle the drawing responsibilities for PSMTabBarCell; once the control has been assigned a style, the background and cells draw consistent with that style.  Design pattern and implementation by David Smith, Seth Willits, and Chris Forsythe, all touch up and errors by John P. :-)
 */

#import "PSMTabBarCell.h"
#import "PSMTabBarControl.h"

@protocol PSMTabStyle <NSObject>

// identity
- (NSString *)name;

// control specific parameters
- (CGFloat)leftMarginForTabBarControl;
- (CGFloat)rightMarginForTabBarControl;
- (CGFloat)topMarginForTabBarControl;
- (void)setOrientation:(PSMTabBarOrientation)value;

// add tab button
- (NSImage *)addTabButtonImage;
- (NSImage *)addTabButtonPressedImage;
- (NSImage *)addTabButtonRolloverImage;

// cell specific parameters
- (NSRect)dragRectForTabCell:(PSMTabBarCell *)cell orientation:(PSMTabBarOrientation)orientation;
- (NSRect)closeButtonRectForTabCell:(PSMTabBarCell *)cell withFrame:(NSRect)cellFrame;
- (NSRect)iconRectForTabCell:(PSMTabBarCell *)cell;
- (NSRect)indicatorRectForTabCell:(PSMTabBarCell *)cell;
- (NSRect)objectCounterRectForTabCell:(PSMTabBarCell *)cell;
- (CGFloat)minimumWidthOfTabCell:(PSMTabBarCell *)cell;
- (CGFloat)desiredWidthOfTabCell:(PSMTabBarCell *)cell;
- (CGFloat)tabCellHeight;

// cell values
- (NSAttributedString *)attributedObjectCountValueForTabCell:(PSMTabBarCell *)cell;
- (NSAttributedString *)attributedStringValueForTabCell:(PSMTabBarCell *)cell;

// drawing
- (void)drawTabCell:(PSMTabBarCell *)cell;
- (void)drawBackgroundInRect:(NSRect)rect;
- (void)drawTabBar:(PSMTabBarControl *)bar inRect:(NSRect)rect;

@end

@interface PSMTabBarControl (StyleAccessors)

- (NSMutableArray *)cells;

@end
