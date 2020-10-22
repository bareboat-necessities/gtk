/* GdkMacosGLView.c
 *
 * Copyright 2020 Christian Hergert <chergert@redhat.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include <CoreGraphics/CoreGraphics.h>
#include <OpenGL/gl.h>

#include "gdkinternals.h"
#include "gdkmacossurface-private.h"

#import "GdkMacosGLView.h"

@implementation GdkMacosGLView

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

+(NSOpenGLPixelFormat *)defaultPixelFormat
{
  static const NSOpenGLPixelFormatAttribute attrs[] = {
    NSOpenGLPFAOpenGLProfile, NSOpenGLProfileVersion4_1Core,
    NSOpenGLPFAAccelerated,
    NSOpenGLPFADoubleBuffer,
    NSOpenGLPFADepthSize, 32,
    NSOpenGLPFAStencilSize, 8,

    (NSOpenGLPixelFormatAttribute)nil
  };

  return [[NSOpenGLPixelFormat alloc] initWithAttributes:attrs];
}

-(id)initWithFrame:(NSRect)frameRect pixelFormat:(NSOpenGLPixelFormat*)format
{
  self = [super initWithFrame:frameRect];

  if (self != nil)
    {
      NSNotificationCenter *center;

      _pixelFormat = [format retain];
      center = [NSNotificationCenter defaultCenter];

      [center addObserver:self
                 selector:@selector(_surfaceNeedsUpdate:)
                     name:NSViewGlobalFrameDidChangeNotification
                   object:self];

      [center addObserver:self
                 selector:@selector(_surfaceNeedsUpdate:)
                     name:NSViewFrameDidChangeNotification
                   object:self];
    }

  return self;
}

-(void)setPixelFormat:(NSOpenGLPixelFormat*)pixelFormat
{
  _pixelFormat = pixelFormat;
}

-(NSOpenGLPixelFormat*)pixelFormat
{
  return _pixelFormat;
}

-(void)_surfaceNeedsUpdate:(NSNotification *)notification
{
  [[self openGLContext] makeCurrentContext];
  [self update];
  [self reshape];
}

-(void)reshape
{
  [self update];
}

-(void)lockFocus
{
  NSOpenGLContext *context;

  [super lockFocus];

  context = [self openGLContext];

  if ([context view] != self)
    [context setView: self];
}

-(void)viewDidMoveToWindow
{
  [super viewDidMoveToWindow];

  if ([self window] == nil)
    {
      [[self openGLContext] clearDrawable];
      return;
    }

  [[self openGLContext] setView: self];
}

-(void)update
{
  [[self openGLContext] update];
}

-(void)drawRect:(NSRect)rect
{
}

-(void)clearGLContext
{
  _openGLContext = nil;
}

-(void)setOpenGLContext:(NSOpenGLContext*)context
{
  [context setView:self];
  _openGLContext = context;
}

-(NSOpenGLContext *)openGLContext
{
  return _openGLContext;
}

-(void)dealloc
{
  NSNotificationCenter *center = [NSNotificationCenter defaultCenter];

  [center removeObserver:self
                    name:NSViewGlobalFrameDidChangeNotification
                  object:self];

  [center removeObserver:self
                    name:NSViewFrameDidChangeNotification
                  object:self];

  [super dealloc];
}

-(BOOL)isOpaque
{
  return NO;
}

-(BOOL)isFlipped
{
  return YES;
}

-(BOOL)acceptsFirstMouse
{
  return YES;
}

-(BOOL)mouseDownCanMoveWindow
{
  return NO;
}

-(void)invalidateRegion:(const cairo_region_t *)region
{
  if (region != NULL)
    {
      guint n_rects = cairo_region_num_rectangles (region);

      for (guint i = 0; i < n_rects; i++)
        {
          cairo_rectangle_int_t rect;
          NSRect nsrect;

          cairo_region_get_rectangle (region, i, &rect);
          nsrect = NSMakeRect (rect.x, rect.y, rect.width, rect.height);

          [self setNeedsDisplayInRect:nsrect];
        }
    }
}

G_GNUC_END_IGNORE_DEPRECATIONS

@end
