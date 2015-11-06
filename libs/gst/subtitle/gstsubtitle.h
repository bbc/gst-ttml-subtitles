/* GStreamer
 * Copyright (C) <2015> British Broadcasting Corporation
 *   Author: Chris Bass <dash@rd.bbc.co.uk>
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

#ifndef __GST_SUBTITLE_H__
#define __GST_SUBTITLE_H__

#include <glib.h>
#include <gst/gst.h>
#include <gst/gstminiobject.h>

G_BEGIN_DECLS

typedef struct _GstSubtitleColor GstSubtitleColor;
typedef struct _GstSubtitleStyleSet GstSubtitleStyleSet;
typedef struct _GstSubtitleElement GstSubtitleElement;
typedef struct _GstSubtitleBlock GstSubtitleBlock;
typedef struct _GstSubtitleRegion GstSubtitleRegion;

/**
 * GstSubtitleWritingMode:
 * @GST_SUBTITLE_WRITING_MODE_LRTB: text is written left-to-right,
 * top-to-bottom.
 * @GST_SUBTITLE_WRITING_MODE_RLTB: text is written right-to-left,
 * top-to-bottom.
 * @GST_SUBTITLE_WRITING_MODE_TBRL: text is written top-to-bottom,
 * right-to-left.
 * @GST_SUBTITLE_WRITING_MODE_TBLR: text is written top-to-bottom,
 * left-to-right.
 *
 * Writing mode of text content.
 */
typedef enum {
    GST_SUBTITLE_WRITING_MODE_LRTB,
    GST_SUBTITLE_WRITING_MODE_RLTB,
    GST_SUBTITLE_WRITING_MODE_TBRL,
    GST_SUBTITLE_WRITING_MODE_TBLR
} GstSubtitleWritingMode; /* Or GstSubtitleTextProgression? */

typedef enum {
    GST_SUBTITLE_DISPLAY_ALIGN_BEFORE,
    GST_SUBTITLE_DISPLAY_ALIGN_CENTER,
    GST_SUBTITLE_DISPLAY_ALIGN_AFTER
} GstSubtitleDisplayAlign;

typedef enum {
    GST_SUBTITLE_BACKGROUND_MODE_ALWAYS,
    GST_SUBTITLE_BACKGROUND_MODE_WHEN_ACTIVE
} GstSubtitleBackgroundMode;

typedef enum {
    GST_SUBTITLE_OVERFLOW_MODE_HIDDEN,
    GST_SUBTITLE_OVERFLOW_MODE_VISIBLE
} GstSubtitleOverflowMode;

struct _GstSubtitleColor {
  guint8 r;
  guint8 g;
  guint8 b;
  guint8 a;
};

typedef enum {
  GST_SUBTITLE_TEXT_DIRECTION_LTR,
  GST_SUBTITLE_TEXT_DIRECTION_RTL
} GstSubtitleTextDirection;

typedef enum {
  GST_SUBTITLE_TEXT_ALIGN_START,
  GST_SUBTITLE_TEXT_ALIGN_LEFT,
  GST_SUBTITLE_TEXT_ALIGN_CENTER,
  GST_SUBTITLE_TEXT_ALIGN_RIGHT,
  GST_SUBTITLE_TEXT_ALIGN_END
} GstSubtitleTextAlign;

typedef enum {
  GST_SUBTITLE_FONT_STYLE_NORMAL,
  GST_SUBTITLE_FONT_STYLE_ITALIC
} GstSubtitleFontStyle;

typedef enum {
  GST_SUBTITLE_FONT_WEIGHT_NORMAL,
  GST_SUBTITLE_FONT_WEIGHT_BOLD
} GstSubtitleFontWeight;

typedef enum {
  GST_SUBTITLE_TEXT_DECORATION_NONE,
  GST_SUBTITLE_TEXT_DECORATION_UNDERLINE
} GstSubtitleTextDecoration;

typedef enum {
  GST_SUBTITLE_UNICODE_BIDI_NORMAL,
  GST_SUBTITLE_UNICODE_BIDI_EMBED,
  GST_SUBTITLE_UNICODE_BIDI_OVERRIDE
} GstSubtitleUnicodeBidi;

typedef enum {
  GST_SUBTITLE_WRAPPING_ON,
  GST_SUBTITLE_WRAPPING_OFF
} GstSubtitleWrapping;

typedef enum {
  GST_SUBTITLE_MULTI_ROW_ALIGN_AUTO,
  GST_SUBTITLE_MULTI_ROW_ALIGN_START,
  GST_SUBTITLE_MULTI_ROW_ALIGN_CENTER,
  GST_SUBTITLE_MULTI_ROW_ALIGN_END
} GstSubtitleMultiRowAlign;

struct _GstSubtitleStyleSet {
  GstSubtitleTextDirection text_direction;
  gchar *font_family;
  gdouble font_size;
  gdouble line_height;
  GstSubtitleTextAlign text_align;
  GstSubtitleColor color;
  GstSubtitleColor bg_color;
  GstSubtitleFontStyle font_style;
  GstSubtitleFontWeight font_weight;
  GstSubtitleTextDecoration text_decoration;
  GstSubtitleUnicodeBidi unicode_bidi;
  GstSubtitleWrapping wrap_option;
  GstSubtitleMultiRowAlign multi_row_align;
  gdouble line_padding;
  gdouble origin_x, origin_y;
  gdouble extent_w, extent_h;
  GstSubtitleDisplayAlign display_align;
  gdouble padding_start, padding_end, padding_before, padding_after;
  GstSubtitleWritingMode writing_mode;
  GstSubtitleBackgroundMode show_background;
  GstSubtitleOverflowMode overflow;
};

GstSubtitleStyleSet * gst_subtitle_style_set_new ();

void gst_subtitle_style_set_free (GstSubtitleStyleSet * styleset);


/**
 * GstSubtitleElement:
 * @mini_object: the parent #GstMiniObject.
 * @style_set:
 * @text_index:
 *
 */
struct _GstSubtitleElement
{
  GstMiniObject mini_object;

  GstSubtitleStyleSet *style_set;
  guint text_index;

  /*< private >*/
  gpointer _gst_reserved[GST_PADDING];
};

GType gst_subtitle_element_get_type (void);

GstSubtitleElement * gst_subtitle_element_new (GstSubtitleStyleSet * style_set,
    guint text_index);

/**
 * gst_subtitle_element_ref:
 * @element: a #GstSubtitleElement.
 *
 * Increments the refcount of @element.
 *
 * Returns: (transfer full): @element.
 */
static inline GstSubtitleElement *
gst_subtitle_element_ref (GstSubtitleElement * element)
{
  return (GstSubtitleElement *)
    gst_mini_object_ref (GST_MINI_OBJECT_CAST (element));
}

/**
 * gst_subtitle_element_unref:
 * @element: (transfer full): a #GstSubtitleElement.
 *
 * Decrements the refcount of @element. If the refcount reaches 0, @element
 * will be freed.
 */
static inline void
gst_subtitle_element_unref (GstSubtitleElement * element)
{
  gst_mini_object_unref (GST_MINI_OBJECT_CAST (element));
}


/**
 * GstSubtitleBlock:
 * @mini_object: the parent #GstMiniObject.
 * @style_set:
 *
 */
struct _GstSubtitleBlock
{
  GstMiniObject mini_object;

  GstSubtitleStyleSet *style_set;

  /*< private >*/
  GPtrArray *elements;
  gpointer _gst_reserved[GST_PADDING];
};

GType gst_subtitle_block_get_type (void);

GstSubtitleBlock * gst_subtitle_block_new (GstSubtitleStyleSet * style_set);

void gst_subtitle_block_add_element (
    GstSubtitleBlock * block,
    GstSubtitleElement * element);

guint gst_subtitle_block_get_element_count (const GstSubtitleBlock * block);

GstSubtitleElement * gst_subtitle_block_get_element (
    const GstSubtitleBlock * block, guint index);

/**
 * gst_subtitle_block_ref:
 * @block: a #GstSubtitleBlock.
 *
 * Increments the refcount of @block.
 *
 * Returns: (transfer full): @block.
 */
static inline GstSubtitleBlock *
gst_subtitle_block_ref (GstSubtitleBlock * block)
{
  return (GstSubtitleBlock *)
    gst_mini_object_ref (GST_MINI_OBJECT_CAST (block));
}

/**
 * gst_subtitle_block_unref:
 * @block: (transfer full): a #GstSubtitleBlock.
 *
 * Decrements the refcount of @block. If the refcount reaches 0, @block will
 * be freed.
 */
static inline void
gst_subtitle_block_unref (GstSubtitleBlock * block)
{
  gst_mini_object_unref (GST_MINI_OBJECT_CAST (block));
}


/**
 * GstSubtitleRegion:
 * @mini_object: the parent #GstMiniObject.
 * @style_set:
 *
 */
struct _GstSubtitleRegion
{
  GstMiniObject mini_object;

  GstSubtitleStyleSet *style_set;

  /*< private >*/
  GPtrArray *blocks;
  gpointer _gst_reserved[GST_PADDING];
};

GType gst_subtitle_region_get_type (void);

GstSubtitleRegion * gst_subtitle_region_new (GstSubtitleStyleSet * style_set);

void gst_subtitle_region_add_block (
    GstSubtitleRegion * region,
    GstSubtitleBlock * block);

guint gst_subtitle_region_get_block_count (const GstSubtitleRegion * region);

GstSubtitleBlock * gst_subtitle_region_get_block (
    const GstSubtitleRegion * region, guint index);

/**
 * gst_subtitle_region_ref:
 * @region: a #GstSubtitleRegion.
 *
 * Increments the refcount of @region.
 *
 * Returns: (transfer full): @region.
 */
static inline GstSubtitleRegion *
gst_subtitle_region_ref (GstSubtitleRegion * region)
{
  return (GstSubtitleRegion *)
    gst_mini_object_ref (GST_MINI_OBJECT_CAST (region));
}

/**
 * gst_subtitle_region_unref:
 * @region: (transfer full): a #GstSubtitleRegion.
 *
 * Decrements the refcount of @region. If the refcount reaches 0, @region will be
 * freed.
 */
static inline void
gst_subtitle_region_unref (GstSubtitleRegion * region)
{
  gst_mini_object_unref (GST_MINI_OBJECT_CAST (region));
}

G_END_DECLS

#endif /* __GST_SUBTITLE_H__ */
