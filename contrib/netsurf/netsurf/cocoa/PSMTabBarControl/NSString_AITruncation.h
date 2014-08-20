//
//  NSString_AITruncation.h
//  PSMTabBarControl
//
//  Created by Evan Schoenberg on 7/14/07.
//

#import <Cocoa/Cocoa.h>

@interface NSString (AITruncation)
- (NSString *)stringWithEllipsisByTruncatingToLength:(NSUInteger)length;
@end
