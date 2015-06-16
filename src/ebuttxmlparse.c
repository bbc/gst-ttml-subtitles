#include <glib.h>
#include <gst/subtitle/subtitle.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

#include "ebuttxmlparse.h"

GST_DEBUG_CATEGORY_STATIC (ebuttdparse);


gchar * get_xml_property (const xmlNode * node, const char * name);


static gboolean
media_time_is_valid (GstEbuttdMediaTime time)
{
  return (time.hours != G_MAXUINT);
}


static guint8
hex_pair_to_byte (const gchar * hex_pair)
{
  gint hi_digit, lo_digit;

  g_return_val_if_fail (hex_pair != NULL, 0U);
  g_return_val_if_fail (strlen (hex_pair) >= 2, 0U);

  hi_digit = g_ascii_xdigit_value (*hex_pair);
  lo_digit = g_ascii_xdigit_value (*(hex_pair + 1));
  return (hi_digit << 4) + lo_digit;
}


static GstSubtitleColor
parse_ebuttd_colorstring (const gchar * color)
{
  guint length;
  const gchar *c = NULL;
  GstSubtitleColor ret = { 0, 0, 0, 0 };

  g_return_val_if_fail (color != NULL, ret);

  /* Color strings in EBU-TT-D can have the form "#RRBBGG" or "#RRBBGGAA". */
  length = strlen (color);
  if (((length == 7) || (length == 9)) && *color == '#') {
    c = color + 1;

    ret.r = hex_pair_to_byte (c) / 255.0;
    ret.g = hex_pair_to_byte (c + 2) / 255.0;
    ret.b = hex_pair_to_byte (c + 4) / 255.0;

    if (length == 7)
      ret.a = 1.0;
    else
      ret.a = hex_pair_to_byte (c + 6) / 255.0;

    GST_CAT_LOG (ebuttdparse, "Returning color - r:%g  b:%g  g:%g  a:%g",
        ret.r, ret.b, ret.g, ret.a);
  } else if (g_strcmp0 (color, "yellow") == 0) { /* XXX:Hack for test stream. */
    ret = parse_ebuttd_colorstring ("#ffff00");
  } else {
    GST_CAT_ERROR (ebuttdparse, "Invalid color string.");
  }

  return ret;
}


static void
_print_element (GstEbuttdElement * element)
{
  g_return_if_fail (element != NULL);

  if (element->id)
    GST_CAT_DEBUG (ebuttdparse, "Element ID: %s", element->id);
  switch (element->type) {
    case GST_EBUTTD_ELEMENT_TYPE_STYLE:
      GST_CAT_DEBUG (ebuttdparse, "Element type: <style>");
      break;
    case GST_EBUTTD_ELEMENT_TYPE_REGION:
      GST_CAT_DEBUG (ebuttdparse, "Element type: <region>");
      break;
    case GST_EBUTTD_ELEMENT_TYPE_BODY:
      GST_CAT_DEBUG (ebuttdparse, "Element type: <body>");
      break;
    case GST_EBUTTD_ELEMENT_TYPE_DIV:
      GST_CAT_DEBUG (ebuttdparse, "Element type: <div>");
      break;
    case GST_EBUTTD_ELEMENT_TYPE_P:
      GST_CAT_DEBUG (ebuttdparse, "Element type: <p>");
      break;
    case GST_EBUTTD_ELEMENT_TYPE_SPAN:
      GST_CAT_DEBUG (ebuttdparse, "Element type: <span>");
      break;
    case GST_EBUTTD_ELEMENT_TYPE_ANON_SPAN:
      GST_CAT_DEBUG (ebuttdparse, "Element type: <anon-span>");
      break;
    case GST_EBUTTD_ELEMENT_TYPE_BR:
      GST_CAT_DEBUG (ebuttdparse, "Element type: <br>");
      break;
  }
  if (element->region)
    GST_CAT_DEBUG (ebuttdparse, "Element region: %s", element->region);
  if (element->begin != GST_CLOCK_TIME_NONE)
    GST_CAT_DEBUG (ebuttdparse, "Element begin: %llu", element->begin);
  if (element->end != GST_CLOCK_TIME_NONE)
    GST_CAT_DEBUG (ebuttdparse, "Element end: %llu", element->end);
  if (element->text)
    GST_CAT_DEBUG (ebuttdparse, "Element text: %s", element->text);
}

#if 1
static void
_print_style (GstEbuttdStyle * style)
{
  g_return_if_fail (style != NULL);

  GST_CAT_LOG (ebuttdparse, "Style %p:", style);
  GST_CAT_LOG (ebuttdparse, "\t\ttextDirection: %d", style->text_direction);
  if (style->font_family)
    GST_CAT_LOG (ebuttdparse, "\t\tfontFamily: %s", style->font_family);
  GST_CAT_LOG (ebuttdparse, "\t\tfontSize: %g", style->font_size);
  GST_CAT_LOG (ebuttdparse, "\t\tlineHeight: %g", style->line_height);
  GST_CAT_LOG (ebuttdparse, "\t\ttextAlign: %d", style->text_align);
  GST_CAT_LOG (ebuttdparse, "\t\tcolor: r:%g g:%g b:%g a: %g",
      style->color.r, style->color.g, style->color.b, style->color.a);
  GST_CAT_LOG (ebuttdparse, "\t\tbg_color: r:%g g:%g b:%g a: %g",
      style->bg_color.r, style->bg_color.g, style->bg_color.b,
      style->bg_color.a);
  GST_CAT_LOG (ebuttdparse, "\t\tfontStyle: %d", style->font_style);
  GST_CAT_LOG (ebuttdparse, "\t\tfontWeight: %d", style->font_weight);
  GST_CAT_LOG (ebuttdparse, "\t\ttextDecoration: %d", style->text_decoration);
  GST_CAT_LOG (ebuttdparse, "\t\tunicodeBidi: %d", style->unicode_bidi);
  GST_CAT_LOG (ebuttdparse, "\t\twrapOption: %d", style->wrap_option);
  GST_CAT_LOG (ebuttdparse, "\t\tmultiRowAlign: %d", style->multi_row_align);
  GST_CAT_LOG (ebuttdparse, "\t\tlinePadding: %g", style->line_padding);
}
#endif


static void
_print_style_set (GstEbuttdStyleSet * set)
{
  g_return_if_fail (set != NULL);

  GST_CAT_LOG (ebuttdparse, "Style set %p:", set);
  if (set->text_direction)
    GST_CAT_LOG (ebuttdparse, "\t\ttext_direction: %s", set->text_direction);
  if (set->font_family)
    GST_CAT_LOG (ebuttdparse, "\t\tfont_family: %s", set->font_family);
  if (set->font_size)
    GST_CAT_LOG (ebuttdparse, "\t\tfont_size: %s", set->font_size);
  if (set->line_height)
    GST_CAT_LOG (ebuttdparse, "\t\tline_height: %s", set->line_height);
  if (set->text_align)
    GST_CAT_LOG (ebuttdparse, "\t\ttext_align: %s", set->text_align);
  if (set->color)
    GST_CAT_LOG (ebuttdparse, "\t\tcolor: %s", set->color);
  if (set->bg_color)
    GST_CAT_LOG (ebuttdparse, "\t\tbg_color: %s", set->bg_color);
  if (set->font_style)
    GST_CAT_LOG (ebuttdparse, "\t\tfont_style: %s", set->font_style);
  if (set->font_weight)
    GST_CAT_LOG (ebuttdparse, "\t\tfont_weight: %s", set->font_weight);
  if (set->text_decoration)
    GST_CAT_LOG (ebuttdparse, "\t\ttext_decoration: %s", set->text_decoration);
  if (set->unicode_bidi)
    GST_CAT_LOG (ebuttdparse, "\t\tunicode_bidi: %s", set->unicode_bidi);
  if (set->wrap_option)
    GST_CAT_LOG (ebuttdparse, "\t\twrap_option: %s", set->wrap_option);
  if (set->multi_row_align)
    GST_CAT_LOG (ebuttdparse, "\t\tmulti_row_align: %s", set->multi_row_align);
  if (set->line_padding)
    GST_CAT_LOG (ebuttdparse, "\t\tline_padding: %s", set->line_padding);
  if (set->origin)
    GST_CAT_LOG (ebuttdparse, "\t\torigin: %s", set->origin);
  if (set->extent)
    GST_CAT_LOG (ebuttdparse, "\t\textent: %s", set->extent);
  if (set->display_align)
    GST_CAT_LOG (ebuttdparse, "\t\tdisplay_align: %s", set->display_align);
  if (set->overflow)
    GST_CAT_LOG (ebuttdparse, "\t\toverflow: %s", set->overflow);
  if (set->padding)
    GST_CAT_LOG (ebuttdparse, "\t\tpadding: %s", set->padding);
  if (set->writing_mode)
    GST_CAT_LOG (ebuttdparse, "\t\twriting_mode: %s", set->writing_mode);
  if (set->show_background)
    GST_CAT_LOG (ebuttdparse, "\t\tshow_background: %s", set->show_background);
}


static void
_print_region (GstEbuttdRegion * region)
{
  GST_CAT_LOG (ebuttdparse, "Region %p:", region);
  GST_CAT_LOG (ebuttdparse, "\t\toriginX: %g", region->origin_x);
  GST_CAT_LOG (ebuttdparse, "\t\toriginY: %g", region->origin_y);
  GST_CAT_LOG (ebuttdparse, "\t\textentW: %g", region->extent_w);
  GST_CAT_LOG (ebuttdparse, "\t\textentH: %g", region->extent_h);
  /*GST_CAT_LOG (ebuttdparse, "\t\tstyle: %s", region->style);*/
  GST_CAT_LOG (ebuttdparse, "\t\tdisplay_align: %d", region->display_align);
  GST_CAT_LOG (ebuttdparse, "\t\tpadding_start: %g", region->padding_start);
  GST_CAT_LOG (ebuttdparse, "\t\tpadding_end: %g", region->padding_end);
  GST_CAT_LOG (ebuttdparse, "\t\tpadding_before: %g", region->padding_before);
  GST_CAT_LOG (ebuttdparse, "\t\tpadding_after: %g", region->padding_after);
  GST_CAT_LOG (ebuttdparse, "\t\twriting_mode: %d", region->writing_mode);
  GST_CAT_LOG (ebuttdparse, "\t\tshow_background: %d", region->show_background);
  GST_CAT_LOG (ebuttdparse, "\t\toverflow: %d", region->overflow);
}


static GstEbuttdStyleSet *
parse_style_set (const xmlNode * node)
{
  GstEbuttdStyleSet *s = g_new0 (GstEbuttdStyleSet, 1);
  gchar *value = NULL;

  if ((value = get_xml_property (node, "id"))) {
    /*s->id = g_strdup (value);*/
    g_free (value);
  } else {
    GST_CAT_ERROR (ebuttdparse, "styles must have an ID.");
    return NULL;
  }

  if ((value = get_xml_property (node, "direction"))) {
    s->text_direction = g_strdup (value);
    g_free (value);
  }

  if ((value = get_xml_property (node, "fontFamily"))) {
    s->font_family = g_strdup (value);
    g_free (value);
  }

  if ((value = get_xml_property (node, "fontSize"))) {
    s->font_size = g_strdup (value);
    g_free (value);
  }

  if ((value = get_xml_property (node, "lineHeight"))) {
    s->line_height = g_strdup (value);
    g_free (value);
  }

  if ((value = get_xml_property (node, "textAlign"))) {
    s->text_align = g_strdup (value);
    g_free (value);
  }

  if ((value = get_xml_property (node, "color"))) {
    s->color = g_strdup (value);
    g_free (value);
  }

  if ((value = get_xml_property (node, "backgroundColor"))) {
    s->bg_color = g_strdup (value);
    g_free (value);
  }

  if ((value = get_xml_property (node, "fontStyle"))) {
    s->font_style = g_strdup (value);
    g_free (value);
  }

  if ((value = get_xml_property (node, "fontWeight"))) {
    s->font_weight = g_strdup (value);
    g_free (value);
  }

  if ((value = get_xml_property (node, "textDecoration"))) {
    s->text_decoration = g_strdup (value);
    g_free (value);
  }

  if ((value = get_xml_property (node, "unicodeBidi"))) {
    s->unicode_bidi = g_strdup (value);
    g_free (value);
  }

  if ((value = get_xml_property (node, "wrapOption"))) {
    s->wrap_option = g_strdup (value);
    g_free (value);
  }

  if ((value = get_xml_property (node, "multiRowAlign"))) {
    s->multi_row_align = g_strdup (value);
    g_free (value);
  }

  if ((value = get_xml_property (node, "linePadding"))) {
    s->line_padding = g_strdup (value);
    g_free (value);
  }

  if ((value = get_xml_property (node, "origin"))) {
    s->origin = g_strdup (value);
    g_free (value);
  }

  if ((value = get_xml_property (node, "extent"))) {
    s->extent = g_strdup (value);
    g_free (value);
  }

  if ((value = get_xml_property (node, "display_align"))) {
    s->display_align = g_strdup (value);
    g_free (value);
  }

  if ((value = get_xml_property (node, "overflow"))) {
    s->overflow = g_strdup (value);
    g_free (value);
  }

  if ((value = get_xml_property (node, "padding"))) {
    s->padding = g_strdup (value);
    g_free (value);
  }

  if ((value = get_xml_property (node, "writing_mode"))) {
    s->writing_mode = g_strdup (value);
    g_free (value);
  }

  if ((value = get_xml_property (node, "show_background"))) {
    s->show_background = g_strdup (value);
    g_free (value);
  }

  return s;
}

static void
delete_style_set (GstEbuttdStyleSet * style)
{
  g_return_if_fail (style != NULL);
  GST_CAT_DEBUG (ebuttdparse, "Deleting style set %p...", style);
  if (style->text_direction) g_free ((gpointer) style->text_direction);
  if (style->font_family) g_free ((gpointer) style->font_family);
  if (style->font_size) g_free ((gpointer) style->font_size);
  if (style->line_height) g_free ((gpointer) style->line_height);
  if (style->text_align) g_free ((gpointer) style->text_align);
  if (style->color) g_free ((gpointer) style->color);
  if (style->bg_color) g_free ((gpointer) style->bg_color);
  if (style->font_style) g_free ((gpointer) style->font_style);
  if (style->font_weight) g_free ((gpointer) style->font_weight);
  if (style->text_decoration) g_free ((gpointer) style->text_decoration);
  if (style->unicode_bidi) g_free ((gpointer) style->unicode_bidi);
  if (style->wrap_option) g_free ((gpointer) style->wrap_option);
  if (style->multi_row_align) g_free ((gpointer) style->multi_row_align);
  if (style->line_padding) g_free ((gpointer) style->line_padding);
  if (style->origin) g_free ((gpointer) style->origin);
  if (style->extent) g_free ((gpointer) style->extent);
  if (style->display_align) g_free ((gpointer) style->display_align);
  if (style->overflow) g_free ((gpointer) style->overflow);
  if (style->padding) g_free ((gpointer) style->padding);
  if (style->writing_mode) g_free ((gpointer) style->writing_mode);
  if (style->show_background) g_free ((gpointer) style->show_background);
  g_free ((gpointer) style);
}


static void
delete_style (GstEbuttdStyle * style)
{
  g_return_if_fail (style != NULL);
  GST_CAT_DEBUG (ebuttdparse, "Deleting style %p...", style);
  /* XXX: Don't we also need to free font name? */
  g_free ((gpointer) style);
}


static void
delete_element (GstEbuttdElement * element)
{
  g_return_if_fail (element != NULL);
  GST_CAT_DEBUG (ebuttdparse, "Deleting element %p...", element);
  gchar *id;
  gchar **styles;
  gchar *region;
  GstClockTime begin;
  GstClockTime end;
  GstEbuttdStyleSet *style_set;
  GstEbuttdStyle *style;
  gchar *text;

  if (element->id) g_free ((gpointer) element->id);
  if (element->styles) g_strfreev (element->styles);
  if (element->region) g_free ((gpointer) element->region);
  if (element->style_set) delete_style_set (element->style_set);
  if (element->style) delete_style (element->style);
  if (element->text) g_free ((gpointer) element->text);
  g_free ((gpointer) element);
}


#if 0
static GstEbuttdStyle *
create_new_style (GstEbuttdStyleSet * desc)
{
  GstEbuttdStyle *s = g_new0 (GstEbuttdStyle, 1);

  g_return_val_if_fail (desc != NULL, NULL);

  if (desc->text_direction) {
    if (g_strcmp0 (desc->text_direction, "rtl") == 0)
      s->text_direction = GST_EBUTTD_TEXT_DIRECTION_RTL;
    else
      s->text_direction = GST_EBUTTD_TEXT_DIRECTION_LTR;
  }

  if (desc->font_family) {
    gsize length = g_strlcpy (s->font_family, desc->font_family,
        MAX_FONT_FAMILY_NAME_LENGTH);
    if (length > MAX_FONT_FAMILY_NAME_LENGTH)
      GST_CAT_ERROR (ebuttdparse, "Font family name is too long.");
  }

  if (desc->font_size) {
    s->font_size = g_ascii_strtod (desc->font_size, NULL);
  }

  if (desc->line_height) {
    if (g_strcmp0 (desc->line_height, "normal") == 0)
      s->line_height = 125.0;
    else
      s->line_height = g_ascii_strtod (desc->line_height, NULL);
  } else {
      s->line_height = 125.0;
  }

  if (desc->text_align) {
    if (g_strcmp0 (desc->text_align, "left") == 0)
      s->text_align = GST_EBUTTD_TEXT_ALIGN_LEFT;
    else if (g_strcmp0 (desc->text_align, "center") == 0)
      s->text_align = GST_EBUTTD_TEXT_ALIGN_CENTER;
    else if (g_strcmp0 (desc->text_align, "right") == 0)
      s->text_align = GST_EBUTTD_TEXT_ALIGN_RIGHT;
    else if (g_strcmp0 (desc->text_align, "end") == 0)
      s->text_align = GST_EBUTTD_TEXT_ALIGN_END;
    else
      s->text_align = GST_EBUTTD_TEXT_ALIGN_START;
  }

  if (desc->color) {
    s->color = parse_ebuttd_colorstring (desc->color);
  }

  if (desc->bg_color) {
    s->bg_color = parse_ebuttd_colorstring (desc->bg_color);
  }

  if (desc->font_style) {
    if (g_strcmp0 (desc->font_style, "italic") == 0)
      s->font_style = GST_EBUTTD_FONT_STYLE_ITALIC;
    else
      s->font_style = GST_EBUTTD_FONT_STYLE_NORMAL;
  }

  if (desc->font_weight) {
    if (g_strcmp0 (desc->font_weight, "bold") == 0)
      s->font_weight = GST_EBUTTD_FONT_WEIGHT_BOLD;
    else
      s->font_weight = GST_EBUTTD_FONT_WEIGHT_NORMAL;
  }

  if (desc->text_decoration) {
    if (g_strcmp0 (desc->text_decoration, "underline") == 0)
      s->text_decoration = GST_EBUTTD_TEXT_DECORATION_UNDERLINE;
    else
      s->text_decoration = GST_EBUTTD_TEXT_DECORATION_NONE;
  }

  if (desc->unicode_bidi) {
    if (g_strcmp0 (desc->unicode_bidi, "embed") == 0)
      s->unicode_bidi = GST_EBUTTD_UNICODE_BIDI_EMBED;
    else if (g_strcmp0 (desc->unicode_bidi, "bidiOverride") == 0)
      s->unicode_bidi = GST_EBUTTD_UNICODE_BIDI_OVERRIDE;
    else
      s->unicode_bidi = GST_EBUTTD_UNICODE_BIDI_NORMAL;
  }

  if (desc->wrap_option) {
    if (g_strcmp0 (desc->wrap_option, "noWrap") == 0)
      s->wrap_option = GST_EBUTTD_WRAPPING_OFF;
    else
      s->wrap_option = GST_EBUTTD_WRAPPING_ON;
  }

  if (desc->multi_row_align) {
    if (g_strcmp0 (desc->multi_row_align, "start") == 0)
      s->multi_row_align = GST_EBUTTD_MULTI_ROW_ALIGN_START;
    else if (g_strcmp0 (desc->multi_row_align, "center") == 0)
      s->multi_row_align = GST_EBUTTD_MULTI_ROW_ALIGN_CENTER;
    else if (g_strcmp0 (desc->multi_row_align, "end") == 0)
      s->multi_row_align = GST_EBUTTD_MULTI_ROW_ALIGN_END;
    else
      s->multi_row_align = GST_EBUTTD_MULTI_ROW_ALIGN_AUTO;
  }

  if (desc->line_padding) {
    s->line_padding = g_ascii_strtod (desc->line_padding, NULL);
  }

  return s;
}
#endif


static GstEbuttdRegion *
create_new_region (const xmlNode * node)
{
  GstEbuttdRegion *r = g_new0 (GstEbuttdRegion, 1);
  gchar *value = NULL;

  if ((value = get_xml_property (node, "origin"))) {
    gchar *c;
    r->origin_x = g_ascii_strtod (value, &c);
    while (!g_ascii_isdigit (*c) && *c != '+' && *c != '-') ++c;
    r->origin_y = g_ascii_strtod (c, NULL);
    /*GST_CAT_DEBUG (ebuttdparse, "origin_x: %g   origin_y: %g", r->origin_x, r->origin_y);*/
    g_free (value);
  }

  if ((value = get_xml_property (node, "extent"))) {
    gchar *c;
    r->extent_w = g_ascii_strtod (value, &c);
    r->extent_w = (r->extent_w > 100.0) ? 100.0 : r->extent_w;
    while (!g_ascii_isdigit (*c) && *c != '+' && *c != '-') ++c;
    r->extent_h = g_ascii_strtod (c, NULL);
    r->extent_h = (r->extent_h > 100.0) ? 100.0 : r->extent_h;
    /*GST_CAT_DEBUG (ebuttdparse, "extent_w: %g   extent_h: %g", r->extent_w, r->extent_h);*/
    g_free (value);
  }

  if ((value = get_xml_property (node, "displayAlign"))) {
    if (g_strcmp0 (value, "center") == 0)
      r->display_align = GST_EBUTTD_DISPLAY_ALIGN_CENTER;
    else if (g_strcmp0 (value, "after") == 0)
      r->display_align = GST_EBUTTD_DISPLAY_ALIGN_AFTER;
    else
      r->display_align = GST_EBUTTD_DISPLAY_ALIGN_BEFORE;
    /*GST_CAT_DEBUG (ebuttdparse, "displayAlign: %d", r->display_align);*/
    g_free (value);
  }

  if ((value = get_xml_property (node, "padding"))) {
    gchar **decimals;
    guint n_decimals;
    gint i;

    decimals = g_strsplit (value, "%", 0);
    n_decimals = g_strv_length (decimals) - 1;
    for (i = 0; i < n_decimals; ++i) {
      g_strstrip (decimals[i]);
    }

    switch (n_decimals) {
      case 1:
        r->padding_start = r->padding_end =
          r->padding_before = r->padding_after =
          g_ascii_strtod (decimals[0], NULL);
        break;

      case 2:
        r->padding_before = r->padding_after =
          g_ascii_strtod (decimals[0], NULL);
        r->padding_start = r->padding_end =
          g_ascii_strtod (decimals[1], NULL);
        break;

      case 3:
        r->padding_before = g_ascii_strtod (decimals[0], NULL);
        r->padding_start = r->padding_end =
          g_ascii_strtod (decimals[1], NULL);
        r->padding_after = g_ascii_strtod (decimals[2], NULL);
        break;

      case 4:
        r->padding_before = g_ascii_strtod (decimals[0], NULL);
        r->padding_end = g_ascii_strtod (decimals[1], NULL);
        r->padding_after = g_ascii_strtod (decimals[2], NULL);
        r->padding_start = g_ascii_strtod (decimals[3], NULL);
        break;
    }
    /*g_print ("paddingStart: %g  padding_end: %g  padding_before: %g "
        "padding_after: %g\n", r->padding_start, r->padding_end,
        r->padding_before, r->padding_after);*/
    g_strfreev (decimals);
    g_free (value);
  }

  if ((value = get_xml_property (node, "writingMode"))) {
    if (g_str_has_prefix (value, "rl"))
      r->writing_mode = GST_EBUTTD_WRITING_MODE_RLTB;
    else if ((g_strcmp0 (value, "tbrl") == 0) || (g_strcmp0 (value, "tb") == 0))
      r->writing_mode = GST_EBUTTD_WRITING_MODE_TBRL;
    else if (g_strcmp0 (value, "tblr") == 0)
      r->writing_mode = GST_EBUTTD_WRITING_MODE_TBLR;
    else
      r->writing_mode = GST_EBUTTD_WRITING_MODE_LRTB;
    /*GST_CAT_DEBUG (ebuttdparse, "writingMode: %d", r->writing_mode);*/
    g_free (value);
  }

  if ((value = get_xml_property (node, "showBackground"))) {
    if (g_strcmp0 (value, "whenActive") == 0)
      r->show_background = GST_EBUTTD_BACKGROUND_MODE_WHEN_ACTIVE;
    else
      r->show_background = GST_EBUTTD_BACKGROUND_MODE_ALWAYS;
    /*GST_CAT_DEBUG (ebuttdparse, "showBackground: %d", r->show_background);*/
    g_free (value);
  }

  if ((value = get_xml_property (node, "overflow"))) {
    if (g_strcmp0 (value, "visible") == 0)
      r->overflow = GST_EBUTTD_OVERFLOW_MODE_VISIBLE;
    else
      r->overflow = GST_EBUTTD_OVERFLOW_MODE_HIDDEN;
    /*GST_CAT_DEBUG (ebuttdparse, "overflow: %d", r->overflow);*/
    g_free (value);
  }

  return r;
}


static void
delete_region (GstEbuttdRegion * region)
{
  g_return_if_fail (region != NULL);
  GST_CAT_DEBUG (ebuttdparse, "Deleting region %p...", region);
  g_free ((gpointer) region);
}


gchar *
glist_print (InheritanceList * list)
{
  InheritanceList *tempList = list;
  gchar *ret;
  gchar *old_ret;

  ret = g_strconcat ("[", (gchar *) (tempList->data), NULL);
  tempList = tempList->next;

  while (tempList != NULL) {
    old_ret = ret;
    ret = g_strconcat (old_ret, " ,", (gchar *) (tempList->data), NULL);
    g_free (old_ret);
    tempList = tempList->next;
  }

  old_ret = ret;
  ret = g_strconcat (old_ret, "]\0", NULL);
  g_free (old_ret);

  return ret;
}

gchar *
fetch_element (gchar * element_name, const gchar * parent_text)
{                               /* TODO: make this const? */
  gchar *begin_text;
  gchar *end_text;
  int text_len;
  gchar *ret_text = NULL;

  begin_text = g_strstr_len (parent_text, -1, element_name);    /* find the element */
  if (begin_text) {
    /* now get following " */
    begin_text = g_strstr_len (begin_text, -1, "\"");
    begin_text++;               /* advance insdie of the " " */
    end_text = g_strstr_len (begin_text, -1, "\"");     /* final tag is the close element tag */
    text_len = strlen (begin_text) - strlen (end_text);
    ret_text = g_strndup (begin_text, text_len);        /* trim and make copy */
  }
  return ret_text;              /* NULL if not found */
}

gchar *
fetch_element_name (const gchar * prefix, const gchar * suffix, const gchar * parent_text)
{                               /* TODO: make this const? */
  gchar *begin_text;
  gchar *end_text;
  int text_len, prefix_len;
  gchar *ret_text = NULL;

  prefix_len = strlen (prefix);

  begin_text = g_strstr_len (parent_text, -1, prefix);  /* find the element */
  if (begin_text) {
    begin_text += prefix_len;   /* skip to end of prefix */
    end_text = g_strstr_len (begin_text, -1, suffix);   /* final tag is the close element tag */
    if (!end_text) {
      return NULL;
    }
    text_len = strlen (begin_text) - strlen (end_text);
    ret_text = g_strndup (begin_text, text_len);        /* trim and make copy */
  }
  return ret_text;              /* NULL if not found */
}

gchar *
fetch_child (const gchar * parent_text)
{
  gchar *begin_text;
  gchar *end_text;
  int text_len;
  gchar *ret_text = NULL;

  /* now get text */
  begin_text = g_strstr_len (parent_text, -1, ">");     /* first > is the end of the  open p tag */
  begin_text++;                 /* remove first character */
  end_text = g_strrstr (parent_text, "<");      /* final tag is the close p tag */
  text_len = strlen (begin_text) - strlen (end_text);
  ret_text = g_strndup (begin_text, text_len);  /* trim */

  return ret_text;
}

int
is_style_cached (gchar * style_id, StyleList * head_style)
{
  StyleList *current_style;
  current_style = head_style;

  while (1) {
    /* compare id with style_id */
    if (g_strcmp0 (current_style->id, style_id) == 0) {
      return 1;                 /* TRUE - there is a match */
    }
    if (current_style->next != NULL) {
      current_style = current_style->next;
    } else {
      break;                    /* did not find match */
    }
  }
  return 0;                     /* FALSE */
}

StyleList *
retrieve_style (gchar * style_id, StyleList * head_style)
{
  StyleList *current_style = head_style;

  while (1) {
    /* compare id with style_id */
    if (g_strcmp0 (current_style->id, style_id) == 0) {
      return current_style;     /* there is a match */
    }
    if (current_style->next != NULL) {
      current_style = current_style->next;
    } else {
      break;                    /* did not find match */
    }
  }
  return NULL;                  /* FALSE */
}

gchar *
_add_or_update_style (gchar * opening_tag, gchar * attribute_name,
    gchar * attribute_value)
{
  gchar *found_style;
  gchar *after_found_style;
  gchar *ret;
  gchar *opening_tag_copy;

  /* Does opening tag already contain tag? */
  found_style = g_strstr_len (opening_tag, -1, attribute_name); /* pointer to first character */
  if (found_style) {
    found_style = g_strstr_len (found_style, -1, "\""); /* next value */
    found_style++;              /* remove " */
    after_found_style = g_strstr_len (found_style, -1, "\"");   /* remainder of string */

    opening_tag_copy = g_strndup (opening_tag, (strlen (opening_tag) - strlen (found_style)));  /* trim */
    ret = g_strconcat (opening_tag_copy,
        attribute_value, after_found_style, NULL);

    g_free (opening_tag_copy);
  } else {
    ret =
        g_strconcat (opening_tag, attribute_name, attribute_value, "\" ", NULL);
    g_free (opening_tag);
  }
  return (ret);
}

gchar *
_add_pango_style (StyleProp * style, gchar * opening_tag)
{
  if (style->color) {
    opening_tag =
        _add_or_update_style (opening_tag, "foreground=\"", style->color);
  }
  if (style->backgroundColor) {
    opening_tag =
        _add_or_update_style (opening_tag, "background=\"",
        style->backgroundColor);
  }
  if (style->fontStyle) {
    opening_tag =
        _add_or_update_style (opening_tag, "font_style=\"", style->fontStyle);
  }
  if (style->fontFamily) {
    if (g_strcmp0 (style->fontFamily, "sansSerif") == 0) {
      /*gchar sans[] ="sans"; */
      g_free (style->fontFamily);
      style->fontFamily = g_strdup ("sans");
    }
    opening_tag =
        _add_or_update_style (opening_tag, "font_family=\"", style->fontFamily);
  }
  if (style->fontWeight) {
    opening_tag =
        _add_or_update_style (opening_tag, "font_weight=\"", style->fontWeight);
  }
  if (style->textDecoration) {
    if (g_strcmp0 (style->textDecoration, "underline") == 0) {
      opening_tag =
          _add_or_update_style (opening_tag, "underline=\"", "single");
    }
  }
  if (style->direction) {
    if (g_strcmp0 (style->direction, "rtl") == 0) {
      opening_tag = _add_or_update_style (opening_tag, "gravity=\"", "north");
    }
  }
  if (style->linePadding) {
    gchar *number_str = style->linePadding;
    /* convert to integer pixels */
    /* 0.5c is half a cell width for the <p> text */

    if (g_strcmp0 (style->direction, "c") == 0) {
      /* remove last character */
      number_str[strlen (number_str) - 1] = 0;
    }

    opening_tag =
        _add_or_update_style (opening_tag, "line_padding=\"", number_str);
  }
  if (style->multiRowAlign) {
    opening_tag = _add_or_update_style (opening_tag, "multi_row_align=\"",
        style->multiRowAlign);
  }
  if (style->textAlign) {
    opening_tag = _add_or_update_style (opening_tag, "text_align=\"",
        style->textAlign);
  }
  if (style->lineHeight) {
    opening_tag = _add_or_update_style (opening_tag, "line_height=\"",
        style->lineHeight);
  }
  if (style->wrapOption) {
    opening_tag = _add_or_update_style (opening_tag, "wrap_option=\"",
        style->wrapOption);
  }


  /* Need to transtate into pango friendly font size.
   * style->fontSize is in % of cell height.
   * pango needs it in 100ths of a point. so will need to find it in pixels
   * before converting to pts
   * shall create font_size attribute in textoverlay where the cell height is
   * known in pixels
   */
  if (style->fontSize) {
    gchar *font_size_perc;

    if (g_strcmp0 (style->fontSize, "%") == 0) {
      /* remove last character */
      font_size_perc[strlen (font_size_perc) - 1] = 0;
    }

    opening_tag =
        g_strconcat (opening_tag, "font_size=\"", style->fontSize, "\" ", NULL);
  }

  return (opening_tag);
}

gchar *
add_document_metadata_markup (DocMetadata * doc_meta, gchar * opening_tag)
{
  /* based on add_style_markup_depreciated */

  /* append stlyes and their values to the opening tag */
  if (doc_meta->cell_resolution_x)
    opening_tag = _add_or_update_style (opening_tag, "cell_resolution_x=\"",
        doc_meta->cell_resolution_x);

  if (doc_meta->cell_resolution_y)
    opening_tag = _add_or_update_style (opening_tag, "cell_resolution_y=\"",
        doc_meta->cell_resolution_y);

  return opening_tag;
}


void
add_style_markup (gchar ** text, StyleProp * style, gchar * region,
    DocMetadata * doc_meta)
{

  gchar *ret;
  gchar *opening_tag = (gchar *) malloc (7);

  gchar *close_brac = ">";
  gchar *close_tag = "</span>";

  GST_CAT_DEBUG (ebuttdparse, "add_style_markup: region is %s", region);
  strcpy (opening_tag, "<span \0");     /* add to heap in case there are no styles */

  /* append styles and their values to the opening tag */
  opening_tag = _add_pango_style (style, opening_tag);

  /*if (!(doc_meta->sent_document_metadata)) {*/
    opening_tag = add_document_metadata_markup (doc_meta, opening_tag);
    /*doc_meta->sent_document_metadata = TRUE;*/
  /*}*/

  /* XXX: Replace this with call to function that will add region attributes to span tag. */
  /*opening_tag = _add_or_update_style (opening_tag, "region=\"", region);*/

  /* append the opening and close tags */
  ret = g_strconcat (opening_tag, close_brac, *text, close_tag, NULL);  /* todo: free this */

  /* free strings that are finished with */
  g_free (opening_tag);
  g_free (*text);

  /* set text to the return value */
  *text = ret;
}

gchar *
get_xml_property (const xmlNode * node, const char *name)
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

StyleProp *
add_new_style (const gchar * style_id, xmlNodePtr child)
{
  StyleProp *style = g_new0 (StyleProp, 1);

  style->direction = get_xml_property (child, "direction");
  style->fontFamily = get_xml_property (child, "fontFamily");
  style->fontSize = get_xml_property (child, "fontSize");
  style->lineHeight = get_xml_property (child, "lineHeight");
  style->textAlign = get_xml_property (child, "textAlign");
  style->color = get_xml_property (child, "color");
  style->backgroundColor = get_xml_property (child, "backgroundColor");
  style->fontStyle = get_xml_property (child, "fontStyle");
  style->fontWeight = get_xml_property (child, "fontWeight");
  style->textDecoration = get_xml_property (child, "textDecoration");
  style->unicodeBidi = get_xml_property (child, "unicodeBidi");
  style->wrapOption = get_xml_property (child, "wrapOption");
  style->multiRowAlign = get_xml_property (child, "multiRowAlign");
  style->linePadding = get_xml_property (child, "linePadding");
  style->inherited_styles = get_xml_property (child, "style");

  style->id = g_strdup (style_id);         /* populate properties */
  return style;
}


#if 0
RegionProp *
add_new_region (const gchar * region_id, xmlNodePtr child)
{
  RegionProp *region = g_new0 (RegionProp, 1);
  GST_CAT_DEBUG (ebuttdparse, "add_new_region");

  region->origin = get_xml_property (child, "origin");
  region->extent = get_xml_property (child, "extent");
  region->style = get_xml_property (child, "style");
  region->display_align = get_xml_property (child, "displayAlign");
  region->padding = get_xml_property (child, "padding");
  region->writing_mode = get_xml_property (child, "writingMode");
  region->show_background = get_xml_property (child, "showBackground");
  region->overflow = get_xml_property (child, "overflow");

  region->id = g_strdup (region_id);
  GST_CAT_DEBUG (ebuttdparse, "Region added:");
  /*_print_region (region);*/
  return region;
}
#endif


#if 0
static void
delete_style (StyleProp * style)
{
  g_return_if_fail (style != NULL);
  GST_CAT_DEBUG (ebuttdparse, "Deleting style %p...", style);

  g_free (style->direction);
  g_free (style->fontFamily);
  g_free (style->fontSize);
  g_free (style->lineHeight);
  g_free (style->textAlign);
  g_free (style->color);
  g_free (style->backgroundColor);
  g_free (style->fontStyle);
  g_free (style->fontWeight);
  g_free (style->textDecoration);
  g_free (style->unicodeBidi);
  g_free (style->wrapOption);
  g_free (style->multiRowAlign);
  g_free (style->linePadding);
  g_free (style->inherited_styles);
}


static void
delete_region (RegionProp * region)
{
  g_return_if_fail (region != NULL);
  GST_CAT_DEBUG (ebuttdparse, "Deleting region %p...", region);

  g_free (region->id);
  g_free (region->origin);
  g_free (region->extent);
  g_free (region->style);
  g_free (region->display_align);
  g_free (region->padding);
  g_free (region->writing_mode);
  g_free (region->show_background);
  g_free (region->overflow);
}
#endif


gchar *
extract_style_frm_p (const gchar * para_text)
{
  /*
   * passed the p segment of the text, extracts span and span class
   * which correspond to a style.
   */

  gchar *style;
  gchar *end_style;

  /* extract the style */
  style = g_strstr_len (para_text, -1, "style");        /* pointer to first character */
  style = g_strstr_len (style, -1, "\"");
  style++;                      /*remove first character */
  end_style = g_strstr_len (style, -1, "\"");   /* pointer to final style character +1 */
  style = g_strndup (style, end_style - style); /* fetch text between pointers */

  return style;
}

gchar *
extract_text (SubtitleObj * subtitle, const gchar * line)
{
  gchar **removed_br;
  gchar *ret_text = NULL;

  /* now get contents of <p> including style span */
  ret_text = fetch_child (line);

  /* examine span tag for style */
  if (g_strstr_len (ret_text, -1, "<span") != NULL) {
    /* save for later. */
    subtitle->style = extract_style_frm_p (ret_text);
    /* fetch the inner text of <span> */
    ret_text = fetch_child (ret_text);
  }

  /* sort out line breaks */
  /* ...if there are any */
  if (g_strstr_len (ret_text, -1, "<br/>") != NULL) {
    removed_br = g_strsplit (ret_text, "<br/>", -1);    /*assume no spaces <br /> */
    ret_text = g_strjoinv (" \n", removed_br);
  }

  /* save the raw sub text with no markup */
  subtitle->text = ret_text;

  return ret_text;
}

guint64
extract_timestamp (xmlChar * time_as_string)
{
  gchar **split_time;
  gint64 hours;
  gint64 mins;
  gdouble secs;
  gint64 total_seconds;
  gint i;

  /* now convert to GTIME */
  /* First split using : */
  split_time = g_strsplit ((gchar *)time_as_string, ":", -1);

  /* remove leading "0"s */
  for (i = 0; i < 3; ++i) {
    if (split_time[i][0] == '0') {
      split_time[i]++;
    }
  }

  /* convert to ints */
  hours = g_ascii_strtoll (split_time[0], NULL, 10);
  mins = g_ascii_strtoll (split_time[1], NULL, 10);
  secs = g_strtod (split_time[2], NULL);

  //g_strfreev(split_time); /* TODO: What else should I free up?*/

  /* convert to Gstreamer time */
  total_seconds = (1000 * (secs + 60 * mins + 3600 * hours)) * GST_MSECOND;

  return total_seconds;
}

void
add_to_document_metadata (DocMetadata * document_metadata, const gchar * type,
    const gchar * line)
{
  gchar *element_name;
  gchar *element;
  /* extract element name */
  if (g_strcmp0 (type, "xmlns=") == 0) {
    element_name = "xmlns";
  } else {
    element_name = fetch_element_name (type, "=", line);
  }

  element = fetch_element (element_name, line);

  /* first pick prefix */
  if (g_strcmp0 (type, "xmlns:") == 0) {
    /* now element name */
    if (g_strcmp0 (element_name, "ttp") == 0) {
      document_metadata->ttp = element;
    } else if (g_strcmp0 (element_name, "tts") == 0) {
      document_metadata->tts = element;
    } else if (g_strcmp0 (element_name, "ttm") == 0) {
      document_metadata->ttm = element;
    }
    if (g_strcmp0 (element_name, "ebuttm") == 0) {
      document_metadata->ebuttm = element;
    }
    if (g_strcmp0 (element_name, "ebutts") == 0) {
      document_metadata->ebutts = element;
    } else {
      document_metadata->xmlns = element;
    }
  }
  if (g_strcmp0 (type, "xml:") == 0) {
    if (g_strcmp0 (element_name, "space") == 0) {
      document_metadata->space = element;
    }
    if (g_strcmp0 (element_name, "lang") == 0) {
      document_metadata->lang = element;
    }
  }
  if (g_strcmp0 (type, "ttp:") == 0) {
    if (g_strcmp0 (element_name, "timeBase") == 0) {
      document_metadata->timeBase = element;
    }
    if (g_strcmp0 (element_name, "cellResolution") == 0) {
      gchar *end_x, *cell_x, *cell_y;
      document_metadata->cellResolution = element;      /* looks like "40 24" */

      end_x = g_strstr_len (element, -1, " ");  /* pointer between two the values */
      cell_x = g_strndup (element, (end_x - element));  /*cut x off */
      cell_y = end_x + 1;

      document_metadata->cell_resolution_x = cell_x;
      document_metadata->cell_resolution_y = cell_y;
    }
  } else {
    document_metadata->xmlns = element;
  }
}


gint
create_element_tree (const gchar * xml_file_buffer,
    xmlDocPtr * doc_ptr, xmlNodePtr * cur_ptr)
{
  xmlDocPtr doc;
  xmlNodePtr cur;
  /* xmlReadMemory takes a char array and a fake file name */

  doc = xmlReadMemory (xml_file_buffer,
      strlen (xml_file_buffer), "any_doc_name", NULL, 0);

  if (doc == NULL) {
    GST_DEBUG ("Document not parsed successfully");        /* error */
    return 1;
  }

  cur = xmlDocGetRootElement (doc);

  if (cur == NULL) {
    GST_DEBUG ("empty document");
    xmlFreeDoc (doc);
    return 1;
  }

  if (xmlStrcmp (cur->name, (const xmlChar *) "tt")) {
    fprintf (stderr, "document of the wrong type, root node != tt");
    xmlFreeDoc (doc);
    return 1;
  }

  *doc_ptr = doc;
  *cur_ptr = cur;

  return 0;
}

SubSubtitle *
sub_subtitle_list_last (SubSubtitle * list)
{
  if (list) {
    while (list->next)
      list = list->next;
  }
  return list;
}

SubSubtitle *
add_new_subsubtitle (SubSubtitle * sub_subtitle,
    xmlNodePtr child_node,
    InheritanceList * inherited_styles, InheritanceList * inherited_regions)
{
  SubSubtitle *sub_subtitle_new;
  SubSubtitle *last;
  gchar *content;
  gchar *content_minus_whitespace;

  /* remove any whitespace */
  content = (gchar *) xmlNodeGetContent (child_node);
  GST_CAT_DEBUG (ebuttd_parse_debug,
      "Found text: \"%s\" in node in tag: %s",
      content, child_node->parent->name);
  content_minus_whitespace = g_strchug (content);

  /* nothing left? return as is. */
  if (g_strcmp0 (content_minus_whitespace, "") == 0) {
    GST_CAT_DEBUG (ebuttd_parse_debug,
        "false alarm, just whitespace, in tag: %s", child_node->parent->name);
    return sub_subtitle;
  }

  sub_subtitle_new = g_new (SubSubtitle, 1);
  sub_subtitle_new->text = content;
  /* linked list styles and region. have been prepending so reverse */
  sub_subtitle_new->styles = g_list_copy ((GList *) inherited_styles);
  sub_subtitle_new->regions = g_list_copy ((GList *) inherited_regions);
  sub_subtitle_new->next = NULL;

  GST_CAT_DEBUG (ebuttd_parse_debug,
      "Created new sub_subtile: %s with styles: %s and regions %s",
      sub_subtitle_new->text,
      glist_print (inherited_styles), glist_print (inherited_regions));

  if (sub_subtitle) {
    last = sub_subtitle_list_last (sub_subtitle);
    last->next = sub_subtitle_new;
    sub_subtitle_new->previous = last;

    return sub_subtitle;
  } else {
    sub_subtitle_new->previous = NULL;
    return sub_subtitle_new;
  }
}

void
markup_style_add_if_null (StyleProp * markup_style, StyleProp * style_props)
{
  if (!(markup_style->direction))
    markup_style->direction = style_props->direction;
  if (!(markup_style->fontFamily))
    markup_style->fontFamily = style_props->fontFamily;
  if (!(markup_style->fontSize))
    markup_style->fontSize = style_props->fontSize;
  if (!(markup_style->lineHeight))
    markup_style->lineHeight = style_props->lineHeight;
  if (!(markup_style->textAlign))
    markup_style->textAlign = style_props->textAlign;
  if (!(markup_style->color))
    markup_style->color = style_props->color;
  if (!(markup_style->backgroundColor))
    markup_style->backgroundColor = style_props->backgroundColor;
  if (!(markup_style->fontStyle))
    markup_style->fontStyle = style_props->fontStyle;
  if (!(markup_style->fontWeight))
    markup_style->fontWeight = style_props->fontWeight;
  if (!(markup_style->textDecoration))
    markup_style->textDecoration = style_props->textDecoration;
  if (!(markup_style->unicodeBidi))
    markup_style->unicodeBidi = style_props->unicodeBidi;
  if (!(markup_style->wrapOption))
    markup_style->wrapOption = style_props->wrapOption;
  if (!(markup_style->multiRowAlign))
    markup_style->multiRowAlign = style_props->multiRowAlign;
  if (!(markup_style->linePadding))
    markup_style->linePadding = style_props->linePadding;
  if (!(markup_style->inherited_styles))
    markup_style->inherited_styles = style_props->inherited_styles;
}


void
inherit_styles_iterator (gpointer g_style,
    DataForStyleIterator * data_for_iterator)
{
  gchar *style_id = (gchar *) g_style;
  StyleProp *style_props;
  StyleProp *markup_style = data_for_iterator->markup_style;    /* struct holding the styles to be used on this subsubtitle */
  GHashTable *style_hash = data_for_iterator->style_hash;

  /* look up style_id */
  style_props = (StyleProp *) g_hash_table_lookup (style_hash,
      (gconstpointer) style_id);
  GST_CAT_DEBUG (ebuttdparse, "## Style:");
  /*_print_style (style_props);*/
  if (style_props)
    markup_style_add_if_null (markup_style, style_props);
}


static void
_append_region_description (const gchar * region_name, RegionProp * properties,
    gchar ** string)
{
  gchar str_store[512] = { '\0' };
  gchar *s = str_store;

  /*g_return_if_fail (region_name != NULL);*/
  g_return_if_fail (properties != NULL);
  g_return_if_fail (properties->id != NULL);

  /*GST_CAT_DEBUG (ebuttdparse, "_append_region_description; string = %p", string);*/
  /* Create initial string, `<region ` */
  s = g_stpcpy (s, "<region ");

  /* If a property is present, add it to string. */
  if (properties->origin) {
    s = g_stpcpy (s, "origin=\"");
    s = g_stpcpy (s, properties->origin);
    s = g_stpcpy (s, "\" ");
  }
  if (properties->extent) {
    s = g_stpcpy (s, "extent=\"");
    s = g_stpcpy (s, properties->extent);
    s = g_stpcpy (s, "\" ");
  }
  if (properties->style) {
    s = g_stpcpy (s, "style=\"");
    s = g_stpcpy (s, properties->style);
    s = g_stpcpy (s, "\" ");
  }
  if (properties->display_align) {
    s = g_stpcpy (s, "display_align=\"");
    s = g_stpcpy (s, properties->display_align);
    s = g_stpcpy (s, "\" ");
  }
  if (properties->padding) {
    s = g_stpcpy (s, "padding=\"");
    s = g_stpcpy (s, properties->padding);
    s = g_stpcpy (s, "\" ");
  }
  if (properties->writing_mode) {
    s = g_stpcpy (s, "writing_mode=\"");
    s = g_stpcpy (s, properties->writing_mode);
    s = g_stpcpy (s, "\" ");
  }
  if (properties->show_background) {
    s = g_stpcpy (s, "show_background=\"");
    s = g_stpcpy (s, properties->show_background);
    s = g_stpcpy (s, "\" ");
  }
  if (properties->overflow) {
    s = g_stpcpy (s, "overflow=\"");
    s = g_stpcpy (s, properties->overflow);
    s = g_stpcpy (s, "\" ");
  }

  /* Terminate string, `>` */
  s = g_stpcpy (s, ">");
  s = g_strdup (str_store);

  /* Replace input string with the created string. */
  g_free (*string);
  *string = s;
}


static void
add_region_description (gchar ** string, RegionProp * region)
{
  gchar *combined_string = NULL;
  gchar *region_string = g_strdup ("");

  g_return_if_fail (string != NULL);
  g_return_if_fail (*string != NULL);
  g_return_if_fail (region != NULL);

  _append_region_description (NULL, region, &region_string);

  GST_CAT_DEBUG (ebuttdparse, "Region string: %s", region_string);

  /* Add region string to start of marked-up subtitle string. */
  combined_string = g_strconcat (region_string, "\n", *string, NULL);
  GST_CAT_DEBUG (ebuttdparse, "Combined string: %s", combined_string);
  g_free (region_string);
  g_free (*string);
  *string = combined_string;
}


static void
add_region_descriptions (gchar ** string, GHashTable * region_hash)
{
  gchar *combined_string = NULL;
  gchar *region_string = g_strdup ("");

  g_return_if_fail (string != NULL);
  g_return_if_fail (*string != NULL);
  g_return_if_fail (region_hash != NULL);

  g_hash_table_foreach (region_hash, (GHFunc) _append_region_description,
      (gpointer) &region_string);

  GST_CAT_DEBUG (ebuttdparse, "Region string: %s", region_string);

  /* Add region string to start of marked-up subtitle string. */
  combined_string = g_strconcat (region_string, "\n", *string, NULL);
  GST_CAT_DEBUG (ebuttdparse, "Combined string: %s", combined_string);
  g_free (region_string);
  g_free (*string);
  *string = combined_string;
}


gchar *
sub_subtitle_concat_markup (SubSubtitle * sub_subtitle,
    GHashTable * style_hash, GHashTable * region_hash,
    DocMetadata * document_metadata)
{
  /**
   * for each sub_subitle:
   *   apply all style and region info, go up the inheritance hierarchy
   *   only add new styles. Concatenate.
   */
  gchar *ret = g_strdup ("");
  gchar *tmp = NULL;
  SubSubtitle *sub_sub;         /* a single sub_subtitle iterable the list */
  DataForStyleIterator *data_for_iterator = g_new (DataForStyleIterator, 1);
  data_for_iterator->style_hash = style_hash;
  gchar *region;

  for (sub_sub = sub_subtitle; sub_sub != NULL; sub_sub = sub_sub->next) {
    /**
     * flatten styles:
     *   If inherited length of inherited styles is > 1
     *   only add style property to
     *   add style property tuple if not already on the list
     */
    StyleProp *markup_style;    /* the styles to be applied as markup */
    InheritanceList *inherited_styles = sub_sub->styles;

    /* populate data struct for iterator */
    markup_style = g_new0 (StyleProp, 1);
    data_for_iterator->markup_style = markup_style;     /*update on each it. */

    /* takes each style id from inherited styles and creates a
       markup list of styles free markup_style */
    g_list_foreach (inherited_styles,
        (GFunc) inherit_styles_iterator, data_for_iterator);

    /* XXX: Assumes a single region(?) */
    region = g_list_first (sub_sub->regions)->data;

    /* now add styles to this sub_sub text */
    add_style_markup (&(sub_sub->text), markup_style, region,
        document_metadata);

    tmp = ret;
    ret = g_strconcat (ret, sub_sub->text, NULL);
    g_free (tmp);
    g_free (markup_style);      /* TODO deeper free to free pointers too */
  }

  /* Add region information here... */
  /*add_region_descriptions (&ret, region_hash);*/

  return ret;
}

InheritanceList *
inheritance_list_prepend (InheritanceList * inheritance_list, gchar * ids_str)
{
  int i;                        /* iterator */
  gchar **id_array;             /* array to hold split ids eg ["s1", "s2"] */

  id_array = g_strsplit (ids_str, " ", -1);
  i = 0;
  while (id_array[i] != NULL) {
    inheritance_list = g_list_prepend (inheritance_list,
        (gpointer) id_array[i]);

    i++;
  }
  return inheritance_list;
}

InheritanceList *
inheritance_list_remove_first (InheritanceList * inheritance_list,
    gint number_to_remove)
{
  gint i;

  for (i = 0; i < number_to_remove; ++i)
    inheritance_list = g_list_remove_link (inheritance_list, inheritance_list);

  return inheritance_list;      /* new start of list */
}

void
extract_prepend_style_region (xmlNodePtr cur,
    InheritanceList ** styles,
    InheritanceList ** regions, gint * styles_count, gint * regions_count)
{
  gchar *styles_str = NULL;
  gchar *regions_str = NULL;

  if (styles) {
    styles_str = (gchar *) (xmlGetProp (cur, (xmlChar *)"style"));
    if (styles_str) {
      *styles = inheritance_list_prepend (*styles, styles_str);
      if (styles_count)
        (*styles_count)++;

      GST_CAT_DEBUG (ebuttdparse, "From tag: \"%s\" prepended \"%s\" to style list %s",
          cur->name, styles_str, glist_print (*styles));
    }
  }
  if (regions) {
    regions_str = (gchar *) (xmlGetProp (cur, (xmlChar *)"region"));
    if (regions_str) {
      *regions = inheritance_list_prepend (*regions, regions_str);
      if (regions_count)
        (*regions_count)++;

      GST_CAT_DEBUG (ebuttdparse, "From tag: \"%s\" prepended \"%s\" to region list %s",
          cur->name, regions_str, glist_print (*regions));
    }
  }

  if (styles_str)
    g_free (styles_str);
  if (regions_str)
    g_free (regions_str);
}

/*
FIXME: Make this work for <span>First Line <br></span>
*  At present it only works for <span><br>Second Line </span>
**/
int
handle_line_break (xmlNodePtr node, SubSubtitle * sub_subtitle)
{
  gboolean found_br = FALSE;
  SubSubtitle *sub_subtitle_last;

  GST_CAT_DEBUG (ebuttd_parse_debug,
      "Checking for line breaks. Passed node: %s", node->name);

  if (xmlStrcmp (node->name, (const xmlChar *) "br") == 0) {
    /* if there is a line break element add to previous sub_sub
     * TODO: else statement to create a new sub_sub to hold the br tag
     */
    if (sub_subtitle) {
      gchar *new_text;
      found_br = TRUE;

      /* get a pointer to previous sub_subtitle */
      sub_subtitle_last = sub_subtitle_list_last (sub_subtitle);

      /* append a pango line break to the previous sub_subtitle in a new str */
      new_text = g_strconcat (sub_subtitle_last->text, " \n", NULL);

      GST_CAT_DEBUG (ebuttd_parse_debug_ebutt,
          "Have just joined sub_subtitle_last: %s with a %s to create %s",
          sub_subtitle_last->text, " \\n", new_text);

      /* clear the text in the previous sub_subtitle */
      g_free (sub_subtitle_last->text);

      /* point the text pointer at this newly allocated memory */
      sub_subtitle_last->text = new_text;
    }
  }
  return found_br;
}

DocMetadata *
extract_tt_tag_properties (xmlNodePtr ttnode, DocMetadata * document_metadata)
{
  gchar *prop;
  const xmlChar *node_name;
  /*xmlNodePtr prop_node, xmlns_node;*/
  xmlAttrPtr prop_node;

  if (!document_metadata)
    document_metadata = g_new0 (DocMetadata, 1);

#if 0
  prop = (gchar *) xmlNodeGetContent (ttnode);
  document_metadata->xmlns = prop;
  prop = (gchar *) xmlNodeGetContent (ttnode);
  document_metadata->ttp = prop;
  prop = (gchar *) xmlNodeGetContent (ttnode);
  document_metadata->tts = prop;
  prop = (gchar *) xmlNodeGetContent (ttnode);
  document_metadata->ttm = prop;
  prop = (gchar *) xmlNodeGetContent (ttnode);
  document_metadata->ebuttm = prop;
  prop = (gchar *) xmlNodeGetContent (ttnode);
  document_metadata->ebutts = prop;
#endif

  prop_node = ttnode->properties;       /* ->name will give lang etc */

  while (prop_node != NULL) {
    node_name = prop_node->name;
    prop = (gchar *) xmlNodeGetContent (prop_node->children);   /* use first child as the namespace can only have one value */

    if (xmlStrcmp (node_name, (const xmlChar *) "lang") == 0)
      document_metadata->lang = prop;
    if (xmlStrcmp (node_name, (const xmlChar *) "space") == 0)
      document_metadata->space = prop;
    if (xmlStrcmp (node_name, (const xmlChar *) "timeBase") == 0)
      document_metadata->timeBase = prop;
    if (xmlStrcmp (node_name, (const xmlChar *) "cellResolution") == 0) {
      gchar *end_x, *cell_x, *cell_y;
      document_metadata->cellResolution = prop; /* looks like "40 24" */

      end_x = g_strstr_len (prop, -1, " ");     /* pointer between two the values */
      cell_x = g_strndup (prop, (end_x - prop));        /*cut x off */
      cell_y = end_x + 1;

      document_metadata->cell_resolution_x = cell_x;
      document_metadata->cell_resolution_y = cell_y;
    }
    prop_node = prop_node->next;
  }
  return document_metadata;
}


#if 0
static GstEbuttdMediaTime
parse_timecode (const gchar * timestring)
{
  gchar **strings;
  const gchar *dec_point;
  GstEbuttdMediaTime time = { 0U, 0U, 0U, 0U };

  g_return_val_if_fail (timestring != NULL, time);
  /*GST_CAT_DEBUG (ebuttdparse, "parse_timecode (%s)", timestring);*/

  strings = g_strsplit (timestring, ":", 3);
  if (g_strv_length (strings) != 3U) {
    GST_CAT_ERROR (ebuttdparse, "badly formatted time string: %s", timestring);
    return time;
  }

  time.hours = (guint) g_ascii_strtoull (strings[0], NULL, 10U);
  time.minutes = (guint) g_ascii_strtoull (strings[1], NULL, 10U);
  if ((dec_point = g_strstr_len (strings[2], strlen (strings[2]), "."))) {
    char ** substrings = g_strsplit (strings[2], ".", 2);
    time.seconds = (guint) g_ascii_strtoull (substrings[0], NULL, 10U);
    time.milliseconds = (guint) g_ascii_strtoull (substrings[1], NULL, 10U);
    if (strlen (substrings[1]) > 3) {
      GST_CAT_ERROR (ebuttdparse, "badly formatted time string "
          "(too many millisecond digits): %s\n", timestring);
    }
    g_strfreev (substrings);
  } else {
    time.seconds = (guint) g_ascii_strtoull (strings[2], NULL, 10U);
  }

  if (time.minutes > 59 || time.seconds > 60) {
    GST_CAT_ERROR (ebuttdparse, "invalid time string "
        "(minutes or seconds out-of-bounds): %s\n", timestring);
  }

  g_strfreev (strings);
  GST_CAT_LOG (ebuttdparse,
      "hours: %u  minutes: %u  seconds: %u  milliseconds: %u",
      time.hours, time.minutes, time.seconds, time.milliseconds);
  return time;
}
#endif


static GstClockTime
parse_timecode (const gchar * timestring)
{
  gchar **strings;
  const gchar *dec_point;
  guint64 hours = 0, minutes = 0, seconds = 0, milliseconds = 0;
  GstClockTime time = GST_CLOCK_TIME_NONE;

  g_return_val_if_fail (timestring != NULL, time);
  /*GST_CAT_DEBUG (ebuttdparse, "parse_timecode (%s)", timestring);*/

  strings = g_strsplit (timestring, ":", 3);
  if (g_strv_length (strings) != 3U) {
    GST_CAT_ERROR (ebuttdparse, "badly formatted time string: %s", timestring);
    return time;
  }

  hours = g_ascii_strtoull (strings[0], NULL, 10U);
  minutes = g_ascii_strtoull (strings[1], NULL, 10U);
  if ((dec_point = g_strstr_len (strings[2], strlen (strings[2]), "."))) {
    guint n_digits;
    char ** substrings = g_strsplit (strings[2], ".", 2);
    seconds = g_ascii_strtoull (substrings[0], NULL, 10U);
    n_digits = strlen (substrings[1]);
    if (n_digits > 3) {
      GST_CAT_ERROR (ebuttdparse, "badly formatted time string "
          "(too many millisecond digits): %s\n", timestring);
    } else {
      milliseconds = g_ascii_strtoull (substrings[1], NULL, 10U);
      for (n_digits = (3 - n_digits); n_digits; --n_digits)
        milliseconds *= 10;
    }
    g_strfreev (substrings);
  } else {
    seconds = g_ascii_strtoull (strings[2], NULL, 10U);
  }

  if (minutes > 59 || seconds > 60) {
    GST_CAT_ERROR (ebuttdparse, "invalid time string "
        "(minutes or seconds out-of-bounds): %s\n", timestring);
  }

  g_strfreev (strings);
  GST_CAT_LOG (ebuttdparse,
      "hours: %llu  minutes: %llu  seconds: %llu  milliseconds: %llu",
      hours, minutes, seconds, milliseconds);

  time = hours * GST_SECOND * 3600
       + minutes * GST_SECOND * 60
       + seconds * GST_SECOND
       + milliseconds * GST_MSECOND;

  return time;
}


static GstEbuttdElement *
parse_element (const xmlNode * node)
{
  GstEbuttdElement *element;
  xmlChar *string;

  g_return_val_if_fail (node != NULL, NULL);

  element = g_new0 (GstEbuttdElement, 1);
  GST_CAT_DEBUG (ebuttdparse, "Element name: %s", (const char*) node->name);
  if ((g_strcmp0 ((const char*) node->name, "style") == 0)) {
    element->type = GST_EBUTTD_ELEMENT_TYPE_STYLE;
  } else if ((g_strcmp0 ((const char*) node->name, "region") == 0)) {
    element->type = GST_EBUTTD_ELEMENT_TYPE_REGION;
  } else if ((g_strcmp0 ((const char*) node->name, "body") == 0)) {
    element->type = GST_EBUTTD_ELEMENT_TYPE_BODY;
  } else if ((g_strcmp0 ((const char*) node->name, "div") == 0)) {
    element->type = GST_EBUTTD_ELEMENT_TYPE_DIV;
  } else if ((g_strcmp0 ((const char*) node->name, "p") == 0)) {
    element->type = GST_EBUTTD_ELEMENT_TYPE_P;
  } else if ((g_strcmp0 ((const char*) node->name, "span") == 0)) {
    element->type = GST_EBUTTD_ELEMENT_TYPE_SPAN;
  } else if ((g_strcmp0 ((const char*) node->name, "text") == 0)) {
    element->type = GST_EBUTTD_ELEMENT_TYPE_ANON_SPAN;
  } else if ((g_strcmp0 ((const char*) node->name, "br") == 0)) {
    element->type = GST_EBUTTD_ELEMENT_TYPE_BR;
  } else {
    GST_CAT_ERROR (ebuttdparse, "illegal element type: %s",
        (const char*) node->name);
    g_assert (TRUE);
  }

  if ((string = xmlGetProp (node, (const xmlChar*) "id"))) {
    element->id = g_strdup ((const gchar*) string);
    xmlFree (string);
  }

  if ((string = xmlGetProp (node, (const xmlChar*) "style"))) {
    element->styles = g_strsplit ((const gchar*) string, " ", 0);
    GST_CAT_DEBUG (ebuttdparse, "%u style(s) referenced in element.",
        g_strv_length (element->styles));
    xmlFree (string);
  }

  /* XXX: Place parsing of attributes that are common to all element types
   * before this line. */

  if (element->type == GST_EBUTTD_ELEMENT_TYPE_STYLE
      || element->type == GST_EBUTTD_ELEMENT_TYPE_REGION) {
    GstEbuttdStyleSet *ss;
    ss = parse_style_set (node);
    if (ss)
      element->style_set = ss;
    else
      GST_CAT_WARNING (ebuttdparse,
          "Style or Region contains no styling attributes.");
    return element;
  }

  if ((string = xmlGetProp (node, (const xmlChar*) "region"))) {
    element->region = g_strdup ((const gchar*) string);
    xmlFree (string);
  }

  if ((string = xmlGetProp (node, (const xmlChar*) "begin"))) {
    element->begin = parse_timecode ((const gchar*) string);
    xmlFree (string);
  } else {
    element->begin = GST_CLOCK_TIME_NONE;
  }

  if ((string = xmlGetProp (node, (const xmlChar*) "end"))) {
    element->end = parse_timecode ((const gchar*) string);
    xmlFree (string);
  } else {
    element->end = GST_CLOCK_TIME_NONE;
  }
  /*GST_CAT_DEBUG (ebuttdparse, "element->begin: %llu   element->end: %llu",
      element->begin, element->end);*/

  if (node->content) {
    GST_CAT_LOG (ebuttdparse, "Node content: %s", node->content);
    element->text = g_strdup ((gchar*) node->content);
  }

  return element;
}


static GNode *
parse_tree (const xmlNode * node)
{
  GNode *ret;
  GstEbuttdElement *element;

  g_return_val_if_fail (node != NULL, NULL);
  GST_CAT_LOG (ebuttdparse, "parsing node %s", node->name);
  element = parse_element (node);
  ret = g_node_new (element);

  for (node = node->children; node != NULL; node = node->next) {
    GNode *descendants = NULL;
    if (!xmlIsBlankNode (node) && (descendants = parse_tree (node)))
        g_node_append (ret, descendants);
  }

  return ret;
}


static void
update_style_set (GstSubtitleStyleSet * ss, GstEbuttdStyleSet * ess)
{
  g_return_val_if_fail (ss != NULL, NULL);
  g_return_val_if_fail (ess != NULL, NULL);

  if (ess->text_direction) {
    if (g_strcmp0 (ess->text_direction, "rtl") == 0)
      ss->text_direction = GST_EBUTTD_TEXT_DIRECTION_RTL;
    else
      ss->text_direction = GST_EBUTTD_TEXT_DIRECTION_LTR;
  }

  if (ess->font_family) {
    gsize length = g_strlcpy (ss->font_family, ess->font_family,
        MAX_FONT_FAMILY_NAME_LENGTH);
    if (length > MAX_FONT_FAMILY_NAME_LENGTH)
      GST_CAT_ERROR (ebuttdparse, "Font family name is too long.");
  }

  if (ess->font_size) {
    ss->font_size = g_ascii_strtod (ess->font_size, NULL);
  }

  if (ess->line_height) {
    if (g_strcmp0 (ess->line_height, "normal") == 0)
      ss->line_height = 125.0;
    else
      ss->line_height = g_ascii_strtod (ess->line_height, NULL);
  }

  if (ess->text_align) {
    if (g_strcmp0 (ess->text_align, "left") == 0)
      ss->text_align = GST_EBUTTD_TEXT_ALIGN_LEFT;
    else if (g_strcmp0 (ess->text_align, "center") == 0)
      ss->text_align = GST_EBUTTD_TEXT_ALIGN_CENTER;
    else if (g_strcmp0 (ess->text_align, "right") == 0)
      ss->text_align = GST_EBUTTD_TEXT_ALIGN_RIGHT;
    else if (g_strcmp0 (ess->text_align, "end") == 0)
      ss->text_align = GST_EBUTTD_TEXT_ALIGN_END;
    else
      ss->text_align = GST_EBUTTD_TEXT_ALIGN_START;
  }

  if (ess->color) {
    ss->color = parse_ebuttd_colorstring (ess->color);
  }

  if (ess->bg_color) {
    ss->bg_color = parse_ebuttd_colorstring (ess->bg_color);
  }

  if (ess->font_style) {
    if (g_strcmp0 (ess->font_style, "italic") == 0)
      ss->font_style = GST_EBUTTD_FONT_STYLE_ITALIC;
    else
      ss->font_style = GST_EBUTTD_FONT_STYLE_NORMAL;
  }

  if (ess->font_weight) {
    if (g_strcmp0 (ess->font_weight, "bold") == 0)
      ss->font_weight = GST_EBUTTD_FONT_WEIGHT_BOLD;
    else
      ss->font_weight = GST_EBUTTD_FONT_WEIGHT_NORMAL;
  }

  if (ess->text_decoration) {
    if (g_strcmp0 (ess->text_decoration, "underline") == 0)
      ss->text_decoration = GST_EBUTTD_TEXT_DECORATION_UNDERLINE;
    else
      ss->text_decoration = GST_EBUTTD_TEXT_DECORATION_NONE;
  }

  if (ess->unicode_bidi) {
    if (g_strcmp0 (ess->unicode_bidi, "embed") == 0)
      ss->unicode_bidi = GST_EBUTTD_UNICODE_BIDI_EMBED;
    else if (g_strcmp0 (ess->unicode_bidi, "bidiOverride") == 0)
      ss->unicode_bidi = GST_EBUTTD_UNICODE_BIDI_OVERRIDE;
    else
      ss->unicode_bidi = GST_EBUTTD_UNICODE_BIDI_NORMAL;
  }

  if (ess->wrap_option) {
    if (g_strcmp0 (ess->wrap_option, "noWrap") == 0)
      ss->wrap_option = GST_EBUTTD_WRAPPING_OFF;
    else
      ss->wrap_option = GST_EBUTTD_WRAPPING_ON;
  }

  if (ess->multi_row_align) {
    if (g_strcmp0 (ess->multi_row_align, "start") == 0)
      ss->multi_row_align = GST_EBUTTD_MULTI_ROW_ALIGN_START;
    else if (g_strcmp0 (ess->multi_row_align, "center") == 0)
      ss->multi_row_align = GST_EBUTTD_MULTI_ROW_ALIGN_CENTER;
    else if (g_strcmp0 (ess->multi_row_align, "end") == 0)
      ss->multi_row_align = GST_EBUTTD_MULTI_ROW_ALIGN_END;
    else
      ss->multi_row_align = GST_EBUTTD_MULTI_ROW_ALIGN_AUTO;
  }

  if (ess->line_padding) {
    ss->line_padding = g_ascii_strtod (ess->line_padding, NULL);
  }

  if (ess->origin) {
    gchar *c;
    ss->origin_x = g_ascii_strtod (ess->origin, &c);
    while (!g_ascii_isdigit (*c) && *c != '+' && *c != '-') ++c;
    ss->origin_y = g_ascii_strtod (c, NULL);
    /*GST_CAT_DEBUG (ebuttdparse, "origin_x: %g   origin_y: %g", ss->origin_x, ss->origin_y);*/
  }

  if (ess->extent) {
    gchar *c;
    ss->extent_w = g_ascii_strtod (ess->extent, &c);
    ss->extent_w = (ss->extent_w > 100.0) ? 100.0 : ss->extent_w;
    while (!g_ascii_isdigit (*c) && *c != '+' && *c != '-') ++c;
    ss->extent_h = g_ascii_strtod (c, NULL);
    ss->extent_h = (ss->extent_h > 100.0) ? 100.0 : ss->extent_h;
    /*GST_CAT_DEBUG (ebuttdparse, "extent_w: %g   extent_h: %g", ss->extent_w, ss->extent_h);*/
  }

  if (ess->display_align) {
    if (g_strcmp0 (ess->display_align, "center") == 0)
      ss->display_align = GST_EBUTTD_DISPLAY_ALIGN_CENTER;
    else if (g_strcmp0 (ess->display_align, "after") == 0)
      ss->display_align = GST_EBUTTD_DISPLAY_ALIGN_AFTER;
    else
      ss->display_align = GST_EBUTTD_DISPLAY_ALIGN_BEFORE;
    /*GST_CAT_DEBUG (ebuttdparse, "displayAlign: %d", ss->display_align);*/
  }

  if (ess->padding) {
    gchar **decimals;
    guint n_decimals;
    gint i;

    decimals = g_strsplit (ess->padding, "%", 0);
    n_decimals = g_strv_length (decimals) - 1;
    for (i = 0; i < n_decimals; ++i) {
      g_strstrip (decimals[i]);
    }

    switch (n_decimals) {
      case 1:
        ss->padding_start = ss->padding_end =
          ss->padding_before = ss->padding_after =
          g_ascii_strtod (decimals[0], NULL);
        break;

      case 2:
        ss->padding_before = ss->padding_after =
          g_ascii_strtod (decimals[0], NULL);
        ss->padding_start = ss->padding_end =
          g_ascii_strtod (decimals[1], NULL);
        break;

      case 3:
        ss->padding_before = g_ascii_strtod (decimals[0], NULL);
        ss->padding_start = ss->padding_end =
          g_ascii_strtod (decimals[1], NULL);
        ss->padding_after = g_ascii_strtod (decimals[2], NULL);
        break;

      case 4:
        ss->padding_before = g_ascii_strtod (decimals[0], NULL);
        ss->padding_end = g_ascii_strtod (decimals[1], NULL);
        ss->padding_after = g_ascii_strtod (decimals[2], NULL);
        ss->padding_start = g_ascii_strtod (decimals[3], NULL);
        break;
    }
    /*g_print ("paddingStart: %g  padding_end: %g  padding_before: %g "
        "padding_after: %g\n", ss->padding_start, ss->padding_end,
        ss->padding_before, ss->padding_after);*/
    g_strfreev (decimals);
  }

  if (ess->writing_mode) {
    if (g_str_has_prefix (ess->writing_mode, "rl"))
      ss->writing_mode = GST_EBUTTD_WRITING_MODE_RLTB;
    else if ((g_strcmp0 (ess->writing_mode, "tbrl") == 0)
        || (g_strcmp0 (ess->writing_mode, "tb") == 0))
      ss->writing_mode = GST_EBUTTD_WRITING_MODE_TBRL;
    else if (g_strcmp0 (ess->writing_mode, "tblr") == 0)
      ss->writing_mode = GST_EBUTTD_WRITING_MODE_TBLR;
    else
      ss->writing_mode = GST_EBUTTD_WRITING_MODE_LRTB;
    /*GST_CAT_DEBUG (ebuttdparse, "writingMode: %d", ss->writing_mode);*/
  }

  if (ess->show_background) {
    if (g_strcmp0 (ess->show_background, "whenActive") == 0)
      ss->show_background = GST_EBUTTD_BACKGROUND_MODE_WHEN_ACTIVE;
    else
      ss->show_background = GST_EBUTTD_BACKGROUND_MODE_ALWAYS;
    /*GST_CAT_DEBUG (ebuttdparse, "showBackground: %d", ss->show_background);*/
  }

  if (ess->overflow) {
    if (g_strcmp0 (ess->overflow, "visible") == 0)
      ss->overflow = GST_EBUTTD_OVERFLOW_MODE_VISIBLE;
    else
      ss->overflow = GST_EBUTTD_OVERFLOW_MODE_HIDDEN;
    /*GST_CAT_DEBUG (ebuttdparse, "overflow: %d", ss->overflow);*/
  }
}


static GstEbuttdStyleSet *
copy_style_set (GstEbuttdStyleSet * style)
{
  GstEbuttdStyleSet *ret;

  g_return_val_if_fail (style != NULL, NULL);
  ret = g_new0 (GstEbuttdStyleSet, 1);

  if (style->text_direction)
    ret->text_direction = g_strdup (style->text_direction);
  if (style->font_family)
    ret->font_family = g_strdup (style->font_family);
  if (style->font_size)
    ret->font_size = g_strdup (style->font_size);
  if (style->line_height)
    ret->line_height = g_strdup (style->line_height);
  if (style->text_align)
    ret->text_align = g_strdup (style->text_align);
  if (style->color)
    ret->color = g_strdup (style->color);
  if (style->bg_color)
    ret->bg_color = g_strdup (style->bg_color);
  if (style->font_style)
    ret->font_style = g_strdup (style->font_style);
  if (style->font_weight)
    ret->font_weight = g_strdup (style->font_weight);
  if (style->text_decoration)
    ret->text_decoration = g_strdup (style->text_decoration);
  if (style->unicode_bidi)
    ret->unicode_bidi = g_strdup (style->unicode_bidi);
  if (style->wrap_option)
    ret->wrap_option = g_strdup (style->wrap_option);
  if (style->multi_row_align)
    ret->multi_row_align = g_strdup (style->multi_row_align);
  if (style->line_padding)
    ret->line_padding = g_strdup (style->line_padding);
  if (style->origin)
    ret->origin = g_strdup (style->origin);
  if (style->extent)
    ret->extent = g_strdup (style->extent);
  if (style->display_align)
    ret->display_align = g_strdup (style->display_align);
  if (style->overflow)
    ret->overflow = g_strdup (style->overflow);
  if (style->padding)
    ret->padding = g_strdup (style->padding);
  if (style->writing_mode)
    ret->writing_mode = g_strdup (style->writing_mode);
  if (style->show_background)
    ret->show_background = g_strdup (style->show_background);

  return ret;
}


/* s2 overrides s1. Unlike style inheritance, merging will result in all values from s1 being merged into s2. */
static GstEbuttdStyleSet *
merge_style_sets (GstEbuttdStyleSet * set1, GstEbuttdStyleSet * set2)
{
  GstEbuttdStyleSet *ret = NULL;

  if (set1) {
    ret = copy_style_set (set1);

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
      if (set2->bg_color)
        ret->bg_color = g_strdup (set2->bg_color);
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
    ret = copy_style_set (set2);
  }

  return ret;
}


static GstEbuttdStyleSet *
inherit_styling (GstEbuttdStyleSet * parent, GstEbuttdStyleSet * child)
{
  GstEbuttdStyleSet *ret = NULL;

  /*
   * The following styling attributes are not inherited:
   *   - backgroundColor
   *   - origin
   *   - extent
   *   - displayAlign
   *   - overflow
   *   - padding
   *   - writingMode
   *   - showBackground
   *   - unicodeBidi
   */

  if (parent) {
    ret = copy_style_set (parent);

    if (child) {
      if (child->text_direction)
        ret->text_direction = g_strdup (child->text_direction);
      if (child->font_family)
        ret->font_family = g_strdup (child->font_family);
      if (child->font_size)
        ret->font_size = g_strdup (child->font_size);
      if (child->line_height)
        ret->line_height = g_strdup (child->line_height);
      if (child->text_align)
        ret->text_align = g_strdup (child->text_align);
      if (child->color)
        ret->color = g_strdup (child->color);
      if (child->font_style)
        ret->font_style = g_strdup (child->font_style);
      if (child->font_weight)
        ret->font_weight = g_strdup (child->font_weight);
      if (child->text_decoration)
        ret->text_decoration = g_strdup (child->text_decoration);
      if (child->wrap_option)
        ret->wrap_option = g_strdup (child->wrap_option);
      if (child->multi_row_align)
        ret->multi_row_align = g_strdup (child->multi_row_align);
      if (child->line_padding)
        ret->line_padding = g_strdup (child->line_padding);
    }
  } else if (child) {
    ret = copy_style_set (child);
  }

  return ret;
}


static void
merge_region_styles (gpointer key, gpointer value, gpointer user_data)
{
  GstEbuttdStyleSet *tmp = NULL;
  gchar *id = (gchar *)key;
  GstEbuttdElement *region = (GstEbuttdElement *)value;
  GstEbuttdElement *style = NULL;
  GHashTable *style_hash = (GHashTable *)user_data;
  gint i;

  GST_CAT_DEBUG (ebuttdparse, "Resolving styles for region %s", id);
  for (i = 0; i < g_strv_length (region->styles); ++i) {
    tmp = region->style_set;
    GST_CAT_DEBUG (ebuttdparse, "Merging style %s...", region->styles[i]);
    style = g_hash_table_lookup (style_hash, region->styles[i]);
    g_assert (style != NULL);
    region->style_set = merge_style_sets (region->style_set, style->style_set);
    g_free (tmp);
  }

  GST_CAT_DEBUG (ebuttdparse, "Final style set:");
  _print_style_set (region->style_set);
}


static void
resolve_region_styles (GHashTable * region_hash, GHashTable * style_hash)
{
  g_return_if_fail (region_hash != NULL);
  g_return_if_fail (style_hash != NULL);
  g_hash_table_foreach (region_hash, merge_region_styles, style_hash);
}


gboolean
resolve_element_style (GNode * node, gpointer data)
{
  /* Combine styles with resolved style of parent; styles listed later override
   * those earlier in the list. */
  GstEbuttdStyleSet *tmp = NULL;
  GstEbuttdElement *element, *parent, *style;
  GHashTable *style_hash;
  gint i;

  g_return_val_if_fail (node != NULL, FALSE);
  g_return_val_if_fail (data != NULL, FALSE);
  style_hash = (GHashTable *)data;
  element = node->data;

  switch (element->type) {
    case GST_EBUTTD_ELEMENT_TYPE_STYLE:
      GST_CAT_DEBUG (ebuttdparse, "Element type: <style>");
      break;
    case GST_EBUTTD_ELEMENT_TYPE_REGION:
      GST_CAT_DEBUG (ebuttdparse, "Element type: <region>");
      break;
    case GST_EBUTTD_ELEMENT_TYPE_BODY:
      GST_CAT_DEBUG (ebuttdparse, "Element type: <body>");
      break;
    case GST_EBUTTD_ELEMENT_TYPE_DIV:
      GST_CAT_DEBUG (ebuttdparse, "Element type: <div>");
      break;
    case GST_EBUTTD_ELEMENT_TYPE_P:
      GST_CAT_DEBUG (ebuttdparse, "Element type: <p>");
      break;
    case GST_EBUTTD_ELEMENT_TYPE_SPAN:
      GST_CAT_DEBUG (ebuttdparse, "Element type: <span>");
      break;
    case GST_EBUTTD_ELEMENT_TYPE_ANON_SPAN:
      GST_CAT_DEBUG (ebuttdparse, "Element type: <anon-span>");
      break;
    case GST_EBUTTD_ELEMENT_TYPE_BR:
      GST_CAT_DEBUG (ebuttdparse, "Element type: <br>");
      break;
  }

#if 0
  g_print ("resolve_element_style: ");
  if (node->parent) {
    parent = node->parent->data;
    g_print ("[parent]:%s ", parent->style_set->id);
  } else {
    g_print ("[parent]:NULL ");
  }
  if (element->styles) {
    for (i = 0; i < g_strv_length (element->styles); ++i) {
      g_print ("[%u]:%s ", i, element->styles[i]);
    }
  }
  GST_CAT_DEBUG (ebuttdparse, "");
#endif

  /* Merge referenced styles. */
  if (element->styles) {
    for (i = 0; i < g_strv_length (element->styles); ++i) {
      tmp = element->style_set;
      GST_CAT_DEBUG (ebuttdparse, "Merging style %s...", element->styles[i]);
      style = g_hash_table_lookup (style_hash, element->styles[i]);
      g_assert (style != NULL);
      element->style_set = merge_style_sets (element->style_set,
          style->style_set);
      g_free (tmp);
    }
  }

  /* Inherit styling attributes from parent. */
  if (node->parent) {
    parent = node->parent->data;
    if (parent->style_set) {
      tmp = element->style_set;
      element->style_set = inherit_styling (parent->style_set,
          element->style_set);
      g_free (tmp);
    }
  }

  if (element->style_set) {
    GST_CAT_LOG (ebuttdparse, "Resolved style:");
    _print_style_set (element->style_set);
  }

  return FALSE;
}


#if 0
static gboolean
create_style (GNode * node, gpointer data)
{
  GstEbuttdElement *element;
  element = node->data;
  element->style = create_new_style (element->style_set);
  GST_CAT_LOG (ebuttdparse, "created style for leaf node:");
  _print_style (element->style);
  return FALSE;
}
#endif


static void
resolve_body_styles (GNode * tree, GHashTable * style_hash)
{
  g_node_traverse (tree, G_PRE_ORDER, G_TRAVERSE_ALL, -1, resolve_element_style,
      style_hash);
  /*g_node_traverse (tree, G_PRE_ORDER, G_TRAVERSE_LEAVES, -1, create_style,
      NULL);*/
}


static gboolean
remove_if_break (GNode * node, gpointer data)
{
  GstEbuttdElement *element;
  element = node->data;
  if (element->type == GST_EBUTTD_ELEMENT_TYPE_BR) {
    GST_CAT_LOG (ebuttdparse, "Stripping <br>...");
    g_node_unlink (node);
  }
  return FALSE;
}


static void
strip_breaks (GNode * tree)
{
  g_node_traverse (tree, G_PRE_ORDER, G_TRAVERSE_ALL, -1, remove_if_break,
      NULL);
}


static gboolean
resolve_element_timings (GNode * node, gpointer data)
{
  GstEbuttdElement *element, *leaf;

  g_return_val_if_fail (node != NULL, FALSE);
  leaf = element = node->data;
  /*GST_CAT_DEBUG (ebuttdparse, "leaf_type: %u   leaf->begin: %llu   leaf->end: %llu",
      element->type, element->begin, element->end);*/

  if (GST_CLOCK_TIME_IS_VALID (leaf->begin)
      && GST_CLOCK_TIME_IS_VALID (leaf->end)) {
    GST_CAT_DEBUG (ebuttdparse, "Leaf node already has timing.");
    return FALSE;
  }

  while (node->parent && !GST_CLOCK_TIME_IS_VALID (element->begin)) {
    node = node->parent;
    element = node->data;
    /*GST_CAT_DEBUG (ebuttdparse, "type: %u   element->begin: %llu   element->end: %llu",
        element->type, element->begin, element->end);*/
  }

  if (!GST_CLOCK_TIME_IS_VALID (element->begin)) {
    GST_CAT_WARNING (ebuttdparse,
        "No timing found for element. Removing from tree...");
    g_node_unlink (node);
  } else {
    leaf->begin = element->begin;
    leaf->end = element->end;
    GST_CAT_LOG (ebuttdparse, "Leaf begin: %llu", leaf->begin);
    GST_CAT_LOG (ebuttdparse, "Leaf end: %llu", leaf->end);
  }

  return FALSE;
}


static void
resolve_timings (GNode * tree)
{
  g_node_traverse (tree, G_PRE_ORDER, G_TRAVERSE_LEAVES, -1,
      resolve_element_timings, NULL);
}


static gboolean
resolve_leaf_region (GNode * node, gpointer data)
{
  GstEbuttdElement *element, *leaf;

  g_return_val_if_fail (node != NULL, FALSE);
  leaf = element = node->data;

  while (node->parent && !element->region) {
    node = node->parent;
    element = node->data;
  }

  if (!element->region) {
    GST_CAT_WARNING (ebuttdparse,
        "No region found above leaf element. Removing from tree...");
    g_node_unlink (node);
  } else {
    leaf->region = g_strdup (element->region);
    GST_CAT_LOG (ebuttdparse, "Leaf region: %s", leaf->region);
  }

  return FALSE;
}


static void
resolve_regions (GNode * tree)
{
  g_node_traverse (tree, G_PRE_ORDER, G_TRAVERSE_LEAVES, -1,
      resolve_leaf_region, NULL);
}


static gboolean
inherit_region_style (GNode * node, gpointer data)
{
  GstEbuttdElement *element, *region;
  GHashTable *region_hash;
  GstEbuttdStyleSet *tmp = NULL;

  g_return_val_if_fail (node != NULL, FALSE);
  g_return_val_if_fail (data != NULL, FALSE);
  element = node->data;
  region_hash = (GHashTable *)data;

  g_assert (element->region != NULL);
  region = g_hash_table_lookup (region_hash, element->region);
  g_assert (region != NULL);
  g_assert (region->style_set != NULL);
  GST_CAT_DEBUG (ebuttdparse, "Inheriting styling from region %s...",
      element->region);
  tmp = element->style_set;
  element->style_set = inherit_styling (region->style_set, element->style_set);
  g_free (tmp);

  GST_CAT_LOG (ebuttdparse, "Style is now as follows:");
  _print_style_set (element->style_set);

  return FALSE;
}


static void
inherit_region_styles (GNode * tree, GHashTable * region_hash)
{
  g_node_traverse (tree, G_PRE_ORDER, G_TRAVERSE_LEAVES, -1,
      inherit_region_style, region_hash);
}


typedef struct {
  GstClockTime start_time;
  GstClockTime next_transition_time;
} TrState;


static gboolean
update_transition_time (GNode * node, gpointer data)
{
  GstEbuttdElement *element;
  TrState *state;

  g_return_val_if_fail (node != NULL, FALSE);
  element = node->data;
  state = (TrState *)data;

  /*GST_CAT_DEBUG (ebuttdparse, "begin: %llu  end: %llu  start_time: %llu",
      element->begin, element->end, state->start_time);*/

  if ((element->begin > state->start_time)
      && (element->begin < state->next_transition_time)) {
    state->next_transition_time = element->begin;
    /*GST_CAT_DEBUG (ebuttdparse,
        "Updating next transition time to element begin time (%llu)",
        state->next_transition_time);*/
  } else if ((element->end > state->start_time)
      && (element->end < state->next_transition_time)) {
    state->next_transition_time = element->end;
    /*GST_CAT_DEBUG (ebuttdparse,
        "Updating next transition time to element end time (%llu)",
        state->next_transition_time);*/
  }

  return FALSE;
}


static gboolean
find_transitioning_element (GNode * node, gpointer data)
{
  GstEbuttdElement *element;
  GstEbuttdTransition *transition;

  g_return_val_if_fail (node != NULL, FALSE);
  g_return_val_if_fail (data != NULL, FALSE);
  element = node->data;
  transition = (GstEbuttdTransition *)data;

  if (element->begin == transition->time) {
    transition->appearing_elements =
      g_list_append (transition->appearing_elements, element);
    /*GST_CAT_DEBUG (ebuttdparse, "Found element appearing at time %llu",
        transition->time);*/
  } else if (element->end == transition->time) {
    transition->disappearing_elements =
      g_list_append (transition->disappearing_elements, element);
    /*GST_CAT_DEBUG (ebuttdparse, "Found element disappearing at time %llu",
        transition->time);*/
  }

  return FALSE;
}

/* Return details about the next transition after @time. */
static GstEbuttdTransition *
find_next_transition (GNode * tree, GstClockTime time)
{
  GstEbuttdTransition * transition;
  TrState state;

  g_return_val_if_fail (tree != NULL, NULL);
  state.start_time = GST_CLOCK_TIME_IS_VALID (time) ? time : 0;
  state.next_transition_time = GST_CLOCK_TIME_NONE;
  g_node_traverse (tree, G_PRE_ORDER, G_TRAVERSE_LEAVES, -1,
      update_transition_time, &state);

  if (state.next_transition_time == GST_CLOCK_TIME_NONE)
    return NULL;

  transition = g_new0 (GstEbuttdTransition, 1);
  transition->time = state.next_transition_time;
  /*GST_CAT_DEBUG (ebuttdparse, "Next transition is at %llu",
      state.next_transition_time);*/

  /* Find which elements start/end at the transition time. */
  g_node_traverse (tree, G_PRE_ORDER, G_TRAVERSE_LEAVES, -1,
      find_transitioning_element, transition);

  return transition;
}


static GList *
update_active_element_list (GList * active_elements,
    GstEbuttdTransition * transition)
{
  GList *disappearing_element;
  GList *appearing_element;

  g_return_val_if_fail (transition != NULL, NULL);

  disappearing_element = g_list_first (transition->disappearing_elements);
  appearing_element = g_list_first (transition->appearing_elements);

  /* If elements in transition->disappearing_elements are in active_elements,
   * remove them. */
  while (disappearing_element) {
    active_elements =
      g_list_remove (active_elements, disappearing_element->data);
    disappearing_element = disappearing_element->next;
  }

  /* If elements in transition->appearing_elements are not in active_elements,
   * add them. */
  while (appearing_element) {
    active_elements =
      g_list_append (active_elements, appearing_element->data);
    appearing_element = appearing_element->next;
  }

  return active_elements;
}


static GList *
create_scenes (GNode * tree)
{
  GstEbuttdScene *cur_scene = NULL;
  GList *output_scenes = NULL;
  GList *active_elements = NULL;
  GstClockTime timestamp = GST_CLOCK_TIME_NONE;
  GstEbuttdTransition *transition;

  g_return_val_if_fail (tree != NULL, NULL);

  while ((transition = find_next_transition (tree, timestamp))) {
    GST_CAT_DEBUG (ebuttdparse, "Next transition found at time %llu",
        transition->time);
    if (cur_scene) cur_scene->end = transition->time;

    active_elements = update_active_element_list (active_elements, transition);
    GST_CAT_DEBUG (ebuttdparse, "There will be %u active elements after transition", g_list_length (active_elements));

    if (active_elements) {
      GstEbuttdScene * new_scene = g_new0 (GstEbuttdScene, 1);
      new_scene->begin = transition->time;
      new_scene->elements = g_list_copy (active_elements);
      output_scenes = g_list_append (output_scenes, new_scene);
      cur_scene = new_scene;
    }
    timestamp = transition->time;
  }

  return output_scenes;
}


static gboolean
strip_whitespace (GNode * node, gpointer data)
{
  GstEbuttdElement *element;
  element = node->data;
  if (element->text) g_strstrip (element->text);
  return FALSE;
}


static void
strip_surrounding_whitespace (GNode * tree)
{
  g_return_if_fail (tree != NULL);
  g_node_traverse (tree, G_PRE_ORDER, G_TRAVERSE_LEAVES, -1, strip_whitespace,
      NULL);
}


void
xml_process_head (xmlNodePtr head_cur, GHashTable * style_hash,
    GHashTable * region_hash)
{
  xmlNodePtr head_child, node_ptr; /* pointers to different levels */

  head_child = head_cur->children;
  while (head_child != NULL) {
    if (xmlStrcmp (head_child->name, (const xmlChar *) "styling") == 0) {
      GST_CAT_DEBUG (ebuttdparse, "parsing styling element...");
      node_ptr = head_child->children;
      while (node_ptr != NULL) {
        if (xmlStrcmp (node_ptr->name, (const xmlChar *) "style") == 0) {
          /* use style id as key, create style properties object for value */
          GstEbuttdElement *element;
          element = parse_element (node_ptr);

          if (element) {
            g_assert (element->id != NULL);
            /* XXX: should check that style ID is unique. */
            g_hash_table_insert (style_hash,
                (gpointer) (element->id), (gpointer) element);
            GST_CAT_DEBUG (ebuttdparse, "added style %s to style_hash",
                element->id);
            _print_style_set (element->style_set);
          }
        }
        node_ptr = node_ptr->next;
      }
    }
#if 1
    if (xmlStrcmp (head_child->name, (const xmlChar *) "layout") == 0) {
      GST_CAT_DEBUG (ebuttdparse, "parsing layout element...");
      node_ptr = head_child->children;
      while (node_ptr != NULL) {
        if (xmlStrcmp (node_ptr->name, (const xmlChar *) "region") == 0) {
          /* use region id as key, create style properties object for value */
          GstEbuttdElement *element;
          element = parse_element (node_ptr);

          if (element) {
            g_assert (element->id != NULL);
              /* XXX: should check that region ID is unique. */
            g_hash_table_insert (region_hash,
                (gpointer) (element->id), (gpointer) element);
            GST_CAT_DEBUG (ebuttdparse, "added region %s to region_hash",
                element->id);
            _print_style_set (element->style_set);
          }
        }
        node_ptr = node_ptr->next;
      }
    }
    /* XXX: Add code to parse metadata. */
#endif
    head_child = head_child->next;
  }
}


static GHashTable *
split_scenes_by_region (GList * active_elements)
{
  GHashTable *ret = NULL;

  g_return_val_if_fail (active_elements != NULL, NULL);

  ret = g_hash_table_new (g_str_hash, g_str_equal);

  while (active_elements) {
    GstEbuttdElement *element = active_elements->data;
    _print_element (element);
    GList *list = g_hash_table_lookup (ret, element->region);
    list = g_list_append (list, element);
    GST_CAT_DEBUG (ebuttdparse, "Inserting list under the following key: %s",
        element->region);
    g_hash_table_insert (ret, element->region, list);
    active_elements = active_elements->next;
  }

  GST_CAT_DEBUG (ebuttdparse, "Active elements have been split into %u regions",
      g_hash_table_size (ret));

    return ret;
}


static GNode *
create_isd_tree (GNode * tree, GList * active_elements)
{
  GList *leaves;
  GstEbuttdElement *leaf, *e;
  GQueue *node_stack;
  GNode *element_node, *foo, *junction, *sibling;
  GNode *ret = NULL;

  g_return_val_if_fail (tree != NULL, NULL);
  g_return_val_if_fail (active_elements != NULL, NULL);

  node_stack = g_queue_new ();

  g_print ("\n");
  GST_CAT_DEBUG (ebuttdparse, "There are %u active elements",
      g_list_length (active_elements));

  for (leaves = g_list_first (active_elements); leaves != NULL;
      leaves = leaves->next) {
    GNode * new_leaf;
    leaf = leaves->data;
    /* XXX: Revert to storing nodes in active_elements to avoid find
     * operation? */
    element_node = g_node_find (tree, G_PRE_ORDER, G_TRAVERSE_LEAVES, leaf);
    g_assert (element_node != NULL);
    GST_CAT_DEBUG (ebuttdparse, "Finding ancestors for following element:");
    _print_element (element_node->data);

    for (foo = element_node->parent; foo; foo = foo->parent) {
      if (ret && (junction = g_node_find (ret, G_PRE_ORDER,
              G_TRAVERSE_ALL, foo->data))) {
          GST_CAT_DEBUG (ebuttdparse, "Element already exists in output tree:");
          _print_element (foo->data);
          break;
      } else {
        g_queue_push_head (node_stack, foo->data);
        GST_CAT_DEBUG (ebuttdparse, "Added following element to stack:");
        _print_element (foo->data);
        GST_CAT_DEBUG (ebuttdparse, "Stack depth is now %u",
            g_queue_get_length (node_stack));
      }
    }

    while ((e = g_queue_pop_head (node_stack))) {
      GNode *n = g_node_new (e);
      if (junction) {
        junction = g_node_append (junction, n);
        GST_CAT_DEBUG (ebuttdparse, "Appended following element to ret:");
        _print_element (junction->data);
      } else {
        GST_CAT_DEBUG (ebuttdparse,
            "Setting the following element at the head of ret:");
        _print_element (n->data);
        ret = junction = n;
      }
    }

    /* Append active element to tip of branch. */
    new_leaf = g_node_new (element_node->data);
    g_node_append (junction, new_leaf);

    /* XXX: Possible optimisation for sibling elements: */
#if 0
    /* Check if any of this element's siblings are active; if so, remove them
     * from list - we already have their ancestors in the output list. */
    /* Remove the element and any siblings from the list of active elements. */
    for (sibling = element_node->parent->children; sibling; sibling = sibling->next) {
      if (g_list_find (active_elements, sibling->data)) {
        g_list_remove (active_elements, sibling->data);
        GST_CAT_DEBUG (ebuttdparse,"Removed sibling element from active_elements; there are now %u active elements.",
            g_list_length (active_elements));
      }
    }
#endif
  }
  return ret;
}


static GstSubtitleArea *
create_subtitle_area (GNode * tree)
{
  GstSubtitleArea *area;
  GstSubtitleStyleSet *region_style;
  GstSubtitleColor body_colour;
  GstEbuttdElement *element;
  GNode *node;

  g_return_val_if_fail (tree != NULL, NULL);
  element = tree->data;
  g_assert (element->type == GST_EBUTTD_ELEMENT_TYPE_REGION);

  /* Create SubtitleArea from region. */
  region_style = gst_subtitle_style_set_new ();
  update_style_set (region_style, element->style_set);
  area = gst_subtitle_area_new (region_style);
  g_assert (area != NULL);

  node = tree->children;
  g_assert (node->next == NULL);
  element = node->data;
  g_assert (element->type == GST_EBUTTD_ELEMENT_TYPE_BODY);
  /*body_colour = parse_ebuttd_colorstring (element->style_set->bg_color);*/

  node = node->children;
  while (node) {
    GstSubtitleColor div_color;
    GNode *p_node;

    element = node->data;
    g_assert (element->type == GST_EBUTTD_ELEMENT_TYPE_DIV);
    /*div_color = parse_ebuttd_colorstring (element->style_set->bg_color);*/

    p_node = node->children;
    while (p_node) {
      GstSubtitleBlock *block;
      GstSubtitleStyleSet *block_style;
      GNode *span_node;

      element = p_node->data;
      g_assert (element->type == GST_EBUTTD_ELEMENT_TYPE_P);
      block_style = gst_subtitle_style_set_new ();
      update_style_set (block_style, element->style_set);

      /* XXX: blend bg colors from body, div and p here. */

      block = gst_subtitle_block_new (block_style);
      g_assert (block != NULL);

      span_node = p_node->children;
      while (span_node) {
        GstSubtitleElement *e;
        GstSubtitleStyleSet *element_style;

        element = span_node->data;
        g_assert (element->type == GST_EBUTTD_ELEMENT_TYPE_SPAN
            || element->type == GST_EBUTTD_ELEMENT_TYPE_ANON_SPAN);

        if (element->type == GST_EBUTTD_ELEMENT_TYPE_SPAN) {
          element = span_node->children->data;
          g_assert (element->type == GST_EBUTTD_ELEMENT_TYPE_ANON_SPAN);
        }

        element_style = gst_subtitle_style_set_new ();
        update_style_set (element_style, element->style_set);
        e = gst_subtitle_element_new (element_style, element->text_index);

        gst_subtitle_block_add_element (block, e);
        GST_CAT_DEBUG (ebuttdparse, "Added element to block; there are now %u elements in the block.", gst_subtitle_block_get_element_count (block));
        span_node = span_node->next;
      }

      gst_subtitle_area_add_block (area, block);
      GST_CAT_DEBUG (ebuttdparse, "Added block to area; there are now %u blocks in the area.", gst_subtitle_area_get_block_count (area));
      p_node = p_node->next;
    }
    node = node->next;
  }

  return area;
}


static GNode *
create_isds (GNode * tree, GList * scenes, GHashTable * region_hash)
{
  g_return_if_fail (tree != NULL);
  g_return_if_fail (scenes != NULL);
  g_return_if_fail (region_hash != NULL);

  GST_CAT_DEBUG (ebuttdparse, "About to create ISDs...");

  while (scenes) {
    GstEbuttdScene * scene = scenes->data;
    GHashTable *elements_by_region;
    GHashTableIter iter;
    gpointer key, value;
    GList *l;
    GPtrArray *areas = g_ptr_array_new ();

    g_assert (scene != NULL);
    GST_CAT_DEBUG (ebuttdparse, "\n\n==== Handling scene ====");

    /* Split the active nodes by region. XXX: Remember to free hash table after
     * use. */
    elements_by_region = split_scenes_by_region (scene->elements);

    GST_CAT_DEBUG (ebuttdparse, "Hash table has %u entries.",
        g_hash_table_size (elements_by_region));

    g_hash_table_iter_init (&iter, elements_by_region);
    while (g_hash_table_iter_next (&iter, &key, &value)) {
      GstEbuttdElement *region;
      GNode *region_node;
      GNode *isd_tree;
      gchar *region_name = (gchar *)key;
      GList *region_elements = (GList *)value;
      GstSubtitleArea *area;

      isd_tree = create_isd_tree (tree, region_elements);
      GST_CAT_DEBUG (ebuttdparse, "Returned tree has %u nodes",
          g_node_n_nodes (isd_tree, G_TRAVERSE_ALL));

      /* Retrieve region element and wrap in a node. */
      GST_CAT_DEBUG (ebuttdparse, "About to retrieve %s from hash table %p",
          region_name, region_hash);
      region = g_hash_table_lookup (region_hash, region_name);
      g_assert (region != NULL);
      region_node = g_node_new (region);

      /* Reparent tree to region node. */
      g_node_prepend (region_node, isd_tree);

      area = create_subtitle_area (region_node);
      g_ptr_array_add (areas, area);
      g_node_destroy (isd_tree);
    }
    GST_CAT_DEBUG (ebuttdparse, "Finished handling scene...");
    /*GST_CAT_DEBUG (ebuttdparse, "Have created %u areas", g_list_length (areas));*/

    gst_buffer_add_subtitle_meta (scene->buf, areas);
    scenes = g_list_next (scenes);
  }
  GST_CAT_DEBUG (ebuttdparse, "Finished handling all scenes.");
  return NULL;
}


static void
fill_buffers (GList * scenes)
{
  GstEbuttdScene *scene;
  GList *elements;
  GstEbuttdElement *element;

  while (scenes) {
    guint text_index = 0U;
    scene = scenes->data;
    elements = scene->elements;
    scene->buf = gst_buffer_new ();
    GST_BUFFER_PTS (scene->buf) = scene->begin;
    GST_BUFFER_DURATION (scene->buf) = (scene->end - scene->begin);

    while (elements) {
      GstMemory *mem;
      element = elements->data;

      if (element->text) {
        mem = gst_allocator_alloc (NULL, strlen (element->text) + 1, NULL);
        gst_buffer_insert_memory (scene->buf, -1, mem);
        GST_CAT_DEBUG (ebuttdparse, "Inserted text at memory position %u in GstBuffer; GstBuffer now contains %u GstMemorys.", text_index, gst_buffer_n_memory (scene->buf));
        element->text_index = text_index++;
      }
      elements = elements->next;
    }
    scenes = scenes->next;
  }
}


GList * create_buffer_list (GList * scenes)
{
  GList *ret = NULL;

  while (scenes) {
    GstEbuttdScene *scene = scenes->data;
    ret = g_list_prepend (ret, scene->buf);
    scenes = scenes->next;
  }
  return g_list_reverse (ret);
}


GList *
ebutt_xml_parse (const gchar * xml_file_buffer)
{
  gint element_tree;            /* 0 if successfully created */
  xmlDocPtr doc;                /* pointer for tree */
  xmlNodePtr cur;

  InheritanceList *inherited_styles, *inherited_regions;
  GList *subtitle_list = NULL;

  inherited_styles = NULL;
  inherited_regions = NULL;

  GHashTable *style_hash = g_hash_table_new_full (g_str_hash, g_str_equal,
      NULL, (GDestroyNotify) delete_element);
  GHashTable *region_hash = g_hash_table_new_full (g_str_hash, g_str_equal,
      NULL, (GDestroyNotify) delete_element);
  DocMetadata *document_metadata = NULL;
  GNode *body = NULL;
  GList *scenes = NULL;
  GList *buffer_list = NULL;

  GST_DEBUG_CATEGORY_INIT (ebuttdparse, "ebuttdparser", 0,
      "EBU-TT-D debug category");
  GST_CAT_DEBUG (ebuttdparse, "Input file:\n%s", xml_file_buffer);

  /* create element tree */
  element_tree = create_element_tree (xml_file_buffer, &doc, &cur);

  if (element_tree != 0) {
    GST_CAT_ERROR (ebuttd_parse_debug,
        "Failed to parse document, returning NULL Glist");
    return subtitle_list;       /* failed to parse returning NULL list */
  }

  /* handle <tt tag namespace elements */
  if (xmlStrcmp (cur->name, (const xmlChar *) "tt") == 0) {
    /**
     * Go cur->ns->href for the url of xmlns
     * and cur->ns->next->href etc for the xmlns properties
     *
     * For the other properties, go:
     * cur->properties->children->content for xml:lang
     * and use next to get to cellResolution etc.
     *
     */
    document_metadata = extract_tt_tag_properties (cur, document_metadata);
  }

  /* handle <tt tag namespace elements */
  if (xmlStrcmp (cur->name, (const xmlChar *) "tt") == 0) {
    /**
     * Go cur->ns->href for the url of xmlns
     * and cur->ns->next->href etc for the xmlns properties
     *
     * For the other properties, go:
     * cur->properties->children->content for xml:lang
     * and use next to get to cellResolution etc.
     *
     */
    document_metadata = extract_tt_tag_properties (cur, document_metadata);
  }


  cur = cur->children;
  while (cur != NULL) {
    /* Process head of xml doc */
    if (xmlStrcmp (cur->name, (const xmlChar *) "head") == 0) {
      xml_process_head (cur, style_hash, region_hash);
    }
    /* Process Body of xml doc */
    else if (xmlStrcmp (cur->name, (const xmlChar *) "body") == 0) {
      body = parse_tree (cur);
      GST_CAT_DEBUG (ebuttdparse, "Body tree contains %u nodes.",
          g_node_n_nodes (body, G_TRAVERSE_ALL));
      GST_CAT_DEBUG (ebuttdparse, "Body tree height is %u",
          g_node_max_height (body));

      /* What to do next?
       * - Check that structure is valid according to EBU-TT-D spec.
       * - Resolve styles for leaf nodes.
       * - Generate ISDs by looking at leaf nodes.
       */

      resolve_region_styles (region_hash, style_hash);
      resolve_body_styles (body, style_hash);
      strip_breaks (body);
      GST_CAT_DEBUG (ebuttdparse, "Body tree now contains %u nodes.",
          g_node_n_nodes (body, G_TRAVERSE_ALL));
      strip_surrounding_whitespace (body);
      resolve_timings (body);
      resolve_regions (body);
      inherit_region_styles (body, region_hash);
      scenes = create_scenes (body);
      GST_CAT_DEBUG (ebuttdparse, "There are %u scenes in all.", g_list_length (scenes));
      GST_CAT_DEBUG (ebuttdparse, "Region hash address: %p", region_hash);
      fill_buffers (scenes);
      create_isds (body, scenes, region_hash);
      buffer_list = create_buffer_list (scenes);
      GST_CAT_DEBUG (ebuttdparse, "There are %u buffers in output list.",
          g_list_length (buffer_list));

      /* XXX: Increment buffer refcount by 1 when adding to buffer_list, then free the scenes before returning. */

#if 0
      /**
       * Extract the styles and regions inherited from this level
       * TODO: extract things like tts:textAlign="center" from body tag
      */
      gint body_styles_count = 0;
      gint body_regions_count = 0;

      /* extract styles (no region in body) */
      extract_prepend_style_region (cur,
          &inherited_styles,
          &inherited_regions, &body_styles_count, &body_regions_count);

      /* fetched div is 1...* So need to go through this too */
      body_child = cur->children;
      while (body_child != NULL) {
        if (xmlStrcmp (body_child->name, (const xmlChar *) "div") == 0) {
          /**
           * all div region and styles will apply to the following <p>s
           * Todo: note down styles and regions
           * use StyleTree to keep track
           * <p> is 1...*, so could just be one <p> with spans inside.
           */

          gint div_styles_count = 0;
          gint div_regions_count = 0;
          xmlChar *region = NULL;

          /* add style and region ids to inheritance lists */
          extract_prepend_style_region (body_child,
              &inherited_styles,
              &inherited_regions, &div_styles_count, &div_regions_count);

          region = xmlGetProp (body_child, (xmlChar *)"region");

          div_child = body_child->children;     /* div children are p tags */
          while (div_child != NULL) {
            if (xmlStrcmp (div_child->name, (const xmlChar *) "p") == 0) {
              /**
               * p may or may not contain span tags
               * if p does not contain timing information then child must
               */
              xmlChar *begin = NULL, *end = NULL;
              xmlChar *r = NULL;
              guint64 begin_timestamp, end_timestamp;
              gchar *ret = NULL;
              xmlNodePtr p_tag = div_child;
              SubSubtitle *sub_subtitle;
              gint p_styles_count, p_regions_count;
              p_styles_count = 0;
              p_regions_count = 0;

              begin = xmlGetProp (p_tag, (xmlChar *)"begin");
              if (begin) {
                end = xmlGetProp (p_tag, (xmlChar *)"end");
                GST_CAT_DEBUG (ebuttd_parse_debug,
                    "In tag \"%s\" found start time %s and end time %s",
                    div_child->name, begin, end);
              }

              r = xmlGetProp (p_tag, (xmlChar *)"region");
              if (r) {
                xmlFree (region);
                region = r;
              }
              GST_CAT_DEBUG (ebuttdparse, "Paragraph to be displayed in region %s",
                  (gchar *)region);

              /* add style and region ids to inheritance lists */
              extract_prepend_style_region (p_tag,
                  &inherited_styles,
                  &inherited_regions, &p_styles_count, &p_regions_count);

              p_child = div_child->children;    /* these could be text
                                                   or spans with children text */

              /* Create an empty list of sub subtitles list */
              sub_subtitle = NULL;

              while (p_child != NULL) {
                if (xmlStrcmp (p_child->name, (const xmlChar *) "text") == 0) {

                  /* create a new element and append to end of linked list */
                  sub_subtitle = add_new_subsubtitle (sub_subtitle,
                      p_child, inherited_styles, inherited_regions);
                } else if (xmlStrcmp (p_child->name,
                        (const xmlChar *) "span") == 0) {
                  /**
                  * 0...* so could be none
                  * assume that they can't be nested
                  * 0 ... * so a copy of inheritance lists
                  */
                  InheritanceList *inherited_styles_copy,
                      *inherited_regions_copy;
                  inherited_styles_copy = g_list_copy (inherited_styles);
                  inherited_regions_copy = g_list_copy (inherited_regions);

                  /* add style and region ids to inheritance lists */
                  extract_prepend_style_region (p_child,
                      &inherited_styles_copy,
                      &inherited_regions_copy, NULL, NULL);
                  span_child = p_child->children;
                  while (span_child != NULL) {
                    if (xmlStrcmp (span_child->name,
                            (const xmlChar *) "text") == 0) {
                      sub_subtitle =
                          add_new_subsubtitle (sub_subtitle, span_child,
                          inherited_styles_copy, inherited_regions_copy);
                    } else {
                      /* check for line breaks */
                      handle_line_break (span_child, sub_subtitle);
                    }
                    span_child = span_child->next;
                  }

                  /* this will only ever be p or the span, never both */
                  if (!begin) {
                    begin = xmlGetProp (p_child, (xmlChar *)"begin");
                    end = xmlGetProp (p_child, (xmlChar *)"end");

                    GST_CAT_DEBUG (ebuttd_parse_debug,
                        "In tag \"%s\" found start time %s and end time %s",
                        span_child->name, begin, end);
                  }
                } else {
                  /* check for line breaks */
                  handle_line_break (span_child, sub_subtitle);
                }
                p_child = p_child->next;
              }
              /*
               * Finished collecting subsubtitles from p tag
               */
              if (begin && end) {
                SubStateObj *new_sub_n_state;
                ParserState *new_state;
                RegionProp *r;

                begin_timestamp = extract_timestamp (begin);
                end_timestamp = extract_timestamp (end);

                ret = sub_subtitle_concat_markup (sub_subtitle,
                    style_hash, region_hash, document_metadata);

                if (region) {
                  r = g_hash_table_lookup (region_hash, region);
                  add_region_description (&ret, r);
                }

                /* free subsub after use */
                new_state = g_new (ParserState, 1);
                new_state->start_time = begin_timestamp;
                new_state->duration = end_timestamp - begin_timestamp;
                new_state->max_duration = end_timestamp - begin_timestamp;

                new_sub_n_state = g_new (SubStateObj, 1);
                GST_CAT_DEBUG (ebuttdparse, "Adding following text to state: \n%s", ret);
                new_sub_n_state->text = ret;
                new_sub_n_state->state = new_state;

                /* more efficient to use prepend and then reverse at end */
                subtitle_list = g_list_prepend (subtitle_list, new_sub_n_state);
              }
              /* revert inheritance lists */
              inherited_styles =
                  inheritance_list_remove_first (inherited_styles,
                  p_styles_count);
              inheritance_list_remove_first (inherited_regions,
                  p_regions_count);
              p_styles_count = 0;
              p_regions_count = 0;
            }
            div_child = div_child->next;
          }
          /* revert inheritance lists */
          inherited_styles =
              inheritance_list_remove_first (inherited_styles,
              div_styles_count);
          inheritance_list_remove_first (inherited_regions, div_regions_count);
          div_styles_count = 0;
          div_regions_count = 0;
          if (region) xmlFree (region);
        }
        body_child = body_child->next;
      }
#endif
    }
    cur = cur->next;
  }

  xmlFreeDoc (doc);
  g_hash_table_destroy (style_hash);
  g_hash_table_destroy (region_hash);
  if (document_metadata) g_free (document_metadata);
  /** to free:
  * style hash table
  * free inheritance lists including span copies
  * free subsubtitles and contents
  */

  return buffer_list;
}
