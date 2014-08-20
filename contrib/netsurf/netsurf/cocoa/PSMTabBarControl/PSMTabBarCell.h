//
//  PSMTabBarCell.h
//  PSMTabBarControl
//
//  Created by John Pannell on 10/13/05.
//  Copyright 2005 Positive Spin Media. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#import "PSMTabBarControl.h"
#import "PSMProgressIndicator.h"


@interface PSMTabBarCell : NSActionCell {
	// sizing
	NSRect									_frame;
	NSSize									_stringSize;
	NSInteger								_currentStep;
	BOOL									_isPlaceholder;

	// state
	NSInteger								_tabState;
	NSTrackingRectTag						_closeButtonTrackingTag;				// left side tracking, if dragging
	NSTrackingRectTag						_cellTrackingTag;							// right side tracking, if dragging
	BOOL									_closeButtonOver;
	BOOL									_closeButtonPressed;
	PSMProgressIndicator					*_indicator;
	BOOL									_isInOverflowMenu;
	BOOL									_hasCloseButton;
	BOOL									_isCloseButtonSuppressed;
	BOOL									_hasIcon;
	BOOL									_hasLargeImage;
	NSInteger								_count;
	NSColor								*_countColor;
	BOOL									_isEdited;
}

// creation/destruction
- (id)initWithControlView:(PSMTabBarControl *)controlView;
- (id)initPlaceholderWithFrame:(NSRect) frame expanded:(BOOL) value inControlView:(PSMTabBarControl *)controlView;
- (void)dealloc;

// accessors
- (id)controlView;
- (void)setControlView:(id)view;
- (NSTrackingRectTag)closeButtonTrackingTag;
- (void)setCloseButtonTrackingTag:(NSTrackingRectTag)tag;
- (NSTrackingRectTag)cellTrackingTag;
- (void)setCellTrackingTag:(NSTrackingRectTag)tag;
- (CGFloat)width;
- (NSRect)frame;
- (void)setFrame:(NSRect)rect;
- (void)setStringValue:(NSString *)aString;
- (NSSize)stringSize;
- (NSAttributedString *)attributedStringValue;
- (NSInteger)tabState;
- (void)setTabState:(NSInteger)state;
- (NSProgressIndicator *)indicator;
- (BOOL)isInOverflowMenu;
- (void)setIsInOverflowMenu:(BOOL)value;
- (BOOL)closeButtonPressed;
- (void)setCloseButtonPressed:(BOOL)value;
- (BOOL)closeButtonOver;
- (void)setCloseButtonOver:(BOOL)value;
- (BOOL)hasCloseButton;
- (void)setHasCloseButton:(BOOL)set;
- (void)setCloseButtonSuppressed:(BOOL)suppress;
- (BOOL)isCloseButtonSuppressed;
- (BOOL)hasIcon;
- (void)setHasIcon:(BOOL)value;
- (BOOL)hasLargeImage;
- (void)setHasLargeImage:(BOOL)value;
- (NSInteger)count;
- (void)setCount:(NSInteger)value;
- (NSColor *)countColor;
- (void)setCountColor:(NSColor *)value;
- (BOOL)isPlaceholder;
- (void)setIsPlaceholder:(BOOL)value;
- (NSInteger)currentStep;
- (void)setCurrentStep:(NSInteger)value;
- (BOOL)isEdited;
- (void)setIsEdited:(BOOL)value;

// component attributes
- (NSRect)indicatorRectForFrame:(NSRect)cellFrame;
- (NSRect)closeButtonRectForFrame:(NSRect)cellFrame;
- (CGFloat)minimumWidthOfCell;
- (CGFloat)desiredWidthOfCell;

// drawing
- (void)drawWithFrame:(NSRect) cellFrame inView:(NSView *)controlView;

// tracking the mouse
- (void)mouseEntered:(NSEvent *)theEvent;
- (void)mouseExited:(NSEvent *)theEvent;

// drag support
- (NSImage *)dragImage;

// archiving
- (void)encodeWithCoder:(NSCoder *)aCoder;
- (id)initWithCoder:(NSCoder *)aDecoder;

@end

@interface PSMTabBarControl (CellAccessors)

- (id<PSMTabStyle>)style;

@end

@interface NSObject (IdentifierAccesors)

- (NSImage *)largeImage;

@end
