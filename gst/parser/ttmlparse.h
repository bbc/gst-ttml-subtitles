/* GStreamer TTML subtitle parser
 * Copyright (C) <2015> British Broadcasting Corporation
 *   Authors:
 *     Chris Bass <dash@rd.bbc.co.uk>
 *     Peter Taylour <dash@rd.bbc.co.uk>
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

#ifndef _TTML_PARSE_H_
#define _TTML_PARSE_H_

#include "gstttmlparse.h"

G_BEGIN_DECLS

typedef struct _TtmlStyleSet TtmlStyleSet;
typedef struct _TtmlElement TtmlElement;
typedef struct _TtmlScene TtmlScene;


struct _TtmlStyleSet {
  const gchar *text_direction;
  const gchar *font_family;
  const gchar *font_size;
  const gchar *line_height;
  const gchar *text_align;
  const gchar *color;
  const gchar *background_color;
  const gchar *font_style;
  const gchar *font_weight;
  const gchar *text_decoration;
  const gchar *unicode_bidi;
  const gchar *wrap_option;
  const gchar *multi_row_align;
  const gchar *line_padding;
  const gchar *origin;
  const gchar *extent;
  const gchar *display_align;
  const gchar *overflow;
  const gchar *padding;
  const gchar *writing_mode;
  const gchar *show_background;
};


typedef enum {
  TTML_ELEMENT_TYPE_STYLE,
  TTML_ELEMENT_TYPE_REGION,
  TTML_ELEMENT_TYPE_BODY,
  TTML_ELEMENT_TYPE_DIV,
  TTML_ELEMENT_TYPE_P,
  TTML_ELEMENT_TYPE_SPAN,
  TTML_ELEMENT_TYPE_ANON_SPAN,
  TTML_ELEMENT_TYPE_BR
} TtmlElementType;


typedef enum {
  TTML_WHITESPACE_MODE_NONE,
  TTML_WHITESPACE_MODE_DEFAULT,
  TTML_WHITESPACE_MODE_PRESERVE,
} TtmlWhitespaceMode;


struct _TtmlElement {
  TtmlElementType type;
  gchar *id;
  gchar **styles;
  gchar *region;
  GstClockTime begin;
  GstClockTime end;
  TtmlStyleSet *style_set;
  gchar *text;
  guint text_index;
  TtmlWhitespaceMode whitespace_mode;
};


/* Represents a static scene consisting of one or more text elements that
 * should be visible over a specific period of time. */
struct _TtmlScene {
  GstClockTime begin;
  GstClockTime end;
  GList *elements;
  GstBuffer *buf;
};


GList *ttml_parse (const gchar * file, GstClockTime begin,
    GstClockTime duration);

G_END_DECLS
#endif /* _TTML_PARSE_H_ */
