//
//  PSMTabBarCell.m
//  PSMTabBarControl
//
//  Created by John Pannell on 10/13/05.
//  Copyright 2005 Positive Spin Media. All rights reserved.
//

#import "PSMTabBarCell.h"
#import "PSMTabBarControl.h"
#import "PSMTabStyle.h"
#import "PSMProgressIndicator.h"
#import "PSMTabDragAssistant.h"

@interface PSMTabBarControl (Private)
- (void)update;
@end

@implementation PSMTabBarCell

#pragma mark -
#pragma mark Creation/Destruction
- (id)initWithControlView:(PSMTabBarControl *)controlView {
	if((self = [super init])) {
		_controlView = controlView;
		_closeButtonTrackingTag = 0;
		_cellTrackingTag = 0;
		_closeButtonOver = NO;
		_closeButtonPressed = NO;
		_indicator = [[PSMProgressIndicator alloc] initWithFrame:NSMakeRect(0.0, 0.0, kPSMTabBarIndicatorWidth, kPSMTabBarIndicatorWidth)];
		[_indicator setStyle:NSProgressIndicatorSpinningStyle];
		[_indicator setAutoresizingMask:NSViewMinYMargin];
		_hasCloseButton = YES;
		_isCloseButtonSuppressed = NO;
		_count = 0;
		_countColor = nil;
		_isEdited = NO;
		_isPlaceholder = NO;
	}
	return self;
}

- (id)initPlaceholderWithFrame:(NSRect)frame expanded:(BOOL)value inControlView:(PSMTabBarControl *)controlView {
	if((self = [super init])) {
		_controlView = controlView;
		_isPlaceholder = YES;
		if(!value) {
			if([controlView orientation] == PSMTabBarHorizontalOrientation) {
				frame.size.width = 0.0;
			} else {
				frame.size.height = 0.0;
			}
		}
		[self setFrame:frame];
		_closeButtonTrackingTag = 0;
		_cellTrackingTag = 0;
		_closeButtonOver = NO;
		_closeButtonPressed = NO;
		_indicator = nil;
		_hasCloseButton = YES;
		_isCloseButtonSuppressed = NO;
		_count = 0;
		_countColor = nil;
		_isEdited = NO;

		if(value) {
			[self setCurrentStep:(kPSMTabDragAnimationSteps - 1)];
		} else {
			[self setCurrentStep:0];
		}
	}
	return self;
}

- (void)dealloc {
	[_countColor release];

	[_indicator removeFromSuperviewWithoutNeedingDisplay];

	[_indicator release];
	[super dealloc];
}

#pragma mark -
#pragma mark Accessors

- (id)controlView {
	return _controlView;
}

- (void)setControlView:(id)view {
	// no retain release pattern, as this simply switches a tab to another view.
	_controlView = view;
}

- (NSTrackingRectTag)closeButtonTrackingTag {
	return _closeButtonTrackingTag;
}

- (void)setCloseButtonTrackingTag:(NSTrackingRectTag)tag {
	_closeButtonTrackingTag = tag;
}

- (NSTrackingRectTag)cellTrackingTag {
	return _cellTrackingTag;
}

- (void)setCellTrackingTag:(NSTrackingRectTag)tag {
	_cellTrackingTag = tag;
}

- (CGFloat)width {
	return _frame.size.width;
}

- (NSRect)frame {
	return _frame;
}

- (void)setFrame:(NSRect)rect {
	_frame = rect;

	//move the status indicator along with the rest of the cell
	if(![[self indicator] isHidden] && ![_controlView isTabBarHidden]) {
		[[self indicator] setFrame:[self indicatorRectForFrame:rect]];
	}
}

- (void)setStringValue:(NSString *)aString {
	[super setStringValue:aString];
	_stringSize = [[self attributedStringValue] size];
	// need to redisplay now - binding observation was too quick.
	[_controlView update];
}

- (NSSize)stringSize {
	return _stringSize;
}

- (NSAttributedString *)attributedStringValue {
	return [(id < PSMTabStyle >)[_controlView style] attributedStringValueForTabCell:self];
}

- (NSInteger)tabState {
	return _tabState;
}

- (void)setTabState:(NSInteger)state {
	_tabState = state;
}

- (NSProgressIndicator *)indicator {
	return _indicator;
}

- (BOOL)isInOverflowMenu {
	return _isInOverflowMenu;
}

- (void)setIsInOverflowMenu:(BOOL)value {
	if(_isInOverflowMenu != value) {
		_isInOverflowMenu = value;
		if([[[self controlView] delegate] respondsToSelector:@selector(tabView:tabViewItem:isInOverflowMenu:)]) {
			[[[self controlView] delegate] tabView:[self controlView] tabViewItem:[self representedObject] isInOverflowMenu:_isInOverflowMenu];
		}
	}
}

- (BOOL)closeButtonPressed {
	return _closeButtonPressed;
}

- (void)setCloseButtonPressed:(BOOL)value {
	_closeButtonPressed = value;
}

- (BOOL)closeButtonOver {
	return(_closeButtonOver && ([_controlView allowsBackgroundTabClosing] || ([self tabState] & PSMTab_SelectedMask) || [[NSApp currentEvent] modifierFlags] & NSCommandKeyMask));
}

- (void)setCloseButtonOver:(BOOL)value {
	_closeButtonOver = value;
}

- (BOOL)hasCloseButton {
	return _hasCloseButton;
}

- (void)setHasCloseButton:(BOOL)set;
{
	_hasCloseButton = set;
}

- (void)setCloseButtonSuppressed:(BOOL)suppress;
{
	_isCloseButtonSuppressed = suppress;
}

- (BOOL)isCloseButtonSuppressed;
{
	return _isCloseButtonSuppressed;
}

- (BOOL)hasIcon {
	return _hasIcon;
}

- (void)setHasIcon:(BOOL)value {
	_hasIcon = value;
	//[_controlView update:[[self controlView] automaticallyAnimates]]; // binding notice is too fast
}

- (BOOL)hasLargeImage {
	return _hasLargeImage;
}

- (void)setHasLargeImage:(BOOL)value {
	_hasLargeImage = value;
}


- (NSInteger)count {
	return _count;
}

- (void)setCount:(NSInteger)value {
	_count = value;
	//[_controlView update:[[self controlView] automaticallyAnimates]]; // binding notice is too fast
}

- (NSColor *)countColor {
	return _countColor;
}

- (void)setCountColor:(NSColor *)color {
	[_countColor release];
	_countColor = [color retain];
}

- (BOOL)isPlaceholder {
	return _isPlaceholder;
}

- (void)setIsPlaceholder:(BOOL)value;
{
	_isPlaceholder = value;
}

- (NSInteger)currentStep {
	return _currentStep;
}

- (void)setCurrentStep:(NSInteger)value {
	if(value < 0) {
		value = 0;
	}

	if(value > (kPSMTabDragAnimationSteps - 1)) {
		value = (kPSMTabDragAnimationSteps - 1);
	}

	_currentStep = value;
}

- (BOOL)isEdited {
	return _isEdited;
}

- (void)setIsEdited:(BOOL)value {
	_isEdited = value;
	//[_controlView update:[[self controlView] automaticallyAnimates]]; // binding notice is too fast
}

#pragma mark -
#pragma mark Bindings

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary *)change context:(void *)context {
	// the progress indicator, label, icon, or count has changed - redraw the control view
	//[_controlView update];
	//I seem to have run into some odd issue with update not being called at the right time. This seems to avoid the problem.
	[_controlView performSelector:@selector(update) withObject:nil afterDelay:0.0];
}

#pragma mark -
#pragma mark Component Attributes

- (NSRect)indicatorRectForFrame:(NSRect)cellFrame {
	return [(id < PSMTabStyle >)[_controlView style] indicatorRectForTabCell:self];
}

- (NSRect)closeButtonRectForFrame:(NSRect)cellFrame {
	return [(id < PSMTabStyle >)[_controlView style] closeButtonRectForTabCell:self withFrame:cellFrame];
}

- (CGFloat)minimumWidthOfCell {
	return [(id < PSMTabStyle >)[_controlView style] minimumWidthOfTabCell:self];
}

- (CGFloat)desiredWidthOfCell {
	return [(id < PSMTabStyle >)[_controlView style] desiredWidthOfTabCell:self];
}

#pragma mark -
#pragma mark Drawing

- (void)drawWithFrame:(NSRect)cellFrame inView:(NSView *)controlView {
	if(_isPlaceholder) {
		[[NSColor colorWithCalibratedWhite:0.0 alpha:0.2] set];
		NSRectFillUsingOperation(cellFrame, NSCompositeSourceAtop);
		return;
	}

	[(id < PSMTabStyle >)[_controlView style] drawTabCell:self];
}

#pragma mark -
#pragma mark Tracking

- (void)mouseEntered:(NSEvent *)theEvent {
	// check for which tag
	if([theEvent trackingNumber] == _closeButtonTrackingTag) {
		_closeButtonOver = YES;
	}
	if([theEvent trackingNumber] == _cellTrackingTag) {
		[self setHighlighted:YES];
		[_controlView setNeedsDisplay:NO];
	}

	// scrubtastic
	if([_controlView allowsScrubbing] && ([theEvent modifierFlags] & NSAlternateKeyMask)) {
		[_controlView performSelector:@selector(tabClick:) withObject:self];
	}

	// tell the control we only need to redraw the affected tab
	[_controlView setNeedsDisplayInRect:NSInsetRect([self frame], -2, -2)];
}

- (void)mouseExited:(NSEvent *)theEvent {
	// check for which tag
	if([theEvent trackingNumber] == _closeButtonTrackingTag) {
		_closeButtonOver = NO;
	}

	if([theEvent trackingNumber] == _cellTrackingTag) {
		[self setHighlighted:NO];
		[_controlView setNeedsDisplay:NO];
	}

	//tell the control we only need to redraw the affected tab
	[_controlView setNeedsDisplayInRect:NSInsetRect([self frame], -2, -2)];
}

#pragma mark -
#pragma mark Drag Support

- (NSImage *)dragImage {
	NSRect cellFrame = [(id < PSMTabStyle >)[(PSMTabBarControl *)_controlView style] dragRectForTabCell:self orientation:(PSMTabBarOrientation)[(PSMTabBarControl *)_controlView orientation]];
	//NSRect cellFrame = [self frame];

	[_controlView lockFocus];
	NSBitmapImageRep *rep = [[[NSBitmapImageRep alloc] initWithFocusedViewRect:cellFrame] autorelease];
	[_controlView unlockFocus];
	NSImage *image = [[[NSImage alloc] initWithSize:[rep size]] autorelease];
	[image addRepresentation:rep];
	NSImage *returnImage = [[[NSImage alloc] initWithSize:[rep size]] autorelease];
	[returnImage lockFocus];
	[image compositeToPoint:NSMakePoint(0.0, 0.0) operation:NSCompositeSourceOver fraction:1.0];
	[returnImage unlockFocus];
	if(![[self indicator] isHidden]) {
		NSImage *pi = [[NSImage alloc] initByReferencingFile:[[PSMTabBarControl bundle] pathForImageResource:@"pi"]];
		[returnImage lockFocus];
		NSPoint indicatorPoint = NSMakePoint([self frame].size.width - MARGIN_X - kPSMTabBarIndicatorWidth, MARGIN_Y);
		[pi compositeToPoint:indicatorPoint operation:NSCompositeSourceOver fraction:1.0];
		[returnImage unlockFocus];
		[pi release];
	}
	return returnImage;
}

#pragma mark -
#pragma mark Archiving

- (void)encodeWithCoder:(NSCoder *)aCoder {
	[super encodeWithCoder:aCoder];
	if([aCoder allowsKeyedCoding]) {
		[aCoder encodeRect:_frame forKey:@"frame"];
		[aCoder encodeSize:_stringSize forKey:@"stringSize"];
		[aCoder encodeInteger:_currentStep forKey:@"currentStep"];
		[aCoder encodeBool:_isPlaceholder forKey:@"isPlaceholder"];
		[aCoder encodeInteger:_tabState forKey:@"tabState"];
		[aCoder encodeInteger:_closeButtonTrackingTag forKey:@"closeButtonTrackingTag"];
		[aCoder encodeInteger:_cellTrackingTag forKey:@"cellTrackingTag"];
		[aCoder encodeBool:_closeButtonOver forKey:@"closeButtonOver"];
		[aCoder encodeBool:_closeButtonPressed forKey:@"closeButtonPressed"];
		[aCoder encodeObject:_indicator forKey:@"indicator"];
		[aCoder encodeBool:_isInOverflowMenu forKey:@"isInOverflowMenu"];
		[aCoder encodeBool:_hasCloseButton forKey:@"hasCloseButton"];
		[aCoder encodeBool:_isCloseButtonSuppressed forKey:@"isCloseButtonSuppressed"];
		[aCoder encodeBool:_hasIcon forKey:@"hasIcon"];
		[aCoder encodeBool:_hasLargeImage forKey:@"hasLargeImage"];
		[aCoder encodeInteger:_count forKey:@"count"];
		[aCoder encodeBool:_isEdited forKey:@"isEdited"];
	}
}

- (id)initWithCoder:(NSCoder *)aDecoder {
	self = [super initWithCoder:aDecoder];
	if(self) {
		if([aDecoder allowsKeyedCoding]) {
			_frame = [aDecoder decodeRectForKey:@"frame"];
			_stringSize = [aDecoder decodeSizeForKey:@"stringSize"];
			_currentStep = [aDecoder decodeIntegerForKey:@"currentStep"];
			_isPlaceholder = [aDecoder decodeBoolForKey:@"isPlaceholder"];
			_tabState = [aDecoder decodeIntegerForKey:@"tabState"];
			_closeButtonTrackingTag = [aDecoder decodeIntegerForKey:@"closeButtonTrackingTag"];
			_cellTrackingTag = [aDecoder decodeIntegerForKey:@"cellTrackingTag"];
			_closeButtonOver = [aDecoder decodeBoolForKey:@"closeButtonOver"];
			_closeButtonPressed = [aDecoder decodeBoolForKey:@"closeButtonPressed"];
			_indicator = [[aDecoder decodeObjectForKey:@"indicator"] retain];
			_isInOverflowMenu = [aDecoder decodeBoolForKey:@"isInOverflowMenu"];
			_hasCloseButton = [aDecoder decodeBoolForKey:@"hasCloseButton"];
			_isCloseButtonSuppressed = [aDecoder decodeBoolForKey:@"isCloseButtonSuppressed"];
			_hasIcon = [aDecoder decodeBoolForKey:@"hasIcon"];
			_hasLargeImage = [aDecoder decodeBoolForKey:@"hasLargeImage"];
			_count = [aDecoder decodeIntegerForKey:@"count"];
			_isEdited = [aDecoder decodeBoolForKey:@"isEdited"];
		}
	}
	return self;
}

#pragma mark -
#pragma mark Accessibility

-(BOOL)accessibilityIsIgnored {
	return NO;
}

- (id)accessibilityAttributeValue:(NSString *)attribute {
	id attributeValue = nil;

	if([attribute isEqualToString: NSAccessibilityRoleAttribute]) {
		attributeValue = NSAccessibilityButtonRole;
	} else if([attribute isEqualToString: NSAccessibilityHelpAttribute]) {
		if([[[self controlView] delegate] respondsToSelector:@selector(accessibilityStringForTabView:objectCount:)]) {
			attributeValue = [NSString stringWithFormat:@"%@, %lu %@", [self stringValue],
							  (unsigned long)[self count],
							  [[[self controlView] delegate] accessibilityStringForTabView:[[self controlView] tabView] objectCount:[self count]]];
		} else {
			attributeValue = [self stringValue];
		}
	} else if([attribute isEqualToString: NSAccessibilityFocusedAttribute]) {
		attributeValue = [NSNumber numberWithBool:([self tabState] == 2)];
	} else {
		attributeValue = [super accessibilityAttributeValue:attribute];
	}

	return attributeValue;
}

- (NSArray *)accessibilityActionNames {
	static NSArray *actions;

	if(!actions) {
		actions = [[NSArray alloc] initWithObjects:NSAccessibilityPressAction, nil];
	}
	return actions;
}

- (NSString *)accessibilityActionDescription:(NSString *)action {
	return NSAccessibilityActionDescription(action);
}

- (void)accessibilityPerformAction:(NSString *)action {
	if([action isEqualToString:NSAccessibilityPressAction]) {
		// this tab was selected
		[_controlView performSelector:@selector(tabClick:) withObject:self];
	}
}

- (id)accessibilityHitTest:(NSPoint)point {
	return NSAccessibilityUnignoredAncestor(self);
}

- (id)accessibilityFocusedUIElement:(NSPoint)point {
	return NSAccessibilityUnignoredAncestor(self);
}

@end
