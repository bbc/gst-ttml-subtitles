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
 * SECTION:gstsubtitlemeta
 * @short_description: Metadata class for ISOBMFF Common Encryption.
 *
 * The GstSubtitleMeta class enables the information needed to decrypt a
 * subtitle-protected #GstBuffer to be attached to that buffer.
 *
 * Typically, an ISOBMFF demuxer element would attach GstSubtitleMeta objects to
 * the buffers that it pushes downstream. The demuxer would parse the
 * protection information for a video/audio frame from its input data and use
 * this information to populate #GstSubtitleSampleProperties and
 * #GstSubtitleSampleCryptoInfo objects, which are then encapsulated in a
 * GstSubtitleMeta object and attached to the corresponding output buffer using the
 * gst_buffer_add_subtitle_meta() function. The information in this attached
 * GstSubtitleMeta would be used by a downstream decrypter element to recover the
 * original unencrypted frame.
 */

#include "gstsubtitlemeta.h"

GType
gst_subtitle_meta_api_get_type (void)
{
  static volatile GType type;
  static const gchar *tags[] = { "memory", NULL };

  if (g_once_init_enter (&type)) {
    GType _type = gst_meta_api_type_register ("GstSubtitleMetaAPI", tags);
    g_once_init_leave (&type, _type);
  }
  return type;
}

gboolean
gst_subtitle_meta_init (GstMeta * meta, gpointer params, GstBuffer * buffer)
{
  /* XXX: Do we need checks that parameters are non-NULL? */
  GstSubtitleMeta *subtitle_meta = (GstSubtitleMeta *) meta;

  subtitle_meta->areas = NULL;
  return TRUE;
}

void
gst_subtitle_meta_free (GstMeta * meta, GstBuffer * buffer)
{
  /* XXX: Do we need checks that parameters are non-NULL? */
  GstSubtitleMeta *subtitle_meta = (GstSubtitleMeta *) meta;

  if (subtitle_meta->areas)
    g_ptr_array_unref (subtitle_meta->areas);
}

const GstMetaInfo *
gst_subtitle_meta_get_info (void)
{
  static const GstMetaInfo *subtitle_meta_info = NULL;

  if (g_once_init_enter (&subtitle_meta_info)) {
    const GstMetaInfo *meta =
        gst_meta_register (GST_SUBTITLE_META_API_TYPE, "GstSubtitleMeta",
          sizeof (GstSubtitleMeta), gst_subtitle_meta_init,
          gst_subtitle_meta_free, (GstMetaTransformFunction) NULL);
    g_once_init_leave (&subtitle_meta_info, meta);
  }
  return subtitle_meta_info;
}

/**
 * gst_buffer_add_subtitle_meta:
 * @buffer: #GstBuffer holding a subtitle-protected sample, to which subtitle metadata
 * should be added.
 * @areas: a #GstSubtitleSampleProperties describing the areas of the
 * sample contained in @buffer.
 * @crypto_info: (allow-none): a #GstSubtitleSampleCryptoInfo holding cryptographic
 * information relating to the sample contained in @buffer. If
 * @areas->is_encrypted is %FALSE, @crypto_info may be %NULL.
 *
 * Attaches subtitle metadata to a #GstBuffer. If @areas->is_encrypted is
 * %TRUE, @crypto_info must point to a #GstSubtitleSampleCryptoInfo whose
 * initialization vector size equals @areas->iv_size.
 *
 * Returns: a pointer to the added #GstSubtitleMeta if successful; %NULL if
 * unsuccessful.
 */
GstSubtitleMeta *
gst_buffer_add_subtitle_meta (GstBuffer * buffer, GPtrArray * areas)
{
  GstSubtitleMeta *meta;

  g_return_val_if_fail (GST_IS_BUFFER (buffer), NULL);
  g_return_val_if_fail (areas != NULL, NULL);

  meta = (GstSubtitleMeta *) gst_buffer_add_meta (buffer,
      GST_SUBTITLE_META_INFO, NULL);

  /*gst_subtitle_sample_areas_ref (areas);*/ /* XXX: Take ownership of areas array(?) */

  meta->areas = areas;
  return meta;
}
