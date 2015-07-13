/* GStreamer SAMI subtitle parser
 * Copyright (c) 2006  Young-Ho Cha <ganadist chollian net>
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

#ifndef _EBU_PARSE_H_
#define _EBU_PARSE_H_

#include "gstsubparse.h"
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

#define MAX_FONT_FAMILY_NAME_LENGTH 128


G_BEGIN_DECLS

typedef struct _GstEbuttdStyleSet GstEbuttdStyleSet;
typedef struct _GstEbuttdElement GstEbuttdElement;
typedef struct _GstEbuttdScene GstEbuttdScene;
typedef struct _GstEbuttdTransition GstEbuttdTransition;


struct _GstEbuttdStyleSet {
  const gchar *text_direction;
  const gchar *font_family;
  const gchar *font_size;
  const gchar *line_height;
  const gchar *text_align;
  const gchar *color;
  const gchar *bg_color;
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
  GST_EBUTTD_ELEMENT_TYPE_STYLE,
  GST_EBUTTD_ELEMENT_TYPE_REGION,
  GST_EBUTTD_ELEMENT_TYPE_BODY,
  GST_EBUTTD_ELEMENT_TYPE_DIV,
  GST_EBUTTD_ELEMENT_TYPE_P,
  GST_EBUTTD_ELEMENT_TYPE_SPAN,
  GST_EBUTTD_ELEMENT_TYPE_ANON_SPAN,
  GST_EBUTTD_ELEMENT_TYPE_BR,
} GstEbuttdElementType;


struct _GstEbuttdElement {
  GstEbuttdElementType type;
  gchar *id;
  gchar **styles;
  gchar *region;
  GstClockTime begin;
  GstClockTime end;
  GstEbuttdStyleSet *style_set;
  gchar *text;
  guint text_index;
};


/* Represents a static scene consisting of one or more text elements that
 * should be visible over a specific period of time. */
struct _GstEbuttdScene {
  GstClockTime begin;
  GstClockTime end;
  GList *elements;
  GstBuffer *buf;
};


/* Represents a transition, i.e., a point in time where one or more elements
 * change from being visible to invisible (and/or vice-versa). */
struct _GstEbuttdTransition {
  GstClockTime time;
  GList *appearing_elements;
  GList *disappearing_elements;
};



/* struct to hold metadata before the head metadata
* TODO: Rename Namespaces or something more descriptive
* TODO: This only records the namespaces if the recommend names
* are used. These prefixes are not required by the ebuttd
*/
typedef struct
{
  gchar *xmlns;
  gchar *ttp;
  gchar *tts;
  gchar *ttm;
  gchar *ebuttm;
  gchar *ebutts;
  gchar *lang;
  gchar *space;
  gchar *timeBase;
  gchar *cellResolution;        /* "<#col> <#row>"  eg "32 15" */

  gchar *cell_resolution_x;     /*"32" */
  gchar *cell_resolution_y;     /*"15" */

  gboolean sent_document_metadata;


} DocMetadata;

GList *ebutt_xml_parse (const gchar * xml_file_buffer);

/**
 * creates element tree from a string representation of an xml file
 * @param  xml_file_buffer string of xml
 * @param  doc             doc pointer to be filled
 * @param  cur             cursor p;ointer to be filled
 * @return                 0 for success
 */
gint create_element_tree (const gchar * xml_file_buffer,
    xmlDocPtr * doc, xmlNodePtr * cur);

/**
 * pass head cursor and will do all the processing of the head portion
 * of the xml doc
 * @param cur         xmlNodePtr for the head element
 * @param style_hash  hash table containing the head data
 * @param region_hash hash table containing the region data
 */
void xml_process_head (xmlNodePtr head_cur, GHashTable * style_hash,
    GHashTable * region_hash);

DocMetadata *extract_tt_tag_properties (xmlNodePtr ttnode,
    DocMetadata * document_metadata);

G_END_DECLS
#endif /* _EBU_PARSE_H_ */
