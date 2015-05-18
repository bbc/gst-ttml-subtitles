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

typedef struct _GstEbuttdRegion GstEbuttdRegion;
typedef struct _GstEbuttdStyle GstEbuttdStyle;
typedef struct _GstEbuttdStyleSet GstEbuttdStyleSet;
typedef struct _GstEbuttdColor GstEbuttdColor;
typedef struct _GstEbuttdMediaTime GstEbuttdMediaTime;
typedef struct _GstEbuttdElement GstEbuttdElement;
typedef struct _GstEbuttdScene GstEbuttdScene;
typedef struct _GstEbuttdTransition GstEbuttdTransition;
typedef struct _GstEbuttdTree GstEbuttdTree;
typedef struct _GstEbuttdTextElement GstEbuttdTextElement;
typedef struct _GstEbuttdTextBlock GstEbuttdTextBlock;
typedef struct _GstEbuttdTextArea GstEbuttdTextArea;

/**
 * GstEbuttdWritingMode:
 * @GST_EBUTTD_WRITING_MODE_LRTB: text is written left-to-right, top-to-bottom.
 * @GST_EBUTTD_WRITING_MODE_RLTB: text is written right-to-left, top-to-bottom.
 * @GST_EBUTTD_WRITING_MODE_TBRL: text is written top-to-bottom, right-to-left.
 * @GST_EBUTTD_WRITING_MODE_TBLR: text is written top-to-bottom, left-to-right.
 *
 * Writing mode of text content.
 */
typedef enum {
    GST_EBUTTD_WRITING_MODE_LRTB,
    GST_EBUTTD_WRITING_MODE_RLTB,
    GST_EBUTTD_WRITING_MODE_TBRL,
    GST_EBUTTD_WRITING_MODE_TBLR
} GstEbuttdWritingMode; /* Or GstEbuttdTextProgression? */

typedef enum {
    GST_EBUTTD_DISPLAY_ALIGN_BEFORE,
    GST_EBUTTD_DISPLAY_ALIGN_CENTER,
    GST_EBUTTD_DISPLAY_ALIGN_AFTER
} GstEbuttdDisplayAlign;

typedef enum {
    GST_EBUTTD_BACKGROUND_MODE_ALWAYS,
    GST_EBUTTD_BACKGROUND_MODE_WHEN_ACTIVE,
} GstEbuttdBackgroundMode;

typedef enum {
    GST_EBUTTD_OVERFLOW_MODE_HIDDEN,
    GST_EBUTTD_OVERFLOW_MODE_VISIBLE,
} GstEbuttdOverflowMode;

struct _GstEbuttdRegion {
    /*
     * Properties of region from EBU-TT-D spec:
     *
     *   origin - coordinates of region origin in % of width & height of root
     *   container.
     *
     *   extent - size of region, again %age of width and height of root
     *   container.
     *
     *   displayAlign - not quite sure I understand this one, but this seems to
     *   be some kind of vertical aligment,
     *
     *   padding - padding to be applied on all sides of the region area.
     *
     *   writingMode - specifies the direction in which text progresses, both
     *   horizontally and vertically.
     *
     *   showBackground - controls whether the background colour of the region
     *   is always shown, or shown only when there is some text that is
     *   rendered in the region.
     *
     *   overflow - determines whether or not content that overflows the region
     *   area is clipped.
     */
    gdouble origin_x, origin_y;
    gdouble extent_w, extent_h;
    GstEbuttdDisplayAlign display_align;
    gdouble padding_start, padding_end, padding_before, padding_after;
    GstEbuttdWritingMode writing_mode;
    GstEbuttdBackgroundMode show_background;
    GstEbuttdOverflowMode overflow;
};


struct _GstEbuttdColor {
  gdouble r;
  gdouble g;
  gdouble b;
  gdouble a;
};


struct _GstEbuttdTextElement {
  GstEbuttdStyle *style_attr;
  gboolean newline;
  gchar *text;
};

struct _GstEbuttdTextBlock {
  GstEbuttdColor bg_color;
  GSList *elements;
};

struct _GstEbuttdTextArea {
  GstEbuttdRegion *region_attr;
  GstEbuttdColor bg_color;
  GSList *text_blocks;
};


typedef enum {
  GST_EBUTTD_TEXT_DIRECTION_LTR,
  GST_EBUTTD_TEXT_DIRECTION_RTL
} GstEbuttdTextDirection;

typedef enum {
  GST_EBUTTD_TEXT_ALIGN_START,
  GST_EBUTTD_TEXT_ALIGN_LEFT,
  GST_EBUTTD_TEXT_ALIGN_CENTER,
  GST_EBUTTD_TEXT_ALIGN_RIGHT,
  GST_EBUTTD_TEXT_ALIGN_END
} GstEbuttdTextAlign;

typedef enum {
  GST_EBUTTD_FONT_STYLE_NORMAL,
  GST_EBUTTD_FONT_STYLE_ITALIC
} GstEbuttdFontStyle;

typedef enum {
  GST_EBUTTD_FONT_WEIGHT_NORMAL,
  GST_EBUTTD_FONT_WEIGHT_BOLD,
} GstEbuttdFontWeight;

typedef enum {
  GST_EBUTTD_TEXT_DECORATION_NONE,
  GST_EBUTTD_TEXT_DECORATION_UNDERLINE
} GstEbuttdTextDecoration;

typedef enum {
  GST_EBUTTD_UNICODE_BIDI_NORMAL,
  GST_EBUTTD_UNICODE_BIDI_EMBED,
  GST_EBUTTD_UNICODE_BIDI_OVERRIDE
} GstEbuttdUnicodeBidi;

typedef enum {
  GST_EBUTTD_WRAPPING_ON,
  GST_EBUTTD_WRAPPING_OFF,
} GstEbuttdWrapping;

typedef enum {
  GST_EBUTTD_MULTI_ROW_ALIGN_AUTO,
  GST_EBUTTD_MULTI_ROW_ALIGN_START,
  GST_EBUTTD_MULTI_ROW_ALIGN_CENTER,
  GST_EBUTTD_MULTI_ROW_ALIGN_END,
} GstEbuttdMultiRowAlign;

struct _GstEbuttdStyle {
  GstEbuttdTextDirection text_direction;
  gchar font_family[MAX_FONT_FAMILY_NAME_LENGTH];
  gdouble font_size;
  gdouble line_height;
  GstEbuttdTextAlign text_align;
  GstEbuttdColor color;
  GstEbuttdColor bg_color;
  GstEbuttdFontStyle font_style;
  GstEbuttdFontWeight font_weight;
  GstEbuttdTextDirection text_decoration;
  GstEbuttdUnicodeBidi unicode_bidi;
  GstEbuttdWrapping wrap_option;
  GstEbuttdMultiRowAlign multi_row_align;
  gdouble line_padding;
  gdouble origin_x, origin_y;
  gdouble extent_w, extent_h;
  GstEbuttdDisplayAlign display_align;
  gdouble padding_start, padding_end, padding_before, padding_after;
  GstEbuttdWritingMode writing_mode;
  GstEbuttdBackgroundMode show_background;
  GstEbuttdOverflowMode overflow;
  /*guint cellres_x, cellres_y;*/
};


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

struct _GstEbuttdMediaTime {
  guint hours;
  guint minutes;
  guint seconds;
  guint milliseconds;
};

struct _GstEbuttdElement {
  GstEbuttdElementType type;
  gchar *id;
  gchar **styles;
  gchar *region;
  GstClockTime begin;
  GstClockTime end;
  GstEbuttdStyleSet *style_set;
  GstEbuttdStyle *style;
  gchar *text;
};


/* Represents a static scene consisting of one or more text elements that
 * should be visible over a specific period of time. */
struct _GstEbuttdScene {
  GstClockTime begin;
  GstClockTime end;
  GList *elements;
};


/* Represents a transition, i.e., a point in time where one or more elements
 * change from being visible to invisible (and/or vice-versa). */
struct _GstEbuttdTransition {
  GstClockTime time;
  GList *appearing_elements;
  GList *disappearing_elements;
};


struct _GstEbuttdTree {
  GNode *root;
};

/* struct to hold text and formatting of the subtitle */
    typedef struct
{
  gchar *text;                  /* text to be asigned as a buffer */
  gchar *style;                 /* reference to the header eg s1 */
  GList *styles_to_inherit;     /*https://developer.gnome.org/glib/stable/glib-Doubly-Linked-Lists.html#GList */
  guint64 start_time;
  guint64 end_time;

  guint64 duration;

} SubtitleObj;
/**
 * the possible styles a SubtitleObj could use to fill the
 * styles_to_inherit GList
 */
typedef struct
{
  gchar *body_regions;          //etc
} StyleTree;


typedef GList InheritanceList;



/**
 * A linked list.
 * if a subtitle is broken into chunks with different styles,
 * use SubSubtitle to hold lists of which styles and regions.
 * concatenate for full sub.
 */
struct sub_subtitle
{
  gchar *text;                  /* String of the subsub */
  InheritanceList *styles;      /* List up generations  */
  InheritanceList *regions;     /* List up generations  */
  struct sub_subtitle *next;    /* join for  full sub   */
  struct sub_subtitle *previous;
};
typedef struct sub_subtitle SubSubtitle;

/* struct to hold metadata before the head metadata */
typedef struct
{
  gchar *text;
  ParserState *state;
} SubStateObj;



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

struct style_properties
{
  gchar *direction;             /* pointer as dont know how long . "ltr"/"rtl" */
  gchar *fontFamily;
  gchar *fontSize;
  gchar *lineHeight;
  gchar *textAlign;             /* left, center, right, start, end */
  gchar *color;                 /* forground colour eg yellow */
  gchar *backgroundColor;
  gchar *fontStyle;             /* normal italic */
  gchar *fontWeight;            /* normal bold */
  gchar *textDecoration;        /* none underline */
  gchar *unicodeBidi;           /* normal embed bidiOverride */
  gchar *wrapOption;            /* wrap noWrap */
  gchar *multiRowAlign;         /* start center end auto */
  gchar *linePadding;           /* initial value is "0c" */

  gchar *id;                    /* style id used to identify when used in span tags etc */
  gchar *inherited_styles;      /* space seperated styles */
  struct style_properties *next;        /* pointer to next style in the list */

  struct DocMetadata *document_metadata;        /* pointer to one time only metadata */
};

typedef struct style_properties StyleList;


struct single_style_properties
{
  gchar *direction;             /* pointer as dont know how long . "ltr"/"rtl" */
  gchar *fontFamily;
  gchar *fontSize;
  gchar *lineHeight;
  gchar *textAlign;             /* left, center, right, start, end */
  gchar *color;                 /* forground colour eg yellow */
  gchar *backgroundColor;
  gchar *fontStyle;             /* normal italic */
  gchar *fontWeight;            /* normal bold */
  gchar *textDecoration;        /* none underline */
  gchar *unicodeBidi;           /* normal embed bidiOverride */
  gchar *wrapOption;            /* wrap noWrap */
  gchar *multiRowAlign;         /* start center end auto */
  gchar *linePadding;           /* initial value is "0c" */

  gchar *id;                    /* style id used to identify when used in span tags etc */
  gchar *inherited_styles;      /* space seperated styles */
};
typedef struct single_style_properties StyleProp;

#if 0
struct _region_properties
{
  gdouble origin_x;
  gdouble origin_y;
  gdouble extent_w;
  gdouble extent_h;
  gchar *padding;
  gchar *writing_mode;
  gboolean show_background;
  gboolean overflow;
};
#endif

struct _region_properties
{
  gchar *id;
  gchar *origin;
  gchar *extent;
  gchar *style;
  gchar *display_align;
  gchar *padding;
  gchar *writing_mode;
  gchar *show_background;
  gchar *overflow;
};
typedef struct _region_properties RegionProp;


/**
 * for passing around a style property structure and a look up table for
 * such structures.
 */
typedef struct
{
  StyleProp *markup_style;
  GHashTable *style_hash;
} DataForStyleIterator;


gchar *fetch_child (const gchar * parent_text);

/**
 * Finds next uccurance of element_name in a string parent_text
 * and returns the contence in the following " " pair.
 *
 * Note: some overlap with fetch_child which fetches the contence.
 *
 * @param  element_name eg "id"
 * @param  parent_text  eg <style id="s1">
 * @return eg "s1" . Returns NULL if not found.
 */
gchar *fetch_element (gchar * element_name, const gchar * parent_text);

/**
 * fetch the name of an element with matching prefix and suffix
 * @param  prefix
 * @param  suffix
 * @param  parent_text
 * @return
 */
gchar *fetch_element_name (const gchar * prefix, const gchar * suffix,
    const gchar * parent_text);


gchar *extract_style (const gchar * para_text);

/**
 * handed list of styles and checks to see if there is a match between
 * style_id and the id of one of the styles in the list.
 * @param  style_id
 * @param  head_style
 * @return none-zero if there is a match
 */
int is_style_cached (gchar * style_id, StyleList * head_style);

/**
 * pass list of styles and style_id. Returns matching style.
 * @param  style_id
 * @param  head_style
 * @return
 */
StyleList *retrieve_style (gchar * style_id, StyleList * head_style);

/**
 * appends pango style markup to the opening tag. TODO: SHOULD THIS NOT GO IN THE HEADER?
 * @param  style
 * @param  opening_tag
 * @return
 */
gchar *_add_pango_style (StyleProp * style, gchar * opening_tag);
/**
 * add new style to the linked list
 * @param style_id
 * @param head_style
 * @param line
 */
StyleProp * add_new_style (const gchar * style_id, xmlNodePtr parent);

RegionProp * add_new_region (const gchar * region_id, xmlNodePtr child);

/**
 * concatonates return text (ret) with markup tags for all of the supported pango formattings.
 * @param  sub_meta   pointer to metadata describing the subitile p
 * @param  head_style pointer to list of styles for this file
 * @param  ret        return text
 * @return            ret but with markup appended.
 */
gchar *add_style_markup_depreciated (SubtitleObj * sub_meta,
    StyleList * head_style, gchar * raw_sub, DocMetadata * doc_meta);

/**
 * simplified version of add_style_markup_depreciated
 * @param  text
 * @param  style
 * @return
 */
void add_style_markup (gchar ** text, StyleProp * style, gchar * region,
    DocMetadata * doc_meta);

gchar *add_document_metadata_markup (DocMetadata * doc_meta,
    gchar * opening_tag);

gchar *extract_text (SubtitleObj * subtitle, const gchar * line);

GList *ebutt_xml_parse (const gchar * xml_file_buffer);

guint64 extract_timestamp (xmlChar * time_as_string);


void add_to_document_metadata (DocMetadata * document_metadata,
    const gchar * type, const gchar * line);


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
 * Create a new subsubtitle from child of and element
 * @param sub_subtitle_head current head of linked lists
 * @param sub_subtitle_new  New item for linked list
 * @param child_node
 * @param inherited_styles  list of style ids to inherit
 * @param inherited_regions list of region ids to inherit
 */
SubSubtitle *add_new_subsubtitle (SubSubtitle * sub_subtitle_head,
    xmlNodePtr child_node,
    InheritanceList * inherited_styles, InheritanceList * inherited_regions);

/**
 * applies markup to each subtitle and then concatenates.
 * frees subsubtitle
 * @param  sub_subtitle list of subsubtitles
 * @param  style_hash   hash table with all available stlyes
 * @param  region_hash  hash table with all available regions
 * @return              concatenated string. free when used.
 */
gchar *sub_subtitle_concat_markup (SubSubtitle * sub_subtitle,
    GHashTable * style_hash,
    GHashTable * region_hash, DocMetadata * document_metadata);

/**
 * takes a markup_style property struct and adds styles
 * `that do not already exist
 * @param markup_style single instance of StylePro
 * @param StyleProp style_props potential candidates to add
 */
void markup_style_add_if_null (StyleProp * markup_style,
    StyleProp * style_props);

/**
 * Wrapper around glist_prepend
 * add items to the end of an inheritance, handles space separated ids
 * @param inheriteance_list to be appended to
 * @param ids_str       string containing space separated ids
 */
InheritanceList *inheritance_list_prepend (InheritanceList * inheritance_list,
    gchar * ids_str);

/**
 * Remove the first [number_to_remove] elements of an inheritance_list
 * @param  inheritance_list The passed list
 * @param  number_to_remove The number of elements to remove
 * @return                  The new start to the list
 */
InheritanceList *inheritance_list_remove_first (InheritanceList *
    inheritance_list, gint number_to_remove);

void extract_prepend_style_region (xmlNodePtr cur,
    InheritanceList ** styles,
    InheritanceList ** regions, gint * style_count, gint * region_count);


/**
* Like g_list_last
*/
SubSubtitle *sub_subtitle_list_last (SubSubtitle * list);
/**
 * checks node name for br and appends a <br/> to the previous sub_sub
 * if one exists
 * @param  node              the node to check
 * @param  sub_subtitle_head the sub_sub list
 * @return                   1 if br is found.
 */
int handle_line_break (xmlNodePtr node, SubSubtitle * sub_subtitle_head);

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


gchar *glist_print (InheritanceList * list);

/*


void    sami_context_init   (ParserState * state);

void    sami_context_deinit (ParserState * state);

void    sami_context_reset  (ParserState * state);*/

G_END_DECLS
#endif /* _EBU_PARSE_H_ */
