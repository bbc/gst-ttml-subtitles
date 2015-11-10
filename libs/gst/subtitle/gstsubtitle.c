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
 * @short_description: Library for describing sets of static subtitles.
 *
 * This library enables the description of static text scenes made up of a
 * number of regions, which may contain a number of block and inline text
 * elements. It is derived from the concepts and features defined in the Timed
 * Text Markup Language 1 (TTML1), Second Edition
 * (http://www.w3.org/TR/ttaf1-dfxp), and the EBU-TT-D profile of TTML1
 * (https://tech.ebu.ch/files/live/sites/tech/files/shared/tech/tech3380.pdf).
 */

#include "gstsubtitle.h"

/**
 * gst_subtitle_style_set_new:
 *
 * Create a new #GstSubtitleStyleSet with default values for all properties.
 *
 * Returns: (transfer full): A newly-allocated #GstSubtitleStyleSet.
 * */
GstSubtitleStyleSet *
gst_subtitle_style_set_new ()
{
  GstSubtitleStyleSet *ret = g_slice_new0 (GstSubtitleStyleSet);
  GstSubtitleColor white = { 255, 255, 255, 255 };
  GstSubtitleColor transparent = { 0, 0, 0, 0 };

  ret->font_family = g_strdup ("default");
  ret->font_size = 1.0;
  ret->line_height = 1.25;
  ret->color = white;
  ret->bg_color = transparent;
  ret->line_padding = 0.0;
  ret->origin_x = ret->origin_y = 0.0;
  ret->extent_w = ret->extent_h = 0.0;
  ret->padding_start = ret->padding_end
    = ret->padding_before = ret->padding_after = 0.0;

  return ret;
}

/**
 * gst_subtitle_style_set_free:
 * @styleset: A #GstSubtitleStyleSet.
 *
 * Free @styleset and its associated memory.
 */
void gst_subtitle_style_set_free (GstSubtitleStyleSet * styleset)
{
  g_return_if_fail (styleset != NULL);
  g_free (styleset->font_family);
  g_slice_free (GstSubtitleStyleSet, styleset);
}


static void
_gst_subtitle_element_free (GstSubtitleElement * element)
{
  g_return_if_fail (element != NULL);
  gst_subtitle_style_set_free (element->style_set);
  g_slice_free (GstSubtitleElement, element);
}

GST_DEFINE_MINI_OBJECT_TYPE (GstSubtitleElement, gst_subtitle_element);

/**
 * gst_subtitle_element_new:
 * @style_set: (transfer full): a #GstSubtitleStyleSet that defines the styling
 * and layout associated with this inline text element.
 * @text_index: the index within a #GstBuffer of the #GstMemory that contains
 * the text of this inline text element.
 *
 * Allocates a new #GstSubtitleElement.
 *
 * Returns: (transfer full): a newly-allocated #GstSubtitleElement. Unref
 * with gst_subtitle_element_unref() when no longer needed.
 */
GstSubtitleElement *
gst_subtitle_element_new (GstSubtitleStyleSet * style_set,
    guint text_index)
{
  GstSubtitleElement *element;

  g_return_val_if_fail (style_set != NULL, NULL);

  element = g_slice_new0 (GstSubtitleElement);
  gst_mini_object_init (GST_MINI_OBJECT_CAST (element), 0,
      gst_subtitle_element_get_type (), NULL, NULL,
      (GstMiniObjectFreeFunction) _gst_subtitle_element_free);

  element->style_set = style_set;
  element->text_index = text_index;

  return element;
}

static void
_gst_subtitle_block_free (GstSubtitleBlock * block)
{
  g_return_if_fail (block != NULL);
  gst_subtitle_style_set_free (block->style_set);
  g_slice_free (GstSubtitleBlock, block);
}

GST_DEFINE_MINI_OBJECT_TYPE (GstSubtitleBlock, gst_subtitle_block);


/**
 * gst_subtitle_block_new:
 * @style_set: (transfer full): a #GstSubtitleStyleSet that defines the styling
 * and layout associated with this block of text elements.
 *
 * Allocates a new #GstSubtitleBlock.
 *
 * Returns: (transfer full): a newly-allocated #GstSubtitleBlock. Unref
 * with gst_subtitle_block_unref() when no longer needed.
 */
GstSubtitleBlock *
gst_subtitle_block_new (GstSubtitleStyleSet * style_set)
{
  GstSubtitleBlock *block;

  g_return_val_if_fail (style_set != NULL, NULL);

  block = g_slice_new0 (GstSubtitleBlock);
  gst_mini_object_init (GST_MINI_OBJECT_CAST (block), 0,
      gst_subtitle_block_get_type (), NULL, NULL,
      (GstMiniObjectFreeFunction) _gst_subtitle_block_free);

  block->style_set = style_set;

  return block;
}

/**
 * gst_subtitle_block_add_element:
 * @block: a #GstSubtitleBlock.
 * @element: a #GstSubtitleElement to add.
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
  gst_subtitle_style_set_free (region->style_set);
  g_slice_free (GstSubtitleRegion, region);
}

GST_DEFINE_MINI_OBJECT_TYPE (GstSubtitleRegion, gst_subtitle_region);


/**
 * gst_subtitle_region_new:
 * @style_set: (transfer full): a #GstSubtitleStyleSet that defines the styling
 * and layout associated with this region.
 *
 * Allocates a new #GstSubtitleRegion.
 *
 * Returns: (transfer full): a newly-allocated #GstSubtitleRegion. Unref
 * with gst_subtitle_region_unref() when no longer needed.
 */
GstSubtitleRegion *
gst_subtitle_region_new (GstSubtitleStyleSet * style_set)
{
  GstSubtitleRegion *region;

  g_return_val_if_fail (style_set != NULL, NULL);

  region = g_slice_new0 (GstSubtitleRegion);
  gst_mini_object_init (GST_MINI_OBJECT_CAST (region), 0,
      gst_subtitle_region_get_type (), NULL, NULL,
      (GstMiniObjectFreeFunction) _gst_subtitle_region_free);

  region->style_set = style_set;

  return region;
}

/**
 * gst_subtitle_region_add_block:
 * @region: a #GstSubtitleRegion.
 * @block: (transfer full): a #GstSubtitleBlock which should be added
 * to @region's array of blocks.
 *
 * Adds a #GstSubtitleBlock to the end of the array of blocks held by @region.
 * @region will take ownership of @block, and will unref it when @region
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

