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
  if (element->text) {
    GST_CAT_DEBUG (ebuttdparse, "Element text: %s", element->text);
    GST_CAT_DEBUG (ebuttdparse, "Element text index: %u", element->text_index);
  }
}


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


static GstEbuttdStyleSet *
parse_style_set (const xmlNode * node)
{
  GstEbuttdStyleSet *s = g_new0 (GstEbuttdStyleSet, 1);
  gchar *value = NULL;

  if ((!get_xml_property (node, "id"))) {
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

  if ((value = get_xml_property (node, "displayAlign"))) {
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

  if ((value = get_xml_property (node, "writingMode"))) {
    s->writing_mode = g_strdup (value);
    g_free (value);
  }

  if ((value = get_xml_property (node, "showBackground"))) {
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
delete_element (GstEbuttdElement * element)
{
  g_return_if_fail (element != NULL);
  GST_CAT_DEBUG (ebuttdparse, "Deleting element %p...", element);

  if (element->id) g_free ((gpointer) element->id);
  if (element->styles) g_strfreev (element->styles);
  if (element->region) g_free ((gpointer) element->region);
  if (element->style_set) delete_style_set (element->style_set);
  if (element->text) g_free ((gpointer) element->text);
  g_free ((gpointer) element);
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


static gint
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
    return 2;
  }

  if (xmlStrcmp (cur->name, (const xmlChar *) "tt")) {
    GST_CAT_ERROR (ebuttdparse, "document of the wrong type; root node != tt");
    xmlFreeDoc (doc);
    return 3;
  }

  *doc_ptr = doc;
  *cur_ptr = cur;
  return 0;
}


static DocMetadata *
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


static GstClockTime
parse_timecode (const gchar * timestring)
{
  gchar **strings;
  guint64 hours = 0, minutes = 0, seconds = 0, milliseconds = 0;
  GstClockTime time = GST_CLOCK_TIME_NONE;

  g_return_val_if_fail (timestring != NULL, time);
  GST_CAT_LOG (ebuttdparse, "time string: %s", timestring);

  strings = g_strsplit (timestring, ":", 3);
  if (g_strv_length (strings) != 3U) {
    GST_CAT_ERROR (ebuttdparse, "badly formatted time string: %s", timestring);
    return time;
  }

  hours = g_ascii_strtoull (strings[0], NULL, 10U);
  minutes = g_ascii_strtoull (strings[1], NULL, 10U);
  if (g_strstr_len (strings[2], -1, ".")) {
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


/* XXX: Do we need to put defaults in here, seeing that the passed-in style set should have default values, and if a value isn't recognized it seems reasonable to do nothing...? */
static void
update_style_set (GstSubtitleStyleSet * ss, GstEbuttdStyleSet * ess,
    guint cellres_x, guint cellres_y)
{
  g_return_val_if_fail (ss != NULL, NULL);
  g_return_val_if_fail (ess != NULL, NULL);

  GST_CAT_DEBUG (ebuttdparse, "cellres_x: %u  cellres_y: %u", cellres_x,
      cellres_y);

  if (ess->text_direction) {
    if (g_strcmp0 (ess->text_direction, "rtl") == 0)
      ss->text_direction = GST_SUBTITLE_TEXT_DIRECTION_RTL;
    else
      ss->text_direction = GST_SUBTITLE_TEXT_DIRECTION_LTR;
  }

  if (ess->font_family) {
    gsize length = g_strlcpy (ss->font_family, ess->font_family,
        MAX_FONT_FAMILY_NAME_LENGTH);
    if (length > MAX_FONT_FAMILY_NAME_LENGTH)
      GST_CAT_ERROR (ebuttdparse, "Font family name is too long.");
  }

  if (ess->font_size) {
    ss->font_size = g_ascii_strtod (ess->font_size, NULL) / 100.0;
    ss->font_size *= (1.0 / cellres_y);
  }

  if (ess->line_height) {
    if (g_strcmp0 (ess->line_height, "normal") == 0)
      ss->line_height = 1.25;
    else
      ss->line_height = g_ascii_strtod (ess->line_height, NULL) / 100.0;
  }

  if (ess->text_align) {
    if (g_strcmp0 (ess->text_align, "left") == 0)
      ss->text_align = GST_SUBTITLE_TEXT_ALIGN_LEFT;
    else if (g_strcmp0 (ess->text_align, "center") == 0)
      ss->text_align = GST_SUBTITLE_TEXT_ALIGN_CENTER;
    else if (g_strcmp0 (ess->text_align, "right") == 0)
      ss->text_align = GST_SUBTITLE_TEXT_ALIGN_RIGHT;
    else if (g_strcmp0 (ess->text_align, "end") == 0)
      ss->text_align = GST_SUBTITLE_TEXT_ALIGN_END;
    else
      ss->text_align = GST_SUBTITLE_TEXT_ALIGN_START;
  }

  if (ess->color) {
    ss->color = parse_ebuttd_colorstring (ess->color);
  }

  if (ess->bg_color) {
    ss->bg_color = parse_ebuttd_colorstring (ess->bg_color);
  }

  if (ess->font_style) {
    if (g_strcmp0 (ess->font_style, "italic") == 0)
      ss->font_style = GST_SUBTITLE_FONT_STYLE_ITALIC;
    else
      ss->font_style = GST_SUBTITLE_FONT_STYLE_NORMAL;
  }

  if (ess->font_weight) {
    if (g_strcmp0 (ess->font_weight, "bold") == 0)
      ss->font_weight = GST_SUBTITLE_FONT_WEIGHT_BOLD;
    else
      ss->font_weight = GST_SUBTITLE_FONT_WEIGHT_NORMAL;
  }

  if (ess->text_decoration) {
    if (g_strcmp0 (ess->text_decoration, "underline") == 0)
      ss->text_decoration = GST_SUBTITLE_TEXT_DECORATION_UNDERLINE;
    else
      ss->text_decoration = GST_SUBTITLE_TEXT_DECORATION_NONE;
  }

  if (ess->unicode_bidi) {
    if (g_strcmp0 (ess->unicode_bidi, "embed") == 0)
      ss->unicode_bidi = GST_SUBTITLE_UNICODE_BIDI_EMBED;
    else if (g_strcmp0 (ess->unicode_bidi, "bidiOverride") == 0)
      ss->unicode_bidi = GST_SUBTITLE_UNICODE_BIDI_OVERRIDE;
    else
      ss->unicode_bidi = GST_SUBTITLE_UNICODE_BIDI_NORMAL;
  }

  if (ess->wrap_option) {
    if (g_strcmp0 (ess->wrap_option, "noWrap") == 0)
      ss->wrap_option = GST_SUBTITLE_WRAPPING_OFF;
    else
      ss->wrap_option = GST_SUBTITLE_WRAPPING_ON;
  }

  if (ess->multi_row_align) {
    if (g_strcmp0 (ess->multi_row_align, "start") == 0)
      ss->multi_row_align = GST_SUBTITLE_MULTI_ROW_ALIGN_START;
    else if (g_strcmp0 (ess->multi_row_align, "center") == 0)
      ss->multi_row_align = GST_SUBTITLE_MULTI_ROW_ALIGN_CENTER;
    else if (g_strcmp0 (ess->multi_row_align, "end") == 0)
      ss->multi_row_align = GST_SUBTITLE_MULTI_ROW_ALIGN_END;
    else
      ss->multi_row_align = GST_SUBTITLE_MULTI_ROW_ALIGN_AUTO;
  }

  if (ess->line_padding) {
    ss->line_padding = g_ascii_strtod (ess->line_padding, NULL);
    ss->line_padding *= (1.0 / cellres_x);
  }

  if (ess->origin) {
    gchar *c;
    ss->origin_x = g_ascii_strtod (ess->origin, &c) / 100.0;
    while (!g_ascii_isdigit (*c) && *c != '+' && *c != '-') ++c;
    ss->origin_y = g_ascii_strtod (c, NULL) / 100.0;
  }

  if (ess->extent) {
    gchar *c;
    ss->extent_w = g_ascii_strtod (ess->extent, &c) / 100.0;
    if ((ss->origin_x + ss->extent_w) > 1.0) {
      ss->extent_w = 1.0 - ss->origin_x;
    }
    while (!g_ascii_isdigit (*c) && *c != '+' && *c != '-') ++c;
    ss->extent_h = g_ascii_strtod (c, NULL) / 100.0;
    if ((ss->origin_y + ss->extent_h) > 1.0) {
      ss->extent_h = 1.0 - ss->origin_y;
    }
  }

  if (ess->display_align) {
    if (g_strcmp0 (ess->display_align, "center") == 0)
      ss->display_align = GST_SUBTITLE_DISPLAY_ALIGN_CENTER;
    else if (g_strcmp0 (ess->display_align, "after") == 0)
      ss->display_align = GST_SUBTITLE_DISPLAY_ALIGN_AFTER;
    else
      ss->display_align = GST_SUBTITLE_DISPLAY_ALIGN_BEFORE;
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
          g_ascii_strtod (decimals[0], NULL) / 100.0;
        break;

      case 2:
        ss->padding_before = ss->padding_after =
          g_ascii_strtod (decimals[0], NULL) / 100.0;
        ss->padding_start = ss->padding_end =
          g_ascii_strtod (decimals[1], NULL) / 100.0;
        break;

      case 3:
        ss->padding_before = g_ascii_strtod (decimals[0], NULL) / 100.0;
        ss->padding_start = ss->padding_end =
          g_ascii_strtod (decimals[1], NULL) / 100.0;
        ss->padding_after = g_ascii_strtod (decimals[2], NULL) / 100.0;
        break;

      case 4:
        ss->padding_before = g_ascii_strtod (decimals[0], NULL) / 100.0;
        ss->padding_end = g_ascii_strtod (decimals[1], NULL) / 100.0;
        ss->padding_after = g_ascii_strtod (decimals[2], NULL) / 100.0;
        ss->padding_start = g_ascii_strtod (decimals[3], NULL) / 100.0;
        break;
    }
    g_strfreev (decimals);
  }

  if (ess->writing_mode) {
    if (g_str_has_prefix (ess->writing_mode, "rl"))
      ss->writing_mode = GST_SUBTITLE_WRITING_MODE_RLTB;
    else if ((g_strcmp0 (ess->writing_mode, "tbrl") == 0)
        || (g_strcmp0 (ess->writing_mode, "tb") == 0))
      ss->writing_mode = GST_SUBTITLE_WRITING_MODE_TBRL;
    else if (g_strcmp0 (ess->writing_mode, "tblr") == 0)
      ss->writing_mode = GST_SUBTITLE_WRITING_MODE_TBLR;
    else
      ss->writing_mode = GST_SUBTITLE_WRITING_MODE_LRTB;
  }

  if (ess->show_background) {
    if (g_strcmp0 (ess->show_background, "whenActive") == 0)
      ss->show_background = GST_SUBTITLE_BACKGROUND_MODE_WHEN_ACTIVE;
    else
      ss->show_background = GST_SUBTITLE_BACKGROUND_MODE_ALWAYS;
  }

  if (ess->overflow) {
    if (g_strcmp0 (ess->overflow, "visible") == 0)
      ss->overflow = GST_SUBTITLE_OVERFLOW_MODE_VISIBLE;
    else
      ss->overflow = GST_SUBTITLE_OVERFLOW_MODE_HIDDEN;
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

  if (child) {
    ret = copy_style_set (child);
  } else {
    ret = g_new0 (GstEbuttdStyleSet, 1);
  }

  if (parent) {
    if (parent->text_direction && !ret->text_direction)
      ret->text_direction = g_strdup (parent->text_direction);
    if (parent->font_family && !ret->font_family)
      ret->font_family = g_strdup (parent->font_family);
    if (parent->font_size && !ret->font_size)
      ret->font_size = g_strdup (parent->font_size);
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


static void
merge_region_styles (gpointer key, gpointer value, gpointer user_data)
{
  GstEbuttdStyleSet *tmp = NULL;
  gchar *id = (gchar *)key;
  GstEbuttdElement *region = (GstEbuttdElement *)value;
  GstEbuttdElement *style = NULL;
  GHashTable *style_hash = (GHashTable *)user_data;
  gint i;

  if (!region->styles)
    return;

  GST_CAT_DEBUG (ebuttdparse, "Resolving styles for region %s", id);
  for (i = 0; i < g_strv_length (region->styles); ++i) {
    tmp = region->style_set;
    GST_CAT_DEBUG (ebuttdparse, "Merging style %s...", region->styles[i]);
    style = g_hash_table_lookup (style_hash, region->styles[i]);
    g_assert (style != NULL);
    region->style_set = merge_style_sets (region->style_set, style->style_set);
    g_free (tmp);
  }

  GST_CAT_LOG (ebuttdparse, "Final style set:");
  _print_style_set (region->style_set);
}


static void
resolve_region_styles (GHashTable * region_hash, GHashTable * style_hash)
{
  g_return_if_fail (region_hash != NULL);
  g_return_if_fail (style_hash != NULL);
  g_hash_table_foreach (region_hash, merge_region_styles, style_hash);
}


static gchar *
get_element_type_string (GstEbuttdElement * element)
{
  switch (element->type) {
    case GST_EBUTTD_ELEMENT_TYPE_STYLE:
      return g_strdup ("<style>");
      break;
    case GST_EBUTTD_ELEMENT_TYPE_REGION:
      return g_strdup ("<region>");
      break;
    case GST_EBUTTD_ELEMENT_TYPE_BODY:
      return g_strdup ("<body>");
      break;
    case GST_EBUTTD_ELEMENT_TYPE_DIV:
      return g_strdup ("<div>");
      break;
    case GST_EBUTTD_ELEMENT_TYPE_P:
      return g_strdup ("<p>");
      break;
    case GST_EBUTTD_ELEMENT_TYPE_SPAN:
      return g_strdup ("<span>");
      break;
    case GST_EBUTTD_ELEMENT_TYPE_ANON_SPAN:
      return g_strdup ("<anon-span>");
      break;
    case GST_EBUTTD_ELEMENT_TYPE_BR:
      return g_strdup ("<br>");
      break;
    default:
      return g_strdup ("Unknown");
      break;
  }
}


gboolean
resolve_element_style (GNode * node, gpointer data)
{
  GstEbuttdStyleSet *tmp = NULL;
  GstEbuttdElement *element, *parent, *style;
  GHashTable *style_hash;
  gchar *type_string;
  gint i;

  g_return_val_if_fail (node != NULL, FALSE);
  g_return_val_if_fail (data != NULL, FALSE);
  style_hash = (GHashTable *)data;
  element = node->data;

  type_string = get_element_type_string (element);
  GST_CAT_DEBUG (ebuttdparse, "Element type: %s", type_string);
  g_free (type_string);

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
      if (element->type == GST_EBUTTD_ELEMENT_TYPE_ANON_SPAN) {
        /* Anon spans should merge all style attributes from their parent. */
        element->style_set = merge_style_sets (parent->style_set,
            element->style_set);
      } else {
        element->style_set = inherit_styling (parent->style_set,
            element->style_set);
      }
      _print_style_set (element->style_set);
      if (tmp) g_free (tmp);
    }
  }

  if (element->style_set) {
    GST_CAT_DEBUG (ebuttdparse, "Resolved style:");
    _print_style_set (element->style_set);
  }

  return FALSE;
}


static void
resolve_body_styles (GNode * tree, GHashTable * style_hash)
{
  g_node_traverse (tree, G_PRE_ORDER, G_TRAVERSE_ALL, -1, resolve_element_style,
      style_hash);
}


static gboolean
resolve_element_timings (GNode * node, gpointer data)
{
  GstEbuttdElement *element, *leaf;

  g_return_val_if_fail (node != NULL, FALSE);
  leaf = element = node->data;

  if (GST_CLOCK_TIME_IS_VALID (leaf->begin)
      && GST_CLOCK_TIME_IS_VALID (leaf->end)) {
    GST_CAT_LOG (ebuttdparse, "Leaf node already has timing.");
    return FALSE;
  }

  /* Inherit timings from ancestor. */
  while (node->parent && !GST_CLOCK_TIME_IS_VALID (element->begin)) {
    node = node->parent;
    element = node->data;
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
  GST_CAT_DEBUG (ebuttdparse, "Inheriting styling from region %s",
      element->region);
  tmp = element->style_set;
  element->style_set = inherit_styling (region->style_set, element->style_set);
  if (tmp) delete_style_set (tmp);

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

  GST_CAT_LOG (ebuttdparse, "begin: %llu  end: %llu  start_time: %llu",
      element->begin, element->end, state->start_time);

  if ((element->begin > state->start_time)
      && (element->begin < state->next_transition_time)) {
    state->next_transition_time = element->begin;
    GST_CAT_LOG (ebuttdparse,
        "Updating next transition time to element begin time (%llu)",
        state->next_transition_time);
  } else if ((element->end > state->start_time)
      && (element->end < state->next_transition_time)) {
    state->next_transition_time = element->end;
    GST_CAT_LOG (ebuttdparse,
        "Updating next transition time to element end time (%llu)",
        state->next_transition_time);
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
    GST_CAT_LOG (ebuttdparse, "Found element appearing at time %llu",
        transition->time);
  } else if (element->end == transition->time) {
    transition->disappearing_elements =
      g_list_append (transition->disappearing_elements, element);
    GST_CAT_LOG (ebuttdparse, "Found element disappearing at time %llu",
        transition->time);
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
  GST_CAT_LOG (ebuttdparse, "Next transition is at %llu",
      state.next_transition_time);

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
    GST_CAT_LOG (ebuttdparse, "Next transition found at time %llu",
        transition->time);
    if (cur_scene) cur_scene->end = transition->time;

    active_elements = update_active_element_list (active_elements, transition);
    GST_CAT_LOG (ebuttdparse, "There will be %u active elements after"
        "transition", g_list_length (active_elements));

    if (active_elements) {
      GstEbuttdScene * new_scene = g_new0 (GstEbuttdScene, 1);
      new_scene->begin = transition->time;
      new_scene->elements = g_list_copy (active_elements);
      output_scenes = g_list_append (output_scenes, new_scene);
      cur_scene = new_scene;
    } else {
      cur_scene = NULL;
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


static void
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
  GQueue *node_stack;
  GNode *element_node, *ancestor, *junction = NULL;
  GNode *ret = NULL;

  g_return_val_if_fail (tree != NULL, NULL);
  g_return_val_if_fail (active_elements != NULL, NULL);

  node_stack = g_queue_new ();

  GST_CAT_DEBUG (ebuttdparse, "There are %u active elements",
      g_list_length (active_elements));

  /* Create a new tree containing all active elements and their ancestors. */
  for (leaves = g_list_first (active_elements); leaves != NULL;
      leaves = leaves->next) {
    GNode * new_leaf;
    GstEbuttdElement *element = leaves->data;
    /* XXX: Revert to storing nodes in active_elements to avoid find
     * operation? */
    element_node = g_node_find (tree, G_PRE_ORDER, G_TRAVERSE_LEAVES, element);
    g_assert (element_node != NULL);
    GST_CAT_DEBUG (ebuttdparse, "Finding ancestors for following element:");
    _print_element (element_node->data);

    for (ancestor = element_node->parent; ancestor;
        ancestor = ancestor->parent) {
      /* Don't include ancestors already in output tree. */
      if (ret && (junction = g_node_find (ret, G_PRE_ORDER,
              G_TRAVERSE_ALL, ancestor->data))) {
          GST_CAT_DEBUG (ebuttdparse, "Element already exists in output tree:");
          _print_element (ancestor->data);
          break;
      } else {
        g_queue_push_head (node_stack, ancestor->data);
        GST_CAT_DEBUG (ebuttdparse, "Added following element to stack:");
        _print_element (ancestor->data);
        GST_CAT_DEBUG (ebuttdparse, "Stack depth is now %u",
            g_queue_get_length (node_stack));
      }
    }

    /* Graft nodes in queue onto output tree. */
    while ((element = g_queue_pop_head (node_stack))) {
      GNode *node = g_node_new (element);
      if (junction) {
        junction = g_node_append (junction, node);
        GST_CAT_DEBUG (ebuttdparse, "Appended following element to ret:");
        _print_element (junction->data);
      } else {
        GST_CAT_DEBUG (ebuttdparse,
            "Setting the following element at the head of ret:");
        _print_element (node->data);
        ret = junction = node;
      }
    }

    /* Append active element to tip of branch. */
    new_leaf = g_node_new (element_node->data);
    junction = g_node_append (junction, new_leaf);
  }

  return ret;
}


static guint
add_text_to_buffer (GstBuffer * buf, const gchar * text)
{
  GstMemory *mem;
  GstMapInfo map;
  guint ret;

  mem = gst_allocator_alloc (NULL, strlen (text) + 1, NULL);
  if (!gst_memory_map (mem, &map, GST_MAP_WRITE))
    GST_CAT_ERROR (ebuttdparse, "Failed to map memory.");

  g_strlcpy ((gchar *)map.data, text, map.size);
  GST_CAT_DEBUG (ebuttdparse, "Inserted following text into buffer: %s",
      (gchar *)map.data);
  gst_memory_unmap (mem, &map);

  ret = gst_buffer_n_memory (buf);
  gst_buffer_insert_memory (buf, -1, mem);
  return ret;
}


static GstSubtitleArea *
create_subtitle_area (GstEbuttdScene * scene, GNode * tree, guint cellres_x,
    guint cellres_y)
{
  GstSubtitleArea *area;
  GstSubtitleStyleSet *region_style;
  GstEbuttdElement *element;
  GNode *node;

  g_return_val_if_fail (tree != NULL, NULL);
  element = tree->data;
  g_assert (element->type == GST_EBUTTD_ELEMENT_TYPE_REGION);

  /* Create SubtitleArea from region. */
  region_style = gst_subtitle_style_set_new ();
  update_style_set (region_style, element->style_set, cellres_x, cellres_y);
  area = gst_subtitle_area_new (region_style);
  g_assert (area != NULL);

  node = tree->children;
  g_assert (node->next == NULL);
  element = node->data;
  g_assert (element->type == GST_EBUTTD_ELEMENT_TYPE_BODY);

  node = node->children;
  while (node) {
    GNode *p_node;

    element = node->data;
    g_assert (element->type == GST_EBUTTD_ELEMENT_TYPE_DIV);

    p_node = node->children;
    while (p_node) {
      GstSubtitleBlock *block;
      GstSubtitleStyleSet *block_style;
      GNode *content_node;

      element = p_node->data;
      g_assert (element->type == GST_EBUTTD_ELEMENT_TYPE_P);
      block_style = gst_subtitle_style_set_new ();
      update_style_set (block_style, element->style_set, cellres_x, cellres_y);

      /* TODO: blend bg colors from body, div and p here. */

      block = gst_subtitle_block_new (block_style);
      g_assert (block != NULL);

      content_node = p_node->children;
      while (content_node) {
        GstSubtitleElement *e;
        GstSubtitleStyleSet *element_style;
        guint buffer_index;
        GNode *anon_node;

        element = content_node->data;
        g_assert (element->type == GST_EBUTTD_ELEMENT_TYPE_SPAN
            || element->type == GST_EBUTTD_ELEMENT_TYPE_ANON_SPAN);

        /* XXX: A lot of code below is repetitive (the code that creates a
         * GstSubtitleElement and puts its text into the GstBuffer), and so
         * should be moved into a separate function, or this function should be
         * made recursive. */
        if (element->type == GST_EBUTTD_ELEMENT_TYPE_BR) {
          element_style = gst_subtitle_style_set_new ();
          update_style_set (element_style, element->style_set,
              cellres_x, cellres_y);
          GST_CAT_DEBUG (ebuttdparse, "Creating element with text index %u",
              element->text_index);

          /* Create new memory holding element text and append to scene's
           * buffer. */
          buffer_index = add_text_to_buffer (scene->buf, "\n");
          GST_CAT_DEBUG (ebuttdparse, "Inserted text at index %u in GstBuffer.",
              buffer_index);
          e = gst_subtitle_element_new (element_style, buffer_index);

          gst_subtitle_block_add_element (block, e);
          GST_CAT_DEBUG (ebuttdparse, "Added element to block; there are now %u"
              " elements in the block.",
              gst_subtitle_block_get_element_count (block));
        } else if (element->type == GST_EBUTTD_ELEMENT_TYPE_ANON_SPAN) {
          element_style = gst_subtitle_style_set_new ();
          update_style_set (element_style, element->style_set,
              cellres_x, cellres_y);
          GST_CAT_DEBUG (ebuttdparse, "Creating element with text index %u",
              element->text_index);

          /* Create new memory holding element text and append to scene's
           * buffer. */
          g_assert (element->text != NULL);
          buffer_index = add_text_to_buffer (scene->buf, element->text);
          GST_CAT_DEBUG (ebuttdparse, "Inserted text at index %u in GstBuffer.",
              buffer_index);
          e = gst_subtitle_element_new (element_style, buffer_index);

          gst_subtitle_block_add_element (block, e);
          GST_CAT_DEBUG (ebuttdparse, "Added element to block; there are now %u"
              " elements in the block.",
              gst_subtitle_block_get_element_count (block));
        } else if (element->type == GST_EBUTTD_ELEMENT_TYPE_SPAN) {
          /* Loop through anon-span children of this span. */
          anon_node = content_node->children;
          while (anon_node) {
            GstSubtitleElement *e;
            GstSubtitleStyleSet *element_style;
            guint buffer_index;

            element = anon_node->data;

            if (element->type == GST_EBUTTD_ELEMENT_TYPE_BR) {
              element_style = gst_subtitle_style_set_new ();
              update_style_set (element_style, element->style_set,
                  cellres_x, cellres_y);
              GST_CAT_DEBUG (ebuttdparse, "Creating element with text index %u",
                  element->text_index);

              /* Create new memory holding element text and append to scene's
               * buffer. */
              buffer_index = add_text_to_buffer (scene->buf, "\n");
              GST_CAT_DEBUG (ebuttdparse, "Inserted text at index %u in "
                  "GstBuffer.", buffer_index);
              e = gst_subtitle_element_new (element_style, buffer_index);

              gst_subtitle_block_add_element (block, e);
              GST_CAT_DEBUG (ebuttdparse, "Added element to block; there are "
                  "now %u elements in the block.",
                  gst_subtitle_block_get_element_count (block));
            } else if (element->type == GST_EBUTTD_ELEMENT_TYPE_ANON_SPAN) {
              element_style = gst_subtitle_style_set_new ();
              update_style_set (element_style, element->style_set,
                  cellres_x, cellres_y);
              GST_CAT_DEBUG (ebuttdparse, "Creating element with text index %u",
                  element->text_index);

              /* Create new memory holding element text and append to scene's
               * buffer. */
              g_assert (element->text != NULL);
              buffer_index = add_text_to_buffer (scene->buf, element->text);
              GST_CAT_DEBUG (ebuttdparse, "Inserted text at index %u in "
                  "GstBuffer.", buffer_index);
              e = gst_subtitle_element_new (element_style, buffer_index);

              gst_subtitle_block_add_element (block, e);
              GST_CAT_DEBUG (ebuttdparse, "Added element to block; there are "
                  "now %u elements in the block.",
                  gst_subtitle_block_get_element_count (block));
            } else {
              GST_CAT_ERROR (ebuttdparse,
                  "Element type not allowed at this level of document.");
            }
            anon_node = anon_node->next;
          }
        } else {
          GST_CAT_ERROR (ebuttdparse,
              "Element type not allowed at this level of document.");
        }

        content_node = content_node->next;
      }

      gst_subtitle_area_add_block (area, block);
      GST_CAT_DEBUG (ebuttdparse, "Added block to area; there are now %u blocks"
          " in the area.", gst_subtitle_area_get_block_count (area));
      p_node = p_node->next;
    }
    node = node->next;
  }

  return area;
}


static GNode *
create_and_attach_metadata (GNode * tree, GList * scenes,
    GHashTable * region_hash, guint cellres_x, guint cellres_y)
{
  g_return_if_fail (tree != NULL);
  g_return_if_fail (scenes != NULL);
  g_return_if_fail (region_hash != NULL);

  while (scenes) {
    GstEbuttdScene * scene = scenes->data;
    GHashTable *elements_by_region;
    GHashTableIter iter;
    gpointer key, value;
    GPtrArray *areas = g_ptr_array_new ();

    g_assert (scene != NULL);

    scene->buf = gst_buffer_new ();
    GST_BUFFER_PTS (scene->buf) = scene->begin;
    GST_BUFFER_DURATION (scene->buf) = (scene->end - scene->begin);

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
      GST_CAT_LOG (ebuttdparse, "Returned tree has %u nodes",
          g_node_n_nodes (isd_tree, G_TRAVERSE_ALL));

      /* Retrieve region element and wrap in a node. */
      GST_CAT_LOG (ebuttdparse, "About to retrieve %s from hash table %p",
          region_name, region_hash);
      region = g_hash_table_lookup (region_hash, region_name);
      g_assert (region != NULL);
      region_node = g_node_new (region);

      /* Reparent tree to region node. */
      g_node_prepend (region_node, isd_tree);

      area = create_subtitle_area (scene, region_node, cellres_x, cellres_y);
      g_ptr_array_add (areas, area);
      g_node_destroy (isd_tree);
    }

    gst_buffer_add_subtitle_meta (scene->buf, areas);
    scenes = g_list_next (scenes);
  }

  return NULL;
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
  xmlDocPtr doc;                /* pointer for tree */
  xmlNodePtr cur;

  GHashTable *style_hash = g_hash_table_new_full (g_str_hash, g_str_equal,
      NULL, (GDestroyNotify) delete_element);
  GHashTable *region_hash = g_hash_table_new_full (g_str_hash, g_str_equal,
      NULL, (GDestroyNotify) delete_element);
  DocMetadata *document_metadata = NULL;
  GNode *body = NULL;
  GList *scenes = NULL;
  GList *buffer_list = NULL;
  guint cellres_x, cellres_y;

  GST_DEBUG_CATEGORY_INIT (ebuttdparse, "ebuttdparser", 0,
      "EBU-TT-D debug category");
  GST_CAT_LOG (ebuttdparse, "Input file:\n%s", xml_file_buffer);

  /* create element tree */
  if (create_element_tree (xml_file_buffer, &doc, &cur)) {
    GST_CAT_ERROR (ebuttd_parse_debug, "Failed to parse document.");
    return NULL;
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

  cellres_x = (guint) g_ascii_strtoull (document_metadata->cell_resolution_x,
      NULL, 10U);
  cellres_y = (guint) g_ascii_strtoull (document_metadata->cell_resolution_y,
      NULL, 10U);

  cur = cur->children;
  while (cur != NULL) {
    /* Process head of xml doc */
    if (xmlStrcmp (cur->name, (const xmlChar *) "head") == 0) {
      xml_process_head (cur, style_hash, region_hash);
    }
    /* Process Body of xml doc */
    else if (xmlStrcmp (cur->name, (const xmlChar *) "body") == 0) {
      body = parse_tree (cur);
      GST_CAT_LOG (ebuttdparse, "Body tree contains %u nodes.",
          g_node_n_nodes (body, G_TRAVERSE_ALL));
      GST_CAT_LOG (ebuttdparse, "Body tree height is %u",
          g_node_max_height (body));

      resolve_region_styles (region_hash, style_hash);
      resolve_body_styles (body, style_hash);
      GST_CAT_LOG (ebuttdparse, "Body tree now contains %u nodes.",
          g_node_n_nodes (body, G_TRAVERSE_ALL));
      strip_surrounding_whitespace (body);
      resolve_timings (body);
      resolve_regions (body);
      inherit_region_styles (body, region_hash);
      scenes = create_scenes (body);
      GST_CAT_LOG (ebuttdparse, "There are %u scenes in all.",
          g_list_length (scenes));
      create_and_attach_metadata (body, scenes, region_hash, cellres_x,
          cellres_y);
      buffer_list = create_buffer_list (scenes);
      GST_CAT_LOG (ebuttdparse, "There are %u buffers in output list.",
          g_list_length (buffer_list));
    }
    cur = cur->next;
  }

  xmlFreeDoc (doc);
  g_hash_table_destroy (style_hash);
  g_hash_table_destroy (region_hash);
  if (document_metadata) g_free (document_metadata);

  return buffer_list;
}
