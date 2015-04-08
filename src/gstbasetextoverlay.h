/* GStreamer
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
 * Copyright (C) <2003> David Schleef <ds@schleef.org>
 * Copyright (C) <2006> Julien Moutte <julien@moutte.net>
 * Copyright (C) <2006> Zeeshan Ali <zeeshan.ali@nokia.com>
 * Copyright (C) <2006-2008> Tim-Philipp Müller <tim centricular net>
 * Copyright (C) <2009> Young-Ho Cha <ganadist@gmail.com>
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
#include <pango/pangocairo.h>

G_BEGIN_DECLS

#define GST_TYPE_BASE_EBUTTD_OVERLAY            (gst_base_ebuttd_overlay_get_type())
#define GST_BASE_EBUTTD_OVERLAY(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),\
                                         GST_TYPE_BASE_EBUTTD_OVERLAY, GstBaseEbuttdOverlay))
#define GST_BASE_EBUTTD_OVERLAY_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),\
                                         GST_TYPE_BASE_EBUTTD_OVERLAY,GstBaseEbuttdOverlayClass))
#define GST_BASE_EBUTTD_OVERLAY_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),\
                                         GST_TYPE_BASE_EBUTTD_OVERLAY, GstBaseEbuttdOverlayClass))
#define GST_IS_BASE_EBUTTD_OVERLAY(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),\
                                         GST_TYPE_BASE_EBUTTD_OVERLAY))
#define GST_IS_BASE_EBUTTD_OVERLAY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),\
                                         GST_TYPE_BASE_EBUTTD_OVERLAY))

typedef struct _GstBaseEbuttdOverlay      GstBaseEbuttdOverlay;
typedef struct _GstBaseEbuttdOverlayClass GstBaseEbuttdOverlayClass;
typedef struct _GstBaseEbuttdOverlayRegion GstBaseEbuttdOverlayRegion;
typedef struct _GstBaseEbuttdOverlayStyle GstBaseEbuttdOverlayStyle;
typedef struct _GstBaseEbuttdOverlayColor GstBaseEbuttdOverlayColor;
typedef struct _GstBaseEbuttdOverlayLayer GstBaseEbuttdOverlayLayer;

/**
 * GstBaseEbuttdOverlayVAlign:
 * @GST_BASE_EBUTTD_OVERLAY_VALIGN_BASELINE: draw text on the baseline
 * @GST_BASE_EBUTTD_OVERLAY_VALIGN_BOTTOM: draw text on the bottom
 * @GST_BASE_EBUTTD_OVERLAY_VALIGN_TOP: draw text on top
 * @GST_BASE_EBUTTD_OVERLAY_VALIGN_POS: draw text according to the #GstBaseEbuttdOverlay:ypos property
 * @GST_BASE_EBUTTD_OVERLAY_VALIGN_CENTER: draw text vertically centered
 *
 * Vertical alignment of the text.
 */
typedef enum {
    GST_BASE_EBUTTD_OVERLAY_VALIGN_BASELINE,
    GST_BASE_EBUTTD_OVERLAY_VALIGN_BOTTOM,
    GST_BASE_EBUTTD_OVERLAY_VALIGN_TOP,
    GST_BASE_EBUTTD_OVERLAY_VALIGN_POS,
    GST_BASE_EBUTTD_OVERLAY_VALIGN_CENTER
} GstBaseEbuttdOverlayVAlign;

/**
 * GstBaseEbuttdOverlayHAlign:
 * @GST_BASE_EBUTTD_OVERLAY_HALIGN_LEFT: align text left
 * @GST_BASE_EBUTTD_OVERLAY_HALIGN_CENTER: align text center
 * @GST_BASE_EBUTTD_OVERLAY_HALIGN_RIGHT: align text right
 * @GST_BASE_EBUTTD_OVERLAY_HALIGN_POS: position text according to the #GstBaseEbuttdOverlay:xpos property
 *
 * Horizontal alignment of the text.
 */
/* FIXME 0.11: remove GST_BASE_EBUTTD_OVERLAY_HALIGN_UNUSED */
typedef enum {
    GST_BASE_EBUTTD_OVERLAY_HALIGN_LEFT,
    GST_BASE_EBUTTD_OVERLAY_HALIGN_CENTER,
    GST_BASE_EBUTTD_OVERLAY_HALIGN_RIGHT,
    GST_BASE_EBUTTD_OVERLAY_HALIGN_UNUSED,
    GST_BASE_EBUTTD_OVERLAY_HALIGN_POS
} GstBaseEbuttdOverlayHAlign;

/**
 * GstBaseEbuttdOverlayWrapMode:
 * @GST_BASE_EBUTTD_OVERLAY_WRAP_MODE_NONE: no wrapping
 * @GST_BASE_EBUTTD_OVERLAY_WRAP_MODE_WORD: do word wrapping
 * @GST_BASE_EBUTTD_OVERLAY_WRAP_MODE_CHAR: do char wrapping
 * @GST_BASE_EBUTTD_OVERLAY_WRAP_MODE_WORD_CHAR: do word and char wrapping
 *
 * Whether to wrap the text and if so how.
 */
typedef enum {
    GST_BASE_EBUTTD_OVERLAY_WRAP_MODE_NONE = -1,
    GST_BASE_EBUTTD_OVERLAY_WRAP_MODE_WORD = PANGO_WRAP_WORD,
    GST_BASE_EBUTTD_OVERLAY_WRAP_MODE_CHAR = PANGO_WRAP_CHAR,
    GST_BASE_EBUTTD_OVERLAY_WRAP_MODE_WORD_CHAR = PANGO_WRAP_WORD_CHAR
} GstBaseEbuttdOverlayWrapMode;

/**
 * GstBaseEbuttdOverlayLineAlign:
 * @GST_BASE_EBUTTD_OVERLAY_LINE_ALIGN_LEFT: lines are left-aligned
 * @GST_BASE_EBUTTD_OVERLAY_LINE_ALIGN_CENTER: lines are center-aligned
 * @GST_BASE_EBUTTD_OVERLAY_LINE_ALIGN_RIGHT: lines are right-aligned
 *
 * Alignment of text lines relative to each other
 */
typedef enum {
    GST_BASE_EBUTTD_OVERLAY_LINE_ALIGN_LEFT = PANGO_ALIGN_LEFT,
    GST_BASE_EBUTTD_OVERLAY_LINE_ALIGN_CENTER = PANGO_ALIGN_CENTER,
    GST_BASE_EBUTTD_OVERLAY_LINE_ALIGN_RIGHT = PANGO_ALIGN_RIGHT
} GstBaseEbuttdOverlayLineAlign;

/**
 * GstBaseEbuttdOverlayWritingMode:
 * @GST_BASE_EBUTTD_OVERLAY_WRITING_MODE_LRTB: text is written left-to-right,
 * top-to-bottom.
 * @GST_BASE_EBUTTD_OVERLAY_WRITING_MODE_RLTB: text is written right-to-left,
 * top-to-bottom.
 * @GST_BASE_EBUTTD_OVERLAY_WRITING_MODE_TBRL: text is written top-to-bottom,
 * right-to-left.
 * @GST_BASE_EBUTTD_OVERLAY_WRITING_MODE_TBLR: text is written top-to-bottom,
 * left-to-right.
 *
 * Writing mode of text content.
 */
typedef enum {
    GST_BASE_EBUTTD_OVERLAY_WRITING_MODE_LRTB,
    GST_BASE_EBUTTD_OVERLAY_WRITING_MODE_RLTB,
    GST_BASE_EBUTTD_OVERLAY_WRITING_MODE_TBRL,
    GST_BASE_EBUTTD_OVERLAY_WRITING_MODE_TBLR
} GstBaseEbuttdOverlayWritingMode; /* Or GstBaseEbuttdOverlayTextProgression? */

typedef enum {
    GST_BASE_EBUTTD_OVERLAY_DISPLAY_ALIGN_BEFORE,
    GST_BASE_EBUTTD_OVERLAY_DISPLAY_ALIGN_CENTER,
    GST_BASE_EBUTTD_OVERLAY_DISPLAY_ALIGN_AFTER
} GstBaseEbuttdOverlayDisplayAlign;

typedef enum {
    GST_BASE_EBUTTD_OVERLAY_BACKGROUND_MODE_ALWAYS,
    GST_BASE_EBUTTD_OVERLAY_BACKGROUND_MODE_WHEN_ACTIVE,
} GstBaseEbuttdOverlayBackgroundMode;

typedef enum {
    GST_BASE_EBUTTD_OVERLAY_OVERFLOW_MODE_HIDDEN,
    GST_BASE_EBUTTD_OVERLAY_OVERFLOW_MODE_VISIBLE,
} GstBaseEbuttdOverlayOverflowMode;

struct _GstBaseEbuttdOverlayRegion {
    /*
     * Properties of region from EBU-TT-D spec:
     *
     *   origin - coordinates of region origin in % of width & height of root
     *   container.
     *
     *   extent - size of region, again %age of width and height of root
     *   container.
     *
     *   displayAlign - not quite sure I understand this one, but this seems to
     *   be some kind of vertical aligment,
     *
     *   padding - padding to be applied on all sides of the region area.
     *
     *   writingMode - specifies the direction in which text progresses, both
     *   horizontally and vertically.
     *
     *   showBackground - controls whether the background colour of the region
     *   is always shown, or shown only when there is some text that is
     *   rendered in the region.
     *
     *   overflow - determines whether or not content that overflows the region
     *   area is clipped.
     */
    const gchar *id;
    gdouble origin_x, origin_y;
    gdouble extent_w, extent_h;
    gint x_bk, y_bk;
    gint width_bk, height_bk;
    GstBaseEbuttdOverlayDisplayAlign display_align;
    gdouble padding_start, padding_end, padding_before, padding_after;
    GstBaseEbuttdOverlayWritingMode writing_mode;
    GstBaseEbuttdOverlayBackgroundMode show_background;
    GstBaseEbuttdOverlayOverflowMode overflow;
    const gchar *bg_color;

    /* Data objects needed to render a region of subtitles. */
    PangoLayout *layout;   /* Pango layout that will handle text rendering. */
    GstBuffer *text_image; /* Destination buffer into which text will be
                              rendered.*/
    GstBuffer *bg_image;   /* Destination buffer into which background
                              rectangle will be rendered. */
    GstBuffer *region_bg_image;
    GstVideoOverlayComposition *composition;
};


typedef enum {
  GST_BASE_EBUTTD_OVERLAY_TEXT_DIRECTION_LTR,
  GST_BASE_EBUTTD_OVERLAY_TEXT_DIRECTION_RTL
} GstBaseEbuttdOverlayTextDirection;

typedef enum {
  GST_BASE_EBUTTD_OVERLAY_TEXT_ALIGN_START,
  GST_BASE_EBUTTD_OVERLAY_TEXT_ALIGN_LEFT,
  GST_BASE_EBUTTD_OVERLAY_TEXT_ALIGN_CENTER,
  GST_BASE_EBUTTD_OVERLAY_TEXT_ALIGN_RIGHT,
  GST_BASE_EBUTTD_OVERLAY_TEXT_ALIGN_END
} GstBaseEbuttdOverlayTextAlign;

typedef enum {
  GST_BASE_EBUTTD_OVERLAY_FONT_STYLE_NORMAL,
  GST_BASE_EBUTTD_OVERLAY_FONT_STYLE_ITALIC
} GstBaseEbuttdOverlayFontStyle;

typedef enum {
  GST_BASE_EBUTTD_OVERLAY_FONT_WEIGHT_NORMAL,
  GST_BASE_EBUTTD_OVERLAY_FONT_WEIGHT_BOLD,
} GstBaseEbuttdOverlayFontWeight;

typedef enum {
  GST_BASE_EBUTTD_OVERLAY_TEXT_DECORATION_NONE,
  GST_BASE_EBUTTD_OVERLAY_TEXT_DECORATION_UNDERLINE
} GstBaseEbuttdOverlayTextDecoration;

typedef enum {
  GST_BASE_EBUTTD_OVERLAY_UNICODE_BIDI_NORMAL,
  GST_BASE_EBUTTD_OVERLAY_UNICODE_BIDI_EMBED,
  GST_BASE_EBUTTD_OVERLAY_UNICODE_BIDI_OVERRIDE
} GstBaseEbuttdOverlayUnicodeBidi;

typedef enum {
  GST_BASE_EBUTTD_OVERLAY_WRAPPING_ON,
  GST_BASE_EBUTTD_OVERLAY_WRAPPING_OFF,
} GstBaseEbuttdOverlayWrapping;

typedef enum {
  GST_BASE_EBUTTD_OVERLAY_MULTI_ROW_ALIGN_AUTO,
  GST_BASE_EBUTTD_OVERLAY_MULTI_ROW_ALIGN_START,
  GST_BASE_EBUTTD_OVERLAY_MULTI_ROW_ALIGN_CENTER,
  GST_BASE_EBUTTD_OVERLAY_MULTI_ROW_ALIGN_END,
} GstBaseEbuttdOverlayMultiRowAlign;

struct _GstBaseEbuttdOverlayStyle {
  GstBaseEbuttdOverlayTextDirection text_direction;
  const gchar *font_family;
  gdouble font_size;
  gdouble line_height;
  GstBaseEbuttdOverlayTextAlign text_align;
  const gchar *color;
  const gchar *bg_color;
  GstBaseEbuttdOverlayFontStyle font_style;
  GstBaseEbuttdOverlayFontWeight font_weight;
  GstBaseEbuttdOverlayTextDirection text_decoration;
  GstBaseEbuttdOverlayUnicodeBidi unicode_bidi;
  GstBaseEbuttdOverlayWrapping wrap_option;
  GstBaseEbuttdOverlayMultiRowAlign multi_row_align;
  gdouble line_padding;
  guint cellres_x, cellres_y;
};

struct _GstBaseEbuttdOverlayColor {
  gdouble r;
  gdouble g;
  gdouble b;
  gdouble a;
};


struct _GstBaseEbuttdOverlayLayer {
  GstBuffer *image;
  guint xpos;
  guint ypos;
  guint width;
  guint height;
  GstVideoOverlayRectangle *rectangle;
};

/**
 * GstBaseEbuttdOverlay:
 *
 * Opaque textoverlay object structure
 */
struct _GstBaseEbuttdOverlay {
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

    GstBaseEbuttdOverlayVAlign     valign;
    GstBaseEbuttdOverlayHAlign     halign;
    GstBaseEbuttdOverlayWrapMode   wrap_mode;
    GstBaseEbuttdOverlayLineAlign  line_align;

    gint                     xpad;
    gint                     ypad;
    gint                     deltax;
    gint                     deltay;
    gdouble                  xpos;
    gdouble                  ypos;
    gchar                   *default_text;
    gboolean                 want_shading;
    gboolean                 want_background;
    gboolean                 silent;
    gboolean                 wait_text;
    guint                    color, outline_color;

    PangoLayout             *layout;
    gdouble                  shadow_offset;
    gdouble                  outline_offset;
    GstBuffer               *text_image;
    GstBuffer               *background_image; /* P Taylour added */
    gint                     image_width;
    gint                     image_height;
    gint                     image_height_bk; /* P Taylour added */
    gint                     image_width_bk; /* P Taylour added */
    gint                     baseline_y;
    gint                     text_height_px;

    gboolean                 auto_adjust_size;
    gboolean                 need_render;

    gint                     shading_value;  /* for timeoverlay subclass */

    gboolean                 have_pango_markup;
    gboolean                 use_vertical_render;

    gboolean                 attach_compo_to_buffer;

    GstVideoOverlayComposition *composition;

    gint                     line_padding; /* P Taylour added */
    gint64                   cell_resolution_x; /*not the most appropriate place for these */
    gint64                   cell_resolution_y;
    gint                     background_ypad;
    gchar                   *background_color; /* for overwriting pangos background */

    GSList *layers;
};

struct _GstBaseEbuttdOverlayClass {
    GstElementClass parent_class;

    PangoContext *pango_context;
    GMutex       *pango_lock;

    gchar *     (*get_text) (GstBaseEbuttdOverlay *overlay, GstBuffer *video_frame);
};

GType gst_base_ebuttd_overlay_get_type(void) G_GNUC_CONST;

G_END_DECLS

#endif /* __GST_BASE_TEXT_OVERLAY_H */
