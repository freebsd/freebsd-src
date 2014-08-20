//
//  PSMTabBarController.m
//  PSMTabBarControl
//
//  Created by Kent Sutherland on 11/24/06.
//  Copyright 2006 Kent Sutherland. All rights reserved.
//

#import "PSMTabBarController.h"
#import "PSMTabBarControl.h"
#import "PSMTabBarCell.h"
#import "PSMTabStyle.h"
#import "NSString_AITruncation.h"

#define MAX_OVERFLOW_MENUITEM_TITLE_LENGTH      60

@interface PSMTabBarController (Private)
- (NSArray *)_generateWidthsFromCells:(NSArray *)cells;
- (void)_setupCells:(NSArray *)cells withWidths:(NSArray *)widths;
@end

@implementation PSMTabBarController

/*!
    @method     initWithTabBarControl:
    @abstract   Creates a new PSMTabBarController instance.
    @discussion Creates a new PSMTabBarController for controlling a PSMTabBarControl. Should only be called by
                PSMTabBarControl.
    @param      A PSMTabBarControl.
    @returns    A newly created PSMTabBarController instance.
 */

- (id)initWithTabBarControl:(PSMTabBarControl *)control {
	if((self = [super init])) {
		_control = control;
		_cellTrackingRects = [[NSMutableArray alloc] init];
		_closeButtonTrackingRects = [[NSMutableArray alloc] init];
		_cellFrames = [[NSMutableArray alloc] init];
		_addButtonRect = NSZeroRect;
	}
	return self;
}

- (void)dealloc {
	[_cellTrackingRects release];
	[_closeButtonTrackingRects release];
	[_cellFrames release];
	[super dealloc];
}

/*!
    @method     addButtonRect
    @abstract   Returns the position for the add tab button.
    @discussion Returns the position for the add tab button.
    @returns    The rect  for the add button rect.
 */

- (NSRect)addButtonRect {
	return _addButtonRect;
}

/*!
    @method     overflowMenu
    @abstract   Returns current overflow menu or nil if there is none.
    @discussion Returns current overflow menu or nil if there is none.
    @returns    The current overflow menu.
 */

- (NSMenu *)overflowMenu {
	return _overflowMenu;
}

/*!
    @method     cellTrackingRectAtIndex:
    @abstract   Returns the rect for the tracking rect at the requested index.
    @discussion Returns the rect for the tracking rect at the requested index.
    @param      Index of a cell.
    @returns    The tracking rect of the cell at the requested index.
 */

- (NSRect)cellTrackingRectAtIndex:(NSUInteger)index {
	NSRect rect;
	if(index < [_cellTrackingRects count]) {
		rect = [[_cellTrackingRects objectAtIndex:index] rectValue];
	} else {
		NSLog(@"cellTrackingRectAtIndex: Invalid index (%ld)", (long)index);
		rect = NSZeroRect;
	}
	return rect;
}

/*!
    @method     closeButtonTrackingRectAtIndex:
    @abstract   Returns the tracking rect for the close button at the requested index.
    @discussion Returns the tracking rect for the close button at the requested index.
    @param      Index of a cell.
    @returns    The close button tracking rect of the cell at the requested index.
 */

- (NSRect)closeButtonTrackingRectAtIndex:(NSUInteger)index {
	NSRect rect;
	if(index < [_closeButtonTrackingRects count]) {
		rect = [[_closeButtonTrackingRects objectAtIndex:index] rectValue];
	} else {
		NSLog(@"closeButtonTrackingRectAtIndex: Invalid index (%ld)", (long)index);
		rect = NSZeroRect;
	}
	return rect;
}

/*!
    @method     cellFrameAtIndex:
    @abstract   Returns the frame for the cell at the requested index.
    @discussion Returns the frame for the cell at the requested index.
    @param      Index of a cell.
    @returns    The frame of the cell at the requested index.
 */

- (NSRect)cellFrameAtIndex:(NSUInteger)index {
	NSRect rect;

	if(index < [_cellFrames count]) {
		rect = [[_cellFrames objectAtIndex:index] rectValue];
	} else {
		NSLog(@"cellFrameAtIndex: Invalid index (%ld)", (long)index);
		rect = NSZeroRect;
	}
	return rect;
}

/*!
    @method     setSelectedCell:
    @abstract   Changes the cell states so the given cell is the currently selected cell.
    @discussion Makes the given cell the active cell and properly recalculates the tab states for surrounding cells.
    @param      An instance of PSMTabBarCell to make active.
 */

- (void)setSelectedCell:(PSMTabBarCell *)cell {
	NSArray *cells = [_control cells];
	NSEnumerator *enumerator = [cells objectEnumerator];
	PSMTabBarCell *lastCell = nil, *nextCell;

	//deselect the previously selected tab
	while((nextCell = [enumerator nextObject]) && ([nextCell state] == NSOffState)) {
		lastCell = nextCell;
	}

	[nextCell setState:NSOffState];
	[nextCell setTabState:PSMTab_PositionMiddleMask];

	if(lastCell && lastCell != [_control lastVisibleTab]) {
		[lastCell setTabState:~[lastCell tabState] & PSMTab_RightIsSelectedMask];
	}

	if((nextCell = [enumerator nextObject])) {
		[nextCell setTabState:~[lastCell tabState] & PSMTab_LeftIsSelectedMask];
	}

	[cell setState:NSOnState];
	[cell setTabState:PSMTab_SelectedMask];

	if(![cell isInOverflowMenu]) {
		NSUInteger cellIndex = [cells indexOfObject:cell];

		if(cellIndex > 0) {
			nextCell = [cells objectAtIndex:cellIndex - 1];
			[nextCell setTabState:[nextCell tabState] | PSMTab_RightIsSelectedMask];
		}

		if(cellIndex < [cells count] - 1) {
			nextCell = [cells objectAtIndex:cellIndex + 1];
			[nextCell setTabState:[nextCell tabState] | PSMTab_LeftIsSelectedMask];
		}
	}
}

/*!
    @method     layoutCells
    @abstract   Recalculates cell positions and states.
    @discussion This method calculates the proper frame, tabState and overflow menu status for all cells in the
                tab bar control.
 */

- (void)layoutCells {
	NSArray *cells = [_control cells];
	NSInteger cellCount = [cells count];

	// make sure all of our tabs are accounted for before updating
	if([[_control tabView] numberOfTabViewItems] != cellCount) {
		return;
	}

	[_cellTrackingRects removeAllObjects];
	[_closeButtonTrackingRects removeAllObjects];
	[_cellFrames removeAllObjects];

	NSArray *cellWidths = [self _generateWidthsFromCells:cells];
	[self _setupCells:cells withWidths:cellWidths];

	//set up the rect from the add tab button
	_addButtonRect = [_control genericCellRect];
	_addButtonRect.size = [[_control addTabButton] frame].size;
	if([_control orientation] == PSMTabBarHorizontalOrientation) {
		_addButtonRect.origin.y = MARGIN_Y;
		_addButtonRect.origin.x += [[cellWidths valueForKeyPath:@"@sum.floatValue"] doubleValue] + 2;
	} else {
		_addButtonRect.origin.x = 0;
		_addButtonRect.origin.y = [[cellWidths lastObject] doubleValue];
	}
}

/*!
 *  @method _shrinkWidths:towardMinimum:withAvailableWidth:
 *  @abstract Decreases widths in an array toward a minimum until they fit within availableWidth, if possible
 *  @param An array of NSNumbers
 *  @param The target minimum
 *  @param The maximum available width
 *  @returns The amount by which the total array width was shrunk
 */
- (NSInteger)_shrinkWidths:(NSMutableArray *)newWidths towardMinimum:(NSInteger)minimum withAvailableWidth:(CGFloat)availableWidth {
	BOOL changed = NO;
	NSInteger count = [newWidths count];
	NSInteger totalWidths = [[newWidths valueForKeyPath:@"@sum.intValue"] integerValue];
	NSInteger originalTotalWidths = totalWidths;

	do {
		changed = NO;

		for(NSInteger q = (count - 1); q >= 0; q--) {
			CGFloat cellWidth = [[newWidths objectAtIndex:q] doubleValue];
			if(cellWidth - 1 >= minimum) {
				cellWidth--;
				totalWidths--;

				[newWidths replaceObjectAtIndex:q
				 withObject:[NSNumber numberWithDouble:cellWidth]];

				changed = YES;
			}
		}
	} while(changed && (totalWidths > availableWidth));

	return(originalTotalWidths - totalWidths);
}

/*!
 * @function   potentialMinimumForArray()
 * @abstract   Calculate the minimum total for a given array of widths
 * @discussion The array is summed using, for each item, the minimum between the current value and the passed minimum value.
 *             This is useful for getting a sum if the array has size-to-fit widths which will be allowed to be less than the
 *             specified minimum.
 * @param      An array of widths
 * @param      The minimum
 * @returns    The smallest possible sum for the array
 */
static NSInteger potentialMinimumForArray(NSArray *array, NSInteger minimum){
	NSInteger runningTotal = 0;
	NSInteger count = [array count];

	for(NSInteger i = 0; i < count; i++) {
		NSInteger currentValue = [[array objectAtIndex:i] integerValue];
		runningTotal += MIN(currentValue, minimum);
	}

	return runningTotal;
}

/*!
    @method     _generateWidthsFromCells:
    @abstract   Calculates the width of cells that would be visible.
    @discussion Calculates the width of cells in the tab bar and returns an array of widths for the cells that would be
                visible. Uses large blocks of code that were previously in PSMTabBarControl's update method.
    @param      An array of PSMTabBarCells.
    @returns    An array of numbers representing the widths of cells that would be visible.
 */

- (NSArray *)_generateWidthsFromCells:(NSArray *)cells {
	NSInteger cellCount = [cells count], i, numberOfVisibleCells = ([_control orientation] == PSMTabBarHorizontalOrientation) ? 1 : 0;
	NSMutableArray *newWidths = [NSMutableArray arrayWithCapacity:cellCount];
	id <PSMTabStyle> style = [_control style];
	CGFloat availableWidth = [_control availableCellWidth], currentOrigin = 0, totalOccupiedWidth = 0.0, width;
	NSRect cellRect = [_control genericCellRect], controlRect = [_control frame];
	PSMTabBarCell *currentCell;

	if([_control orientation] == PSMTabBarVerticalOrientation) {
		currentOrigin = [style topMarginForTabBarControl];
	}

	//Don't let cells overlap the add tab button if it is visible
	if([_control showAddTabButton]) {
		availableWidth -= [self addButtonRect].size.width;
	}

	for(i = 0; i < cellCount; i++) {
		currentCell = [cells objectAtIndex:i];

		// supress close button?
		[currentCell setCloseButtonSuppressed:((cellCount == 1 && [_control canCloseOnlyTab] == NO) ||
											   [_control disableTabClose] ||
											   ([[_control delegate] respondsToSelector:@selector(tabView:disableTabCloseForTabViewItem:)] &&
												[[_control delegate] tabView:[_control tabView] disableTabCloseForTabViewItem:[currentCell representedObject]]))];

		if([_control orientation] == PSMTabBarHorizontalOrientation) {
			// Determine cell width
			if([_control sizeCellsToFit]) {
				width = [currentCell desiredWidthOfCell];
				if(width > [_control cellMaxWidth]) {
					width = [_control cellMaxWidth];
				}
			} else {
				width = [_control cellOptimumWidth];
			}

			width = ceil(width);

			//check to see if there is not enough space to place all tabs as preferred
			if(totalOccupiedWidth + width >= availableWidth) {
				//There's not enough space to add currentCell at its preferred width!

				//If we're not going to use the overflow menu, cram all the tab cells into the bar regardless of minimum width
				if(![_control useOverflowMenu]) {
					NSInteger j, averageWidth = (availableWidth / cellCount);

					numberOfVisibleCells = cellCount;
					[newWidths removeAllObjects];

					for(j = 0; j < cellCount; j++) {
						CGFloat desiredWidth = [[cells objectAtIndex:j] desiredWidthOfCell];
						[newWidths addObject:[NSNumber numberWithDouble:(desiredWidth < averageWidth && [_control sizeCellsToFit]) ? desiredWidth : averageWidth]];
					}

					totalOccupiedWidth = [[newWidths valueForKeyPath:@"@sum.intValue"] integerValue];
					break;
				}

				//We'll be using the overflow menu if needed.
				numberOfVisibleCells = i;
				if([_control sizeCellsToFit]) {
					BOOL remainingCellsMustGoToOverflow = NO;

					totalOccupiedWidth = [[newWidths valueForKeyPath:@"@sum.intValue"] integerValue];

					/* Can I squeeze it in without violating min cell width? This is the width we would take up
					 * if every cell so far were at the control minimum size (or their current size if that is less than the control minimum).
					 */
					if((potentialMinimumForArray(newWidths, [_control cellMinWidth]) + MIN(width, [_control cellMinWidth])) <= availableWidth) {
						/* It's definitely possible for cells so far to be visible.
						 * Shrink other cells to allow this one to fit
						 */
						NSInteger cellMinWidth = [_control cellMinWidth];

						/* Start off adding it to the array; we know that it will eventually fit because
						 * (the potential minimum <= availableWidth)
						 *
						 * This allows average and minimum aggregates on the NSArray to work.
						 */
						[newWidths addObject:[NSNumber numberWithDouble:width]];
						numberOfVisibleCells++;

						totalOccupiedWidth += width;

						//First, try to shrink tabs toward the average. Tabs smaller than average won't change
						totalOccupiedWidth -= [self _shrinkWidths:newWidths
											   towardMinimum:[[newWidths valueForKeyPath:@"@avg.intValue"] integerValue]
											   withAvailableWidth:availableWidth];



						if(totalOccupiedWidth > availableWidth) {
							//Next, shrink tabs toward the smallest of the existing tabs. The smallest tab won't change.
							NSInteger smallestTabWidth = [[newWidths valueForKeyPath:@"@min.intValue"] integerValue];
							if(smallestTabWidth > cellMinWidth) {
								totalOccupiedWidth -= [self _shrinkWidths:newWidths
													   towardMinimum:smallestTabWidth
													   withAvailableWidth:availableWidth];
							}
						}

						if(totalOccupiedWidth > availableWidth) {
							//Finally, shrink tabs toward the imposed minimum size.  All tabs larger than the minimum wll change.
							totalOccupiedWidth -= [self _shrinkWidths:newWidths
												   towardMinimum:cellMinWidth
												   withAvailableWidth:availableWidth];
						}

						if(totalOccupiedWidth > availableWidth) {
							NSLog(@"**** -[PSMTabBarController generateWidthsFromCells:] This is a failure (available %f, total %f, width is %f)",
								  availableWidth, totalOccupiedWidth, width);
							remainingCellsMustGoToOverflow = YES;
						}

						if(totalOccupiedWidth < availableWidth) {
							/* We're not using all available space not but exceeded available width before;
							 * stretch all cells to fully fit the bar
							 */
							NSInteger leftoverWidth = availableWidth - totalOccupiedWidth;
							if(leftoverWidth > 0) {
								NSInteger q;
								for(q = numberOfVisibleCells - 1; q >= 0; q--) {
									NSInteger desiredAddition = (NSInteger)leftoverWidth / (q + 1);
									NSInteger newCellWidth = (NSInteger)[[newWidths objectAtIndex:q] doubleValue] + desiredAddition;
									[newWidths replaceObjectAtIndex:q withObject:[NSNumber numberWithDouble:newCellWidth]];
									leftoverWidth -= desiredAddition;
									totalOccupiedWidth += desiredAddition;
								}
							}
						}
					} else {
						// stretch - distribute leftover room among cells, since we can't add this cell
						NSInteger leftoverWidth = availableWidth - totalOccupiedWidth;
						NSInteger q;
						for(q = i - 1; q >= 0; q--) {
							NSInteger desiredAddition = (NSInteger)leftoverWidth / (q + 1);
							NSInteger newCellWidth = (NSInteger)[[newWidths objectAtIndex:q] doubleValue] + desiredAddition;
							[newWidths replaceObjectAtIndex:q withObject:[NSNumber numberWithDouble:newCellWidth]];
							leftoverWidth -= desiredAddition;
						}

						remainingCellsMustGoToOverflow = YES;
					}

					// done assigning widths; remaining cells go in overflow menu
					if(remainingCellsMustGoToOverflow) {
						break;
					}
				} else {
					//We're not using size-to-fit
					NSInteger revisedWidth = availableWidth / (i + 1);
					if(revisedWidth >= [_control cellMinWidth]) {
						NSUInteger q;
						totalOccupiedWidth = 0;

						for(q = 0; q < [newWidths count]; q++) {
							[newWidths replaceObjectAtIndex:q withObject:[NSNumber numberWithDouble:revisedWidth]];
							totalOccupiedWidth += revisedWidth;
						}
						// just squeezed this one in...
						[newWidths addObject:[NSNumber numberWithDouble:revisedWidth]];
						totalOccupiedWidth += revisedWidth;
						numberOfVisibleCells++;
					} else {
						// couldn't fit that last one...
						break;
					}
				}
			} else {
				//(totalOccupiedWidth < availableWidth)
				numberOfVisibleCells = cellCount;
				[newWidths addObject:[NSNumber numberWithDouble:width]];
				totalOccupiedWidth += width;
			}
		} else {
			//lay out vertical tabs
			if(currentOrigin + cellRect.size.height <= controlRect.size.height) {
				[newWidths addObject:[NSNumber numberWithDouble:currentOrigin]];
				numberOfVisibleCells++;
				currentOrigin += cellRect.size.height;
			} else {
				//out of room, the remaining tabs go into overflow
				if([newWidths count] > 0 && controlRect.size.height - currentOrigin < 17) {
					[newWidths removeLastObject];
					numberOfVisibleCells--;
				}
				break;
			}
		}
	}

	//make sure there are at least two items in the horizontal tab bar
	if([_control orientation] == PSMTabBarHorizontalOrientation) {
		if(numberOfVisibleCells < 2 && [cells count] > 1) {
			PSMTabBarCell *cell1 = [cells objectAtIndex:0], *cell2 = [cells objectAtIndex:1];
			NSNumber *cellWidth;

			[newWidths removeAllObjects];
			totalOccupiedWidth = 0;

			cellWidth = [NSNumber numberWithDouble:[cell1 desiredWidthOfCell] < availableWidth * 0.5f ?[cell1 desiredWidthOfCell] : availableWidth * 0.5f];
			[newWidths addObject:cellWidth];
			totalOccupiedWidth += [cellWidth doubleValue];

			cellWidth = [NSNumber numberWithDouble:[cell2 desiredWidthOfCell] < (availableWidth - totalOccupiedWidth) ?[cell2 desiredWidthOfCell] : (availableWidth - totalOccupiedWidth)];
			[newWidths addObject:cellWidth];
			totalOccupiedWidth += [cellWidth doubleValue];

			if(totalOccupiedWidth < availableWidth) {
				[newWidths replaceObjectAtIndex:0 withObject:[NSNumber numberWithDouble:availableWidth - [cellWidth doubleValue]]];
			}

			numberOfVisibleCells = 2;
		}
	}

	return newWidths;
}

/*!
    @method     _setupCells:withWidths
    @abstract   Creates tracking rect arrays and sets the frames of the visible cells.
    @discussion Creates tracking rect arrays and sets the cells given in the widths array.
 */

- (void)_setupCells:(NSArray *)cells withWidths:(NSArray *)widths {
	NSUInteger i, tabState, cellCount = [cells count];
	NSRect cellRect = [_control genericCellRect];
	PSMTabBarCell *cell;
	NSTabViewItem *selectedTabViewItem = [[_control tabView] selectedTabViewItem];
	NSMenuItem *menuItem;

	[_overflowMenu release], _overflowMenu = nil;

	for(i = 0; i < cellCount; i++) {
		cell = [cells objectAtIndex:i];

		if(i < [widths count]) {
			tabState = 0;

			// set cell frame
			if([_control orientation] == PSMTabBarHorizontalOrientation) {
				cellRect.size.width = [[widths objectAtIndex:i] doubleValue];
			} else {
				cellRect.size.width = [_control frame].size.width;
				cellRect.origin.y = [[widths objectAtIndex:i] doubleValue];
				cellRect.origin.x = 0;
			}

			[_cellFrames addObject:[NSValue valueWithRect:cellRect]];

			//add tracking rects to arrays
			[_closeButtonTrackingRects addObject:[NSValue valueWithRect:[cell closeButtonRectForFrame:cellRect]]];
			[_cellTrackingRects addObject:[NSValue valueWithRect:cellRect]];

			if([[cell representedObject] isEqualTo:selectedTabViewItem]) {
				[cell setState:NSOnState];
				tabState |= PSMTab_SelectedMask;
				// previous cell
				if(i > 0) {
					[[cells objectAtIndex:i - 1] setTabState:([(PSMTabBarCell *)[cells objectAtIndex:i - 1] tabState] | PSMTab_RightIsSelectedMask)];
				}
				// next cell - see below
			} else {
				[cell setState:NSOffState];
				// see if prev cell was selected
				if((i > 0) && ([[cells objectAtIndex:i - 1] state] == NSOnState)) {
					tabState |= PSMTab_LeftIsSelectedMask;
				}
			}

			// more tab states
			if([widths count] == 1) {
				tabState |= PSMTab_PositionLeftMask | PSMTab_PositionRightMask | PSMTab_PositionSingleMask;
			} else if(i == 0) {
				tabState |= PSMTab_PositionLeftMask;
			} else if(i == [widths count] - 1) {
				tabState |= PSMTab_PositionRightMask;
			}

			[cell setTabState:tabState];
			[cell setIsInOverflowMenu:NO];

			// indicator
			if(![[cell indicator] isHidden] && ![_control isTabBarHidden]) {
				if(![[_control subviews] containsObject:[cell indicator]]) {
					[_control addSubview:[cell indicator]];
					[[cell indicator] startAnimation:self];
				}
			}

			// next...
			cellRect.origin.x += [[widths objectAtIndex:i] doubleValue];
		} else {
			[cell setState:NSOffState];
			[cell setIsInOverflowMenu:YES];
			[[cell indicator] removeFromSuperview];

			//position the cell well offscreen
			if([_control orientation] == PSMTabBarHorizontalOrientation) {
				cellRect.origin.x += [[_control style] rightMarginForTabBarControl] + 20;
			} else {
				cellRect.origin.y = [_control frame].size.height + 2;
			}

			[_cellFrames addObject:[NSValue valueWithRect:cellRect]];

			if(_overflowMenu == nil) {
				_overflowMenu = [[NSMenu alloc] init];
				[_overflowMenu insertItemWithTitle:@"" action:nil keyEquivalent:@"" atIndex:0]; // Because the overflowPupUpButton is a pull down menu
				[_overflowMenu setDelegate:self];
			}

			// Each item's title is limited to 60 characters. If more than 60 characters, use an ellipsis to indicate that more exists.
			menuItem = [_overflowMenu addItemWithTitle:[[[cell attributedStringValue] string] stringWithEllipsisByTruncatingToLength:MAX_OVERFLOW_MENUITEM_TITLE_LENGTH]
						action:@selector(overflowMenuAction:)
						keyEquivalent:@""];
			[menuItem setTarget:_control];
			[menuItem setRepresentedObject:[cell representedObject]];

			if([cell count] > 0) {
				[menuItem setTitle:[[menuItem title] stringByAppendingFormat:@" (%lu)", (unsigned long)[cell count]]];
			}
		}
	}
}

- (BOOL)menu:(NSMenu *)menu updateItem:(NSMenuItem *)menuItem atIndex:(NSInteger)index shouldCancel:(BOOL)shouldCancel {
	if(menu == _overflowMenu) {
		if([[[menuItem representedObject] identifier] respondsToSelector:@selector(icon)]) {
			[menuItem setImage:[[[menuItem representedObject] identifier] valueForKey:@"icon"]];
		}
	}

	return TRUE;
}

- (NSInteger)numberOfItemsInMenu:(NSMenu *)menu {
	if(menu == _overflowMenu) {
		return [_overflowMenu numberOfItems];
	} else {
		NSLog(@"Warning: Unexpected menu delegate call for menu %@", menu);
		return 0;
	}
}

@end

/*
   PSMTabBarController will store what the current tab frame state should be like based off the last layout. PSMTabBarControl
   has to handle fetching the new frame and then changing the tab cell frame.
    Tab states will probably be changed immediately.

   Tabs that aren't going to be visible need to have their frame set offscreen. Treat them as if they were visible.

   The overflow menu is rebuilt and stored by the controller.

   Arrays of tracking rects will be created here, but not applied.
    Tracking rects are removed and added by PSMTabBarControl at the end of an animate/display cycle.

   The add tab button frame is handled by this controller. Visibility and location are set by the control.

   isInOverflowMenu should probably be removed in favor of a call that returns yes/no to if a cell is in overflow. (Not yet implemented)

   Still need to rewrite most of the code in PSMTabDragAssistant.
 */
