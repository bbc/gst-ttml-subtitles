/**
 * This parser has two jobs to do, which it does in the parse_ebutt function
 *
 * 1. return a string that is a g_string_free (The buffer grows automatically)
 *
 * 2. Use pointer to state to set properties of the ParseState struct. To be used to 
 * set  the buffer attributes.
 *
 * Looks like this.
 * 
* typedef struct {
              int      state;
              GString *buf;
              guint64  start_time;
              guint64  duration;
              guint64  max_duration; 
              GstSegment *segment;
              gpointer user_data;
              gboolean have_internal_fps; 
              gint fps_n, fps_d;     
            } ParserState;

 */

#include "ebuttxmlparse.h"

#include <glib.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

void
_print_style (StyleProp * style)
{
  g_print ("Style %p:\n", style);
  g_print ("\tdirection: %s\n", style->direction);
  g_print ("\tfontFamily: %s\n", style->fontFamily);
  g_print ("\tfontSize: %s\n", style->fontSize);
  g_print ("\tlineHeight;: %s\n", style->lineHeight);
  g_print ("\ttextAlign: %s\n", style->textAlign);
  g_print ("\tcolor: %s\n", style->color);
  g_print ("\tbackgroundColor;: %s\n", style->backgroundColor);
  g_print ("\tfontStyle: %s\n", style->fontStyle);
  g_print ("\tfontWeight: %s\n", style->fontWeight);
  g_print ("\ttextDecoration: %s\n", style->textDecoration);
  g_print ("\tunicodeBidi: %s\n", style->unicodeBidi);
  g_print ("\twrapOption: %s\n", style->wrapOption);
  g_print ("\tmultiRowAlign: %s\n", style->multiRowAlign);
  g_print ("\tlinePadding: %s\n", style->linePadding);
  g_print ("\tid: %s\n", style->id);
  g_print ("\tinherited_styles: %s\n", style->inherited_styles);
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
        g_strconcat (opening_tag, "fontSize=\"", style->fontSize, "\" ", NULL);
  }

  return (opening_tag);
}

gchar *
add_document_metadata_markup (DocMetadata * doc_meta, gchar * opening_tag)
{
  /* based on add_style_markup_depreciated */

  /* append stlyes and their values to the opening tag */
  opening_tag = _add_or_update_style (opening_tag, "cell_resolution_x=\"",
      doc_meta->cell_resolution_x);

  opening_tag = _add_or_update_style (opening_tag, "cell_resolution_y=\"",
      doc_meta->cell_resolution_y);

  return opening_tag;
}


void
add_style_markup (gchar ** text, StyleProp * style, DocMetadata * doc_meta)
{

  gchar *ret;
  gchar *opening_tag = (gchar *) malloc (7);

  gchar *close_brac = ">";
  gchar *close_tag = "</span>";
  strcpy (opening_tag, "<span \0");     /* add to heap in case there are no styles */



  /* append styles and their values to the opening tag */
  opening_tag = _add_pango_style (style, opening_tag);

  if (!(doc_meta->sent_document_metadata)) {
    opening_tag = add_document_metadata_markup (doc_meta, opening_tag);
    doc_meta->sent_document_metadata = TRUE;
  }


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
add_new_style (gchar * style_id, xmlNodePtr child)
{

  StyleProp *style = g_new (StyleProp, 1);

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

  style->id = style_id;         /* populate properties */
  return style;
}

void
delete_style (StyleProp * style)
{
  g_print ("Deleting style %p...\n", style);
  g_return_if_fail (style != NULL);

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
  g_print ("## Style:\n");
  _print_style (style_props);
  if (style_props)
    markup_style_add_if_null (markup_style, style_props);
}


gchar *
sub_subtitle_concat_markup (SubSubtitle * sub_subtitle,
    GHashTable * style_hash,
    GHashTable * region_hash, DocMetadata * document_metadata)
{
  /**
   * for each sub_subitle:
   *   apply all style and region info, go up the inheritance hierarchy
   *   only add new styles. Concatenate.
   */
  gchar *ret = NULL;
  SubSubtitle *sub_sub;         /* a single sub_subtitle iterable the list */
  DataForStyleIterator *data_for_iterator = g_new (DataForStyleIterator, 1);
  data_for_iterator->style_hash = style_hash;

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

    /* now add styles to this sub_sub text */
    add_style_markup (&(sub_sub->text), markup_style, document_metadata);

    /* concatenate to what we have so far */
    if (ret) {
      gchar *old_ret = ret;
      ret = g_strconcat (ret, sub_sub->text, NULL);
      g_free (sub_sub->text);
      g_free (old_ret);
    } else
      ret = sub_sub->text;

    g_free (markup_style);      /* TODO deeper free to free pointers too */
  }

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

      g_print ("From tag: \"%s\" prepended \"%s\" to style list %s\n",
          cur->name, styles_str, glist_print (*styles));
    }
  }
  if (regions) {
    regions_str = (gchar *) (xmlGetProp (cur, (xmlChar *)"region"));
    if (regions_str) {
      *regions = inheritance_list_prepend (*regions, regions_str);
      if (regions_count)
        (*regions_count)++;

      g_print ("From tag: \"%s\" prepended \"%s\" to region list %s\n",
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

void
xml_process_head (xmlNodePtr head_cur, GHashTable * style_hash,
    GHashTable * region_hash)
{
  xmlNodePtr head_child, styling_child; /* pointers to different levels */

  /* 1. extract styles
   *  2. create  Style struct if doesn't already exist
   inherited_styles
   */
  head_child = head_cur->children;
  while (head_child != NULL) {
    if (xmlStrcmp (head_child->name, (const xmlChar *) "styling") == 0) {
      styling_child = head_child->children;
      while (styling_child != NULL) {
        if (xmlStrcmp (styling_child->name, (const xmlChar *) "style") == 0) {
          /* use style id as key, create style properties object for value */
          gchar *key;
          StyleProp *style_properties;
          key = (gchar *) (xmlGetProp (styling_child, (xmlChar *)"id"));
          style_properties = add_new_style (key, styling_child);

          g_hash_table_insert (style_hash,
              (gpointer) key, (gpointer) style_properties);

          GST_CAT_DEBUG (ebuttd_parse_debug, "added style %s to %s",
              key, "style_hash");

        }
        styling_child = styling_child->next;
      }
    }
    head_child = head_child->next;
  }

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


GList *
ebutt_xml_parse (const gchar * xml_file_buffer)
{

  /** ebutt_xml_parse
  *
  * Aim: to take an XML document, in the form of a string, and parse it, 
  * creating a textbuffer for each ISD.
  *
  * Functionality tool set:
  *
  * 1. Create element tree
  * 2. fetch <p> tags
  * 3. check siblings, children 
  * 4. go up layers in the element tree
  * 5. create new gstbuffers
  * 6. Save and resolve style and region information
  * 7. create objects for region information, and for document wide information
  * 8. push global information through events chain. create an event handler for this.
  * 9. markup pango and non pango information 
  *
  * Good opportunity to get rid of the warnings!!
  *
  * Rational:
  * Want a linked list so that we can flexibly store as many styles as are required 
  * Could be more efficient if you read it in the first time and then create a list of 
  * structs of know length. 
  * this way removes the need for that step 
  * StyleLinkedList is a type so that I can verify things being passed to functions 
  **/


  gint element_tree;            /* 0 if successfully created */
  xmlDocPtr doc;                /* pointer for tree */
  xmlNodePtr cur;
  xmlNodePtr body_child, div_child, p_child, span_child;

  /* use g_list_append, g_list_prepend with */
  InheritanceList *inherited_styles, *inherited_regions;
  GList *subtitle_list = NULL;

  inherited_styles = NULL;
  inherited_regions = NULL;

  GHashTable *style_hash = g_hash_table_new_full (g_str_hash, g_str_equal,
      NULL, (GDestroyNotify) delete_style);
  GHashTable *region_hash = g_hash_table_new (g_str_hash, g_str_equal);
  DocMetadata *document_metadata = NULL;

  /*  
   * if you use g_hash_table_new_full then need two functions..
   *(GDestroyNotify)key_destroyed,
   (GDestroyNotify)key_destroyed);
   */

  /* create element tree */
  element_tree = create_element_tree (xml_file_buffer, &doc, &cur);

  if (element_tree != 0) {
    GST_CAT_DEBUG (ebuttd_parse_debug,
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

          /* add style and region ids to inheritance lists */
          extract_prepend_style_region (body_child,
              &inherited_styles,
              &inherited_regions, &div_styles_count, &div_regions_count);

          div_child = body_child->children;     /* div children are p tags */
          while (div_child != NULL) {
            if (xmlStrcmp (div_child->name, (const xmlChar *) "p") == 0) {
              /** 
               * p may or may not contain span tags
               * if p does not contain timing information then child must
               */
              xmlChar *begin = NULL, *end = NULL;
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

                begin_timestamp = extract_timestamp (begin);
                end_timestamp = extract_timestamp (end);

                ret = sub_subtitle_concat_markup (sub_subtitle,
                    style_hash, region_hash, document_metadata);

                /* free subsub after use */
                new_state = g_new (ParserState, 1);
                new_state->start_time = begin_timestamp;
                new_state->duration = end_timestamp - begin_timestamp;
                new_state->max_duration = end_timestamp - begin_timestamp;

                new_sub_n_state = g_new (SubStateObj, 1);
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
        }
        body_child = body_child->next;
      }
    }
    cur = cur->next;
  }

  xmlFreeDoc (doc);
  g_hash_table_destroy (style_hash);
  g_hash_table_destroy (region_hash);
  /** to free:
  * style hash table
  * free inheritance lists including span copies
  * free subsubtitles and contents
  */

  return g_list_reverse (subtitle_list);
}
