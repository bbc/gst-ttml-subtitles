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

/*
 * Parses subtitle files encoded using the EBU-TT-D profile of TTML, as defined
 * in https://tech.ebu.ch/files/live/sites/tech/files/shared/tech/tech3380.pdf
 * and http://www.w3.org/TR/ttaf1-dfxp/, respectively.
 */

#include <glib.h>
#include <gst/subtitle/subtitle.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

#include "ttmlparse.h"

#define DEFAULT_CELLRES_X 32
#define DEFAULT_CELLRES_Y 15
#define MAX_FONT_FAMILY_NAME_LENGTH 128

GST_DEBUG_CATEGORY_STATIC (ttmlparse);

static gchar * ttml_get_xml_property (const xmlNode * node, const char * name);


static guint8
ttml_hex_pair_to_byte (const gchar * hex_pair)
{
  gint hi_digit, lo_digit;

  hi_digit = g_ascii_xdigit_value (*hex_pair);
  lo_digit = g_ascii_xdigit_value (*(hex_pair + 1));
  return (hi_digit << 4) + lo_digit;
}


/* Color strings in EBU-TT-D can have the form "#RRBBGG" or "#RRBBGGAA". */
static GstSubtitleColor
ttml_parse_colorstring (const gchar * color)
{
  guint length;
  const gchar *c = NULL;
  GstSubtitleColor ret = { 0, 0, 0, 0 };

  if (!color)
    return ret;

  length = strlen (color);
  if (((length == 7) || (length == 9)) && *color == '#') {
    c = color + 1;

    ret.r = ttml_hex_pair_to_byte (c);
    ret.g = ttml_hex_pair_to_byte (c + 2);
    ret.b = ttml_hex_pair_to_byte (c + 4);

    if (length == 7)
      ret.a = G_MAXUINT8;
    else
      ret.a = ttml_hex_pair_to_byte (c + 6);

    GST_CAT_LOG (ttmlparse, "Returning color - r:%u  b:%u  g:%u  a:%u",
        ret.r, ret.b, ret.g, ret.a);
  } else {
    GST_CAT_ERROR (ttmlparse, "Invalid color string: %s", color);
  }

  return ret;
}


static void
ttml_print_style_set (TtmlStyleSet * set)
{
  if (!set) {
    GST_CAT_LOG (ttmlparse, "\t\t[NULL]");
    return;
  }

  if (set->text_direction)
    GST_CAT_LOG (ttmlparse, "\t\ttext_direction: %s", set->text_direction);
  if (set->font_family)
    GST_CAT_LOG (ttmlparse, "\t\tfont_family: %s", set->font_family);
  if (set->font_size)
    GST_CAT_LOG (ttmlparse, "\t\tfont_size: %s", set->font_size);
  if (set->line_height)
    GST_CAT_LOG (ttmlparse, "\t\tline_height: %s", set->line_height);
  if (set->text_align)
    GST_CAT_LOG (ttmlparse, "\t\ttext_align: %s", set->text_align);
  if (set->color)
    GST_CAT_LOG (ttmlparse, "\t\tcolor: %s", set->color);
  if (set->background_color)
    GST_CAT_LOG (ttmlparse, "\t\tbackground_color: %s", set->background_color);
  if (set->font_style)
    GST_CAT_LOG (ttmlparse, "\t\tfont_style: %s", set->font_style);
  if (set->font_weight)
    GST_CAT_LOG (ttmlparse, "\t\tfont_weight: %s", set->font_weight);
  if (set->text_decoration)
    GST_CAT_LOG (ttmlparse, "\t\ttext_decoration: %s", set->text_decoration);
  if (set->unicode_bidi)
    GST_CAT_LOG (ttmlparse, "\t\tunicode_bidi: %s", set->unicode_bidi);
  if (set->wrap_option)
    GST_CAT_LOG (ttmlparse, "\t\twrap_option: %s", set->wrap_option);
  if (set->multi_row_align)
    GST_CAT_LOG (ttmlparse, "\t\tmulti_row_align: %s", set->multi_row_align);
  if (set->line_padding)
    GST_CAT_LOG (ttmlparse, "\t\tline_padding: %s", set->line_padding);
  if (set->origin)
    GST_CAT_LOG (ttmlparse, "\t\torigin: %s", set->origin);
  if (set->extent)
    GST_CAT_LOG (ttmlparse, "\t\textent: %s", set->extent);
  if (set->display_align)
    GST_CAT_LOG (ttmlparse, "\t\tdisplay_align: %s", set->display_align);
  if (set->overflow)
    GST_CAT_LOG (ttmlparse, "\t\toverflow: %s", set->overflow);
  if (set->padding)
    GST_CAT_LOG (ttmlparse, "\t\tpadding: %s", set->padding);
  if (set->writing_mode)
    GST_CAT_LOG (ttmlparse, "\t\twriting_mode: %s", set->writing_mode);
  if (set->show_background)
    GST_CAT_LOG (ttmlparse, "\t\tshow_background: %s", set->show_background);
}


static TtmlStyleSet *
ttml_parse_style_set (const xmlNode * node)
{
  TtmlStyleSet *s;
  gchar *value = NULL;

  value = ttml_get_xml_property (node, "id");
  if (!value) {
    GST_CAT_ERROR (ttmlparse, "styles must have an ID.");
    return NULL;
  }
  g_free (value);

  s = g_slice_new0 (TtmlStyleSet);

  if ((value = ttml_get_xml_property (node, "direction"))) {
    s->text_direction = g_strdup (value);
    g_free (value);
  }
  if ((value = ttml_get_xml_property (node, "fontFamily"))) {
    s->font_family = g_strdup (value);
    g_free (value);
  }
  if ((value = ttml_get_xml_property (node, "fontSize"))) {
    s->font_size = g_strdup (value);
    g_free (value);
  }
  if ((value = ttml_get_xml_property (node, "lineHeight"))) {
    s->line_height = g_strdup (value);
    g_free (value);
  }
  if ((value = ttml_get_xml_property (node, "textAlign"))) {
    s->text_align = g_strdup (value);
    g_free (value);
  }
  if ((value = ttml_get_xml_property (node, "color"))) {
    s->color = g_strdup (value);
    g_free (value);
  }
  if ((value = ttml_get_xml_property (node, "backgroundColor"))) {
    s->background_color = g_strdup (value);
    g_free (value);
  }
  if ((value = ttml_get_xml_property (node, "fontStyle"))) {
    s->font_style = g_strdup (value);
    g_free (value);
  }
  if ((value = ttml_get_xml_property (node, "fontWeight"))) {
    s->font_weight = g_strdup (value);
    g_free (value);
  }
  if ((value = ttml_get_xml_property (node, "textDecoration"))) {
    s->text_decoration = g_strdup (value);
    g_free (value);
  }
  if ((value = ttml_get_xml_property (node, "unicodeBidi"))) {
    s->unicode_bidi = g_strdup (value);
    g_free (value);
  }
  if ((value = ttml_get_xml_property (node, "wrapOption"))) {
    s->wrap_option = g_strdup (value);
    g_free (value);
  }
  if ((value = ttml_get_xml_property (node, "multiRowAlign"))) {
    s->multi_row_align = g_strdup (value);
    g_free (value);
  }
  if ((value = ttml_get_xml_property (node, "linePadding"))) {
    s->line_padding = g_strdup (value);
    g_free (value);
  }
  if ((value = ttml_get_xml_property (node, "origin"))) {
    s->origin = g_strdup (value);
    g_free (value);
  }
  if ((value = ttml_get_xml_property (node, "extent"))) {
    s->extent = g_strdup (value);
    g_free (value);
  }
  if ((value = ttml_get_xml_property (node, "displayAlign"))) {
    s->display_align = g_strdup (value);
    g_free (value);
  }
  if ((value = ttml_get_xml_property (node, "overflow"))) {
    s->overflow = g_strdup (value);
    g_free (value);
  }
  if ((value = ttml_get_xml_property (node, "padding"))) {
    s->padding = g_strdup (value);
    g_free (value);
  }
  if ((value = ttml_get_xml_property (node, "writingMode"))) {
    s->writing_mode = g_strdup (value);
    g_free (value);
  }
  if ((value = ttml_get_xml_property (node, "showBackground"))) {
    s->show_background = g_strdup (value);
    g_free (value);
  }

  return s;
}


static void
ttml_delete_style_set (TtmlStyleSet * style_set)
{
  g_free ((gpointer) style_set->text_direction);
  g_free ((gpointer) style_set->font_family);
  g_free ((gpointer) style_set->font_size);
  g_free ((gpointer) style_set->line_height);
  g_free ((gpointer) style_set->text_align);
  g_free ((gpointer) style_set->color);
  g_free ((gpointer) style_set->background_color);
  g_free ((gpointer) style_set->font_style);
  g_free ((gpointer) style_set->font_weight);
  g_free ((gpointer) style_set->text_decoration);
  g_free ((gpointer) style_set->unicode_bidi);
  g_free ((gpointer) style_set->wrap_option);
  g_free ((gpointer) style_set->multi_row_align);
  g_free ((gpointer) style_set->line_padding);
  g_free ((gpointer) style_set->origin);
  g_free ((gpointer) style_set->extent);
  g_free ((gpointer) style_set->display_align);
  g_free ((gpointer) style_set->overflow);
  g_free ((gpointer) style_set->padding);
  g_free ((gpointer) style_set->writing_mode);
  g_free ((gpointer) style_set->show_background);
  g_slice_free (TtmlStyleSet, style_set);
}


static void
ttml_delete_element (TtmlElement * element)
{
  g_free ((gpointer) element->id);
  if (element->styles) g_strfreev (element->styles);
  g_free ((gpointer) element->region);
  if (element->style_set) ttml_delete_style_set (element->style_set);
  g_free ((gpointer) element->text);
  g_slice_free (TtmlElement, element);
}


static gchar *
ttml_get_xml_property (const xmlNode * node, const char *name)
{
  xmlChar *xml_string = NULL;
  gchar *gst_string = NULL;

  g_return_val_if_fail (strlen (name) < 128, NULL);

  xml_string = xmlGetProp (node, (xmlChar *) name);
  if (!xml_string)
    return NULL;
  gst_string = g_strdup ((gchar *) xml_string);
  xmlFree (xml_string);
  return gst_string;
}


/* EBU-TT-D timecodes have format hours:minutes:seconds[.fraction] */
static GstClockTime
ttml_parse_timecode (const gchar * timestring)
{
  gchar **strings;
  guint64 hours = 0, minutes = 0, seconds = 0, milliseconds = 0;
  GstClockTime time = GST_CLOCK_TIME_NONE;

  GST_CAT_LOG (ttmlparse, "time string: %s", timestring);

  strings = g_strsplit (timestring, ":", 3);
  if (g_strv_length (strings) != 3U) {
    GST_CAT_ERROR (ttmlparse, "badly formatted time string: %s", timestring);
    return time;
  }

  hours = g_ascii_strtoull (strings[0], NULL, 10U);
  minutes = g_ascii_strtoull (strings[1], NULL, 10U);
  if (g_strstr_len (strings[2], -1, ".")) {
    guint n_digits;
    gchar ** substrings = g_strsplit (strings[2], ".", 2);
    seconds = g_ascii_strtoull (substrings[0], NULL, 10U);
    n_digits = strlen (substrings[1]);
    milliseconds = g_ascii_strtoull (substrings[1], NULL, 10U);
    milliseconds =
      (guint64) (milliseconds * pow (10.0, (3 - (double) n_digits)));
    g_strfreev (substrings);
  } else {
    seconds = g_ascii_strtoull (strings[2], NULL, 10U);
  }

  if (minutes > 59 || seconds > 60) {
    GST_CAT_ERROR (ttmlparse, "invalid time string "
        "(minutes or seconds out-of-bounds): %s\n", timestring);
  }

  g_strfreev (strings);
  GST_CAT_LOG (ttmlparse,
      "hours: %" G_GUINT64_FORMAT "  minutes: %" G_GUINT64_FORMAT
      "  seconds: %" G_GUINT64_FORMAT "  milliseconds: %" G_GUINT64_FORMAT "",
      hours,  minutes,  seconds,  milliseconds);

  time = hours * GST_SECOND * 3600
       + minutes * GST_SECOND * 60
       + seconds * GST_SECOND
       + milliseconds * GST_MSECOND;

  return time;
}


static TtmlElement *
ttml_parse_element (const xmlNode * node)
{
  TtmlElement *element;
  TtmlElementType type;
  gchar *value;

  GST_CAT_DEBUG (ttmlparse, "Element name: %s", (const char*) node->name);
  if ((g_strcmp0 ((const char*) node->name, "style") == 0)) {
    type = TTML_ELEMENT_TYPE_STYLE;
  } else if ((g_strcmp0 ((const char*) node->name, "region") == 0)) {
    type = TTML_ELEMENT_TYPE_REGION;
  } else if ((g_strcmp0 ((const char*) node->name, "body") == 0)) {
    type = TTML_ELEMENT_TYPE_BODY;
  } else if ((g_strcmp0 ((const char*) node->name, "div") == 0)) {
    type = TTML_ELEMENT_TYPE_DIV;
  } else if ((g_strcmp0 ((const char*) node->name, "p") == 0)) {
    type = TTML_ELEMENT_TYPE_P;
  } else if ((g_strcmp0 ((const char*) node->name, "span") == 0)) {
    type = TTML_ELEMENT_TYPE_SPAN;
  } else if ((g_strcmp0 ((const char*) node->name, "text") == 0)) {
    type = TTML_ELEMENT_TYPE_ANON_SPAN;
  } else if ((g_strcmp0 ((const char*) node->name, "br") == 0)) {
    type = TTML_ELEMENT_TYPE_BR;
  } else {
    return NULL;
  }

  element = g_slice_new0 (TtmlElement);
  element->type = type;

  if ((value = ttml_get_xml_property (node, "id"))) {
    element->id = g_strdup (value);
    g_free (value);
  }

  if ((value = ttml_get_xml_property (node, "style"))) {
    element->styles = g_strsplit (value, " ", 0);
    GST_CAT_DEBUG (ttmlparse, "%u style(s) referenced in element.",
        g_strv_length (element->styles));
    g_free (value);
  }

  if (element->type == TTML_ELEMENT_TYPE_STYLE
      || element->type == TTML_ELEMENT_TYPE_REGION) {
    TtmlStyleSet *ss;
    ss = ttml_parse_style_set (node);
    if (ss)
      element->style_set = ss;
    else
      GST_CAT_WARNING (ttmlparse,
          "Style or Region contains no styling attributes.");
  }

  if ((value = ttml_get_xml_property (node, "region"))) {
    element->region = g_strdup (value);
    g_free (value);
  }

  if ((value = ttml_get_xml_property (node, "begin"))) {
    element->begin = ttml_parse_timecode (value);
    g_free (value);
  } else {
    element->begin = GST_CLOCK_TIME_NONE;
  }

  if ((value = ttml_get_xml_property (node, "end"))) {
    element->end = ttml_parse_timecode (value);
    g_free (value);
  } else {
    element->end = GST_CLOCK_TIME_NONE;
  }

  if (node->content) {
    GST_CAT_LOG (ttmlparse, "Node content: %s", node->content);
    element->text = g_strdup ((const gchar*) node->content);
  }

  if ((value = ttml_get_xml_property (node, "space"))) {
    if (g_strcmp0 (value, "preserve") == 0)
      element->whitespace_mode = TTML_WHITESPACE_MODE_PRESERVE;
    else if (g_strcmp0 (value, "default") == 0)
      element->whitespace_mode = TTML_WHITESPACE_MODE_DEFAULT;
    g_free (value);
  }

  return element;
}


static GNode *
ttml_parse_body (const xmlNode * node)
{
  GNode *ret;
  TtmlElement *element;

  GST_CAT_LOG (ttmlparse, "parsing node %s", node->name);
  element = ttml_parse_element (node);
  if (element)
    ret = g_node_new (element);
  else
    return NULL;

  for (node = node->children; node != NULL; node = node->next) {
    GNode *descendants = NULL;
    if (!xmlIsBlankNode (node) && (descendants = ttml_parse_body (node)))
        g_node_append (ret, descendants);
  }

  return ret;
}


/* Update the fields of a GstSubtitleStyleSet, @style_set, according to the
 * values defined in a TtmlStyleSet, @tss, and a given cell resolution. */
static void
ttml_update_style_set (GstSubtitleStyleSet * style_set, TtmlStyleSet * tss,
    guint cellres_x, guint cellres_y)
{
  if (tss->text_direction) {
    if (g_strcmp0 (tss->text_direction, "rtl") == 0)
      style_set->text_direction = GST_SUBTITLE_TEXT_DIRECTION_RTL;
    else
      style_set->text_direction = GST_SUBTITLE_TEXT_DIRECTION_LTR;
  }

  if (tss->font_family) {
    if (strlen (tss->font_family) <= MAX_FONT_FAMILY_NAME_LENGTH) {
      g_free (style_set->font_family);
      style_set->font_family = g_strdup (tss->font_family);
    } else {
      GST_CAT_WARNING (ttmlparse,
          "Ignoring font family name as it's overly long.");
    }
  }

  if (tss->font_size) {
    style_set->font_size = g_ascii_strtod (tss->font_size, NULL) / 100.0;
  }
  style_set->font_size *= (1.0 / cellres_y);

  if (tss->line_height) {
    /* The TTML spec (section 8.2.12) recommends using a line height of 125%
     * when "normal" is specified. */
    if (g_strcmp0 (tss->line_height, "normal") == 0)
      style_set->line_height = 1.25;
    else
      style_set->line_height = g_ascii_strtod (tss->line_height, NULL) / 100.0;
  }

  if (tss->text_align) {
    if (g_strcmp0 (tss->text_align, "left") == 0)
      style_set->text_align = GST_SUBTITLE_TEXT_ALIGN_LEFT;
    else if (g_strcmp0 (tss->text_align, "center") == 0)
      style_set->text_align = GST_SUBTITLE_TEXT_ALIGN_CENTER;
    else if (g_strcmp0 (tss->text_align, "right") == 0)
      style_set->text_align = GST_SUBTITLE_TEXT_ALIGN_RIGHT;
    else if (g_strcmp0 (tss->text_align, "end") == 0)
      style_set->text_align = GST_SUBTITLE_TEXT_ALIGN_END;
    else
      style_set->text_align = GST_SUBTITLE_TEXT_ALIGN_START;
  }

  if (tss->color) {
    style_set->color = ttml_parse_colorstring (tss->color);
  }

  if (tss->background_color) {
    style_set->background_color =
      ttml_parse_colorstring (tss->background_color);
  }

  if (tss->font_style) {
    if (g_strcmp0 (tss->font_style, "italic") == 0)
      style_set->font_style = GST_SUBTITLE_FONT_STYLE_ITALIC;
    else
      style_set->font_style = GST_SUBTITLE_FONT_STYLE_NORMAL;
  }

  if (tss->font_weight) {
    if (g_strcmp0 (tss->font_weight, "bold") == 0)
      style_set->font_weight = GST_SUBTITLE_FONT_WEIGHT_BOLD;
    else
      style_set->font_weight = GST_SUBTITLE_FONT_WEIGHT_NORMAL;
  }

  if (tss->text_decoration) {
    if (g_strcmp0 (tss->text_decoration, "underline") == 0)
      style_set->text_decoration = GST_SUBTITLE_TEXT_DECORATION_UNDERLINE;
    else
      style_set->text_decoration = GST_SUBTITLE_TEXT_DECORATION_NONE;
  }

  if (tss->unicode_bidi) {
    if (g_strcmp0 (tss->unicode_bidi, "embed") == 0)
      style_set->unicode_bidi = GST_SUBTITLE_UNICODE_BIDI_EMBED;
    else if (g_strcmp0 (tss->unicode_bidi, "bidiOverride") == 0)
      style_set->unicode_bidi = GST_SUBTITLE_UNICODE_BIDI_OVERRIDE;
    else
      style_set->unicode_bidi = GST_SUBTITLE_UNICODE_BIDI_NORMAL;
  }

  if (tss->wrap_option) {
    if (g_strcmp0 (tss->wrap_option, "noWrap") == 0)
      style_set->wrap_option = GST_SUBTITLE_WRAPPING_OFF;
    else
      style_set->wrap_option = GST_SUBTITLE_WRAPPING_ON;
  }

  if (tss->multi_row_align) {
    if (g_strcmp0 (tss->multi_row_align, "start") == 0)
      style_set->multi_row_align = GST_SUBTITLE_MULTI_ROW_ALIGN_START;
    else if (g_strcmp0 (tss->multi_row_align, "center") == 0)
      style_set->multi_row_align = GST_SUBTITLE_MULTI_ROW_ALIGN_CENTER;
    else if (g_strcmp0 (tss->multi_row_align, "end") == 0)
      style_set->multi_row_align = GST_SUBTITLE_MULTI_ROW_ALIGN_END;
    else
      style_set->multi_row_align = GST_SUBTITLE_MULTI_ROW_ALIGN_AUTO;
  }

  if (tss->line_padding) {
    style_set->line_padding = g_ascii_strtod (tss->line_padding, NULL);
    style_set->line_padding *= (1.0 / cellres_x);
  }

  if (tss->origin) {
    gchar *c;
    style_set->origin_x = g_ascii_strtod (tss->origin, &c) / 100.0;
    while (!g_ascii_isdigit (*c) && *c != '+' && *c != '-') ++c;
    style_set->origin_y = g_ascii_strtod (c, NULL) / 100.0;
  }

  if (tss->extent) {
    gchar *c;
    style_set->extent_w = g_ascii_strtod (tss->extent, &c) / 100.0;
    if ((style_set->origin_x + style_set->extent_w) > 1.0) {
      style_set->extent_w = 1.0 - style_set->origin_x;
    }
    while (!g_ascii_isdigit (*c) && *c != '+' && *c != '-') ++c;
    style_set->extent_h = g_ascii_strtod (c, NULL) / 100.0;
    if ((style_set->origin_y + style_set->extent_h) > 1.0) {
      style_set->extent_h = 1.0 - style_set->origin_y;
    }
  }

  if (tss->display_align) {
    if (g_strcmp0 (tss->display_align, "center") == 0)
      style_set->display_align = GST_SUBTITLE_DISPLAY_ALIGN_CENTER;
    else if (g_strcmp0 (tss->display_align, "after") == 0)
      style_set->display_align = GST_SUBTITLE_DISPLAY_ALIGN_AFTER;
    else
      style_set->display_align = GST_SUBTITLE_DISPLAY_ALIGN_BEFORE;
  }

  if (tss->padding) {
    gchar **decimals;
    guint n_decimals;
    guint i;

    decimals = g_strsplit (tss->padding, "%", 0);
    n_decimals = g_strv_length (decimals) - 1;
    for (i = 0; i < n_decimals; ++i)
      g_strstrip (decimals[i]);

    switch (n_decimals) {
      case 1:
        style_set->padding_start = style_set->padding_end =
          style_set->padding_before = style_set->padding_after =
          g_ascii_strtod (decimals[0], NULL) / 100.0;
        break;

      case 2:
        style_set->padding_before = style_set->padding_after =
          g_ascii_strtod (decimals[0], NULL) / 100.0;
        style_set->padding_start = style_set->padding_end =
          g_ascii_strtod (decimals[1], NULL) / 100.0;
        break;

      case 3:
        style_set->padding_before = g_ascii_strtod (decimals[0], NULL) / 100.0;
        style_set->padding_start = style_set->padding_end =
          g_ascii_strtod (decimals[1], NULL) / 100.0;
        style_set->padding_after = g_ascii_strtod (decimals[2], NULL) / 100.0;
        break;

      case 4:
        style_set->padding_before = g_ascii_strtod (decimals[0], NULL) / 100.0;
        style_set->padding_end = g_ascii_strtod (decimals[1], NULL) / 100.0;
        style_set->padding_after = g_ascii_strtod (decimals[2], NULL) / 100.0;
        style_set->padding_start = g_ascii_strtod (decimals[3], NULL) / 100.0;
        break;
    }
    g_strfreev (decimals);

    /* Padding values in TTML files are relative to the region width & height;
     * make them relative to the overall display width & height like all other
     * dimensions. */
    style_set->padding_before *= style_set->extent_h;
    style_set->padding_after *= style_set->extent_h;
    style_set->padding_end *= style_set->extent_w;
    style_set->padding_start *= style_set->extent_w;
  }

  if (tss->writing_mode) {
    if (g_str_has_prefix (tss->writing_mode, "rl"))
      style_set->writing_mode = GST_SUBTITLE_WRITING_MODE_RLTB;
    else if ((g_strcmp0 (tss->writing_mode, "tbrl") == 0)
        || (g_strcmp0 (tss->writing_mode, "tb") == 0))
      style_set->writing_mode = GST_SUBTITLE_WRITING_MODE_TBRL;
    else if (g_strcmp0 (tss->writing_mode, "tblr") == 0)
      style_set->writing_mode = GST_SUBTITLE_WRITING_MODE_TBLR;
    else
      style_set->writing_mode = GST_SUBTITLE_WRITING_MODE_LRTB;
  }

  if (tss->show_background) {
    if (g_strcmp0 (tss->show_background, "whenActive") == 0)
      style_set->show_background = GST_SUBTITLE_BACKGROUND_MODE_WHEN_ACTIVE;
    else
      style_set->show_background = GST_SUBTITLE_BACKGROUND_MODE_ALWAYS;
  }

  if (tss->overflow) {
    if (g_strcmp0 (tss->overflow, "visible") == 0)
      style_set->overflow = GST_SUBTITLE_OVERFLOW_MODE_VISIBLE;
    else
      style_set->overflow = GST_SUBTITLE_OVERFLOW_MODE_HIDDEN;
  }
}


static TtmlStyleSet *
ttml_copy_style_set (TtmlStyleSet * style_set)
{
  TtmlStyleSet *ret;

  ret = g_slice_new0 (TtmlStyleSet);

  if (style_set->text_direction)
    ret->text_direction = g_strdup (style_set->text_direction);
  if (style_set->font_family)
    ret->font_family = g_strdup (style_set->font_family);
  if (style_set->font_size)
    ret->font_size = g_strdup (style_set->font_size);
  if (style_set->line_height)
    ret->line_height = g_strdup (style_set->line_height);
  if (style_set->text_align)
    ret->text_align = g_strdup (style_set->text_align);
  if (style_set->color)
    ret->color = g_strdup (style_set->color);
  if (style_set->background_color)
    ret->background_color = g_strdup (style_set->background_color);
  if (style_set->font_style)
    ret->font_style = g_strdup (style_set->font_style);
  if (style_set->font_weight)
    ret->font_weight = g_strdup (style_set->font_weight);
  if (style_set->text_decoration)
    ret->text_decoration = g_strdup (style_set->text_decoration);
  if (style_set->unicode_bidi)
    ret->unicode_bidi = g_strdup (style_set->unicode_bidi);
  if (style_set->wrap_option)
    ret->wrap_option = g_strdup (style_set->wrap_option);
  if (style_set->multi_row_align)
    ret->multi_row_align = g_strdup (style_set->multi_row_align);
  if (style_set->line_padding)
    ret->line_padding = g_strdup (style_set->line_padding);
  if (style_set->origin)
    ret->origin = g_strdup (style_set->origin);
  if (style_set->extent)
    ret->extent = g_strdup (style_set->extent);
  if (style_set->display_align)
    ret->display_align = g_strdup (style_set->display_align);
  if (style_set->overflow)
    ret->overflow = g_strdup (style_set->overflow);
  if (style_set->padding)
    ret->padding = g_strdup (style_set->padding);
  if (style_set->writing_mode)
    ret->writing_mode = g_strdup (style_set->writing_mode);
  if (style_set->show_background)
    ret->show_background = g_strdup (style_set->show_background);

  return ret;
}


/* s2 overrides s1. Unlike style inheritance, merging will result in all values from s1 being merged into s2. */
static TtmlStyleSet *
ttml_merge_style_sets (TtmlStyleSet * set1, TtmlStyleSet * set2)
{
  TtmlStyleSet *ret = NULL;

  if (set1) {
    ret = ttml_copy_style_set (set1);

    if (set2) {
      if (set2->text_direction)
        ret->text_direction = g_strdup (set2->text_direction);
      if (set2->font_family)
        ret->font_family = g_strdup (set2->font_family);
      if (set2->font_size)
        ret->font_size = g_strdup (set2->font_size);
      if (set2->line_height)
        ret->line_height = g_strdup (set2->line_height);
      if (set2->text_align)
        ret->text_align = g_strdup (set2->text_align);
      if (set2->background_color)
        ret->background_color = g_strdup (set2->background_color);
      if (set2->color)
        ret->color = g_strdup (set2->color);
      if (set2->font_style)
        ret->font_style = g_strdup (set2->font_style);
      if (set2->font_weight)
        ret->font_weight = g_strdup (set2->font_weight);
      if (set2->text_decoration)
        ret->text_decoration = g_strdup (set2->text_decoration);
      if (set2->unicode_bidi)
        ret->unicode_bidi = g_strdup (set2->unicode_bidi);
      if (set2->wrap_option)
        ret->wrap_option = g_strdup (set2->wrap_option);
      if (set2->multi_row_align)
        ret->multi_row_align = g_strdup (set2->multi_row_align);
      if (set2->line_padding)
        ret->line_padding = g_strdup (set2->line_padding);
      if (set2->origin)
        ret->origin = g_strdup (set2->origin);
      if (set2->extent)
        ret->extent = g_strdup (set2->extent);
      if (set2->display_align)
        ret->display_align = g_strdup (set2->display_align);
      if (set2->overflow)
        ret->overflow = g_strdup (set2->overflow);
      if (set2->padding)
        ret->padding = g_strdup (set2->padding);
      if (set2->writing_mode)
        ret->writing_mode = g_strdup (set2->writing_mode);
      if (set2->show_background)
        ret->show_background = g_strdup (set2->show_background);
    }
  } else if (set2) {
    ret = ttml_copy_style_set (set2);
  }

  return ret;
}


static const gchar *
ttml_get_relative_font_size (const gchar * parent_size,
    const gchar * child_size)
{
  guint psize = (guint) g_ascii_strtoull (parent_size, NULL, 10U);
  guint csize = (guint) g_ascii_strtoull (child_size, NULL, 10U);
  csize = (csize * psize) / 100U;
  return g_strdup_printf ("%u%%", csize);
}


static TtmlStyleSet *
ttml_inherit_styling (TtmlStyleSet * parent, TtmlStyleSet * child)
{
  TtmlStyleSet *ret = NULL;

  /*
   * The following styling attributes are not inherited:
   *   - tts:backgroundColor
   *   - tts:origin
   *   - tts:extent
   *   - tts:displayAlign
   *   - tts:overflow
   *   - tts:padding
   *   - tts:writingMode
   *   - tts:showBackground
   *   - tts:unicodeBidi
   */

  if (child) {
    ret = ttml_copy_style_set (child);
  } else {
    ret = g_slice_new0 (TtmlStyleSet);
  }

  if (parent) {
    if (parent->text_direction && !ret->text_direction)
      ret->text_direction = g_strdup (parent->text_direction);
    if (parent->font_family && !ret->font_family)
      ret->font_family = g_strdup (parent->font_family);

    /* In TTML, if an element which has a defined fontSize is the child of an
     * element that also has a defined fontSize, the child's font size is
     * relative to that of its parent. If its parent doesn't have a defined
     * fontSize, then the child's fontSize is relative to the document's cell
     * size. Therefore, if the former is true, we calculate the value of
     * font_size based on the parent's font_size; otherwise, we simply keep the
     * value defined in the child's style set. */
    if (parent->font_size) {
      if (!ret->font_size) {
        ret->font_size = g_strdup (parent->font_size);
      } else {
        const gchar *tmp = ret->font_size;
        ret->font_size = ttml_get_relative_font_size (parent->font_size,
            child->font_size);
        GST_CAT_LOG (ttmlparse, "Calculated font size: %s", ret->font_size);
        g_free ((gpointer) tmp);
      }
    }

    if (parent->line_height && !ret->line_height)
      ret->line_height = g_strdup (parent->line_height);
    if (parent->text_align && !ret->text_align)
      ret->text_align = g_strdup (parent->text_align);
    if (parent->color && !ret->color)
      ret->color = g_strdup (parent->color);
    if (parent->font_style && !ret->font_style)
      ret->font_style = g_strdup (parent->font_style);
    if (parent->font_weight && !ret->font_weight)
      ret->font_weight = g_strdup (parent->font_weight);
    if (parent->text_decoration && !ret->text_decoration)
      ret->text_decoration = g_strdup (parent->text_decoration);
    if (parent->wrap_option && !ret->wrap_option)
      ret->wrap_option = g_strdup (parent->wrap_option);
    if (parent->multi_row_align && !ret->multi_row_align)
      ret->multi_row_align = g_strdup (parent->multi_row_align);
    if (parent->line_padding && !ret->line_padding)
      ret->line_padding = g_strdup (parent->line_padding);
  }

  return ret;
}


static gchar *
ttml_get_element_type_string (TtmlElement * element)
{
  switch (element->type) {
    case TTML_ELEMENT_TYPE_STYLE:
      return g_strdup ("<style>");
      break;
    case TTML_ELEMENT_TYPE_REGION:
      return g_strdup ("<region>");
      break;
    case TTML_ELEMENT_TYPE_BODY:
      return g_strdup ("<body>");
      break;
    case TTML_ELEMENT_TYPE_DIV:
      return g_strdup ("<div>");
      break;
    case TTML_ELEMENT_TYPE_P:
      return g_strdup ("<p>");
      break;
    case TTML_ELEMENT_TYPE_SPAN:
      return g_strdup ("<span>");
      break;
    case TTML_ELEMENT_TYPE_ANON_SPAN:
      return g_strdup ("<anon-span>");
      break;
    case TTML_ELEMENT_TYPE_BR:
      return g_strdup ("<br>");
      break;
    default:
      return g_strdup ("Unknown");
      break;
  }
}


/* Merge styles referenced by an element. */
gboolean
ttml_resolve_styles (GNode * node, gpointer data)
{
  TtmlStyleSet *tmp = NULL;
  TtmlElement *element, *style;
  GHashTable *styles_table;
  gchar *type_string;
  guint i;

  styles_table = (GHashTable *)data;
  element = node->data;

  type_string = ttml_get_element_type_string (element);
  GST_CAT_LOG (ttmlparse, "Element type: %s", type_string);
  g_free (type_string);

  if (!element->styles)
    return FALSE;

  for (i = 0; i < g_strv_length (element->styles); ++i) {
    tmp = element->style_set;
    style = g_hash_table_lookup (styles_table, element->styles[i]);
    if (style) {
      GST_CAT_LOG (ttmlparse, "Merging style %s...", element->styles[i]);
      element->style_set = ttml_merge_style_sets (element->style_set,
          style->style_set);
      if (tmp)
        ttml_delete_style_set (tmp);
    } else {
      GST_CAT_WARNING (ttmlparse, "Element references an unknown style (%s)",
          element->styles[i]);
    }
  }

  GST_CAT_LOG (ttmlparse, "Style set after merging:");
  ttml_print_style_set (element->style_set);

  return FALSE;
}


static void
ttml_resolve_referenced_styles (GList * trees, GHashTable * styles_table)
{
  GList * tree;

  for (tree = g_list_first (trees); tree; tree = tree->next) {
    GNode *root = (GNode *)tree->data;
    g_node_traverse (root, G_PRE_ORDER, G_TRAVERSE_ALL, -1, ttml_resolve_styles,
        styles_table);
  }
}


/* Inherit styling attributes from parent. */
gboolean
ttml_inherit_styles (GNode * node, gpointer data)
{
  TtmlStyleSet *tmp = NULL;
  TtmlElement *element, *parent;
  gchar *type_string;

  element = node->data;

  type_string = ttml_get_element_type_string (element);
  GST_CAT_LOG (ttmlparse, "Element type: %s", type_string);
  g_free (type_string);

  if (node->parent) {
    parent = node->parent->data;
    if (parent->style_set) {
      tmp = element->style_set;
      if (element->type == TTML_ELEMENT_TYPE_ANON_SPAN) {
        /* Anon spans should merge all style attributes from their parent. */
        element->style_set = ttml_merge_style_sets (parent->style_set,
            element->style_set);
      } else {
        element->style_set = ttml_inherit_styling (parent->style_set,
            element->style_set);
      }
      if (tmp) ttml_delete_style_set (tmp);
    }
  }

  GST_CAT_LOG (ttmlparse, "Style set after inheriting:");
  ttml_print_style_set (element->style_set);

  return FALSE;
}


static void
ttml_inherit_element_styles (GList * trees)
{
  GList * tree;

  for (tree = g_list_first (trees); tree; tree = tree->next) {
    GNode *root = (GNode *)tree->data;
    g_node_traverse (root, G_PRE_ORDER, G_TRAVERSE_ALL, -1, ttml_inherit_styles,
        NULL);
  }
}


/* If whitespace_mode isn't explicitly set for this element, inherit from its
 * parent. If this element is the root of the tree, set whitespace_mode to
 * that of the overall document. */
static gboolean
ttml_inherit_element_whitespace_mode (GNode * node, gpointer data)
{
  TtmlWhitespaceMode *doc_mode = (TtmlWhitespaceMode *)data;
  TtmlElement *element = node->data;
  TtmlElement *parent;

  if (element->whitespace_mode != TTML_WHITESPACE_MODE_NONE)
    return FALSE;

  if (G_NODE_IS_ROOT (node)) {
    element->whitespace_mode = *doc_mode;
    return FALSE;
  }

  parent = node->parent->data;
  element->whitespace_mode = parent->whitespace_mode;
  return FALSE;
}


static void
ttml_inherit_whitespace_mode (GNode * tree, TtmlWhitespaceMode doc_mode)
{
  g_node_traverse (tree, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
      ttml_inherit_element_whitespace_mode, &doc_mode);
}


static gboolean
ttml_resolve_element_timings (GNode * node, gpointer data)
{
  TtmlElement *element, *leaf;

  leaf = element = node->data;

  if (GST_CLOCK_TIME_IS_VALID (leaf->begin)
      && GST_CLOCK_TIME_IS_VALID (leaf->end)) {
    GST_CAT_LOG (ttmlparse, "Leaf node already has timing.");
    return FALSE;
  }

  /* Inherit timings from ancestor. */
  while (node->parent && !GST_CLOCK_TIME_IS_VALID (element->begin)) {
    node = node->parent;
    element = node->data;
  }

  if (!GST_CLOCK_TIME_IS_VALID (element->begin)) {
    GST_CAT_WARNING (ttmlparse,
        "No timing found for element. Removing from tree...");
    g_node_unlink (node);
  } else {
    leaf->begin = element->begin;
    leaf->end = element->end;
    GST_CAT_LOG (ttmlparse, "Leaf begin: %" GST_TIME_FORMAT,
        GST_TIME_ARGS (leaf->begin));
    GST_CAT_LOG (ttmlparse, "Leaf end: %" GST_TIME_FORMAT,
        GST_TIME_ARGS (leaf->end));
  }

  return FALSE;
}


static void
ttml_resolve_timings (GNode * tree)
{
  g_node_traverse (tree, G_PRE_ORDER, G_TRAVERSE_LEAVES, -1,
      ttml_resolve_element_timings, NULL);
}


static gboolean
ttml_resolve_leaf_region (GNode * node, gpointer data)
{
  TtmlElement *element, *leaf;
  leaf = element = node->data;

  while (node->parent && !element->region) {
    node = node->parent;
    element = node->data;
  }

  if (element->region) {
    leaf->region = g_strdup (element->region);
    GST_CAT_LOG (ttmlparse, "Leaf region: %s", leaf->region);
  } else {
    GST_CAT_WARNING (ttmlparse, "No region found above leaf element.");
  }

  return FALSE;
}


static void
ttml_resolve_regions (GNode * tree)
{
  g_node_traverse (tree, G_PRE_ORDER, G_TRAVERSE_LEAVES, -1,
      ttml_resolve_leaf_region, NULL);
}


typedef struct {
  GstClockTime start_time;
  GstClockTime next_transition_time;
} TrState;


static gboolean
ttml_update_transition_time (GNode * node, gpointer data)
{
  TtmlElement *element = node->data;
  TrState *state = (TrState *)data;

  if ((element->begin < state->next_transition_time)
      && (element->begin > state->start_time)) {
    state->next_transition_time = element->begin;
    GST_CAT_LOG (ttmlparse,
        "Updating next transition time to element begin time (%"
        GST_TIME_FORMAT ")", GST_TIME_ARGS (state->next_transition_time));
    return FALSE;
  }

  if ((element->end < state->next_transition_time)
      && (element->end > state->start_time)) {
    state->next_transition_time = element->end;
    GST_CAT_LOG (ttmlparse,
        "Updating next transition time to element end time (%"
        GST_TIME_FORMAT ")", GST_TIME_ARGS (state->next_transition_time));
  }

  return FALSE;
}


/* Return details about the next transition after @time. */
static GstClockTime
ttml_find_next_transition (GList * trees, GstClockTime time)
{
  TrState state;
  state.start_time = time;
  state.next_transition_time = GST_CLOCK_TIME_NONE;

  for (trees = g_list_first (trees); trees; trees = trees->next) {
    GNode *tree = (GNode *)trees->data;
    g_node_traverse (tree, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
        ttml_update_transition_time, &state);
  }

  GST_CAT_LOG (ttmlparse, "Next transition is at %" GST_TIME_FORMAT,
      GST_TIME_ARGS (state.next_transition_time));

  return state.next_transition_time;
}


/* Remove nodes from tree that are not visible at @time. */
static GNode *
ttml_remove_nodes_by_time (GNode * node, GstClockTime time)
{
  GNode *child, *next_child;
  TtmlElement *element;
  element = node->data;

  child = node->children;
  next_child = child ? child->next : NULL;
  while (child) {
    ttml_remove_nodes_by_time (child, time);
    child = next_child;
    next_child = child ? child->next : NULL;
  }

  if (!node->children && ((element->begin > time) || (element->end <= time))) {
    g_node_destroy (node);
    node = NULL;
  }

  return node;
}


/* Return a list of trees containing the elements and their ancestors that are
 * visible at @time. */
static GList *
ttml_get_active_elements (GList * element_trees, GstClockTime time)
{
  GList *tree;
  GList *ret = NULL;

  for (tree = g_list_first (element_trees); tree; tree = tree->next) {
    GNode *root = g_node_copy ((GNode *)tree->data);
    GST_CAT_LOG (ttmlparse, "There are %u nodes in tree.",
        g_node_n_nodes (root, G_TRAVERSE_ALL));
    root = ttml_remove_nodes_by_time (root, time);
    if (root) {
      GST_CAT_LOG (ttmlparse, "After filtering there are %u nodes in tree.",
          g_node_n_nodes (root, G_TRAVERSE_ALL));

      ret = g_list_append (ret, root);
    } else {
      GST_CAT_LOG (ttmlparse, "All elements have been filtered from tree.");
    }
  }

  GST_CAT_DEBUG (ttmlparse, "There are %u trees in returned list.",
      g_list_length (ret));
  return ret;
}


static GList *
ttml_create_scenes (GList * region_trees)
{
  TtmlScene *cur_scene = NULL;
  GList *output_scenes = NULL;
  GList *active_elements = NULL;
  GstClockTime timestamp = 0;

  while ((timestamp = ttml_find_next_transition (region_trees, timestamp))
      != GST_CLOCK_TIME_NONE) {
    GST_CAT_LOG (ttmlparse, "Next transition found at time %" GST_TIME_FORMAT,
        GST_TIME_ARGS (timestamp));
    if (cur_scene) {
      cur_scene->end = timestamp;
      output_scenes = g_list_append (output_scenes, cur_scene);
    }

    active_elements = ttml_get_active_elements (region_trees, timestamp);
    GST_CAT_LOG (ttmlparse, "There will be %u active regions after "
        "transition", g_list_length (active_elements));

    if (active_elements) {
      cur_scene = g_slice_new0 (TtmlScene);
      cur_scene->begin = timestamp;
      cur_scene->elements = active_elements;
    } else {
      cur_scene = NULL;
    }
  }

  return output_scenes;
}


/* Handle element whitespace in accordance with section 7.2.3 of the TTML
 * specification. Note that stripping of whitespace at the start and end of
 * line areas can only be done in the renderer once the text from multiple
 * elements has been laid out. */
static gboolean
ttml_handle_element_whitespace (GNode * node, gpointer data)
{
  TtmlElement *element = node->data;
  if (element->text
      && (element->whitespace_mode != TTML_WHITESPACE_MODE_PRESERVE)) {
    guint space_count = 0;
    guint textlen;
    gchar *c;
    gint i;

    /* Replace linefeeds with spaces. */
    for (c = element->text; *c > 0; c = g_utf8_next_char (c)) {
      gchar buf[6] = { 0 };
      gunichar u = g_utf8_get_char (c);
      gint nbytes = g_unichar_to_utf8 (u, buf);
      if (nbytes == 1 && buf[0] == 0xA)
        *c = ' ';
    }

    /* Compress each run of multiple contiguous spaces into a single space. */
    textlen = strlen (element->text);
    for (i = 0; i <= textlen; ++i) {
      c = element->text + i;
      if (*c == ' ') {
        ++space_count;
      } else {
        if (space_count > 1) {
          gchar *new_head = c - space_count + 1;
          g_strlcpy (new_head, c, textlen);
          i = new_head - element->text;
        }
        space_count = 0;
      }
    }
  }
  return FALSE;
}


static void
ttml_handle_whitespace (GNode * tree)
{
  g_node_traverse (tree, G_PRE_ORDER, G_TRAVERSE_LEAVES, -1,
      ttml_handle_element_whitespace, NULL);
}


/* Store child elements of @node with name @element_name in @table, as long as
 * @table doesn't already contain an element with the same ID. */
static void
ttml_store_unique_children (xmlNodePtr node, const gchar * element_name,
    GHashTable * table)
{
  xmlNodePtr ptr;

  for (ptr = node->children; ptr; ptr = ptr->next) {
    if (xmlStrcmp (ptr->name, (const xmlChar *) element_name) == 0) {
      TtmlElement *element = ttml_parse_element (ptr);

      if (element)
        if (!g_hash_table_contains (table, element->id))
          g_hash_table_insert (table, (gpointer) (element->id),
              (gpointer) element);
    }
  }
}


/* Parse style and region elements from @head and store in their respective
 * hash tables for future reference. */
static void
ttml_parse_head (xmlNodePtr head, GHashTable * styles_table,
    GHashTable * regions_table)
{
  xmlNodePtr node;

  for (node = head->children; node; node = node->next) {
    if (xmlStrcmp (node->name, (const xmlChar *) "styling") == 0)
      ttml_store_unique_children (node, "style", styles_table);
    if (xmlStrcmp (node->name, (const xmlChar *) "layout") == 0)
      ttml_store_unique_children (node, "region", regions_table);
  }
}


/* Remove nodes that do not belong to @region, or are not an ancestor of a node
 * belonging to @region. */
static GNode *
ttml_remove_nodes_by_region (GNode * node, const gchar *region)
{
  GNode *child, *next_child;
  TtmlElement *element;
  element = node->data;

  child = node->children;
  next_child = child ? child->next : NULL;
  while (child) {
    ttml_remove_nodes_by_region (child, region);
    child = next_child;
    next_child = child ? child->next : NULL;
  }

  if ((element->type == TTML_ELEMENT_TYPE_ANON_SPAN
        || element->type != TTML_ELEMENT_TYPE_BR)
      && element->region && (g_strcmp0 (element->region, region) != 0)) {
    ttml_delete_element (element);
    g_node_destroy (node);
    return NULL;
  }
  if (element->type != TTML_ELEMENT_TYPE_ANON_SPAN
        && element->type != TTML_ELEMENT_TYPE_BR && !node->children) {
    ttml_delete_element (element);
    g_node_destroy (node);
    return NULL;
  }

  return node;
}


static TtmlElement *
ttml_copy_element (const TtmlElement * element)
{
  TtmlElement *ret = g_slice_new0 (TtmlElement);

  ret->type = element->type;
  if (element->id)
    ret->id = g_strdup (element->id);
  if (element->styles)
    ret->styles  = g_strdupv (element->styles);
  if (element->region)
    ret->region = g_strdup (element->region);
  ret->begin = element->begin;
  ret->end = element->end;
  if (element->style_set)
    ret->style_set = ttml_copy_style_set (element->style_set);
  if (element->text)
    ret->text = g_strdup (element->text);
  ret->text_index = element->text_index;

  return ret;
}


static gpointer
ttml_copy_tree_element (gconstpointer src, gpointer data)
{
  return ttml_copy_element ((TtmlElement *)src);
}


/* Split the body tree into a set of trees, each containing only the elements
 * belonging to a single region. Returns a list of trees, one per region, each
 * with the corresponding region element at its root. */
static GList *
ttml_split_body_by_region (GNode * body, GHashTable * regions)
{
  GHashTableIter iter;
  gpointer key, value;
  GList *ret = NULL;

  g_hash_table_iter_init (&iter, regions);
  while (g_hash_table_iter_next (&iter, &key, &value)) {
    gchar *region_name = (gchar *)key;
    TtmlElement *region = (TtmlElement *)value;
    GNode *region_node = g_node_new (ttml_copy_element (region));
    GNode *body_copy = g_node_copy_deep (body, ttml_copy_tree_element, NULL);

    GST_CAT_DEBUG (ttmlparse, "Creating tree for region %s", region_name);
    GST_CAT_LOG (ttmlparse, "Copy of body has %u nodes.",
        g_node_n_nodes (body_copy, G_TRAVERSE_ALL));

    body_copy = ttml_remove_nodes_by_region (body_copy, region_name);
    if (body_copy) {
      GST_CAT_LOG (ttmlparse, "Copy of body now has %u nodes.",
          g_node_n_nodes (body_copy, G_TRAVERSE_ALL));

      /* Reparent tree to region node. */
      g_node_prepend (region_node, body_copy);
    }
    GST_CAT_LOG (ttmlparse, "Final tree has %u nodes.",
        g_node_n_nodes (region_node, G_TRAVERSE_ALL));
    ret = g_list_append (ret, region_node);
  }

  GST_CAT_DEBUG (ttmlparse, "Returning %u trees.", g_list_length (ret));
  return ret;
}


static guint
ttml_add_text_to_buffer (GstBuffer * buf, const gchar * text)
{
  GstMemory *mem;
  GstMapInfo map;
  guint ret;

  mem = gst_allocator_alloc (NULL, strlen (text) + 1, NULL);
  if (!gst_memory_map (mem, &map, GST_MAP_WRITE))
    GST_CAT_ERROR (ttmlparse, "Failed to map memory.");

  g_strlcpy ((gchar *)map.data, text, map.size);
  GST_CAT_DEBUG (ttmlparse, "Inserted following text into buffer: %s",
      (gchar *)map.data);
  gst_memory_unmap (mem, &map);

  ret = gst_buffer_n_memory (buf);
  gst_buffer_insert_memory (buf, -1, mem);
  return ret;
}


/* Create a GstSubtitleElement from @element, add it to @block, and insert its
 * associated text in @buf. */
static void
ttml_add_element (GstSubtitleBlock * block, TtmlElement * element,
    GstBuffer * buf, guint cellres_x, guint cellres_y)
{
  GstSubtitleStyleSet *element_style = NULL;
  guint buffer_index;
  GstSubtitleElement *sub_element = NULL;

  element_style = gst_subtitle_style_set_new ();
  ttml_update_style_set (element_style, element->style_set,
      cellres_x, cellres_y);
  GST_CAT_DEBUG (ttmlparse, "Creating element with text index %u",
      element->text_index);

  if (element->type != TTML_ELEMENT_TYPE_BR)
    buffer_index = ttml_add_text_to_buffer (buf, element->text);
  else
    buffer_index = ttml_add_text_to_buffer (buf, "\n");

  GST_CAT_DEBUG (ttmlparse, "Inserted text at index %u in GstBuffer.",
      buffer_index);
  sub_element = gst_subtitle_element_new (element_style, buffer_index,
      (element->whitespace_mode != TTML_WHITESPACE_MODE_PRESERVE));

  gst_subtitle_block_add_element (block, sub_element);
  GST_CAT_DEBUG (ttmlparse, "Added element to block; there are now %u"
      " elements in the block.",
      gst_subtitle_block_get_element_count (block));
}


/* Return TRUE if @color is totally transparent. */
static gboolean
ttml_color_is_transparent (const GstSubtitleColor * color)
{
  if (!color)
    return FALSE;
  else
    return (color->a == 0);
}


/* Blend @color2 over @color1 and return the resulting color. This is currently
 * a dummy implementation that simply returns color2 as long as it's
 * not fully transparent. */
/* TODO: Implement actual blending of colors. */
static GstSubtitleColor
ttml_blend_colors (GstSubtitleColor color1, GstSubtitleColor color2)
{
  if (ttml_color_is_transparent (&color2))
    return color1;
  else
    return color2;
}


/* Create the subtitle region and its child blocks and elements for @tree,
 * inserting element text in @buf. Ownership of created region is transferred
 * to caller. */
static GstSubtitleRegion *
ttml_create_subtitle_region (GNode * tree, GstBuffer * buf, guint cellres_x,
    guint cellres_y)
{
  GstSubtitleRegion *region = NULL;
  GstSubtitleStyleSet *region_style;
  GstSubtitleColor block_color;
  TtmlElement *element;
  GNode *node;

  element = tree->data;
  g_assert (element->type == TTML_ELEMENT_TYPE_REGION);

  region_style = gst_subtitle_style_set_new ();
  ttml_update_style_set (region_style, element->style_set, cellres_x,
      cellres_y);
  region = gst_subtitle_region_new (region_style);

  node = tree->children;
  if (!node)
    return region;

  g_assert (node->next == NULL);
  element = node->data;
  g_assert (element->type == TTML_ELEMENT_TYPE_BODY);
  block_color = ttml_parse_colorstring (element->style_set->background_color);

  for (node = node->children; node; node = node->next) {
    GNode *p_node;
    GstSubtitleColor div_color;

    element = node->data;
    g_assert (element->type == TTML_ELEMENT_TYPE_DIV);
    div_color = ttml_parse_colorstring (element->style_set->background_color);
    block_color = ttml_blend_colors (block_color, div_color);

    for (p_node = node->children; p_node; p_node = p_node->next) {
      GstSubtitleBlock *block = NULL;
      GstSubtitleStyleSet *block_style;
      GNode *content_node;
      GstSubtitleColor p_color;

      element = p_node->data;
      g_assert (element->type == TTML_ELEMENT_TYPE_P);
      p_color = ttml_parse_colorstring (element->style_set->background_color);
      block_color = ttml_blend_colors (block_color, p_color);

      block_style = gst_subtitle_style_set_new ();
      ttml_update_style_set (block_style, element->style_set, cellres_x,
          cellres_y);
      block_style->background_color = block_color;
      block = gst_subtitle_block_new (block_style);
      g_assert (block != NULL);

      for (content_node = p_node->children; content_node;
          content_node = content_node->next) {
        GNode *anon_node;
        element = content_node->data;

        if (element->type == TTML_ELEMENT_TYPE_BR
          || element->type == TTML_ELEMENT_TYPE_ANON_SPAN) {
          ttml_add_element (block, element, buf, cellres_x, cellres_y);
        } else if (element->type == TTML_ELEMENT_TYPE_SPAN) {
          /* Loop through anon-span children of this span. */
          for (anon_node = content_node->children; anon_node;
              anon_node = anon_node->next) {
            element = anon_node->data;

            if (element->type == TTML_ELEMENT_TYPE_BR
                || element->type == TTML_ELEMENT_TYPE_ANON_SPAN) {
              ttml_add_element (block, element, buf, cellres_x, cellres_y);
            } else {
              GST_CAT_ERROR (ttmlparse,
                  "Element type not allowed at this level of document.");
            }
          }
        } else {
          GST_CAT_ERROR (ttmlparse,
              "Element type not allowed at this level of document.");
        }
      }

      gst_subtitle_region_add_block (region, block);
      GST_CAT_DEBUG (ttmlparse, "Added block to region; there are now %u blocks"
          " in the region.", gst_subtitle_region_get_block_count (region));
    }
  }

  return region;
}


/* For each scene, create data objects to describe the layout and styling of
 * that scene and attach it as metadata to the GstBuffer that will be used to
 * carry that scene's text. */
static void
ttml_attach_scene_metadata (GList * scenes, guint cellres_x, guint cellres_y)
{
  GList *scene_entry;

  for (scene_entry = g_list_first (scenes); scene_entry;
      scene_entry = scene_entry->next) {
    TtmlScene * scene = scene_entry->data;
    GList *region_tree;
    GPtrArray *regions = g_ptr_array_new_with_free_func (
      (GDestroyNotify) gst_subtitle_region_unref);

    scene->buf = gst_buffer_new ();
    GST_BUFFER_PTS (scene->buf) = scene->begin;
    GST_BUFFER_DURATION (scene->buf) = (scene->end - scene->begin);

    for (region_tree = g_list_first (scene->elements); region_tree;
        region_tree = region_tree->next) {
      GNode *tree = (GNode *)region_tree->data;
      GstSubtitleRegion *region;

      region = ttml_create_subtitle_region (tree, scene->buf, cellres_x,
          cellres_y);
      g_ptr_array_add (regions, region);
    }

    gst_buffer_add_subtitle_meta (scene->buf, regions);
  }
}


GList * create_buffer_list (GList * scenes)
{
  GList *ret = NULL;

  while (scenes) {
    TtmlScene *scene = scenes->data;
    ret = g_list_prepend (ret, gst_buffer_ref (scene->buf));
    scenes = scenes->next;
  }
  return g_list_reverse (ret);
}


static gboolean
ttml_free_node_data (GNode * node, gpointer data)
{
  TtmlElement *element;
  element = node->data;
  ttml_delete_element (element);
  return FALSE;
}


static void
ttml_delete_tree (GNode * tree)
{
  g_node_traverse (tree, G_PRE_ORDER, G_TRAVERSE_ALL, -1, ttml_free_node_data,
      NULL);
  g_node_destroy (tree);
}


static void
ttml_delete_scene (TtmlScene * scene)
{
  if (scene->elements)
    g_list_free_full (scene->elements, (GDestroyNotify) g_node_destroy);
  if (scene->buf)
    gst_buffer_unref (scene->buf);
  g_slice_free (TtmlScene, scene);
}


static void
ttml_assign_region_times (GList *region_trees, GstClockTime doc_begin,
    GstClockTime doc_duration)
{
  GList *tree;

  for (tree = g_list_first (region_trees); tree; tree = tree->next) {
    GNode *region_node = (GNode *)tree->data;
    TtmlElement *region = (TtmlElement *)region_node->data;
    gboolean always_visible = !region->style_set->show_background
      || (g_strcmp0 (region->style_set->show_background, "always") == 0);

    GstSubtitleColor region_color = { 0, 0, 0, 0 };
    if (region->style_set->background_color)
      region_color =
        ttml_parse_colorstring (region->style_set->background_color);

    if (always_visible && !ttml_color_is_transparent (&region_color)) {
      GST_CAT_DEBUG (ttmlparse, "Assigning times to region.");
      /* If the input XML document was not encapsulated in a container that
       * provides timing information for the document as a whole (i.e., its
       * PTS and duration) and the region background should be always visible,
       * set region start time to 40ms and end time to 24 hours. This allows
       * the transition finding logic to work cleanly and ensures that
       * regions with showBackground="always" are visible for [virtually] the
       * entirety of any real-world stream. */
      region->begin = (doc_begin != GST_CLOCK_TIME_NONE) ?
        doc_begin : 40 * GST_MSECOND;
      region->end = (doc_duration != GST_CLOCK_TIME_NONE) ?
        region->begin + doc_duration : 24 * 3600 * GST_SECOND;
    }
  }
}


static xmlNodePtr
ttml_find_child (xmlNodePtr parent, const gchar * name)
{
  xmlNodePtr child = parent->children;
  while (child && xmlStrcmp (child->name, (const xmlChar *) name) != 0)
    child = child->next;
  return child;
}


GList *
ttml_parse (const gchar * input, GstClockTime begin,
    GstClockTime duration)
{
  xmlDocPtr doc;
  xmlNodePtr root_node, head_node, body_node;

  GHashTable *styles_table = g_hash_table_new_full (g_str_hash, g_str_equal,
      NULL, (GDestroyNotify) ttml_delete_element);
  GHashTable *regions_table = g_hash_table_new_full (g_str_hash, g_str_equal,
      NULL, (GDestroyNotify) ttml_delete_element);
  GList *output_buffers = NULL;
  gchar *value;
  guint cellres_x, cellres_y;
  TtmlWhitespaceMode doc_whitespace_mode = TTML_WHITESPACE_MODE_DEFAULT;

  GST_DEBUG_CATEGORY_INIT (ttmlparse, "ttmlparse", 0,
      "TTML parser debug category");
  GST_CAT_LOG (ttmlparse, "Input:\n%s", input);

  /* Parse input. */
  doc = xmlReadMemory (input, strlen (input), "any_doc_name", NULL,
      XML_PARSE_NOBLANKS);
  if (!doc) {
    GST_CAT_ERROR (ttmlparse, "Failed to parse document.");
    return NULL;
  }
  root_node = xmlDocGetRootElement (doc);

  if (xmlStrcmp (root_node->name, (const xmlChar *) "tt") != 0) {
    GST_CAT_ERROR (ttmlparse, "Root element of document is not tt:tt.");
    xmlFreeDoc (doc);
    return NULL;
  }

  if ((value = ttml_get_xml_property (root_node, "cellResolution"))) {
    gchar *ptr = value;
    cellres_x = (guint) g_ascii_strtoull (ptr, &ptr, 10U);
    cellres_y = (guint) g_ascii_strtoull (ptr, NULL, 10U);
    g_free (value);
  } else {
    cellres_x = DEFAULT_CELLRES_X;
    cellres_y = DEFAULT_CELLRES_Y;
  }

  GST_CAT_DEBUG (ttmlparse, "cellres_x: %u   cellres_y: %u", cellres_x,
      cellres_y);

  if ((value = ttml_get_xml_property (root_node, "space"))) {
    if (g_strcmp0 (value, "preserve") == 0) {
      GST_CAT_DEBUG (ttmlparse, "Preserving whitespace...");
      doc_whitespace_mode = TTML_WHITESPACE_MODE_PRESERVE;
    }
    g_free (value);
  }

  if (!(head_node = ttml_find_child (root_node, "head"))) {
    GST_CAT_ERROR (ttmlparse, "No <head> element found.");
    xmlFreeDoc (doc);
    return NULL;
  }
  ttml_parse_head (head_node, styles_table, regions_table);

  if ((body_node = ttml_find_child (root_node, "body"))) {
    GNode *body_tree;
    GList *region_trees = NULL;
    GList *scenes = NULL;

    body_tree = ttml_parse_body (body_node);
    GST_CAT_LOG (ttmlparse, "body_tree tree contains %u nodes.",
        g_node_n_nodes (body_tree, G_TRAVERSE_ALL));
    GST_CAT_LOG (ttmlparse, "body_tree tree height is %u",
        g_node_max_height (body_tree));

    ttml_inherit_whitespace_mode (body_tree, doc_whitespace_mode);
    ttml_handle_whitespace (body_tree);
    ttml_resolve_timings (body_tree);
    ttml_resolve_regions (body_tree);
    region_trees = ttml_split_body_by_region (body_tree, regions_table);
    ttml_resolve_referenced_styles (region_trees, styles_table);
    ttml_inherit_element_styles (region_trees);
    ttml_assign_region_times (region_trees, begin, duration);
    scenes = ttml_create_scenes (region_trees);
    GST_CAT_LOG (ttmlparse, "There are %u scenes in all.",
        g_list_length (scenes));
    ttml_attach_scene_metadata (scenes, cellres_x, cellres_y);
    output_buffers = create_buffer_list (scenes);

    g_list_free_full (scenes, (GDestroyNotify) ttml_delete_scene);
    g_list_free_full (region_trees, (GDestroyNotify) ttml_delete_tree);
    ttml_delete_tree (body_tree);
  }

  xmlFreeDoc (doc);
  g_hash_table_destroy (styles_table);
  g_hash_table_destroy (regions_table);

  return output_buffers;
}
