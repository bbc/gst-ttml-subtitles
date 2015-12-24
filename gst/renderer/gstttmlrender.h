/* GStreamer
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
 * Copyright (C) <2003> David Schleef <ds@schleef.org>
 * Copyright (C) <2006> Julien Moutte <julien@moutte.net>
 * Copyright (C) <2006> Zeeshan Ali <zeeshan.ali@nokia.com>
 * Copyright (C) <2006-2008> Tim-Philipp Müller <tim centricular net>
 * Copyright (C) <2009> Young-Ho Cha <ganadist@gmail.com>
 * Copyright (C) <2015> British Broadcasting Corporation <dash@rd.bbc.co.uk>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef __GST_BASE_TEXT_OVERLAY_H__
#define __GST_BASE_TEXT_OVERLAY_H__

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/video-overlay-composition.h>
#include <gst/subtitle/subtitle.h>
#include <pango/pangocairo.h>

G_BEGIN_DECLS

#define GST_TYPE_TTML_RENDER            (gst_ttml_render_get_type())
#define GST_TTML_RENDER(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),\
                                         GST_TYPE_TTML_RENDER, GstTtmlRender))
#define GST_TTML_RENDER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),\
                                         GST_TYPE_TTML_RENDER,GstTtmlRenderClass))
#define GST_TTML_RENDER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),\
                                         GST_TYPE_TTML_RENDER, GstTtmlRenderClass))
#define GST_IS_TTML_RENDER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),\
                                         GST_TYPE_TTML_RENDER))
#define GST_IS_TTML_RENDER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),\
                                         GST_TYPE_TTML_RENDER))

typedef struct _GstTtmlRender GstTtmlRender;
typedef struct _GstTtmlRenderClass GstTtmlRenderClass;
typedef struct _GstTtmlRenderRenderedImage GstTtmlRenderRenderedImage;
typedef struct _GstTtmlRenderRenderedText GstTtmlRenderRenderedText;

struct _GstTtmlRenderRenderedImage {
  GstBuffer *image;
  gint x;
  gint y;
  guint width;
  guint height;
};

struct _GstTtmlRenderRenderedText {
  GstTtmlRenderRenderedImage *text_image;

  /* In order to get the positions of characters within a paragraph rendered by
   * pango we need to retain a reference to the PangoLayout object that was
   * used to render that paragraph. */
  PangoLayout *layout;

  /* The coordinates in @layout will be offset horizontally with respect to the
   * position of those characters in @text_image. Store that offset here so
   * that the information in @layout can be used to locate the position and
   * extent of text areas in @text_image. */
  guint horiz_offset;
};


/**
 * GstTtmlRenderVAlign:
 * @GST_TTML_RENDER_VALIGN_BASELINE: draw text on the baseline
 * @GST_TTML_RENDER_VALIGN_BOTTOM: draw text on the bottom
 * @GST_TTML_RENDER_VALIGN_TOP: draw text on top
 * @GST_TTML_RENDER_VALIGN_POS: draw text according to the #GstTtmlRender:ypos property
 * @GST_TTML_RENDER_VALIGN_CENTER: draw text vertically centered
 *
 * Vertical alignment of the text.
 */
typedef enum {
    GST_TTML_RENDER_VALIGN_BASELINE,
    GST_TTML_RENDER_VALIGN_BOTTOM,
    GST_TTML_RENDER_VALIGN_TOP,
    GST_TTML_RENDER_VALIGN_POS,
    GST_TTML_RENDER_VALIGN_CENTER
} GstTtmlRenderVAlign;

/**
 * GstTtmlRenderHAlign:
 * @GST_TTML_RENDER_HALIGN_LEFT: align text left
 * @GST_TTML_RENDER_HALIGN_CENTER: align text center
 * @GST_TTML_RENDER_HALIGN_RIGHT: align text right
 * @GST_TTML_RENDER_HALIGN_POS: position text according to the #GstTtmlRender:xpos property
 *
 * Horizontal alignment of the text.
 */
/* FIXME 0.11: remove GST_TTML_RENDER_HALIGN_UNUSED */
typedef enum {
    GST_TTML_RENDER_HALIGN_LEFT,
    GST_TTML_RENDER_HALIGN_CENTER,
    GST_TTML_RENDER_HALIGN_RIGHT,
    GST_TTML_RENDER_HALIGN_UNUSED,
    GST_TTML_RENDER_HALIGN_POS
} GstTtmlRenderHAlign;

/**
 * GstTtmlRenderWrapMode:
 * @GST_TTML_RENDER_WRAP_MODE_NONE: no wrapping
 * @GST_TTML_RENDER_WRAP_MODE_WORD: do word wrapping
 * @GST_TTML_RENDER_WRAP_MODE_CHAR: do char wrapping
 * @GST_TTML_RENDER_WRAP_MODE_WORD_CHAR: do word and char wrapping
 *
 * Whether to wrap the text and if so how.
 */
typedef enum {
    GST_TTML_RENDER_WRAP_MODE_NONE = -1,
    GST_TTML_RENDER_WRAP_MODE_WORD = PANGO_WRAP_WORD,
    GST_TTML_RENDER_WRAP_MODE_CHAR = PANGO_WRAP_CHAR,
    GST_TTML_RENDER_WRAP_MODE_WORD_CHAR = PANGO_WRAP_WORD_CHAR
} GstTtmlRenderWrapMode;

/**
 * GstTtmlRenderLineAlign:
 * @GST_TTML_RENDER_LINE_ALIGN_LEFT: lines are left-aligned
 * @GST_TTML_RENDER_LINE_ALIGN_CENTER: lines are center-aligned
 * @GST_TTML_RENDER_LINE_ALIGN_RIGHT: lines are right-aligned
 *
 * Alignment of text lines relative to each other
 */
typedef enum {
    GST_TTML_RENDER_LINE_ALIGN_LEFT = PANGO_ALIGN_LEFT,
    GST_TTML_RENDER_LINE_ALIGN_CENTER = PANGO_ALIGN_CENTER,
    GST_TTML_RENDER_LINE_ALIGN_RIGHT = PANGO_ALIGN_RIGHT
} GstTtmlRenderLineAlign;


/**
 * GstTtmlRender:
 *
 * Opaque textoverlay object structure
 */
struct _GstTtmlRender {
    GstElement               element;

    GstPad                  *video_sinkpad;
    GstPad                  *text_sinkpad;
    GstPad                  *srcpad;

    GstSegment               segment;
    GstSegment               text_segment;
    GstBuffer               *text_buffer;
    gboolean                text_linked;
    gboolean                video_flushing;
    gboolean                video_eos;
    gboolean                text_flushing;
    gboolean                text_eos;

    GMutex                   lock;
    GCond                    cond;  /* to signal removal of a queued text
                                     * buffer, arrival of a text buffer,
                                     * a text segment update, or a change
                                     * in status (e.g. shutdown, flushing) */

    GstVideoInfo             info;
    GstVideoFormat           format;
    gint                     width;
    gint                     height;

    GstTtmlRenderVAlign     valign;
    GstTtmlRenderHAlign     halign;
    GstTtmlRenderWrapMode   wrap_mode;
    GstTtmlRenderLineAlign  line_align;

    gint                     xpad;
    gint                     ypad;
    gint                     deltax;
    gint                     deltay;
    gdouble                  xpos;
    gdouble                  ypos;
    gboolean                 want_background;
    gboolean                 silent;
    gboolean                 wait_text;
    guint                    color, outline_color;

    PangoLayout             *layout;
    gdouble                  shadow_offset;
    gdouble                  outline_offset;
    GstBuffer               *text_image;
    gint                     image_width;
    gint                     image_height;
    gint                     baseline_y;
    gint                     text_height_px;

    gboolean                 auto_adjust_size;
    gboolean                 need_render;

    gint                     shading_value;  /* for timeoverlay subclass */

    gboolean                 have_pango_markup;
    gboolean                 use_vertical_render;

    gboolean                 attach_compo_to_buffer;

    GstVideoOverlayComposition *composition;

    GList * compositions;
};

struct _GstTtmlRenderClass {
    GstElementClass parent_class;

    PangoContext *pango_context;
    GMutex       *pango_lock;

    gchar *     (*get_text) (GstTtmlRender *overlay, GstBuffer *video_frame);
};

GType gst_ttml_render_get_type(void) G_GNUC_CONST;

G_END_DECLS

#endif /* __GST_BASE_TEXT_OVERLAY_H */
