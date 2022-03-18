/* GStreamer
 * Copyright (C) <2002> David A. Schleef <ds@schleef.org>
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
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

#ifndef __GST_TTMLPARSE_H__
#define __GST_TTMLPARSE_H__

#include <gst/gst.h>
#include <gst/base/gstadapter.h>

GST_DEBUG_CATEGORY_EXTERN (ttml_parse_debug);
#define GST_CAT_DEFAULT ttml_parse_debug

G_BEGIN_DECLS

#define GST_TYPE_TTMLPARSE \
  (gst_ttml_parse_get_type ())
#define GST_TTMLPARSE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_TTMLPARSE, GstTtmlParse))
#define GST_TTMLPARSE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_TTMLPARSE, GstTtmlParseClass))
#define GST_IS_TTMLPARSE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_TTMLPARSE))
#define GST_IS_TTMLPARSE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_TTMLPARSE))

typedef struct _GstTtmlParse GstTtmlParse;
typedef struct _GstTtmlParseClass GstTtmlParseClass;

/* format enum */
typedef enum
{
  GST_TTML_PARSE_FORMAT_UNKNOWN = 0,
  //GST_TTML_PARSE_FORMAT_MDVDSUB   = 1,
  //GST_TTML_PARSE_FORMAT_SUBRIP    = 2,
  //GST_TTML_PARSE_FORMAT_MPSUB     = 3,
  //GST_TTML_PARSE_FORMAT_SAMI      = 4,
  //GST_TTML_PARSE_FORMAT_TMPLAYER  = 5,
  //GST_TTML_PARSE_FORMAT_MPL2      = 6,
  //GST_TTML_PARSE_FORMAT_SUBVIEWER = 7,
  //GST_TTML_PARSE_FORMAT_DKS       = 8,
  //GST_TTML_PARSE_FORMAT_QTTEXT    = 9,
  //GST_TTML_PARSE_FORMAT_LRC       = 10,
  GST_TTML_PARSE_FORMAT_TTML = 11
} GstTtmlParseFormat;

typedef struct {
  int state;
  GString* buf;
  guint64 start_time;
  guint64 duration;
  guint64 max_duration; /* to clamp duration, 0 = no limit (used by tmplayer parser) */
  GstSegment* segment;
  gpointer user_data;
  gboolean have_internal_fps; /* If TRUE don't overwrite fps by property */
  gint fps_n, fps_d;          /* used by frame based parsers */
} ParserState;

typedef gchar* (*Parser) (ParserState* state, const gchar* line);

struct _GstTtmlParse {
  GstElement element;

  GstPad *sinkpad, *srcpad;

  /* contains the input in the input encoding */
  GstAdapter* adapter;
  /* contains the UTF-8 decoded input */
  GString* textbuf;

  GstTtmlParseFormat parser_type;
  gboolean parser_detected;
  const gchar* subtitle_codec;

  Parser parse_line;
  ParserState state;

  /* seek */
  guint64 offset;

  /* Segment */
  GstSegment segment;
  gboolean need_segment;

  gboolean flushing;
  gboolean valid_utf8;
  gchar* detected_encoding;
  gchar* encoding;

  gboolean first_buffer;

  /* used by frame based parsers */
  gint fps_n, fps_d;
};

struct _GstTtmlParseClass {
  GstElementClass parent_class;
};

GType gst_ttml_parse_get_type (void);

G_END_DECLS

#endif /* __GST_TTMLPARSE_H__ */
