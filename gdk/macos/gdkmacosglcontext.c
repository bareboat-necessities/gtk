/*
 * Copyright © 2020 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
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

#include "gdkmacosglcontext-private.h"
#include "gdkmacossurface-private.h"

#include "gdkinternals.h"
#include "gdkintl.h"

#include <OpenGL/gl.h>

#import "GdkMacosGLView.h"

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

G_DEFINE_TYPE (GdkMacosGLContext, gdk_macos_gl_context, GDK_TYPE_GL_CONTEXT)

static NSOpenGLContext *
get_ns_open_gl_context (GdkMacosGLContext  *self,
                        GError            **error)
{
  NSOpenGLContext *gl_context = nil;
  GdkSurface *surface;
  NSView *nsview;

  g_assert (GDK_IS_MACOS_GL_CONTEXT (self));

  if (!(surface = gdk_draw_context_get_surface (GDK_DRAW_CONTEXT (self))) ||
      !(nsview = _gdk_macos_surface_get_view (GDK_MACOS_SURFACE (surface))) ||
      !GDK_IS_MACOS_GL_VIEW (nsview) ||
      !(gl_context = [(GdkMacosGLView *)nsview openGLContext]))
    {
      g_set_error_literal (error,
                           GDK_GL_ERROR,
                           GDK_GL_ERROR_NOT_AVAILABLE,
                           "Cannot access NSOpenGLContext for surface");
      return NULL;
    }

  return gl_context;
}

static gboolean
gdk_macos_gl_context_real_realize (GdkGLContext  *context,
                                   GError       **error)
{
  GdkMacosGLContext *self = (GdkMacosGLContext *)context;
  GdkSurface *surface;
  NSView *nsview;

  g_assert (GDK_IS_MACOS_GL_CONTEXT (self));

  surface = gdk_draw_context_get_surface (GDK_DRAW_CONTEXT (context));
  nsview = _gdk_macos_surface_get_view (GDK_MACOS_SURFACE (surface));

  if (!GDK_IS_MACOS_GL_VIEW (nsview))
    {
      NSOpenGLPixelFormat *pixelFormat;
      NSOpenGLContext *shared_gl_context = nil;
      NSOpenGLContext *gl_context;
      GdkMacosGLView *gl_view;
      GdkGLContext *shared;
      NSWindow *nswindow;
      NSRect contentRect;
      GLint sync_to_framerate = 1;
      GLint opaque = 0;

      if ((shared = gdk_gl_context_get_shared_context (context)))
        {
          if (!(shared_gl_context = get_ns_open_gl_context (GDK_MACOS_GL_CONTEXT (shared), error)))
            return FALSE;
        }

      nswindow = _gdk_macos_surface_get_native (GDK_MACOS_SURFACE (surface));
      pixelFormat = [GdkMacosGLView defaultPixelFormat];
      gl_context = [[NSOpenGLContext alloc] initWithFormat:pixelFormat
                                              shareContext:shared_gl_context];

      if (gl_context == nil)
        {
          [pixelFormat release];
          g_set_error_literal (error,
                               GDK_GL_ERROR,
                               GDK_GL_ERROR_NOT_AVAILABLE,
                               "Failed to create NSOpenGLContext");
          return FALSE;
        }

      [gl_context setValues:&sync_to_framerate forParameter:NSOpenGLCPSwapInterval];
      [gl_context setValues:&opaque forParameter:NSOpenGLCPSurfaceOpacity];

      GDK_NOTE (OPENGL,
                g_print ("Created NSOpenGLContext[%p]\n", gl_context));

      contentRect = [[nswindow contentView] bounds];
      gl_view = [[GdkMacosGLView alloc] initWithFrame:contentRect
                                          pixelFormat:pixelFormat];
      [nswindow setContentView:gl_view];
      [gl_view setOpenGLContext:gl_context];
      [gl_view setWantsBestResolutionOpenGLSurface:YES];
      [gl_view setPostsFrameChangedNotifications: YES];
      [gl_view setNeedsDisplay:YES];

      [gl_context makeCurrentContext];

      [gl_view release];
      [pixelFormat release];
    }

  g_assert (get_ns_open_gl_context (self, NULL) != NULL);

  return TRUE;
}

static void
gdk_macos_gl_context_begin_frame (GdkDrawContext *context,
                                  cairo_region_t *painted)
{
  g_assert (GDK_IS_MACOS_GL_CONTEXT (context));

  GDK_DRAW_CONTEXT_CLASS (gdk_macos_gl_context_parent_class)->begin_frame (context, painted);
}

static void
gdk_macos_gl_context_end_frame (GdkDrawContext *context,
                                cairo_region_t *painted)
{
  GdkMacosGLContext *self = GDK_MACOS_GL_CONTEXT (context);
  NSOpenGLContext *gl_context;

  g_assert (GDK_IS_MACOS_GL_CONTEXT (self));

  if ((gl_context = get_ns_open_gl_context (self, NULL)))
    {
      GdkMacosSurface *surface;
      NSView *nsview;

      surface = GDK_MACOS_SURFACE (gdk_draw_context_get_surface (context));
      nsview = _gdk_macos_surface_get_view (surface);
      [(GdkMacosGLView *)nsview invalidateRegion:painted];

      [gl_context flushBuffer];
    }
}

static void
gdk_macos_gl_context_surface_resized (GdkDrawContext *draw_context)
{
  GdkMacosGLContext *self = (GdkMacosGLContext *)draw_context;
  GdkSurface *surface;

  g_assert (GDK_IS_MACOS_GL_CONTEXT (self));

  surface = gdk_draw_context_get_surface (draw_context);

  self->resize.needed = TRUE;
  self->resize.width = surface->width;
  self->resize.height = surface->height;
}

static void
gdk_macos_gl_context_dispose (GObject *gobject)
{
  GdkMacosGLContext *self = GDK_MACOS_GL_CONTEXT (gobject);
  NSOpenGLContext *gl_context;

  if ((gl_context = get_ns_open_gl_context (self, NULL)))
    [gl_context clearDrawable];

  G_OBJECT_CLASS (gdk_macos_gl_context_parent_class)->dispose (gobject);
}

static void
gdk_macos_gl_context_class_init (GdkMacosGLContextClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkDrawContextClass *draw_context_class = GDK_DRAW_CONTEXT_CLASS (klass);
  GdkGLContextClass *gl_class = GDK_GL_CONTEXT_CLASS (klass);

  object_class->dispose = gdk_macos_gl_context_dispose;

  draw_context_class->begin_frame = gdk_macos_gl_context_begin_frame;
  draw_context_class->end_frame = gdk_macos_gl_context_end_frame;
  draw_context_class->surface_resized = gdk_macos_gl_context_surface_resized;

  gl_class->realize = gdk_macos_gl_context_real_realize;
}

static void
gdk_macos_gl_context_init (GdkMacosGLContext *self)
{
}

GdkGLContext *
_gdk_macos_gl_context_new (GdkMacosSurface  *surface,
                           gboolean          attached,
                           GdkGLContext     *share,
                           GError          **error)
{
  GdkMacosGLContext *context;

  g_return_val_if_fail (GDK_IS_MACOS_SURFACE (surface), NULL);
  g_return_val_if_fail (!share || GDK_IS_MACOS_GL_CONTEXT (share), NULL);

  context = g_object_new (GDK_TYPE_MACOS_GL_CONTEXT,
                          "surface", surface,
                          "shared-context", share,
                          NULL);

  context->is_attached = attached;

  return GDK_GL_CONTEXT (context);
}

gboolean
_gdk_macos_gl_context_make_current (GdkMacosGLContext *self)
{
  NSOpenGLContext *gl_context;

  g_return_val_if_fail (GDK_IS_MACOS_GL_CONTEXT (self), FALSE);

  if ((gl_context = get_ns_open_gl_context (self, NULL)))
    {
      [gl_context makeCurrentContext];
      return TRUE;
    }

  return FALSE;
}

G_GNUC_END_IGNORE_DEPRECATIONS
