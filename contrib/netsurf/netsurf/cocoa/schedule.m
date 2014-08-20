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

#import "utils/errors.h"

#import "cocoa/schedule.h"

@interface ScheduledCallback : NSObject {
	void (*callback)( void *userData );
	void *userData;
}

- initWithCallback: (void (*)(void *))cb userData: (void *)ud;
- (void) schedule: (NSTimeInterval) ti;

@end

@implementation ScheduledCallback

- initWithCallback: (void (*)(void *))cb userData: (void *)ud;
{
	callback = cb;
	userData = ud;
	
	return self;
}

static NSMutableSet *timerSet = nil;

- (void) schedule: (NSTimeInterval) ti;
{
	if (nil == timerSet) {
		timerSet = [[NSMutableSet alloc] init];
	}
	
	[self performSelector: @selector(timerFired) withObject: nil afterDelay: ti];
	[timerSet addObject: self];
}

- (void) timerFired;
{
	if ([timerSet containsObject: self]) {
		[timerSet removeObject: self];
		callback( userData );
	}
}

- (BOOL) isEqual: (id)object
{
	if (object == self) return YES;
	if ([object class] != [self class]) return NO;
	return ((ScheduledCallback *)object)->callback == callback &&	((ScheduledCallback *)object)->userData == userData;
}

- (NSUInteger) hash;
{
	return (NSUInteger)callback + (NSUInteger)userData;
}

@end

/* exported interface documented in cocoa/schedule.h */
nserror cocoa_schedule(int t, void (*callback)(void *p), void *p)
{
	ScheduledCallback *cb = [[ScheduledCallback alloc] initWithCallback: callback userData: p];
	[timerSet removeObject: cb];
        if (t >= 0) {
          [cb schedule: (NSTimeInterval)t / 1000];
        }
	[cb release];

        return NSERROR_OK;
}
