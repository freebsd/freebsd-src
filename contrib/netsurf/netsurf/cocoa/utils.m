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

#import "utils/utils.h"

#define UNIMPL() NSLog( @"Function '%s' unimplemented", __func__ )

void die(const char * const error)
{
	[NSException raise: @"NetsurfDie" format: @"Error: %s", error];
}

void warn_user(const char *warning, const char *detail)
{
	NSRunAlertPanel( NSLocalizedString( @"Warning", @"Warning title" ), 
					NSLocalizedString( @"Warning %s%s%s", @"Warning message" ), 
					NSLocalizedString( @"OK", @"" ), nil, nil, 
					warning, detail != NULL ? ": " : "",
					detail != NULL ? detail : "" );
}

void PDF_Password(char **owner_pass, char **user_pass, char *path)
{
	UNIMPL();
}

