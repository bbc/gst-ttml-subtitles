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

/**
 * SECTION:gstsubtitle
 * @short_description:
 *
 */

#include "gstsubtitle.h"

/* Create a new GstSubtitleStyleSet with default values for all properties. */
GstSubtitleStyleSet *
gst_subtitle_style_set_new ()
{
  GstSubtitleStyleSet *ret = g_slice_new0 (GstSubtitleStyleSet);
  GstSubtitleColor white = { 255, 255, 255, 255 };
  GstSubtitleColor transparent = { 0, 0, 0, 0 };

  ret->text_direction = GST_SUBTITLE_TEXT_DIRECTION_LTR;
  ret->font_family = g_strdup ("default");
  ret->font_size = 1.0;
  ret->line_height = 1.25;
  ret->text_align = GST_SUBTITLE_TEXT_ALIGN_START;
  ret->color = white;
  ret->bg_color = transparent;
  ret->font_style = GST_SUBTITLE_FONT_STYLE_NORMAL;
  ret->font_weight = GST_SUBTITLE_FONT_WEIGHT_NORMAL;
  ret->text_decoration = GST_SUBTITLE_TEXT_DECORATION_NONE;
  ret->unicode_bidi = GST_SUBTITLE_UNICODE_BIDI_NORMAL;
  ret->wrap_option = GST_SUBTITLE_WRAPPING_ON;
  ret->multi_row_align = GST_SUBTITLE_MULTI_ROW_ALIGN_AUTO;
  ret->line_padding = 0.0;
  ret->origin_x = ret->origin_y = 0.0;
  ret->extent_w = ret->extent_h = 0.0;
  ret->display_align = GST_SUBTITLE_DISPLAY_ALIGN_BEFORE;
  ret->padding_start = ret->padding_end
    = ret->padding_before = ret->padding_after = 0.0;
  ret->writing_mode = GST_SUBTITLE_WRITING_MODE_LRTB;
  ret->show_background = GST_SUBTITLE_BACKGROUND_MODE_ALWAYS;
  ret->overflow = GST_SUBTITLE_OVERFLOW_MODE_HIDDEN;

  return ret;
}

void gst_subtitle_style_set_free (GstSubtitleStyleSet * styleset)
{
  g_return_if_fail (styleset != NULL);
  g_free (styleset->font_family);
  g_slice_free (GstSubtitleStyleSet, styleset);
}

void
gst_subtitle_style_set_copy (const GstSubtitleStyleSet * src,
    GstSubtitleStyleSet * dest)
{
  dest->text_direction = src->text_direction;
  dest->font_family = g_strdup (src->font_family);
  dest->font_size = src->font_size;
  dest->line_height = src->line_height;
  dest->text_align = src->text_align;
  dest->color = src->color;
  dest->bg_color = src->bg_color;
  dest->font_style = src->font_style;
  dest->font_weight = src->font_weight;
  dest->text_decoration = src->text_decoration;
  dest->unicode_bidi = src->unicode_bidi;
  dest->wrap_option = src->wrap_option;
  dest->multi_row_align = src->multi_row_align;
  dest->line_padding = src->line_padding;
  dest->origin_x = src->origin_x;
  dest->origin_y = src->origin_y;
  dest->extent_w = src->extent_w;
  dest->extent_h = src->extent_h;
  dest->display_align = src->display_align;
  dest->padding_start = src->padding_start;
  dest->padding_end = src->padding_end;
  dest->padding_before = src->padding_before;
  dest->padding_after = src->padding_after;
  dest->writing_mode = src->writing_mode;
  dest->show_background = src->show_background;
  dest->overflow = src->overflow;
}


static void
_gst_subtitle_element_free (GstSubtitleElement * element)
{
  g_return_if_fail (element != NULL);
  g_slice_free (GstSubtitleElement, element);
}

GST_DEFINE_MINI_OBJECT_TYPE (GstSubtitleElement, gst_subtitle_element);

/**
 * gst_subtitle_element_new:
 * @style_set:
 * @text_index:
 *
 * Allocates a new #GstSubtitleElement.
 *
 * Returns: (transfer full): a newly-allocated #GstSubtitleElement. Unref
 * with gst_subtitle_element_unref() when no longer needed.
 */
GstSubtitleElement *
gst_subtitle_element_new (const GstSubtitleStyleSet * style_set,
    guint text_index)
{
  GstSubtitleElement *element;

  g_return_val_if_fail (style_set != NULL, NULL);

  element = g_slice_new0 (GstSubtitleElement);
  gst_mini_object_init (GST_MINI_OBJECT_CAST (element), 0,
      gst_subtitle_element_get_type (), NULL, NULL,
      (GstMiniObjectFreeFunction) _gst_subtitle_element_free);

  gst_subtitle_style_set_copy (style_set, &(element->style_set));
  element->text_index = text_index;

  return element;
}

static void
_gst_subtitle_block_free (GstSubtitleBlock * block)
{
  g_return_if_fail (block != NULL);
  g_slice_free (GstSubtitleBlock, block);
}

GST_DEFINE_MINI_OBJECT_TYPE (GstSubtitleBlock, gst_subtitle_block);


/**
 * gst_subtitle_block_new:
 * @style_set:
 *
 * Allocates a new #GstSubtitleBlock.
 *
 * Returns: (transfer full): a newly-allocated #GstSubtitleBlock. Unref
 * with gst_subtitle_block_unref() when no longer needed.
 */
GstSubtitleBlock *
gst_subtitle_block_new (const GstSubtitleStyleSet * style_set)
{
  GstSubtitleBlock *block;

  g_return_val_if_fail (style_set != NULL, NULL);

  block = g_slice_new0 (GstSubtitleBlock);
  gst_mini_object_init (GST_MINI_OBJECT_CAST (block), 0,
      gst_subtitle_block_get_type (), NULL, NULL,
      (GstMiniObjectFreeFunction) _gst_subtitle_block_free);

  gst_subtitle_style_set_copy (style_set, &(block->style_set));

  return block;
}

/**
 * gst_subtitle_block_add_element:
 * @block: a #GstSubtitleBlock.
 * @element:
 *
 * Adds a #GstSubtitleElement to @block.
 */
void
gst_subtitle_block_add_element (GstSubtitleBlock * block,
    GstSubtitleElement * element)
{
  g_return_if_fail (block != NULL);
  g_return_if_fail (element != NULL);

  if (!block->elements)
    block->elements = g_ptr_array_new_with_free_func (
        (GDestroyNotify) gst_subtitle_element_unref);

  g_ptr_array_add (block->elements, element);
}

/**
 * gst_subtitle_block_get_element_count:
 * @block: a #GstSubtitleBlock.
 *
 * Returns: the number of #GstSubtitleElements in @block.
 */
guint
gst_subtitle_block_get_element_count (const GstSubtitleBlock * block)
{
  g_return_val_if_fail (block != NULL, 0);

  if (!block->elements)
    return 0;
  else
    return block->elements->len;
}

/**
 * gst_subtitle_block_get_element:
 * @block: a #GstSubtitleBlock.
 * @index: index of the element to get.
 *
 * Gets the #GstSubtitleElement at @index in the array of elements held by
 * @block.
 *
 * Returns: (transfer none): the #GstSubtitleElement at @index in the array of
 * elements held by @block, or %NULL if @index is out-of-bounds. The
 * function does not return a reference; the caller should obtain a reference
 * using gst_subtitle_block_ref(), if needed.
 */
GstSubtitleElement *
gst_subtitle_block_get_element (const GstSubtitleBlock * block, guint index)
{
  g_return_val_if_fail (block != NULL, NULL);

  if (!block->elements || index >= block->elements->len)
    return NULL;
  else
    return g_ptr_array_index (block->elements, index);
}

static void
_gst_subtitle_region_free (GstSubtitleRegion * region)
{
  g_return_if_fail (region != NULL);
  g_slice_free (GstSubtitleRegion, region);
}

GST_DEFINE_MINI_OBJECT_TYPE (GstSubtitleRegion, gst_subtitle_region);


/**
 * gst_subtitle_region_new:
 * @style_set:
 *
 * Allocates a new #GstSubtitleRegion.
 *
 * Returns: (transfer full): a newly-allocated #GstSubtitleRegion. Unref
 * with gst_subtitle_region_unref() when no longer needed.
 */
GstSubtitleRegion *
gst_subtitle_region_new (const GstSubtitleStyleSet * style_set)
{
  GstSubtitleRegion *region;

  g_return_val_if_fail (style_set != NULL, NULL);

  region = g_slice_new0 (GstSubtitleRegion);
  gst_mini_object_init (GST_MINI_OBJECT_CAST (region), 0,
      gst_subtitle_region_get_type (), NULL, NULL,
      (GstMiniObjectFreeFunction) _gst_subtitle_region_free);

  gst_subtitle_style_set_copy (style_set, &(region->style_set));

  return region;
}

/**
 * gst_subtitle_region_add_block:
 * @region: a #GstSubtitleRegion.
 * @block: (transfer full): a #GstSubtitleBlock which should be added
 * to @region's array of blocks.
 *
 * Adds a #GstSubtitleBlock to the end of the array of blocks held by
 * @region. @region will take ownership of @block, and will unref it when @region
 * is freed.
 */
void
gst_subtitle_region_add_block (GstSubtitleRegion * region, GstSubtitleBlock * block)
{
  g_return_if_fail (region != NULL);
  g_return_if_fail (block != NULL);

  if (!region->blocks)
    region->blocks = g_ptr_array_new_with_free_func (
        (GDestroyNotify) gst_subtitle_block_unref);

  g_ptr_array_add (region->blocks, block);
}

/**
 * gst_subtitle_region_get_block_count:
 * @region: a #GstSubtitleRegion.
 *
 * Returns: the number of blocks in @region.
 */
guint
gst_subtitle_region_get_block_count (const GstSubtitleRegion * region)
{
  g_return_val_if_fail (region != NULL, 0);

  if (!region->blocks)
    return 0;
  else
    return region->blocks->len;
}

/**
 * gst_subtitle_region_get_block:
 * @region: a #GstSubtitleRegion.
 * @index: index of the block to get.
 *
 * Gets the block at @index in the array of blocks held by @region.
 *
 * Returns: (transfer none): the #GstSubtitleBlock at @index in the array of
 * blocks held by @region, or %NULL if @index is out-of-bounds. The
 * function does not return a reference; the caller should obtain a reference
 * using gst_subtitle_region_ref(), if needed.
 */
GstSubtitleBlock *
gst_subtitle_region_get_block (const GstSubtitleRegion * region, guint index)
{
  g_return_val_if_fail (region != NULL, NULL);

  if (!region->blocks || index >= region->blocks->len)
    return NULL;
  else
    return g_ptr_array_index (region->blocks, index);
}

