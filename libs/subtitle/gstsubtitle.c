/* GStreamer
 * Copyright (C) <2015> British Broadcasting Corporation
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
 * @short_description: Support library for ISOBMFF Common Encryption.
 *
 * This library includes data types and functions that enable support for
 * ISOBMFF content that is protected using Common Encryption (ISO/IEC 23001-7).
 */

#include "gstsubtitle.h"

/* Create a new GstSubtitleStyleSet with default values for all properties. */
GstSubtitleStyleSet *
gst_subtitle_style_set_new ()
{
  GstSubtitleStyleSet *ret = g_slice_new0 (GstSubtitleStyleSet);
  GstSubtitleColor white = { 1.0, 1.0, 1.0, 1.0 };
  GstSubtitleColor transparent = { 0.0, 0.0, 0.0, 0.0 };

  ret->text_direction = GST_SUBTITLE_TEXT_DIRECTION_LTR;
  g_strlcpy (ret->font_family, "default", MAX_FONT_FAMILY_NAME_LENGTH);
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


static void
_gst_subtitle_element_free (GstSubtitleElement * element)
{
  g_return_if_fail (element != NULL);
  g_slice_free (GstSubtitleElement, element);
}

GST_DEFINE_MINI_OBJECT_TYPE (GstSubtitleElement, gst_subtitle_element);

/**
 * gst_subtitle_element_new:
 * @n_bytes_clear: the number of clear (unencrypted) bytes in the subsample.
 * @n_bytes_encrypted: the number of encrypted bytes in the subsample.
 *
 * Allocates a new #GstSubtitleElement.
 *
 * Returns: (transfer full): a newly-allocated #GstSubtitleElement. Unref
 * with gst_subtitle_element_unref() when no longer needed.
 */
GstSubtitleElement *
gst_subtitle_element_new (const GstSubtitleStyleSet * style, guint text_index)
{
  GstSubtitleElement *element;

  g_return_val_if_fail (style != NULL, NULL);

  element = g_slice_new0 (GstSubtitleElement);
  gst_mini_object_init (GST_MINI_OBJECT_CAST (element), 0,
      gst_subtitle_element_get_type (), NULL, NULL,
      (GstMiniObjectFreeFunction) _gst_subtitle_element_free);

  gst_subtitle_style_set_copy (style, &(element->style));
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
 * @iv_data: (transfer none): pointer to the start of the initialization vector
 * in memory.
 * @iv_size: size in bytes of the initialization vector pointed at by @iv_data;
 * only sizes of 8 or 16 bytes are allowed.
 *
 * Allocates a new #GstSubtitleBlock, using the initialization vector
 * located at @iv_data; the function will take a copy of the data at @iv_data.
 *
 * Returns: (transfer full): a newly-allocated #GstSubtitleBlock. Unref
 * with gst_subtitle_block_unref() when no longer needed.
 */
GstSubtitleBlock *
gst_subtitle_block_new (const GstSubtitleStyleSet * style)
{
  GstSubtitleBlock *block;

  g_return_val_if_fail (style != NULL, NULL);

  block = g_slice_new0 (GstSubtitleBlock);
  gst_mini_object_init (GST_MINI_OBJECT_CAST (block), 0,
      gst_subtitle_block_get_type (), NULL, NULL,
      (GstMiniObjectFreeFunction) _gst_subtitle_block_free);

  gst_subtitle_style_set_copy (style, &(block->style));

  return block;
}

/**
 * gst_subtitle_block_add_element:
 * @block: a #GstSubtitleBlock.
 * @element: (transfer full): a #GstSubtitleElement which should be added
 * to @crypt's array of subsamples.
 *
 * Adds a #GstSubtitleElement to the end of the array of subsamples held by
 * @block. @block will take ownership of @element, and will unref it when @block
 * is freed. A #GstSubtitleBlock may hold a maximum of 2^16 - 1
 * subsamples.
 */
void
gst_subtitle_block_add_element (GstSubtitleBlock * block,
    GstSubtitleElement * element)
{
  g_return_if_fail (block != NULL);
  g_return_if_fail (element != NULL);

  if (!block->elements) {
    block->elements = g_ptr_array_new_with_free_func (
        (GDestroyNotify) gst_subtitle_element_unref);
  }

  g_ptr_array_add (block->elements, element);
}

/**
 * gst_subtitle_block_get_subsample_count:
 * @block: a #GstSubtitleBlock.
 *
 * Returns: the number of subsamples in the subsample array held by @block.
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
 * @index: index of the subsample to get.
 *
 * Gets the subsample at @index in the array of subsamples held by @block.
 *
 * Returns: (transfer none): the #GstSubtitleElement at @index in the array of
 * subsamples held by @block, or %NULL if @index is out-of-bounds. The
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
_gst_subtitle_area_free (GstSubtitleArea * area)
{
  g_return_if_fail (area != NULL);
  g_slice_free (GstSubtitleArea, area);
}

GST_DEFINE_MINI_OBJECT_TYPE (GstSubtitleArea, gst_subtitle_area);


/**
 * gst_subtitle_area_new:
 * @is_encrypted: flag indicating whether the sample is encrypted (%TRUE) or not
 * (%FALSE).
 * @iv_size: size in bytes of the sample's initialization vector; only sizes of
 * 0, 8 or 16 are allowed. If @is_encrypted is %FALSE, @iv_size must be 0;
 * if it is %TRUE, @iv_size must be greater than 0.
 * @key_id_data: (transfer none)(allow-none): pointer to the start of the key
 * ID in memory. If @is_encrypted is %FALSE, @key_id_data must be %NULL;
 * if it is %TRUE, @key_id_data must be non-%NULL.
 *
 * Allocates a new #GstSubtitleArea, using the key ID located at
 * @key_id_data; the function will take a copy of the data at @key_id_data.
 *
 * Returns: (transfer full): a newly-allocated #GstSubtitleArea. Unref
 * with gst_subtitle_area_unref() when no longer needed.
 */
GstSubtitleArea *
gst_subtitle_area_new (const GstSubtitleStyleSet * style)
{
  GstSubtitleArea *area;

  g_return_val_if_fail (style != NULL, NULL);

  area = g_slice_new0 (GstSubtitleArea);
  gst_mini_object_init (GST_MINI_OBJECT_CAST (area), 0,
      gst_subtitle_area_get_type (), NULL, NULL,
      (GstMiniObjectFreeFunction) _gst_subtitle_area_free);

  gst_subtitle_style_set_copy (style, &(area->style));

  return area;
}

/**
 * gst_subtitle_area_add_block:
 * @area: a #GstSubtitleArea.
 * @block: (transfer full): a #GstSubtitleBlock which should be added
 * to @crypt's array of subsamples.
 *
 * Adds a #GstSubtitleBlock to the end of the array of subsamples held by
 * @area. @area will take ownership of @block, and will unref it when @area
 * is freed. A #GstSubtitleArea may hold a maximum of 2^16 - 1
 * subsamples.
 */
void
gst_subtitle_area_add_block (GstSubtitleArea * area, GstSubtitleBlock * block)
{
  g_return_if_fail (area != NULL);
  g_return_if_fail (block != NULL);

  if (!area->blocks) {
    area->blocks = g_ptr_array_new_with_free_func (
        (GDestroyNotify) gst_subtitle_block_unref);
  }

  g_ptr_array_add (area->blocks, block);
}

/**
 * gst_subtitle_area_get_subsample_count:
 * @area: a #GstSubtitleArea.
 *
 * Returns: the number of subsamples in the subsample array held by @area.
 */
guint
gst_subtitle_area_get_block_count (const GstSubtitleArea * area)
{
  g_return_val_if_fail (area != NULL, 0);

  if (!area->blocks)
    return 0;
  else
    return area->blocks->len;
}

/**
 * gst_subtitle_area_get_block:
 * @area: a #GstSubtitleArea.
 * @index: index of the subsample to get.
 *
 * Gets the subsample at @index in the array of subsamples held by @area.
 *
 * Returns: (transfer none): the #GstSubtitleBlock at @index in the array of
 * subsamples held by @area, or %NULL if @index is out-of-bounds. The
 * function does not return a reference; the caller should obtain a reference
 * using gst_subtitle_area_ref(), if needed.
 */
GstSubtitleBlock *
gst_subtitle_area_get_block (const GstSubtitleArea * area, guint index)
{
  g_return_val_if_fail (area != NULL, NULL);

  if (!area->blocks || index >= area->blocks->len)
    return NULL;
  else
    return g_ptr_array_index (area->blocks, index);
}
