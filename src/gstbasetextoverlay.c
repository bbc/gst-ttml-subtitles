/* GStreamer
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
 * Copyright (C) <2003> David Schleef <ds@schleef.org>
 * Copyright (C) <2006> Julien Moutte <julien@moutte.net>
 * Copyright (C) <2006> Zeeshan Ali <zeeshan.ali@nokia.com>
 * Copyright (C) <2006-2008> Tim-Philipp Müller <tim centricular net>
 * Copyright (C) <2009> Young-Ho Cha <ganadist@gmail.com>
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
 * SECTION:element-textoverlay
 * @see_also: #GstTextRender, #GstClockOverlay, #GstTimeOverlay, #GstSubParse
 *
 * This plugin renders text on top of a video stream. This can be either
 * static text or text from buffers received on the text sink pad, e.g.
 * as produced by the subparse element. If the text sink pad is not linked,
 * the text set via the "text" property will be rendered. If the text sink
 * pad is linked, text will be rendered as it is received on that pad,
 * honouring and matching the buffer timestamps of both input streams.
 *
 * The text can contain newline characters and text wrapping is enabled by
 * default.
 *
 * <refsect2>
 * <title>Example launch lines</title>
 * |[
 * gst-launch -v videotestsrc ! textoverlay text="Room A" valign=top halign=left ! xvimagesink
 * ]| Here is a simple pipeline that displays a static text in the top left
 * corner of the video picture
 * |[
 * gst-launch -v filesrc location=subtitles.srt ! subparse ! txt.   videotestsrc ! timeoverlay ! textoverlay name=txt shaded-background=yes ! xvimagesink
 * ]| Here is another pipeline that displays subtitles from an .srt subtitle
 * file, centered at the bottom of the picture and with a rectangular shading
 * around the text in the background:
 * <para>
 * If you do not have such a subtitle file, create one looking like this
 * in a text editor:
 * |[
 * 1
 * 00:00:03,000 --> 00:00:05,000
 * Hello? (3-5s)
 *
 * 2
 * 00:00:08,000 --> 00:00:13,000
 * Yes, this is a subtitle. Don&apos;t
 * you like it? (8-13s)
 *
 * 3
 * 00:00:18,826 --> 00:01:02,886
 * Uh? What are you talking about?
 * I don&apos;t understand  (18-62s)
 * ]|
 * </para>
 * </refsect2>
 */

/* FIXME: alloc segment as part of instance struct */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gst/video/video.h>
#include <gst/video/gstvideometa.h>

#include "gstbasetextoverlay.h"
#include "gsttextoverlay.h"
#include <string.h>

/* FIXME:
 *  - use proper strides and offset for I420
 *  - if text is wider than the video picture, it does not get
 *    clipped properly during blitting (if wrapping is disabled)
 */

GST_DEBUG_CATEGORY (ebuttd_render_debug);
#define GST_CAT_DEFAULT ebuttd_render_debug

#define DEFAULT_PROP_TEXT 	""
#define DEFAULT_PROP_SHADING	FALSE
#define DEFAULT_PROP_VALIGNMENT	GST_BASE_EBUTTD_OVERLAY_VALIGN_BASELINE
#define DEFAULT_PROP_HALIGNMENT	GST_BASE_EBUTTD_OVERLAY_HALIGN_CENTER
#define DEFAULT_PROP_XPAD	25
#define DEFAULT_PROP_YPAD	25
#define DEFAULT_PROP_DELTAX	0
#define DEFAULT_PROP_DELTAY	0
#define DEFAULT_PROP_XPOS       0.5
#define DEFAULT_PROP_YPOS       0.5
#define DEFAULT_PROP_WRAP_MODE  GST_BASE_EBUTTD_OVERLAY_WRAP_MODE_WORD_CHAR
#define DEFAULT_PROP_FONT_DESC	""
#define DEFAULT_PROP_SILENT	FALSE
#define DEFAULT_PROP_LINE_ALIGNMENT GST_BASE_EBUTTD_OVERLAY_LINE_ALIGN_CENTER
#define DEFAULT_PROP_WAIT_TEXT	TRUE
#define DEFAULT_PROP_AUTO_ADJUST_SIZE TRUE
#define DEFAULT_PROP_VERTICAL_RENDER  FALSE
#define DEFAULT_PROP_COLOR      0xffffffff
#define DEFAULT_PROP_OUTLINE_COLOR 0xff000000
#define DEFAULT_PROP_LINE_PADDING 0     /* P TAYLOUR */
#define DEFAULT_PROP_BACKGROUND_YPAD 5

#define DEFAULT_PROP_CELL_RESOLUTION_X 40
#define DEFAULT_PROP_CELL_RESOLUTION_Y 24

#define DEFAULT_PROP_SHADING_VALUE    80

#define MINIMUM_OUTLINE_OFFSET 1.0
#define DEFAULT_SCALE_BASIS    1024

enum
{
  PROP_0,
  PROP_TEXT,
  PROP_SHADING,
  PROP_SHADING_VALUE,
  PROP_HALIGNMENT,
  PROP_VALIGNMENT,
  PROP_XPAD,
  PROP_YPAD,
  PROP_DELTAX,
  PROP_DELTAY,
  PROP_XPOS,
  PROP_YPOS,
  PROP_WRAP_MODE,
  PROP_FONT_DESC,
  PROP_SILENT,
  PROP_LINE_ALIGNMENT,
  PROP_WAIT_TEXT,
  PROP_AUTO_ADJUST_SIZE,
  PROP_VERTICAL_RENDER,
  PROP_COLOR,
  PROP_SHADOW,
  PROP_OUTLINE_COLOR,
  PROP_LINE_PADDING,            /* P TAYLOUR */
  PROP_LAST
};

#define VIDEO_FORMATS GST_VIDEO_OVERLAY_COMPOSITION_BLEND_FORMATS

#define BASE_EBUTTD_OVERLAY_CAPS GST_VIDEO_CAPS_MAKE (VIDEO_FORMATS)

#define BASE_EBUTTD_OVERLAY_ALL_CAPS BASE_EBUTTD_OVERLAY_CAPS ";" \
    GST_VIDEO_CAPS_MAKE_WITH_FEATURES ("ANY", GST_VIDEO_FORMATS_ALL)

static GstStaticCaps sw_template_caps =
GST_STATIC_CAPS (BASE_EBUTTD_OVERLAY_CAPS);

static GstStaticPadTemplate src_template_factory =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (BASE_EBUTTD_OVERLAY_ALL_CAPS)
    );

static GstStaticPadTemplate video_sink_template_factory =
GST_STATIC_PAD_TEMPLATE ("video_sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (BASE_EBUTTD_OVERLAY_ALL_CAPS)
    );

#define GST_TYPE_BASE_EBUTTD_OVERLAY_VALIGN (gst_base_ebuttd_overlay_valign_get_type())
static GType
gst_base_ebuttd_overlay_valign_get_type (void)
{
  static GType base_ebuttd_overlay_valign_type = 0;
  static const GEnumValue base_ebuttd_overlay_valign[] = {
    {GST_BASE_EBUTTD_OVERLAY_VALIGN_BASELINE, "baseline", "baseline"},
    {GST_BASE_EBUTTD_OVERLAY_VALIGN_BOTTOM, "bottom", "bottom"},
    {GST_BASE_EBUTTD_OVERLAY_VALIGN_TOP, "top", "top"},
    {GST_BASE_EBUTTD_OVERLAY_VALIGN_POS, "position", "position"},
    {GST_BASE_EBUTTD_OVERLAY_VALIGN_CENTER, "center", "center"},
    {0, NULL, NULL},
  };

  if (!base_ebuttd_overlay_valign_type) {
    base_ebuttd_overlay_valign_type =
        g_enum_register_static ("GstBaseEbuttdOverlayVAlign",
        base_ebuttd_overlay_valign);
  }
  return base_ebuttd_overlay_valign_type;
}

#define GST_TYPE_BASE_EBUTTD_OVERLAY_HALIGN (gst_base_ebuttd_overlay_halign_get_type())
static GType
gst_base_ebuttd_overlay_halign_get_type (void)
{
  static GType base_ebuttd_overlay_halign_type = 0;
  static const GEnumValue base_ebuttd_overlay_halign[] = {
    {GST_BASE_EBUTTD_OVERLAY_HALIGN_LEFT, "left", "left"},
    {GST_BASE_EBUTTD_OVERLAY_HALIGN_CENTER, "center", "center"},
    {GST_BASE_EBUTTD_OVERLAY_HALIGN_RIGHT, "right", "right"},
    {GST_BASE_EBUTTD_OVERLAY_HALIGN_POS, "position", "position"},
    {0, NULL, NULL},
  };

  if (!base_ebuttd_overlay_halign_type) {
    base_ebuttd_overlay_halign_type =
        g_enum_register_static ("GstBaseEbuttdOverlayHAlign",
        base_ebuttd_overlay_halign);
  }
  return base_ebuttd_overlay_halign_type;
}


#define GST_TYPE_BASE_EBUTTD_OVERLAY_WRAP_MODE (gst_base_ebuttd_overlay_wrap_mode_get_type())
static GType
gst_base_ebuttd_overlay_wrap_mode_get_type (void)
{
  static GType base_ebuttd_overlay_wrap_mode_type = 0;
  static const GEnumValue base_ebuttd_overlay_wrap_mode[] = {
    {GST_BASE_EBUTTD_OVERLAY_WRAP_MODE_NONE, "none", "none"},
    {GST_BASE_EBUTTD_OVERLAY_WRAP_MODE_WORD, "word", "word"},
    {GST_BASE_EBUTTD_OVERLAY_WRAP_MODE_CHAR, "char", "char"},
    {GST_BASE_EBUTTD_OVERLAY_WRAP_MODE_WORD_CHAR, "wordchar", "wordchar"},
    {0, NULL, NULL},
  };

  if (!base_ebuttd_overlay_wrap_mode_type) {
    base_ebuttd_overlay_wrap_mode_type =
        g_enum_register_static ("GstBaseEbuttdOverlayWrapMode",
        base_ebuttd_overlay_wrap_mode);
  }
  return base_ebuttd_overlay_wrap_mode_type;
}

#define GST_TYPE_BASE_EBUTTD_OVERLAY_LINE_ALIGN (gst_base_ebuttd_overlay_line_align_get_type())
static GType
gst_base_ebuttd_overlay_line_align_get_type (void)
{
  static GType base_ebuttd_overlay_line_align_type = 0;
  static const GEnumValue base_ebuttd_overlay_line_align[] = {
    {GST_BASE_EBUTTD_OVERLAY_LINE_ALIGN_LEFT, "left", "left"},
    {GST_BASE_EBUTTD_OVERLAY_LINE_ALIGN_CENTER, "center", "center"},
    {GST_BASE_EBUTTD_OVERLAY_LINE_ALIGN_RIGHT, "right", "right"},
    {0, NULL, NULL}
  };

  if (!base_ebuttd_overlay_line_align_type) {
    base_ebuttd_overlay_line_align_type =
        g_enum_register_static ("GstBaseEbuttdOverlayLineAlign",
        base_ebuttd_overlay_line_align);
  }
  return base_ebuttd_overlay_line_align_type;
}

#define GST_BASE_EBUTTD_OVERLAY_GET_LOCK(ov) (&GST_BASE_EBUTTD_OVERLAY (ov)->lock)
#define GST_BASE_EBUTTD_OVERLAY_GET_COND(ov) (&GST_BASE_EBUTTD_OVERLAY (ov)->cond)
#define GST_BASE_EBUTTD_OVERLAY_LOCK(ov)     (g_mutex_lock (GST_BASE_EBUTTD_OVERLAY_GET_LOCK (ov)))
#define GST_BASE_EBUTTD_OVERLAY_UNLOCK(ov)   (g_mutex_unlock (GST_BASE_EBUTTD_OVERLAY_GET_LOCK (ov)))
#define GST_BASE_EBUTTD_OVERLAY_WAIT(ov)     (g_cond_wait (GST_BASE_EBUTTD_OVERLAY_GET_COND (ov), GST_BASE_EBUTTD_OVERLAY_GET_LOCK (ov)))
#define GST_BASE_EBUTTD_OVERLAY_SIGNAL(ov)   (g_cond_signal (GST_BASE_EBUTTD_OVERLAY_GET_COND (ov)))
#define GST_BASE_EBUTTD_OVERLAY_BROADCAST(ov)(g_cond_broadcast (GST_BASE_EBUTTD_OVERLAY_GET_COND (ov)))

static GstElementClass *parent_class = NULL;
static void gst_base_ebuttd_overlay_base_init (gpointer g_class);
static void gst_base_ebuttd_overlay_class_init (GstBaseEbuttdOverlayClass * klass);
static void gst_base_ebuttd_overlay_init (GstBaseEbuttdOverlay * overlay,
    GstBaseEbuttdOverlayClass * klass);

static GstStateChangeReturn gst_base_ebuttd_overlay_change_state (GstElement *
    element, GstStateChange transition);

static GstCaps *gst_base_ebuttd_overlay_get_videosink_caps (GstPad * pad,
    GstBaseEbuttdOverlay * overlay, GstCaps * filter);
static GstCaps *gst_base_ebuttd_overlay_get_src_caps (GstPad * pad,
    GstBaseEbuttdOverlay * overlay, GstCaps * filter);
static gboolean gst_base_ebuttd_overlay_setcaps (GstBaseEbuttdOverlay * overlay,
    GstCaps * caps);
static gboolean gst_base_ebuttd_overlay_setcaps_txt (GstBaseEbuttdOverlay * overlay,
    GstCaps * caps);
static gboolean gst_base_ebuttd_overlay_src_event (GstPad * pad,
    GstObject * parent, GstEvent * event);
static gboolean gst_base_ebuttd_overlay_src_query (GstPad * pad,
    GstObject * parent, GstQuery * query);

static gboolean gst_base_ebuttd_overlay_video_event (GstPad * pad,
    GstObject * parent, GstEvent * event);
static gboolean gst_base_ebuttd_overlay_video_query (GstPad * pad,
    GstObject * parent, GstQuery * query);
static GstFlowReturn gst_base_ebuttd_overlay_video_chain (GstPad * pad,
    GstObject * parent, GstBuffer * buffer);

static gboolean gst_base_ebuttd_overlay_text_event (GstPad * pad,
    GstObject * parent, GstEvent * event);
static GstFlowReturn gst_base_ebuttd_overlay_text_chain (GstPad * pad,
    GstObject * parent, GstBuffer * buffer);
static GstPadLinkReturn gst_base_ebuttd_overlay_text_pad_link (GstPad * pad,
    GstObject * parent, GstPad * peer);
static void gst_base_ebuttd_overlay_text_pad_unlink (GstPad * pad,
    GstObject * parent);
static void gst_base_ebuttd_overlay_pop_text (GstBaseEbuttdOverlay * overlay);
static void gst_base_ebuttd_overlay_update_render_mode (GstBaseEbuttdOverlay *
    overlay);

static void gst_base_ebuttd_overlay_finalize (GObject * object);
static void gst_base_ebuttd_overlay_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_base_ebuttd_overlay_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static void
gst_base_ebuttd_overlay_adjust_values_with_fontdesc (GstBaseEbuttdOverlay * overlay,
    PangoFontDescription * desc);
static gboolean gst_base_ebuttd_overlay_can_handle_caps (GstCaps * incaps);

/* P TAYLOUR */
void set_non_pango_markup (gchar ** text, GstBaseEbuttdOverlay * overlay);
void convert_from_ebutt_to_pango (gchar ** text, GstBaseEbuttdOverlay * overlay);

gchar *extract_style_then_remove (gchar * property, gchar ** text);
void add_pango_style (gchar * property, gchar * value, gchar ** text);

static gboolean gst_text_overlay_filter_foreground_attr (PangoAttribute * attr,
    gpointer data);

static GstBaseEbuttdOverlayRegion * gst_base_ebuttd_overlay_region_new (
    GstBaseEbuttdOverlay * overlay, gdouble x, gdouble y, gdouble w, gdouble h,
    const gchar * text, PangoContext * context, gint line_padding);

GType
gst_base_ebuttd_overlay_get_type (void)
{
  static GType type = 0;

  if (g_once_init_enter ((gsize *) & type)) {
    static const GTypeInfo info = {
      sizeof (GstBaseEbuttdOverlayClass),
      (GBaseInitFunc) gst_base_ebuttd_overlay_base_init,
      NULL,
      (GClassInitFunc) gst_base_ebuttd_overlay_class_init,
      NULL,
      NULL,
      sizeof (GstBaseEbuttdOverlay),
      0,
      (GInstanceInitFunc) gst_base_ebuttd_overlay_init,
    };

    g_once_init_leave ((gsize *) & type,
        g_type_register_static (GST_TYPE_ELEMENT, "GstBaseEbuttdOverlay", &info,
            0));
  }

  return type;
}

static gchar *
gst_base_ebuttd_overlay_get_text (GstBaseEbuttdOverlay * overlay,
    GstBuffer * video_frame)
{
  return g_strdup (overlay->default_text);
}

static void
gst_base_ebuttd_overlay_base_init (gpointer g_class)
{
  GstBaseEbuttdOverlayClass *klass = GST_BASE_EBUTTD_OVERLAY_CLASS (g_class);
  PangoFontMap *fontmap;

  /* Only lock for the subclasses here, the base class
   * doesn't have this mutex yet and it's not necessary
   * here */
  if (klass->pango_lock)
    g_mutex_lock (klass->pango_lock);
  fontmap = pango_cairo_font_map_get_default ();
  klass->pango_context =
      pango_font_map_create_context (PANGO_FONT_MAP (fontmap));
  if (klass->pango_lock)
    g_mutex_unlock (klass->pango_lock);
}

static void
gst_base_ebuttd_overlay_class_init (GstBaseEbuttdOverlayClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  parent_class = g_type_class_peek_parent (klass);

  gobject_class->finalize = gst_base_ebuttd_overlay_finalize;
  gobject_class->set_property = gst_base_ebuttd_overlay_set_property;
  gobject_class->get_property = gst_base_ebuttd_overlay_get_property;

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&src_template_factory));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&video_sink_template_factory));

  gstelement_class->change_state =
      GST_DEBUG_FUNCPTR (gst_base_ebuttd_overlay_change_state);

  klass->pango_lock = g_slice_new (GMutex);
  g_mutex_init (klass->pango_lock);

  klass->get_text = gst_base_ebuttd_overlay_get_text;

  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_TEXT,
      g_param_spec_string ("text", "text",
          "Text to be display.", DEFAULT_PROP_TEXT,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_SHADING,
      g_param_spec_boolean ("shaded-background", "shaded background",
          "Whether to shade the background under the text area",
          DEFAULT_PROP_SHADING, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_SHADING_VALUE,
      g_param_spec_uint ("shading-value", "background shading value",
          "Shading value to apply if shaded-background is true", 1, 255,
          DEFAULT_PROP_SHADING_VALUE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_VALIGNMENT,
      g_param_spec_enum ("valignment", "vertical alignment",
          "Vertical alignment of the text", GST_TYPE_BASE_EBUTTD_OVERLAY_VALIGN,
          DEFAULT_PROP_VALIGNMENT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_HALIGNMENT,
      g_param_spec_enum ("halignment", "horizontal alignment",
          "Horizontal alignment of the text", GST_TYPE_BASE_EBUTTD_OVERLAY_HALIGN,
          DEFAULT_PROP_HALIGNMENT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_XPAD,
      g_param_spec_int ("xpad", "horizontal paddding",
          "Horizontal paddding when using left/right alignment", 0, G_MAXINT,
          DEFAULT_PROP_XPAD, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_YPAD,
      g_param_spec_int ("ypad", "vertical padding",
          "Vertical padding when using top/bottom alignment", 0, G_MAXINT,
          DEFAULT_PROP_YPAD, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_DELTAX,
      g_param_spec_int ("deltax", "X position modifier",
          "Shift X position to the left or to the right. Unit is pixels.",
          G_MININT, G_MAXINT, DEFAULT_PROP_DELTAX,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_DELTAY,
      g_param_spec_int ("deltay", "Y position modifier",
          "Shift Y position up or down. Unit is pixels.", G_MININT, G_MAXINT,
          DEFAULT_PROP_DELTAY, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  /**
   * GstBaseEbuttdOverlay:xpos:
   *
   * Horizontal position of the rendered text when using positioned alignment.
   */
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_XPOS,
      g_param_spec_double ("xpos", "horizontal position",
          "Horizontal position when using position alignment", 0, 1.0,
          DEFAULT_PROP_XPOS,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
  /**
   * GstBaseEbuttdOverlay:ypos:
   *
   * Vertical position of the rendered text when using positioned alignment.
   */
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_YPOS,
      g_param_spec_double ("ypos", "vertical position",
          "Vertical position when using position alignment", 0, 1.0,
          DEFAULT_PROP_YPOS,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_WRAP_MODE,
      g_param_spec_enum ("wrap-mode", "wrap mode",
          "Whether to wrap the text and if so how.",
          GST_TYPE_BASE_EBUTTD_OVERLAY_WRAP_MODE, DEFAULT_PROP_WRAP_MODE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_FONT_DESC,
      g_param_spec_string ("font-desc", "font description",
          "Pango font description of font to be used for rendering. "
          "See documentation of pango_font_description_from_string "
          "for syntax.", DEFAULT_PROP_FONT_DESC,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  /**
   * GstBaseEbuttdOverlay:color:
   *
   * Color of the rendered text.
   */
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_COLOR,
      g_param_spec_uint ("color", "Color",
          "Color to use for text (big-endian ARGB).", 0, G_MAXUINT32,
          DEFAULT_PROP_COLOR,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
  /**
   * GstTextOverlay:outline-color:
   *
   * Color of the outline of the rendered text.
   */
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_OUTLINE_COLOR,
      g_param_spec_uint ("outline-color", "Text Outline Color",
          "Color to use for outline the text (big-endian ARGB).", 0,
          G_MAXUINT32, DEFAULT_PROP_OUTLINE_COLOR,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));

  /**
   * GstBaseEbuttdOverlay:line-alignment:
   *
   * Alignment of text lines relative to each other (for multi-line text)
   */
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_LINE_ALIGNMENT,
      g_param_spec_enum ("line-alignment", "line alignment",
          "Alignment of text lines relative to each other.",
          GST_TYPE_BASE_EBUTTD_OVERLAY_LINE_ALIGN, DEFAULT_PROP_LINE_ALIGNMENT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  /**
   * GstBaseEbuttdOverlay:silent:
   *
   * If set, no text is rendered. Useful to switch off text rendering
   * temporarily without removing the textoverlay element from the pipeline.
   */
  /* FIXME 0.11: rename to "visible" or "text-visible" or "render-text" */
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_SILENT,
      g_param_spec_boolean ("silent", "silent",
          "Whether to render the text string",
          DEFAULT_PROP_SILENT,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
  /**
   * GstBaseEbuttdOverlay:wait-text:
   *
   * If set, the video will block until a subtitle is received on the text pad.
   * If video and subtitles are sent in sync, like from the same demuxer, this
   * property should be set.
   */
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_WAIT_TEXT,
      g_param_spec_boolean ("wait-text", "Wait Text",
          "Whether to wait for subtitles",
          DEFAULT_PROP_WAIT_TEXT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (G_OBJECT_CLASS (klass),
      PROP_AUTO_ADJUST_SIZE, g_param_spec_boolean ("auto-resize", "auto resize",
          "Automatically adjust font size to screen-size.",
          DEFAULT_PROP_AUTO_ADJUST_SIZE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_VERTICAL_RENDER,
      g_param_spec_boolean ("vertical-render", "vertical render",
          "Vertical Render.", DEFAULT_PROP_VERTICAL_RENDER,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /* P TAYLOUR */
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_LINE_PADDING,
      g_param_spec_int ("line-padding", "line padding",
          "Line Padding.",
          G_MININT, G_MAXINT, DEFAULT_PROP_LINE_PADDING,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
gst_base_ebuttd_overlay_finalize (GObject * object)
{
  GstBaseEbuttdOverlay *overlay = GST_BASE_EBUTTD_OVERLAY (object);

  g_free (overlay->default_text);

  if (overlay->composition) {
    gst_video_overlay_composition_unref (overlay->composition);
    overlay->composition = NULL;
  }

  if (overlay->text_image) {
    gst_buffer_unref (overlay->text_image);
    overlay->text_image = NULL;
  }

  if (overlay->layout) {
    g_object_unref (overlay->layout);
    overlay->layout = NULL;
  }

  if (overlay->text_buffer) {
    gst_buffer_unref (overlay->text_buffer);
    overlay->text_buffer = NULL;
  }

  g_mutex_clear (&overlay->lock);
  g_cond_clear (&overlay->cond);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_base_ebuttd_overlay_init (GstBaseEbuttdOverlay * overlay,
    GstBaseEbuttdOverlayClass * klass)
{
  GstPadTemplate *template;
  PangoFontDescription *desc;

  /* video sink */
  template = gst_static_pad_template_get (&video_sink_template_factory);
  overlay->video_sinkpad = gst_pad_new_from_template (template, "video_sink");
  gst_object_unref (template);
  gst_pad_set_event_function (overlay->video_sinkpad,
      GST_DEBUG_FUNCPTR (gst_base_ebuttd_overlay_video_event));
  gst_pad_set_chain_function (overlay->video_sinkpad,
      GST_DEBUG_FUNCPTR (gst_base_ebuttd_overlay_video_chain));
  gst_pad_set_query_function (overlay->video_sinkpad,
      GST_DEBUG_FUNCPTR (gst_base_ebuttd_overlay_video_query));
  GST_PAD_SET_PROXY_ALLOCATION (overlay->video_sinkpad);
  gst_element_add_pad (GST_ELEMENT (overlay), overlay->video_sinkpad);

  template =
      gst_element_class_get_pad_template (GST_ELEMENT_CLASS (klass),
      "text_sink");
  if (template) {
    /* text sink */
    overlay->text_sinkpad = gst_pad_new_from_template (template, "text_sink");

    gst_pad_set_event_function (overlay->text_sinkpad,
        GST_DEBUG_FUNCPTR (gst_base_ebuttd_overlay_text_event));
    gst_pad_set_chain_function (overlay->text_sinkpad,
        GST_DEBUG_FUNCPTR (gst_base_ebuttd_overlay_text_chain));
    gst_pad_set_link_function (overlay->text_sinkpad,
        GST_DEBUG_FUNCPTR (gst_base_ebuttd_overlay_text_pad_link));
    gst_pad_set_unlink_function (overlay->text_sinkpad,
        GST_DEBUG_FUNCPTR (gst_base_ebuttd_overlay_text_pad_unlink));
    gst_element_add_pad (GST_ELEMENT (overlay), overlay->text_sinkpad);
  }

  /* (video) source */
  template = gst_static_pad_template_get (&src_template_factory);
  overlay->srcpad = gst_pad_new_from_template (template, "src");
  gst_object_unref (template);
  gst_pad_set_event_function (overlay->srcpad,
      GST_DEBUG_FUNCPTR (gst_base_ebuttd_overlay_src_event));
  gst_pad_set_query_function (overlay->srcpad,
      GST_DEBUG_FUNCPTR (gst_base_ebuttd_overlay_src_query));
  gst_element_add_pad (GST_ELEMENT (overlay), overlay->srcpad);

  g_mutex_lock (GST_BASE_EBUTTD_OVERLAY_GET_CLASS (overlay)->pango_lock);
  overlay->line_align = DEFAULT_PROP_LINE_ALIGNMENT;
  overlay->layout =
      pango_layout_new (GST_BASE_EBUTTD_OVERLAY_GET_CLASS
      (overlay)->pango_context);
  desc =
      pango_context_get_font_description (GST_BASE_EBUTTD_OVERLAY_GET_CLASS
      (overlay)->pango_context);
  gst_base_ebuttd_overlay_adjust_values_with_fontdesc (overlay, desc);

  overlay->color = DEFAULT_PROP_COLOR;
  overlay->outline_color = DEFAULT_PROP_OUTLINE_COLOR;
  overlay->halign = DEFAULT_PROP_HALIGNMENT;
  overlay->valign = DEFAULT_PROP_VALIGNMENT;
  overlay->xpad = DEFAULT_PROP_XPAD;
  overlay->ypad = DEFAULT_PROP_YPAD;
  overlay->deltax = DEFAULT_PROP_DELTAX;
  overlay->deltay = DEFAULT_PROP_DELTAY;
  overlay->xpos = DEFAULT_PROP_XPOS;
  overlay->ypos = DEFAULT_PROP_YPOS;

  overlay->wrap_mode = DEFAULT_PROP_WRAP_MODE;

  overlay->want_shading = DEFAULT_PROP_SHADING;
  overlay->shading_value = DEFAULT_PROP_SHADING_VALUE;
  overlay->silent = DEFAULT_PROP_SILENT;
  overlay->wait_text = DEFAULT_PROP_WAIT_TEXT;
  overlay->auto_adjust_size = DEFAULT_PROP_AUTO_ADJUST_SIZE;

  overlay->default_text = g_strdup (DEFAULT_PROP_TEXT);
  overlay->need_render = TRUE;
  overlay->text_image = NULL;
  overlay->background_image = NULL;
  overlay->use_vertical_render = DEFAULT_PROP_VERTICAL_RENDER;
  gst_base_ebuttd_overlay_update_render_mode (overlay);

  overlay->text_buffer = NULL;
  overlay->text_linked = FALSE;

  /* P TAYLOUR */
  overlay->line_padding = DEFAULT_PROP_LINE_PADDING;
  overlay->background_ypad = DEFAULT_PROP_BACKGROUND_YPAD;

  overlay->layers = NULL;

  g_mutex_init (&overlay->lock);
  g_cond_init (&overlay->cond);
  gst_segment_init (&overlay->segment, GST_FORMAT_TIME);
  g_mutex_unlock (GST_BASE_EBUTTD_OVERLAY_GET_CLASS (overlay)->pango_lock);
}


void gst_base_ebuttd_overlay_region_compose (
    GstBaseEbuttdOverlayRegion * region,
    GstVideoOverlayComposition * composition)
{
  GstVideoOverlayRectangle *text_rectangle = NULL;
  GstVideoOverlayRectangle *bg_rectangle= NULL;

  g_return_if_fail (region != NULL);
  g_return_if_fail (composition != NULL);

  if (region->text_image) {
    g_print ("Adding text image...\n");
    g_print ("x: %d  y: %d  w: %u  h: %u, buffer-size: %u\n",
        (gint) region->origin_x, (gint) region->origin_y,
        (guint) region->extent_w, (guint) region->extent_h,
        gst_buffer_get_size (region->text_image));

    g_assert (gst_buffer_is_writable (region->text_image));
    gst_buffer_add_video_meta (region->text_image,
        GST_VIDEO_FRAME_FLAG_NONE, GST_VIDEO_OVERLAY_COMPOSITION_FORMAT_RGB,
        (guint) region->extent_w, (guint) region->extent_h);

    text_rectangle = gst_video_overlay_rectangle_new_raw (
        region->text_image,
        (gint) region->origin_x, (gint) region->origin_y,
        (guint) region->extent_w, (guint) region->extent_h,
        GST_VIDEO_OVERLAY_FORMAT_FLAG_PREMULTIPLIED_ALPHA);
  }

  if (region->bg_image) {
    g_print ("Adding background image...\n");
    g_print ("x: %d  y: %d  w: %u  h: %u, buffer-size: %u\n",
        (gint) region->x_bk, (gint) region->y_bk,
        (guint) region->width_bk, (guint) region->height_bk,
        gst_buffer_get_size (region->bg_image));

    g_assert (gst_buffer_is_writable (region->bg_image));
    gst_buffer_add_video_meta (region->bg_image,
        GST_VIDEO_FRAME_FLAG_NONE, GST_VIDEO_OVERLAY_COMPOSITION_FORMAT_RGB,
        (guint) region->width_bk, (guint) region->height_bk);

    bg_rectangle = gst_video_overlay_rectangle_new_raw (
        region->bg_image,
        (gint) region->x_bk, (gint) region->y_bk,
        (guint) region->width_bk, (guint) region->height_bk,
        GST_VIDEO_OVERLAY_FORMAT_FLAG_PREMULTIPLIED_ALPHA);
  }

  gst_video_overlay_composition_add_rectangle (composition, bg_rectangle);
  gst_video_overlay_composition_add_rectangle (composition, text_rectangle);
}

#if 0
static GstBaseEbuttdOverlayRegion *
gst_base_ebuttd_overlay_region_new (GstBaseEbuttdOverlay * overlay, gdouble x,
    gdouble y, gdouble w, gdouble h, const gchar * text, PangoContext *
    context, gint line_padding)
{
  GstBaseEbuttdOverlayRegion *region;
  gint textlen = strlen (text);
  gint width, height, width_bk, height_bk;
  PangoRectangle ink_rect, logical_rect;
  gdouble shadow_offset = 0.0;
  gdouble outline_offset = 0.0;
  cairo_t *cr, *cr_bk;
  cairo_surface_t *surface, *surface_bk;
  double scalef = 1.0;
  GstMapInfo map, map_bk;
  guint32 outline_color = 0x77777777;
  guint32 text_color = 0xffffffff;
  guint a, r, g, b;
  PangoAttrList *attr_list;
  PangoAttribute *bgcolor;
  const gchar *background_color = "#000000";
  PangoColor pango_color;

  g_return_val_if_fail (textlen < 256, NULL);

  g_print ("line_padding: %d\n", line_padding);

  region = g_new0 (GstBaseEbuttdOverlayRegion, 1);
  region->origin_x = x;
  region->origin_y = y;

  /************* Render text *************/
  region->layout = pango_layout_new (context);
  pango_layout_set_width (region->layout, -1);
  pango_layout_set_markup (region->layout, text, textlen);
  pango_layout_get_pixel_extents (region->layout, &ink_rect, &logical_rect);
  /*pango_layout_set_spacing (region->layout, PANGO_SCALE * 20);*/

  /*width = (logical_rect.width + shadow_offset) * scalef;*/
  width = logical_rect.width;
  /*height =
      (logical_rect.height + logical_rect.y + shadow_offset) * scalef;*/
  height = logical_rect.height;
  region->text_image = gst_buffer_new_allocate (NULL, 4 * width * height, NULL);
  g_assert (gst_buffer_is_writable (region->text_image));
  gst_buffer_map (region->text_image, &map, GST_MAP_READWRITE);
  surface = cairo_image_surface_create_for_data (map.data,
      CAIRO_FORMAT_ARGB32, width, height, width * 4);
  cr = cairo_create (surface);

  region->extent_w = width;
  region->extent_h = height;

  /* clear surface */
  cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
  cairo_paint (cr);

  cairo_set_operator (cr, CAIRO_OPERATOR_OVER);

  a = (outline_color >> 24) & 0xff;
  r = (outline_color >> 16) & 0xff;
  g = (outline_color >> 8) & 0xff;
  b = (outline_color >> 0) & 0xff;

  /* draw outline text */
#if 0
  cairo_save (cr);
  cairo_set_source_rgba (cr, r / 255.0, g / 255.0, b / 255.0, a / 255.0);
  cairo_set_line_width (cr, 4.0);
  pango_cairo_layout_path (cr, region->layout);
  cairo_stroke (cr);
  cairo_restore (cr);
#endif

  a = (text_color >> 24) & 0xff;
  r = (text_color >> 16) & 0xff;
  g = (text_color >> 8) & 0xff;
  b = (text_color >> 0) & 0xff;

  /*attr_list = pango_layout_get_attributes (region->layout);
  bgcolor = pango_attr_background_new (0, 0xffff, 0);
  pango_attr_list_change (attr_list, bgcolor);
  pango_layout_set_attributes (region->layout, attr_list);*/

  /* draw text */
  cairo_save (cr);
  g_print ("Layout text is: %s\n", pango_layout_get_text (region->layout));
  /*cairo_set_source_rgba (cr, r / 255.0, g / 255.0, b / 255.0, a / 255.0);*/
  pango_cairo_show_layout (cr, region->layout);
  cairo_restore (cr);

  cairo_destroy (cr);
  cairo_surface_destroy (surface);
  gst_buffer_unmap (region->text_image, &map);
  g_assert (gst_buffer_is_writable (region->text_image));

  /************* Render background *************/
  region->width_bk = width + 2 * line_padding;
  region->height_bk = height + 2 * DEFAULT_PROP_BACKGROUND_YPAD;
  region->x_bk = region->origin_x - line_padding;
  region->y_bk = region->origin_y - DEFAULT_PROP_BACKGROUND_YPAD;

  region->bg_image = gst_buffer_new_allocate (NULL, 4 * region->width_bk * region->height_bk, NULL);
  g_assert (gst_buffer_is_writable (region->bg_image));

  gst_buffer_map (region->bg_image, &map_bk, GST_MAP_READWRITE);
  surface_bk = cairo_image_surface_create_for_data (map_bk.data,
      CAIRO_FORMAT_ARGB32, region->width_bk, region->height_bk, region->width_bk * 4);
  cr_bk = cairo_create (surface_bk);

  /* clear surface */
  cairo_set_operator (cr_bk, CAIRO_OPERATOR_CLEAR);
  cairo_paint (cr_bk);

  cairo_set_operator (cr_bk, CAIRO_OPERATOR_OVER);

  /*
     If not in hex format. Assume no a and parse using pango.
     */
  /*if (background_color[0] != '#') {*/
    /* convert from CSS format. */
    /* convert to Pango rgb values */
    pango_color_parse (&pango_color, background_color);

    a = 255U;
    r = pango_color.red;
    g = pango_color.green;
    b = pango_color.blue;

    g_print ("r:%u g:%u b:%u\n", r, g, b);

  /*} else {
    guint hex_color;

    hex_color = DEFAULT_PROP_OUTLINE_COLOR;*/

    /* In hex form */
    /*a = (hex_color >> 24) & 0xff;
    r = (hex_color >> 16) & 0xff;
    g = (hex_color >> 8) & 0xff;
    b = (hex_color >> 0) & 0xff;
  }*/

  /* draw background */
  cairo_save (cr_bk);
  /* Components in pango_color seem to be alpha pre-multiplied. */
  cairo_set_source_rgba (cr_bk, r / (a * 255.0), g / (a * 255.0), b / (a * 255.0), a / 255.0);
  cairo_paint (cr_bk);
  cairo_restore (cr_bk);
  cairo_destroy (cr_bk);
  cairo_surface_destroy (surface_bk);
  gst_buffer_unmap (region->bg_image, &map_bk);
  g_assert (gst_buffer_is_writable (region->bg_image));

  overlay->regions = g_slist_append (overlay->regions, region);
  return region;
}
#endif


static void
gst_base_ebuttd_overlay_layer_free (GstBaseEbuttdOverlayLayer * layer)
{
  g_return_if_fail (layer != NULL);

  g_print ("Freeing layer %p...\n", layer);
  if (layer->image) {
    gst_buffer_unref (layer->image);
  }
  if (layer->rectangle) {
    gst_video_overlay_rectangle_unref (layer->rectangle);
  }
  g_free (layer);
}


static void
gst_base_ebuttd_overlay_region_free (GstBaseEbuttdOverlayRegion * region)
{
  g_return_if_fail (region != NULL);
  g_print ("Freeing region %p...\n", region);
  if (region->layout) {
    g_object_unref (region->layout);
  }
  if (region->text_image) {
    gst_buffer_unref (region->text_image);
  }
  if (region->bg_image) {
    gst_buffer_unref (region->bg_image);
  }
  g_free (region);
}


static void
gst_base_ebuttd_overlay_update_wrap_mode (GstBaseEbuttdOverlay * overlay)
{
  if (overlay->wrap_mode == GST_BASE_EBUTTD_OVERLAY_WRAP_MODE_NONE) {
    GST_DEBUG_OBJECT (overlay, "Set wrap mode NONE");
    pango_layout_set_width (overlay->layout, -1);
  } else {
    int width;

    if (overlay->auto_adjust_size) {
      width = DEFAULT_SCALE_BASIS * PANGO_SCALE;
      if (overlay->use_vertical_render) {
        width = width * (overlay->height - overlay->ypad * 2) / overlay->width;
      }
    } else {
      width =
          (overlay->use_vertical_render ? overlay->height : overlay->width) *
          PANGO_SCALE;
    }

    GST_DEBUG_OBJECT (overlay, "Set layout width %d", overlay->width);
    GST_DEBUG_OBJECT (overlay, "Set wrap mode    %d", overlay->wrap_mode);
    pango_layout_set_width (overlay->layout, width);
    pango_layout_set_wrap (overlay->layout, (PangoWrapMode) overlay->wrap_mode);
  }
}

static void
gst_base_ebuttd_overlay_update_render_mode (GstBaseEbuttdOverlay * overlay)
{
  PangoMatrix matrix = PANGO_MATRIX_INIT;
  PangoContext *context = pango_layout_get_context (overlay->layout);

  if (overlay->use_vertical_render) {
    pango_matrix_rotate (&matrix, -90);
    pango_context_set_base_gravity (context, PANGO_GRAVITY_AUTO);
    pango_context_set_matrix (context, &matrix);
    pango_layout_set_alignment (overlay->layout, PANGO_ALIGN_LEFT);
  } else {
    pango_context_set_base_gravity (context, PANGO_GRAVITY_SOUTH);
    pango_context_set_matrix (context, &matrix);
    pango_layout_set_alignment (overlay->layout,
        (PangoAlignment) overlay->line_align);
  }
}

static gboolean
gst_base_ebuttd_overlay_setcaps_txt (GstBaseEbuttdOverlay * overlay, GstCaps * caps)
{
  GstStructure *structure;
  const gchar *format;

  structure = gst_caps_get_structure (caps, 0);
  format = gst_structure_get_string (structure, "format");
  overlay->have_pango_markup = (strcmp (format, "pango-markup") == 0);

  return TRUE;
}

/* only negotiate/query video overlay composition support for now */
static gboolean
gst_base_ebuttd_overlay_negotiate (GstBaseEbuttdOverlay * overlay, GstCaps * caps)
{
  GstQuery *query;
  gboolean attach = FALSE;
  gboolean caps_has_meta = TRUE;
  gboolean ret;
  GstCapsFeatures *f;
  GstCaps *original_caps;
  gboolean original_has_meta = FALSE;
  gboolean allocation_ret = TRUE;

  GST_DEBUG_OBJECT (overlay, "performing negotiation");

  if (!caps)
    caps = gst_pad_get_current_caps (overlay->video_sinkpad);
  else
    gst_caps_ref (caps);

  if (!caps || gst_caps_is_empty (caps))
    goto no_format;

  original_caps = caps;

  /* Try to use the overlay meta if possible */
  f = gst_caps_get_features (caps, 0);

  /* if the caps doesn't have the overlay meta, we query if downstream
   * accepts it before trying the version without the meta
   * If upstream already is using the meta then we can only use it */
  if (!f
      || !gst_caps_features_contains (f,
          GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION)) {
    GstCaps *overlay_caps;

    /* In this case we added the meta, but we can work without it
     * so preserve the original caps so we can use it as a fallback */
    overlay_caps = gst_caps_copy (caps);

    f = gst_caps_get_features (overlay_caps, 0);
    gst_caps_features_add (f,
        GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION);

    ret = gst_pad_peer_query_accept_caps (overlay->srcpad, overlay_caps);
    GST_DEBUG_OBJECT (overlay, "Downstream accepts the overlay meta: %d", ret);
    if (ret) {
      gst_caps_unref (caps);
      caps = overlay_caps;

    } else {
      /* fallback to the original */
      gst_caps_unref (overlay_caps);
      caps_has_meta = FALSE;
    }
  } else {
    original_has_meta = TRUE;
  }
  GST_DEBUG_OBJECT (overlay, "Using caps %" GST_PTR_FORMAT, caps);
  ret = gst_pad_set_caps (overlay->srcpad, caps);

  if (ret) {
    /* find supported meta */
    query = gst_query_new_allocation (caps, FALSE);

    if (!gst_pad_peer_query (overlay->srcpad, query)) {
      /* no problem, we use the query defaults */
      GST_DEBUG_OBJECT (overlay, "ALLOCATION query failed");
      allocation_ret = FALSE;
    }

    if (caps_has_meta && gst_query_find_allocation_meta (query,
            GST_VIDEO_OVERLAY_COMPOSITION_META_API_TYPE, NULL))
      attach = TRUE;

    gst_query_unref (query);
  }

  overlay->attach_compo_to_buffer = attach;

  if (!allocation_ret && overlay->video_flushing) {
    ret = FALSE;
  } else if (original_caps && !original_has_meta && !attach) {
    if (caps_has_meta) {
      /* Some elements (fakesink) claim to accept the meta on caps but won't
         put it in the allocation query result, this leads below
         check to fail. Prevent this by removing the meta from caps */
      gst_caps_unref (caps);
      caps = gst_caps_ref (original_caps);
      ret = gst_pad_set_caps (overlay->srcpad, caps);
      if (ret && !gst_base_ebuttd_overlay_can_handle_caps (caps))
        ret = FALSE;
    }
  }

  if (!ret) {
    GST_DEBUG_OBJECT (overlay, "negotiation failed, schedule reconfigure");
    gst_pad_mark_reconfigure (overlay->srcpad);
  }

  gst_caps_unref (caps);

  return ret;

no_format:
  {
    if (caps)
      gst_caps_unref (caps);
    return FALSE;
  }
}

static gboolean
gst_base_ebuttd_overlay_can_handle_caps (GstCaps * incaps)
{
  gboolean ret;
  GstCaps *caps;
  static GstStaticCaps static_caps = GST_STATIC_CAPS (BASE_EBUTTD_OVERLAY_CAPS);

  caps = gst_static_caps_get (&static_caps);
  ret = gst_caps_is_subset (incaps, caps);
  gst_caps_unref (caps);

  return ret;
}

static gboolean
gst_base_ebuttd_overlay_setcaps (GstBaseEbuttdOverlay * overlay, GstCaps * caps)
{
  GstVideoInfo info;
  gboolean ret = FALSE;

  if (!gst_video_info_from_caps (&info, caps))
    goto invalid_caps;

  overlay->info = info;
  overlay->format = GST_VIDEO_INFO_FORMAT (&info);
  overlay->width = GST_VIDEO_INFO_WIDTH (&info);
  overlay->height = GST_VIDEO_INFO_HEIGHT (&info);

  ret = gst_base_ebuttd_overlay_negotiate (overlay, caps);

  GST_BASE_EBUTTD_OVERLAY_LOCK (overlay);
  g_mutex_lock (GST_BASE_EBUTTD_OVERLAY_GET_CLASS (overlay)->pango_lock);
  if (!overlay->attach_compo_to_buffer &&
      !gst_base_ebuttd_overlay_can_handle_caps (caps)) {
    GST_DEBUG_OBJECT (overlay, "unsupported caps %" GST_PTR_FORMAT, caps);
    ret = FALSE;
  }

  gst_base_ebuttd_overlay_update_wrap_mode (overlay);
  g_mutex_unlock (GST_BASE_EBUTTD_OVERLAY_GET_CLASS (overlay)->pango_lock);
  GST_BASE_EBUTTD_OVERLAY_UNLOCK (overlay);

  return ret;

  /* ERRORS */
invalid_caps:
  {
    GST_DEBUG_OBJECT (overlay, "could not parse caps");
    return FALSE;
  }
}

static void
gst_base_ebuttd_overlay_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstBaseEbuttdOverlay *overlay = GST_BASE_EBUTTD_OVERLAY (object);

  GST_BASE_EBUTTD_OVERLAY_LOCK (overlay);
  switch (prop_id) {
    case PROP_TEXT:
      g_free (overlay->default_text);
      overlay->default_text = g_value_dup_string (value);
      overlay->need_render = TRUE;
      break;
    case PROP_SHADING:
      overlay->want_shading = g_value_get_boolean (value);
      break;
    case PROP_XPAD:
      overlay->xpad = g_value_get_int (value);
      break;
    case PROP_YPAD:
      overlay->ypad = g_value_get_int (value);
      break;
    case PROP_DELTAX:
      overlay->deltax = g_value_get_int (value);
      break;
    case PROP_DELTAY:
      overlay->deltay = g_value_get_int (value);
      break;
    case PROP_XPOS:
      overlay->xpos = g_value_get_double (value);
      break;
    case PROP_YPOS:
      overlay->ypos = g_value_get_double (value);
      break;
    case PROP_VALIGNMENT:
      overlay->valign = g_value_get_enum (value);
      break;
    case PROP_HALIGNMENT:
      overlay->halign = g_value_get_enum (value);
      break;
    case PROP_WRAP_MODE:
      overlay->wrap_mode = g_value_get_enum (value);
      g_mutex_lock (GST_BASE_EBUTTD_OVERLAY_GET_CLASS (overlay)->pango_lock);
      gst_base_ebuttd_overlay_update_wrap_mode (overlay);
      g_mutex_unlock (GST_BASE_EBUTTD_OVERLAY_GET_CLASS (overlay)->pango_lock);
      break;
    case PROP_FONT_DESC:
    {
      PangoFontDescription *desc;
      const gchar *fontdesc_str;

      fontdesc_str = g_value_get_string (value);
      g_mutex_lock (GST_BASE_EBUTTD_OVERLAY_GET_CLASS (overlay)->pango_lock);
      desc = pango_font_description_from_string (fontdesc_str);
      if (desc) {
        GST_LOG_OBJECT (overlay, "font description set: %s", fontdesc_str);
        pango_layout_set_font_description (overlay->layout, desc);
        gst_base_ebuttd_overlay_adjust_values_with_fontdesc (overlay, desc);
        pango_font_description_free (desc);
      } else {
        GST_WARNING_OBJECT (overlay, "font description parse failed: %s",
            fontdesc_str);
      }
      g_mutex_unlock (GST_BASE_EBUTTD_OVERLAY_GET_CLASS (overlay)->pango_lock);
      break;
    }
    case PROP_COLOR:
      overlay->color = g_value_get_uint (value);
      break;
    case PROP_OUTLINE_COLOR:
      overlay->outline_color = g_value_get_uint (value);
      break;
    case PROP_SILENT:
      overlay->silent = g_value_get_boolean (value);
      break;
    case PROP_LINE_ALIGNMENT:
      overlay->line_align = g_value_get_enum (value);
      g_mutex_lock (GST_BASE_EBUTTD_OVERLAY_GET_CLASS (overlay)->pango_lock);
      pango_layout_set_alignment (overlay->layout,
          (PangoAlignment) overlay->line_align);
      g_mutex_unlock (GST_BASE_EBUTTD_OVERLAY_GET_CLASS (overlay)->pango_lock);
      break;
    case PROP_WAIT_TEXT:
      overlay->wait_text = g_value_get_boolean (value);
      break;
    case PROP_AUTO_ADJUST_SIZE:
      overlay->auto_adjust_size = g_value_get_boolean (value);
      overlay->need_render = TRUE;
      break;
    case PROP_VERTICAL_RENDER:
      overlay->use_vertical_render = g_value_get_boolean (value);
      g_mutex_lock (GST_BASE_EBUTTD_OVERLAY_GET_CLASS (overlay)->pango_lock);
      gst_base_ebuttd_overlay_update_render_mode (overlay);
      g_mutex_unlock (GST_BASE_EBUTTD_OVERLAY_GET_CLASS (overlay)->pango_lock);
      overlay->need_render = TRUE;
      break;
    case PROP_SHADING_VALUE:
      overlay->shading_value = g_value_get_uint (value);
      break;
    case PROP_LINE_PADDING:
      overlay->line_padding = g_value_get_int (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

  overlay->need_render = TRUE;
  GST_BASE_EBUTTD_OVERLAY_UNLOCK (overlay);
}

static void
gst_base_ebuttd_overlay_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstBaseEbuttdOverlay *overlay = GST_BASE_EBUTTD_OVERLAY (object);

  GST_BASE_EBUTTD_OVERLAY_LOCK (overlay);
  switch (prop_id) {
    case PROP_TEXT:
      g_value_set_string (value, overlay->default_text);
      break;
    case PROP_SHADING:
      g_value_set_boolean (value, overlay->want_shading);
      break;
    case PROP_XPAD:
      g_value_set_int (value, overlay->xpad);
      break;
    case PROP_YPAD:
      g_value_set_int (value, overlay->ypad);
      break;
    case PROP_DELTAX:
      g_value_set_int (value, overlay->deltax);
      break;
    case PROP_DELTAY:
      g_value_set_int (value, overlay->deltay);
      break;
    case PROP_XPOS:
      g_value_set_double (value, overlay->xpos);
      break;
    case PROP_YPOS:
      g_value_set_double (value, overlay->ypos);
      break;
    case PROP_VALIGNMENT:
      g_value_set_enum (value, overlay->valign);
      break;
    case PROP_HALIGNMENT:
      g_value_set_enum (value, overlay->halign);
      break;
    case PROP_WRAP_MODE:
      g_value_set_enum (value, overlay->wrap_mode);
      break;
    case PROP_SILENT:
      g_value_set_boolean (value, overlay->silent);
      break;
    case PROP_LINE_ALIGNMENT:
      g_value_set_enum (value, overlay->line_align);
      break;
    case PROP_WAIT_TEXT:
      g_value_set_boolean (value, overlay->wait_text);
      break;
    case PROP_AUTO_ADJUST_SIZE:
      g_value_set_boolean (value, overlay->auto_adjust_size);
      break;
    case PROP_VERTICAL_RENDER:
      g_value_set_boolean (value, overlay->use_vertical_render);
      break;
    case PROP_COLOR:
      g_value_set_uint (value, overlay->color);
      break;
    case PROP_OUTLINE_COLOR:
      g_value_set_uint (value, overlay->outline_color);
      break;
#if 1
    case PROP_LINE_PADDING:    /* P TAYLOUR */
      g_value_set_int (value, overlay->line_padding);
      break;
#endif
    case PROP_SHADING_VALUE:
      g_value_set_uint (value, overlay->shading_value);
      break;
    case PROP_FONT_DESC:
    {
      const PangoFontDescription *desc;

      g_mutex_lock (GST_BASE_EBUTTD_OVERLAY_GET_CLASS (overlay)->pango_lock);
      desc = pango_layout_get_font_description (overlay->layout);
      if (!desc)
        g_value_set_string (value, "");
      else {
        g_value_take_string (value, pango_font_description_to_string (desc));
      }
      g_mutex_unlock (GST_BASE_EBUTTD_OVERLAY_GET_CLASS (overlay)->pango_lock);
      break;
    }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

  overlay->need_render = TRUE;
  GST_BASE_EBUTTD_OVERLAY_UNLOCK (overlay);
}

static gboolean
gst_base_ebuttd_overlay_src_query (GstPad * pad, GstObject * parent,
    GstQuery * query)
{
  gboolean ret = FALSE;
  GstBaseEbuttdOverlay *overlay;

  overlay = GST_BASE_EBUTTD_OVERLAY (parent);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CAPS:
    {
      GstCaps *filter, *caps;

      gst_query_parse_caps (query, &filter);
      caps = gst_base_ebuttd_overlay_get_src_caps (pad, overlay, filter);
      gst_query_set_caps_result (query, caps);
      gst_caps_unref (caps);
      ret = TRUE;
      break;
    }
    default:
      ret = gst_pad_query_default (pad, parent, query);
      break;
  }

  return ret;
}

static gboolean
gst_base_ebuttd_overlay_src_event (GstPad * pad, GstObject * parent,
    GstEvent * event)
{
  GstBaseEbuttdOverlay *overlay;
  gboolean ret;

  overlay = GST_BASE_EBUTTD_OVERLAY (parent);

  if (overlay->text_linked) {
    gst_event_ref (event);
    ret = gst_pad_push_event (overlay->video_sinkpad, event);
    gst_pad_push_event (overlay->text_sinkpad, event);
  } else {
    ret = gst_pad_push_event (overlay->video_sinkpad, event);
  }

  return ret;
}

/**
 * gst_base_ebuttd_overlay_add_feature_and_intersect:
 *
 * Creates a new #GstCaps containing the (given caps +
 * given caps feature) + (given caps intersected by the
 * given filter).
 *
 * Returns: the new #GstCaps
 */
static GstCaps *
gst_base_ebuttd_overlay_add_feature_and_intersect (GstCaps * caps,
    const gchar * feature, GstCaps * filter)
{
  int i, caps_size;
  GstCaps *new_caps;

  new_caps = gst_caps_copy (caps);

  caps_size = gst_caps_get_size (new_caps);
  for (i = 0; i < caps_size; i++) {
    GstCapsFeatures *features = gst_caps_get_features (new_caps, i);

    if (!gst_caps_features_is_any (features)) {
      gst_caps_features_add (features, feature);
    }
  }

  gst_caps_append (new_caps, gst_caps_intersect_full (caps,
          filter, GST_CAPS_INTERSECT_FIRST));

  return new_caps;
}

/**
 * gst_base_ebuttd_overlay_intersect_by_feature:
 *
 * Creates a new #GstCaps based on the following filtering rule.
 *
 * For each individual caps contained in given caps, if the
 * caps uses the given caps feature, keep a version of the caps
 * with the feature and an another one without. Otherwise, intersect
 * the caps with the given filter.
 *
 * Returns: the new #GstCaps
 */
static GstCaps *
gst_base_ebuttd_overlay_intersect_by_feature (GstCaps * caps,
    const gchar * feature, GstCaps * filter)
{
  int i, caps_size;
  GstCaps *new_caps;

  new_caps = gst_caps_new_empty ();

  caps_size = gst_caps_get_size (caps);
  for (i = 0; i < caps_size; i++) {
    GstStructure *caps_structure = gst_caps_get_structure (caps, i);
    GstCapsFeatures *caps_features =
        gst_caps_features_copy (gst_caps_get_features (caps, i));
    GstCaps *filtered_caps;
    GstCaps *simple_caps =
        gst_caps_new_full (gst_structure_copy (caps_structure), NULL);
    gst_caps_set_features (simple_caps, 0, caps_features);

    if (gst_caps_features_contains (caps_features, feature)) {
      gst_caps_append (new_caps, gst_caps_copy (simple_caps));

      gst_caps_features_remove (caps_features, feature);
      filtered_caps = gst_caps_ref (simple_caps);
    } else {
      filtered_caps = gst_caps_intersect_full (simple_caps, filter,
          GST_CAPS_INTERSECT_FIRST);
    }

    gst_caps_unref (simple_caps);
    gst_caps_append (new_caps, filtered_caps);
  }

  return new_caps;
}

static GstCaps *
gst_base_ebuttd_overlay_get_videosink_caps (GstPad * pad,
    GstBaseEbuttdOverlay * overlay, GstCaps * filter)
{
  GstPad *srcpad = overlay->srcpad;
  GstCaps *peer_caps = NULL, *caps = NULL, *overlay_filter = NULL;

  if (G_UNLIKELY (!overlay))
    return gst_pad_get_pad_template_caps (pad);

  if (filter) {
    /* filter caps + composition feature + filter caps
     * filtered by the software caps. */
    GstCaps *sw_caps = gst_static_caps_get (&sw_template_caps);
    overlay_filter = gst_base_ebuttd_overlay_add_feature_and_intersect (filter,
        GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION, sw_caps);
    gst_caps_unref (sw_caps);

    GST_DEBUG_OBJECT (overlay, "overlay filter %" GST_PTR_FORMAT,
        overlay_filter);
  }

  peer_caps = gst_pad_peer_query_caps (srcpad, overlay_filter);

  if (overlay_filter)
    gst_caps_unref (overlay_filter);

  if (peer_caps) {

    GST_DEBUG_OBJECT (pad, "peer caps  %" GST_PTR_FORMAT, peer_caps);

    if (gst_caps_is_any (peer_caps)) {
      /* if peer returns ANY caps, return filtered src pad template caps */
      caps = gst_caps_copy (gst_pad_get_pad_template_caps (srcpad));
    } else {

      /* duplicate caps which contains the composition into one version with
       * the meta and one without. Filter the other caps by the software caps */
      GstCaps *sw_caps = gst_static_caps_get (&sw_template_caps);
      caps = gst_base_ebuttd_overlay_intersect_by_feature (peer_caps,
          GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION, sw_caps);
      gst_caps_unref (sw_caps);
    }

    gst_caps_unref (peer_caps);

  } else {
    /* no peer, our padtemplate is enough then */
    caps = gst_pad_get_pad_template_caps (pad);
  }

  if (filter) {
    GstCaps *intersection = gst_caps_intersect_full (filter, caps,
        GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref (caps);
    caps = intersection;
  }

  GST_DEBUG_OBJECT (overlay, "returning  %" GST_PTR_FORMAT, caps);

  return caps;
}

static GstCaps *
gst_base_ebuttd_overlay_get_src_caps (GstPad * pad, GstBaseEbuttdOverlay * overlay,
    GstCaps * filter)
{
  GstPad *sinkpad = overlay->video_sinkpad;
  GstCaps *peer_caps = NULL, *caps = NULL, *overlay_filter = NULL;

  if (G_UNLIKELY (!overlay))
    return gst_pad_get_pad_template_caps (pad);

  if (filter) {
    /* duplicate filter caps which contains the composition into one version
     * with the meta and one without. Filter the other caps by the software
     * caps */
    GstCaps *sw_caps = gst_static_caps_get (&sw_template_caps);
    overlay_filter =
        gst_base_ebuttd_overlay_intersect_by_feature (filter,
        GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION, sw_caps);
    gst_caps_unref (sw_caps);
  }

  peer_caps = gst_pad_peer_query_caps (sinkpad, overlay_filter);

  if (overlay_filter)
    gst_caps_unref (overlay_filter);

  if (peer_caps) {

    GST_DEBUG_OBJECT (pad, "peer caps  %" GST_PTR_FORMAT, peer_caps);

    if (gst_caps_is_any (peer_caps)) {

      /* if peer returns ANY caps, return filtered sink pad template caps */
      caps = gst_caps_copy (gst_pad_get_pad_template_caps (sinkpad));

    } else {

      /* return upstream caps + composition feature + upstream caps
       * filtered by the software caps. */
      GstCaps *sw_caps = gst_static_caps_get (&sw_template_caps);
      caps = gst_base_ebuttd_overlay_add_feature_and_intersect (peer_caps,
          GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION, sw_caps);
      gst_caps_unref (sw_caps);
    }

    gst_caps_unref (peer_caps);

  } else {
    /* no peer, our padtemplate is enough then */
    caps = gst_pad_get_pad_template_caps (pad);
  }

  if (filter) {
    GstCaps *intersection;

    intersection =
        gst_caps_intersect_full (filter, caps, GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref (caps);
    caps = intersection;
  }
  GST_DEBUG_OBJECT (overlay, "returning  %" GST_PTR_FORMAT, caps);

  return caps;
}

static void
gst_base_ebuttd_overlay_adjust_values_with_fontdesc (GstBaseEbuttdOverlay * overlay,
    PangoFontDescription * desc)
{
  gint font_size = pango_font_description_get_size (desc) / PANGO_SCALE;
  overlay->shadow_offset = (double) (font_size) / 13.0;
  overlay->outline_offset = (double) (font_size) / 15.0;
  if (overlay->outline_offset < MINIMUM_OUTLINE_OFFSET)
    overlay->outline_offset = MINIMUM_OUTLINE_OFFSET;
}

static void
gst_base_ebuttd_overlay_get_pos (GstBaseEbuttdOverlay * overlay,
    gint * xpos, gint * ypos)
{
  gint width, height;
  GstBaseEbuttdOverlayVAlign valign;
  GstBaseEbuttdOverlayHAlign halign;

  width = overlay->image_width;
  height = overlay->image_height;

  if (overlay->use_vertical_render)
    halign = GST_BASE_EBUTTD_OVERLAY_HALIGN_RIGHT;
  else
    halign = overlay->halign;

  switch (halign) {
    case GST_BASE_EBUTTD_OVERLAY_HALIGN_LEFT:
      *xpos = overlay->xpad;
      break;
    case GST_BASE_EBUTTD_OVERLAY_HALIGN_CENTER:
      *xpos = (overlay->width - width) / 2;
      break;
    case GST_BASE_EBUTTD_OVERLAY_HALIGN_RIGHT:
      *xpos = overlay->width - width - overlay->xpad;
      break;
    case GST_BASE_EBUTTD_OVERLAY_HALIGN_POS:
      *xpos = (gint) (overlay->width * overlay->xpos) - width / 2;
      *xpos = CLAMP (*xpos, 0, overlay->width - width);
      if (*xpos < 0)
        *xpos = 0;
      break;
    default:
      *xpos = 0;
  }
  *xpos += overlay->deltax;

  if (overlay->use_vertical_render)
    valign = GST_BASE_EBUTTD_OVERLAY_VALIGN_TOP;
  else
    valign = overlay->valign;

  switch (valign) {
    case GST_BASE_EBUTTD_OVERLAY_VALIGN_BOTTOM:
      *ypos = overlay->height - height - overlay->ypad;
      break;
    case GST_BASE_EBUTTD_OVERLAY_VALIGN_BASELINE:
      *ypos = overlay->height - (height + overlay->ypad);
      break;
    case GST_BASE_EBUTTD_OVERLAY_VALIGN_TOP:
      *ypos = overlay->ypad;
      break;
    case GST_BASE_EBUTTD_OVERLAY_VALIGN_POS:
      *ypos = (gint) (overlay->height * overlay->ypos) - height / 2;
      *ypos = CLAMP (*ypos, 0, overlay->height - height);
      break;
    case GST_BASE_EBUTTD_OVERLAY_VALIGN_CENTER:
      *ypos = (overlay->height - height) / 2;
      break;
    default:
      *ypos = overlay->ypad;
      break;
  }
  *ypos += overlay->deltay;
}

static inline void
gst_base_ebuttd_overlay_set_composition (GstBaseEbuttdOverlay * overlay)
{
  gint xpos, ypos;
  GstVideoOverlayRectangle *rectangle, *rectangle_bk = NULL;

  gst_base_ebuttd_overlay_get_pos (overlay, &xpos, &ypos);

  if (overlay->text_image) {
    g_assert (gst_buffer_is_writable (overlay->text_image));
    gst_buffer_add_video_meta (overlay->text_image, GST_VIDEO_FRAME_FLAG_NONE,
        GST_VIDEO_OVERLAY_COMPOSITION_FORMAT_RGB,
        overlay->image_width, overlay->image_height);
    rectangle = gst_video_overlay_rectangle_new_raw (overlay->text_image,
        xpos, ypos, overlay->image_width, overlay->image_height,
        GST_VIDEO_OVERLAY_FORMAT_FLAG_PREMULTIPLIED_ALPHA);

#if 0
    if (overlay->want_background) {
      g_assert (gst_buffer_is_writable (overlay->background_image));
      gst_buffer_add_video_meta (overlay->background_image,
          GST_VIDEO_FRAME_FLAG_NONE, GST_VIDEO_OVERLAY_COMPOSITION_FORMAT_RGB,
          overlay->image_width_bk, overlay->image_height_bk);
      rectangle_bk =
          gst_video_overlay_rectangle_new_raw (overlay->background_image,
          xpos - overlay->line_padding, ypos - overlay->background_ypad,
          overlay->image_width_bk, overlay->image_height_bk,
          GST_VIDEO_OVERLAY_FORMAT_FLAG_PREMULTIPLIED_ALPHA);
    }
#endif

    if (overlay->composition)
      gst_video_overlay_composition_unref (overlay->composition);

    overlay->composition = gst_video_overlay_composition_new (rectangle);
    /*overlay->composition = gst_video_overlay_composition_new (rectangle_bk);
    gst_video_overlay_composition_add_rectangle (overlay->composition,
        rectangle);*/

    /*g_slist_foreach (overlay->regions,
        (GFunc) gst_base_ebuttd_overlay_region_compose,
        overlay->composition);*/

    /* XXX:CB - Why unref the rectangle here? Documentation suggests none of the previously called functions take ownership of rectangle. */
    /*gst_video_overlay_rectangle_unref (rectangle);*/

  } else if (overlay->composition) {
    gst_video_overlay_composition_unref (overlay->composition);
    overlay->composition = NULL;
  }
}


static void
gst_base_ebuttd_overlay_compose_layers (GstBaseEbuttdOverlay * overlay,
    GSList * layers)
{
  GstBaseEbuttdOverlayLayer *layer = NULL;

  g_return_if_fail (overlay != NULL);
  g_return_if_fail (layers != NULL);

  if (overlay->composition)
    gst_video_overlay_composition_unref (overlay->composition);

  layer = (GstBaseEbuttdOverlayLayer *)layers->data;
  g_assert (layer != NULL);

  overlay->composition = gst_video_overlay_composition_new (layer->rectangle);

  while ((layers = g_slist_next (layers))) {
    layer = (GstBaseEbuttdOverlayLayer *)layers->data;
    g_assert (layer != NULL);
    gst_video_overlay_composition_add_rectangle (overlay->composition,
        layer->rectangle);
  }
}


#if 0
static inline void
gst_base_ebuttd_overlay_set_composition2 (GstBaseEbuttdOverlay * overlay,
    GstBaseEbuttdOverlayRegion * region)
{
  gint xpos, ypos;
  gint xpos_bk, ypos_bk;
  gint xpos_rg, ypos_rg;
  GstVideoOverlayRectangle *text_rectangle = NULL;
  GstVideoOverlayRectangle *bg_rectangle = NULL;
  GstVideoOverlayRectangle *region_rectangle = NULL;

  g_return_if_fail (overlay != NULL);
  g_return_if_fail (region != NULL);
  g_return_if_fail (region->text_image != NULL);

  g_print ("width: %d   height: %d\n", overlay->width, overlay->height);
  xpos_rg = (guint) ((region->origin_x * overlay->width) / 100.0);
  ypos_rg = (guint) ((region->origin_y * overlay->height) / 100.0);
  g_print ("xpos_bk: %d   ypos_bk: %d\n", xpos_bk, ypos_bk);
  xpos_bk = xpos_rg + region->padding_start;
  ypos_bk = ypos_rg + region->padding_before;

  xpos = xpos_bk + overlay->line_padding;
  ypos = ypos_bk;

  g_print ("Adding text image...\n");
  g_print ("x: %d  y: %d  w: %u  h: %u, buffer-size: %u\n",
      xpos, ypos,
      (guint) region->extent_w, (guint) region->extent_h,
      gst_buffer_get_size (region->text_image));

  g_assert (gst_buffer_is_writable (region->text_image));
  gst_buffer_add_video_meta (region->text_image,
      GST_VIDEO_FRAME_FLAG_NONE, GST_VIDEO_OVERLAY_COMPOSITION_FORMAT_RGB,
      (guint) region->extent_w, (guint) region->extent_h);

  text_rectangle = gst_video_overlay_rectangle_new_raw (
      region->text_image,
      xpos, ypos, (guint) region->extent_w, (guint) region->extent_h,
      GST_VIDEO_OVERLAY_FORMAT_FLAG_PREMULTIPLIED_ALPHA);

  if (region->bg_image) {
    g_print ("Adding background image...\n");
    g_print ("x: %d  y: %d  w: %u  h: %u, buffer-size: %u\n",
        xpos_bk, ypos_bk,
        (guint) region->width_bk, (guint) region->height_bk,
        gst_buffer_get_size (region->bg_image));

    g_assert (gst_buffer_is_writable (region->bg_image));
    gst_buffer_add_video_meta (region->bg_image,
        GST_VIDEO_FRAME_FLAG_NONE, GST_VIDEO_OVERLAY_COMPOSITION_FORMAT_RGB,
        (guint) region->width_bk, (guint) region->height_bk);

    bg_rectangle = gst_video_overlay_rectangle_new_raw (
        region->bg_image,
        xpos_bk, ypos_bk,
        (guint) region->width_bk, (guint) region->height_bk,
        GST_VIDEO_OVERLAY_FORMAT_FLAG_PREMULTIPLIED_ALPHA);
  }

  if (overlay->composition)
    gst_video_overlay_composition_unref (overlay->composition);

  if (region->bg_image) {
    overlay->composition = gst_video_overlay_composition_new (bg_rectangle);
    gst_video_overlay_composition_add_rectangle (overlay->composition,
        text_rectangle);
  } else {
    overlay->composition = gst_video_overlay_composition_new (text_rectangle);
  }
}
#endif


static gboolean
gst_text_overlay_filter_foreground_attr (PangoAttribute * attr, gpointer data)
{
  if (attr->klass->type == PANGO_ATTR_FOREGROUND) {
    return FALSE;
  } else {
    return TRUE;
  }
}

static void
gst_base_ebuttd_overlay_render_pangocairo (GstBaseEbuttdOverlay * overlay,
    const gchar * string, gint textlen)
{
  cairo_t *cr, *cr_bk;
  cairo_surface_t *surface, *surface_bk;
  PangoRectangle ink_rect, logical_rect;
  cairo_matrix_t cairo_matrix;
  int width, height, width_bk, height_bk;
  double scalef = 1.0;
  double a, r, g, b;
  GstBuffer *buffer, *buffer_bk;
  GstMapInfo map, map_bk;

  g_print ("Input string: %s\n", string);
  g_mutex_lock (GST_BASE_EBUTTD_OVERLAY_GET_CLASS (overlay)->pango_lock);

  if (overlay->auto_adjust_size) {
    /* 640 pixel is default
     * P Taylour: updated to
     */
    scalef = (double) (overlay->width) / DEFAULT_SCALE_BASIS;
  }
  pango_layout_set_width (overlay->layout, -1);
  /* set text on pango layout */
  pango_layout_set_markup (overlay->layout, string, textlen);

  /* get subtitle image size */
  pango_layout_get_pixel_extents (overlay->layout, &ink_rect, &logical_rect);
  g_print ("Pixel extents - w: %d  h: %d  x: %d  y: %d\n", logical_rect.width, logical_rect.height, logical_rect.x, logical_rect.y);

  /* apply scale to get the correct font_size */
  /* This bit added by PT. */
#if 0
  if (overlay->text_height_px) {
    double text_scalef;         /* fraction of required to actual text height */
    PangoLayoutLine *first_line;
    PangoRectangle ink_rect_line, logical_rect_line;

    first_line = pango_layout_get_line (overlay->layout, 0);

    pango_layout_line_get_pixel_extents (first_line,
        &ink_rect_line, &logical_rect_line);
    height = (ink_rect_line.height + overlay->shadow_offset) * scalef;  /* use glyph height here */

    /* we want to match the height of a line to text_height_px */
    text_scalef = overlay->text_height_px / ((double) height);

    /* transform by this scale factor to match height with text_height_px */
    scalef = scalef * text_scalef;      /* apply this scale to the other scale factor */
  }
#endif

  /* CB: Why is the width being scaled? Is it to fit some predeclared overlay size? */
  g_print ("shadow_offset: %f  scalef: %f\n", overlay->shadow_offset, scalef);
  width = (logical_rect.width + overlay->shadow_offset) * scalef;
  g_print ("width 1: %d\n", width);

  if (width + overlay->deltax >
      (overlay->use_vertical_render ? overlay->height : overlay->width)) {
    /*
     * subtitle image width is larger then overlay width
     * so rearrange overlay wrap mode.
     */
    gst_base_ebuttd_overlay_update_wrap_mode (overlay);
    pango_layout_get_pixel_extents (overlay->layout, &ink_rect, &logical_rect);
    width = overlay->width;
  }
  g_print ("width 2: %d\n", width);

  height =
      (logical_rect.height + logical_rect.y + overlay->shadow_offset) * scalef;
  if (height > overlay->height) {
    height = overlay->height;
  }

  width = logical_rect.width;
  height = logical_rect.height;



  if (overlay->use_vertical_render) {
    PangoRectangle rect;
    PangoContext *context;
    PangoMatrix matrix = PANGO_MATRIX_INIT;
    int tmp;

    context = pango_layout_get_context (overlay->layout);

    pango_matrix_rotate (&matrix, -90);

    rect.x = rect.y = 0;
    rect.width = width;
    rect.height = height;
    pango_matrix_transform_pixel_rectangle (&matrix, &rect);
    matrix.x0 = -rect.x;
    matrix.y0 = -rect.y;

    pango_context_set_matrix (context, &matrix);

    cairo_matrix.xx = matrix.xx;
    cairo_matrix.yx = matrix.yx;
    cairo_matrix.xy = matrix.xy;
    cairo_matrix.yy = matrix.yy;
    cairo_matrix.x0 = matrix.x0;
    cairo_matrix.y0 = matrix.y0;
    cairo_matrix_scale (&cairo_matrix, scalef, scalef);

    tmp = height;
    height = width;
    width = tmp;
  } else {
    cairo_matrix_init_scale (&cairo_matrix, scalef, scalef);
  }

  g_print ("Creating text image buffer with width %d and height %d\n",
      width, height);

  /* reallocate overlay buffer */
  buffer = gst_buffer_new_allocate (NULL, 4 * width * height, NULL);
  gst_buffer_replace (&overlay->text_image, buffer);
  gst_buffer_unref (buffer);

  gst_buffer_map (buffer, &map, GST_MAP_READWRITE);
  surface = cairo_image_surface_create_for_data (map.data,
      CAIRO_FORMAT_ARGB32, width, height, width * 4);
  cr = cairo_create (surface);

  /* clear surface */
  cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
  cairo_paint (cr);

  cairo_set_operator (cr, CAIRO_OPERATOR_OVER);

  width_bk = width + 2 * overlay->line_padding;
  height_bk = height + 2 * overlay->background_ypad;
#if 0
  if (overlay->want_background) {
    gchar *background_color;

    buffer_bk = gst_buffer_new_allocate (NULL, 4 * width_bk * height_bk, NULL);
    gst_buffer_replace (&overlay->background_image, buffer_bk);
    gst_buffer_unref (buffer_bk);

    gst_buffer_map (buffer_bk, &map_bk, GST_MAP_READWRITE);
    surface_bk = cairo_image_surface_create_for_data (map_bk.data,
        CAIRO_FORMAT_ARGB32, width_bk, height_bk, width_bk * 4);
    cr_bk = cairo_create (surface_bk);


    /* clear surface */
    cairo_set_operator (cr_bk, CAIRO_OPERATOR_CLEAR);
    cairo_paint (cr_bk);

    cairo_set_operator (cr_bk, CAIRO_OPERATOR_OVER);

    /*
       If not in hex format. Assume no a and parse using pango.
     */
    background_color = (overlay->background_color);
    if (background_color[0] != '#') {
      PangoColor pango_color;
      /* convert from CSS format. */
      /* convert to Pango rgb values */
      pango_color_parse (&pango_color, background_color);

      a = 255;
      r = pango_color.red;
      g = pango_color.green;
      b = pango_color.blue;

    } else {
      guint hex_color;

      hex_color = DEFAULT_PROP_OUTLINE_COLOR;

      /* In hex form */
      a = (hex_color >> 24) & 0xff;
      r = (hex_color >> 16) & 0xff;
      g = (hex_color >> 8) & 0xff;
      b = (hex_color >> 0) & 0xff;
    }


    /* draw background */
    cairo_save (cr_bk);
    cairo_set_source_rgba (cr_bk, r / 255.0, g / 255.0, b / 255.0, a / 255.0);
    cairo_paint (cr_bk);
    cairo_restore (cr_bk);

    overlay->image_width_bk = width_bk;
    overlay->image_height_bk = height_bk;

    cairo_destroy (cr_bk);
    cairo_surface_destroy (surface_bk);
    gst_buffer_unmap (buffer_bk, &map_bk);
  }
#endif


  /* apply transformations */
  cairo_set_matrix (cr, &cairo_matrix);

  /* FIXME: We use show_layout everywhere except for the surface
   * because it's really faster and internally does all kinds of
   * caching. Unfortunately we have to paint to a cairo path for
   * the outline and this is slow. Once Pango supports user fonts
   * we should use them, see
   * https://bugzilla.gnome.org/show_bug.cgi?id=598695
   *
   * Idea would the be, to create a cairo user font that
   * does shadow, outline, text painting in the
   * render_glyph function.
   */

  /* draw shadow text */
#if 0
  {
    PangoAttrList *origin_attr, *filtered_attr, *temp_attr;

    /* Store a ref on the original attributes for later restoration */
    origin_attr =
        pango_attr_list_ref (pango_layout_get_attributes (overlay->layout));
    /* Take a copy of the original attributes, because pango_attr_list_filter
     * modifies the passed list */
    temp_attr = pango_attr_list_copy (origin_attr);
    filtered_attr =
        pango_attr_list_filter (temp_attr,
        gst_text_overlay_filter_foreground_attr, NULL);
    pango_attr_list_unref (temp_attr);

    cairo_save (cr);
    cairo_translate (cr, overlay->shadow_offset, overlay->shadow_offset);
    cairo_set_source_rgba (cr, 0.0, 0.0, 0.0, 0.5);
    pango_layout_set_attributes (overlay->layout, filtered_attr);
    pango_cairo_show_layout (cr, overlay->layout);
    pango_layout_set_attributes (overlay->layout, origin_attr);
    pango_attr_list_unref (filtered_attr);
    pango_attr_list_unref (origin_attr);
    cairo_restore (cr);
  }
#endif

  a = (overlay->outline_color >> 24) & 0xff;
  r = (overlay->outline_color >> 16) & 0xff;
  g = (overlay->outline_color >> 8) & 0xff;
  b = (overlay->outline_color >> 0) & 0xff;

  /* draw outline text */
  cairo_save (cr);
  cairo_set_source_rgba (cr, r / 255.0, g / 255.0, b / 255.0, a / 255.0);
  cairo_set_line_width (cr, overlay->outline_offset);
  pango_cairo_layout_path (cr, overlay->layout);
  cairo_stroke (cr);
  cairo_restore (cr);

  a = (overlay->color >> 24) & 0xff;
  r = (overlay->color >> 16) & 0xff;
  g = (overlay->color >> 8) & 0xff;
  b = (overlay->color >> 0) & 0xff;

  /* draw text */
  cairo_save (cr);
  cairo_set_source_rgba (cr, r / 255.0, g / 255.0, b / 255.0, a / 255.0);
  pango_cairo_show_layout (cr, overlay->layout);
  cairo_restore (cr);

  cairo_destroy (cr);
  cairo_surface_destroy (surface);
  gst_buffer_unmap (buffer, &map);
  overlay->image_width = width;
  overlay->image_height = height;
  overlay->baseline_y = ink_rect.y;
  g_mutex_unlock (GST_BASE_EBUTTD_OVERLAY_GET_CLASS (overlay)->pango_lock);

  /*gst_base_ebuttd_overlay_region_new (overlay, 100.0,
      100.0, 68.0, 20.0, "<span foreground=\"blue\" font_style=\"normal\" font_family=\"sans\" size=\"64000\">A second region!</span>",
      GST_BASE_EBUTTD_OVERLAY_GET_CLASS (overlay)->pango_context, overlay->line_padding);*/

  /*gst_base_ebuttd_overlay_region_new (overlay, 500.0,
      400.0, 68.0, 20.0, "<span foreground=\"green\" font_style=\"normal\" font_family=\"serif\" size=\"32000\">A third region!</span>",
      GST_BASE_EBUTTD_OVERLAY_GET_CLASS (overlay)->pango_context, overlay->line_padding);*/

  gst_base_ebuttd_overlay_set_composition (overlay);
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


static GstBaseEbuttdOverlayColor
parse_ebuttd_colorstring (const gchar * color)
{
  guint length;
  const gchar *c = NULL;
  GstBaseEbuttdOverlayColor ret = { 0, 0, 0, 0 };

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

    g_print ("Returning color - r:%g  b:%g  g:%g  a:%g\n",
        ret.r, ret.b, ret.g, ret.a);
  } else if (g_strcmp0 (color, "yellow") == 0) { /* XXX:Hack for test stream. */
    ret = parse_ebuttd_colorstring ("#ffff00");
  } else {
    g_print ("Invalid color string.\n");
  }

  return ret;
}


static GstBuffer *
draw_rectangle (guint width, guint height, GstBaseEbuttdOverlayColor color)
{
  GstMapInfo map;
  gdouble r, g, b, a;
  cairo_surface_t *surface;
  cairo_t *cairo_state;
  GstBuffer *buffer = gst_buffer_new_allocate (NULL, 4 * width * height, NULL);

  if (buffer) {
    gst_buffer_map (buffer, &map, GST_MAP_READWRITE);
    surface = cairo_image_surface_create_for_data (map.data,
        CAIRO_FORMAT_ARGB32, width, height, width * 4);
    cairo_state = cairo_create (surface);

    /* clear surface */
    cairo_set_operator (cairo_state, CAIRO_OPERATOR_CLEAR);
    cairo_paint (cairo_state);
    cairo_set_operator (cairo_state, CAIRO_OPERATOR_OVER);

    cairo_save (cairo_state);
    cairo_set_source_rgba (cairo_state, color.r, color.g, color.b, color.a);
    cairo_paint (cairo_state);
    cairo_restore (cairo_state);
    cairo_destroy (cairo_state);
    cairo_surface_destroy (surface);
    gst_buffer_unmap (buffer, &map);
  } else {
    g_print ("Couldn't allocate memory to store rectangle.\n");
  }

  return buffer;
}


static GstBuffer *
draw_text (const gchar * text, guint text_height,
    GstBaseEbuttdOverlayColor color, PangoContext *context, guint width,
    guint height, guint * ink_width, guint * ink_height)
{
  GstMapInfo map;
  gdouble r, g, b, a;
  cairo_surface_t *surface;
  cairo_t *cairo_state;
  GstBuffer *buffer = NULL;
  PangoLayout *layout = NULL;
  PangoAttrList *attr_list;
  PangoAttribute *fsize;
  PangoRectangle ink_rect, logical_rect;

  layout = pango_layout_new (context);
  pango_layout_set_width (layout, width * PANGO_SCALE);
  pango_layout_set_height (layout, height * PANGO_SCALE);
  pango_layout_set_markup (layout, text, strlen (text));

  attr_list = pango_layout_get_attributes (layout);
  fsize = pango_attr_size_new (text_height * PANGO_SCALE);
  pango_attr_list_change (attr_list, fsize);
  pango_layout_set_attributes (layout, attr_list);

  pango_layout_get_pixel_extents (layout, &ink_rect, &logical_rect);
  /*pango_layout_set_spacing (layout, PANGO_SCALE * 20);*/

  buffer = gst_buffer_new_allocate (NULL, 4 * width * height, NULL);

  gst_buffer_map (buffer, &map, GST_MAP_READWRITE);
  surface = cairo_image_surface_create_for_data (map.data,
      CAIRO_FORMAT_ARGB32, width, height, width * 4);
  cairo_state = cairo_create (surface);

  /* clear surface */
  cairo_set_operator (cairo_state, CAIRO_OPERATOR_CLEAR);
  cairo_paint (cairo_state);
  cairo_set_operator (cairo_state, CAIRO_OPERATOR_OVER);

  /* draw text */
  cairo_save (cairo_state);
  g_print ("Layout text is: %s\n", pango_layout_get_text (layout));
  cairo_set_source_rgba (cairo_state, color.r, color.g, color.b, color.a);
  pango_cairo_show_layout (cairo_state, layout);
  cairo_restore (cairo_state);

  cairo_destroy (cairo_state);
  cairo_surface_destroy (surface);
  gst_buffer_unmap (buffer, &map);

  if (ink_width) *ink_width = logical_rect.width;
  if (ink_height) *ink_height = logical_rect.height;
  return buffer;
}


static GstBaseEbuttdOverlayLayer *
create_new_layer (GstBuffer * image, guint xpos, guint ypos, guint width,
    guint height)
{
  GstBaseEbuttdOverlayLayer *layer;

  g_return_val_if_fail (image != NULL, NULL);

  layer = g_new0 (GstBaseEbuttdOverlayLayer, 1);
  layer->image = image;
  layer->xpos = xpos;
  layer->ypos = ypos;
  layer->width = width;
  layer->height = height;

  g_print ("Creating layer - x: %d  y: %d  w: %u  h: %u, buffer-size: %u\n",
      xpos, ypos, width, height, gst_buffer_get_size (image));

  gst_buffer_add_video_meta (image, GST_VIDEO_FRAME_FLAG_NONE,
      GST_VIDEO_OVERLAY_COMPOSITION_FORMAT_RGB, width, height);

  layer->rectangle = gst_video_overlay_rectangle_new_raw (image, xpos, ypos,
      width, height, GST_VIDEO_OVERLAY_FORMAT_FLAG_PREMULTIPLIED_ALPHA);

  return layer;
}


static void
gst_base_ebuttd_overlay_render_pangocairo2 (GstBaseEbuttdOverlay * overlay,
    const gchar * string, gint textlen, GstBaseEbuttdOverlayRegion * region,
    GstBaseEbuttdOverlayStyle * style)
{
  GstBaseEbuttdOverlayColor text_color;
  guint cell_pixel_height;
  guint cell_pixel_width;
  guint padding_start_px, padding_end_px, padding_before_px, padding_after_px;
  guint text_height;
  GstBuffer *region_image = NULL;
  guint region_x, region_y;
  guint region_w, region_h;
  GstBaseEbuttdOverlayLayer *region_layer;
  guint line_padding_px;
  GstBuffer *text_image = NULL;
  guint text_x, text_y;
  guint text_w, text_h;
  guint ink_w, ink_h;
  GstBaseEbuttdOverlayLayer *text_layer;
  GstBuffer *bg_image = NULL;
  guint bg_x, bg_y, bg_w, bg_h;
  GstBaseEbuttdOverlayLayer *bg_layer;

  g_return_val_if_fail (textlen < 256, NULL);

  /* Convert relative measurements to pixel measurements based on video frame
   * size. */
  cell_pixel_height = overlay->height / style->cellres_y;
  cell_pixel_width = overlay->width / style->cellres_x;

  line_padding_px = (guint) (style->line_padding * cell_pixel_width);
  g_print ("line_padding_px: %u\n", line_padding_px);

  padding_start_px = (guint) ((region->padding_start * overlay->width) / 100.0);
  padding_end_px = (guint) ((region->padding_end * overlay->width) / 100.0);
  padding_before_px =
    (guint) ((region->padding_before * overlay->height) / 100.0);
  padding_after_px =
    (guint) ((region->padding_after * overlay->height) / 100.0);
  g_print ("pad_start: %u  pad_end: %u  pad_before: %u  pad_after: %u\n",
      padding_start_px, padding_end_px, padding_before_px, padding_after_px);

  region_x = (guint) ((region->origin_x * overlay->width) / 100.0);
  region_y = (guint) ((region->origin_y * overlay->height) / 100.0);
  region_w = (guint) ((region->extent_w * overlay->width) / 100.0);
  region_h = (guint) ((region->extent_h * overlay->height) / 100.0);

  /************* Render text *************/
  text_height = (guint) (style->font_size * cell_pixel_height) / 100.0;
  text_color = parse_ebuttd_colorstring (style->color);
  text_w = region_w - padding_start_px - padding_end_px - (2 * line_padding_px);
  text_h = region_h - padding_before_px - padding_after_px;
  g_print ("text_w: %u   text_h: %u\n", text_w, text_h);
  text_image = draw_text (string, text_height, text_color,
      GST_BASE_EBUTTD_OVERLAY_GET_CLASS (overlay)->pango_context,
      text_w, text_h, &ink_w, &ink_h);

  switch (style->text_align) {
    case (GST_BASE_EBUTTD_OVERLAY_TEXT_ALIGN_START):
    case (GST_BASE_EBUTTD_OVERLAY_TEXT_ALIGN_LEFT):
      text_x = region_x + padding_start_px + line_padding_px;
      break;
    case (GST_BASE_EBUTTD_OVERLAY_TEXT_ALIGN_CENTER):
      text_x = region_x + (region_w - ink_w)/2;
      break;
    case (GST_BASE_EBUTTD_OVERLAY_TEXT_ALIGN_END):
    case (GST_BASE_EBUTTD_OVERLAY_TEXT_ALIGN_RIGHT):
      text_x =
        (region_x + region_w) - (padding_end_px + line_padding_px + ink_w);
      break;
  }

  switch (region->display_align) {
    case (GST_BASE_EBUTTD_OVERLAY_DISPLAY_ALIGN_BEFORE):
      text_y = region_y + padding_before_px;
      break;
    case (GST_BASE_EBUTTD_OVERLAY_DISPLAY_ALIGN_CENTER):
      text_y = region_y + (region_h - ink_h)/2;
      break;
    case (GST_BASE_EBUTTD_OVERLAY_DISPLAY_ALIGN_AFTER):
      text_y = (region_y + region_h) - (padding_after_px + ink_h);
      break;
  }

  text_layer = create_new_layer (text_image, text_x, text_y, text_w, text_h);
  overlay->layers = g_slist_append (overlay->layers, text_layer);

  /************* Render text background *************/
  bg_w = ink_w + (2 * line_padding_px);
  bg_h = ink_h;

  bg_image = draw_rectangle (bg_w, bg_h,
      parse_ebuttd_colorstring (style->bg_color));

  bg_x = text_x - line_padding_px;
  bg_y = text_y;

  bg_layer = create_new_layer (bg_image, bg_x, bg_y, bg_w, bg_h);
  overlay->layers = g_slist_prepend (overlay->layers, bg_layer);

  /************* Render region background *************/
  region_image = draw_rectangle (region_w, region_h,
      parse_ebuttd_colorstring ("#ff000077"));

  region_layer = create_new_layer (region_image, region_x, region_y,
      region_w, region_h);
  overlay->layers = g_slist_prepend (overlay->layers, region_layer);

  gst_base_ebuttd_overlay_compose_layers (overlay, overlay->layers);
}


static inline void
gst_base_ebuttd_overlay_shade_planar_Y (GstBaseEbuttdOverlay * overlay,
    GstVideoFrame * dest, gint x0, gint x1, gint y0, gint y1)
{
  gint i, j, dest_stride;
  guint8 *dest_ptr;

  dest_stride = dest->info.stride[0];
  dest_ptr = dest->data[0];

  for (i = y0; i < y1; ++i) {
    for (j = x0; j < x1; ++j) {
      gint y = dest_ptr[(i * dest_stride) + j] - overlay->shading_value;

      dest_ptr[(i * dest_stride) + j] = CLAMP (y, 0, 255);
    }
  }
}

static inline void
gst_base_ebuttd_overlay_shade_packed_Y (GstBaseEbuttdOverlay * overlay,
    GstVideoFrame * dest, gint x0, gint x1, gint y0, gint y1)
{
  gint i, j;
  guint dest_stride, pixel_stride;
  guint8 *dest_ptr;

  dest_stride = GST_VIDEO_FRAME_COMP_STRIDE (dest, 0);
  dest_ptr = GST_VIDEO_FRAME_COMP_DATA (dest, 0);
  pixel_stride = GST_VIDEO_FRAME_COMP_PSTRIDE (dest, 0);

  if (x0 != 0)
    x0 = GST_VIDEO_FORMAT_INFO_SCALE_WIDTH (dest->info.finfo, 0, x0);
  if (x1 != 0)
    x1 = GST_VIDEO_FORMAT_INFO_SCALE_WIDTH (dest->info.finfo, 0, x1);

  if (y0 != 0)
    y0 = GST_VIDEO_FORMAT_INFO_SCALE_HEIGHT (dest->info.finfo, 0, y0);
  if (y1 != 0)
    y1 = GST_VIDEO_FORMAT_INFO_SCALE_HEIGHT (dest->info.finfo, 0, y1);

  for (i = y0; i < y1; i++) {
    for (j = x0; j < x1; j++) {
      gint y;
      gint y_pos;

      y_pos = (i * dest_stride) + j * pixel_stride;
      y = dest_ptr[y_pos] - overlay->shading_value;

      dest_ptr[y_pos] = CLAMP (y, 0, 255);
    }
  }
}

#define gst_base_ebuttd_overlay_shade_BGRx gst_base_ebuttd_overlay_shade_xRGB
#define gst_base_ebuttd_overlay_shade_RGBx gst_base_ebuttd_overlay_shade_xRGB
#define gst_base_ebuttd_overlay_shade_xBGR gst_base_ebuttd_overlay_shade_xRGB
static inline void
gst_base_ebuttd_overlay_shade_xRGB (GstBaseEbuttdOverlay * overlay,
    GstVideoFrame * dest, gint x0, gint x1, gint y0, gint y1)
{
  gint i, j;
  guint8 *dest_ptr;

  dest_ptr = dest->data[0];

  for (i = y0; i < y1; i++) {
    for (j = x0; j < x1; j++) {
      gint y, y_pos, k;

      y_pos = (i * 4 * overlay->width) + j * 4;
      for (k = 0; k < 4; k++) {
        y = dest_ptr[y_pos + k] - overlay->shading_value;
        dest_ptr[y_pos + k] = CLAMP (y, 0, 255);
      }

/*      dest_ptr[y_pos + 0] += overlay->shading_value;
      dest_ptr[y_pos + 2] = 0;
      dest_ptr[y_pos + 1] = 0;
      dest_ptr[y_pos + 3] = 255;*/


    }
  }
}

/* FIXME: orcify */
static void
gst_base_ebuttd_overlay_shade_rgb24 (GstBaseEbuttdOverlay * overlay,
    GstVideoFrame * frame, gint x0, gint x1, gint y0, gint y1)
{
  const int pstride = 3;
  gint y, x, stride, shading_val, tmp;
  guint8 *p;

  shading_val = -overlay->shading_value;
  stride = GST_VIDEO_FRAME_PLANE_STRIDE (frame, 0);

  for (y = y0; y < y1; ++y) {
    p = GST_VIDEO_FRAME_PLANE_DATA (frame, 0);
    p += (y * stride) + (x0 * pstride);
    for (x = x0; x < x1; ++x) {
      tmp = *p + shading_val;
      *p++ = CLAMP (tmp, 0, 255);
      tmp = *p + shading_val;
      *p++ = CLAMP (tmp, 0, 255);
      tmp = *p + shading_val;
      *p++ = CLAMP (tmp, 0, 255);
    }
  }
}

static void
gst_base_ebuttd_overlay_shade_IYU1 (GstBaseEbuttdOverlay * overlay,
    GstVideoFrame * frame, gint x0, gint x1, gint y0, gint y1)
{
  gint y, x, stride, shading_val, tmp;
  guint8 *p;

  shading_val = -overlay->shading_value;
  stride = GST_VIDEO_FRAME_PLANE_STRIDE (frame, 0);

  /* IYU1: packed 4:1:1 YUV (Cb-Y0-Y1-Cr-Y2-Y3 ...) */
  for (y = y0; y < y1; ++y) {
    p = GST_VIDEO_FRAME_PLANE_DATA (frame, 0);
    /* move to Y0 or Y1 (we pretend the chroma is the last of the 3 bytes) */
    /* FIXME: we're not pixel-exact here if x0 is an odd number, but it's
     * unlikely anyone will notice.. */
    p += (y * stride) + ((x0 / 2) * 3) + 1;
    for (x = x0; x < x1; x += 2) {
      tmp = *p + shading_val;
      *p++ = CLAMP (tmp, 0, 255);
      tmp = *p + shading_val;
      *p++ = CLAMP (tmp, 0, 255);
      /* skip chroma */
      p++;
    }
  }
}

#define ARGB_SHADE_FUNCTION(name, OFFSET)	\
static inline void \
gst_base_ebuttd_overlay_shade_##name (GstBaseEbuttdOverlay * overlay, GstVideoFrame * dest, \
gint x0, gint x1, gint y0, gint y1) \
{ \
  gint i, j;\
  guint8 *dest_ptr;\
  \
  dest_ptr = dest->data[0];\
  \
  for (i = y0; i < y1; i++) {\
    for (j = x0; j < x1; j++) {\
      gint y, y_pos, k;\
      y_pos = (i * 4 * overlay->width) + j * 4;\
      for (k = OFFSET; k < 3+OFFSET; k++) {\
        y = dest_ptr[y_pos + k] - overlay->shading_value;\
        dest_ptr[y_pos + k] = CLAMP (y, 0, 255);\
      }\
    }\
  }\
}
ARGB_SHADE_FUNCTION (ARGB, 1);
ARGB_SHADE_FUNCTION (ABGR, 1);
ARGB_SHADE_FUNCTION (RGBA, 0);
ARGB_SHADE_FUNCTION (BGRA, 0);

static void
gst_base_ebuttd_overlay_render_text (GstBaseEbuttdOverlay * overlay,
    const gchar * text, gint textlen)
{
  gchar *string;

  if (!overlay->need_render) {
    GST_DEBUG ("Using previously rendered text.");
    return;
  }

  /* -1 is the whole string */
  if (text != NULL && textlen < 0) {
    textlen = strlen (text);
  }

  if (text != NULL) {
    string = g_strndup (text, textlen);
  } else {                      /* empty string */
    string = g_strdup (" ");
  }
  g_strdelimit (string, "\r\t", ' ');
  textlen = strlen (string);

  /* FIXME: should we check for UTF-8 here? */

  GST_DEBUG ("Rendering '%s'", string);
  gst_base_ebuttd_overlay_render_pangocairo (overlay, string, textlen);

  g_free (string);

  overlay->need_render = FALSE;
}


static void
gst_base_ebuttd_overlay_render_text2 (GstBaseEbuttdOverlay * overlay,
    const gchar * text, gint textlen, GstBaseEbuttdOverlayRegion * region,
    GstBaseEbuttdOverlayStyle * style)
{
  gchar *string;

  if (!overlay->need_render) {
    GST_DEBUG ("Using previously rendered text.");
    return;
  }

  /* -1 is the whole string */
  if (text != NULL && textlen < 0) {
    textlen = strlen (text);
  }

  if (text != NULL) {
    string = g_strndup (text, textlen);
  } else {                      /* empty string */
    string = g_strdup (" ");
  }
  g_strdelimit (string, "\r\t", ' ');
  textlen = strlen (string);

  /* FIXME: should we check for UTF-8 here? */

  GST_DEBUG ("Rendering '%s'", string);
  gst_base_ebuttd_overlay_render_pangocairo2 (overlay, string, textlen, region,
      style);

  g_free (string);

  overlay->need_render = FALSE;
}


/* FIXME: should probably be relative to width/height (adjusted for PAR) */
#define BOX_XPAD  overlay->line_padding
#define BOX_YPAD  6

static void
gst_base_ebuttd_overlay_shade_background (GstBaseEbuttdOverlay * overlay,
    GstVideoFrame * frame, gint x0, gint x1, gint y0, gint y1)
{
  x0 = CLAMP (x0 - BOX_XPAD, 0, overlay->width);
  x1 = CLAMP (x1 + BOX_XPAD, 0, overlay->width);

  y0 = CLAMP (y0 - BOX_YPAD, 0, overlay->height);
  y1 = CLAMP (y1 + BOX_YPAD, 0, overlay->height);

  switch (overlay->format) {
    case GST_VIDEO_FORMAT_I420:
    case GST_VIDEO_FORMAT_YV12:
    case GST_VIDEO_FORMAT_NV12:
    case GST_VIDEO_FORMAT_NV21:
    case GST_VIDEO_FORMAT_Y41B:
    case GST_VIDEO_FORMAT_Y42B:
    case GST_VIDEO_FORMAT_Y444:
    case GST_VIDEO_FORMAT_YUV9:
    case GST_VIDEO_FORMAT_YVU9:
    case GST_VIDEO_FORMAT_GRAY8:
    case GST_VIDEO_FORMAT_A420:
      gst_base_ebuttd_overlay_shade_planar_Y (overlay, frame, x0, x1, y0, y1);
      break;
    case GST_VIDEO_FORMAT_AYUV:
    case GST_VIDEO_FORMAT_UYVY:
    case GST_VIDEO_FORMAT_YUY2:
    case GST_VIDEO_FORMAT_v308:
      gst_base_ebuttd_overlay_shade_packed_Y (overlay, frame, x0, x1, y0, y1);
      break;
    case GST_VIDEO_FORMAT_xRGB:
      gst_base_ebuttd_overlay_shade_xRGB (overlay, frame, x0, x1, y0, y1);
      break;
    case GST_VIDEO_FORMAT_xBGR:
      gst_base_ebuttd_overlay_shade_xBGR (overlay, frame, x0, x1, y0, y1);
      break;
    case GST_VIDEO_FORMAT_BGRx:
      gst_base_ebuttd_overlay_shade_BGRx (overlay, frame, x0, x1, y0, y1);
      break;
    case GST_VIDEO_FORMAT_RGBx:
      gst_base_ebuttd_overlay_shade_RGBx (overlay, frame, x0, x1, y0, y1);
      break;
    case GST_VIDEO_FORMAT_ARGB:
      gst_base_ebuttd_overlay_shade_ARGB (overlay, frame, x0, x1, y0, y1);
      break;
    case GST_VIDEO_FORMAT_ABGR:
      gst_base_ebuttd_overlay_shade_ABGR (overlay, frame, x0, x1, y0, y1);
      break;
    case GST_VIDEO_FORMAT_RGBA:
      gst_base_ebuttd_overlay_shade_RGBA (overlay, frame, x0, x1, y0, y1);
      break;
    case GST_VIDEO_FORMAT_BGRA:
      gst_base_ebuttd_overlay_shade_BGRA (overlay, frame, x0, x1, y0, y1);
      break;
    case GST_VIDEO_FORMAT_BGR:
    case GST_VIDEO_FORMAT_RGB:
      gst_base_ebuttd_overlay_shade_rgb24 (overlay, frame, x0, x1, y0, y1);
      break;
    case GST_VIDEO_FORMAT_IYU1:
      gst_base_ebuttd_overlay_shade_IYU1 (overlay, frame, x0, x1, y0, y1);
      break;
    default:
      GST_FIXME_OBJECT (overlay, "implement background shading for format %s",
          gst_video_format_to_string (GST_VIDEO_FRAME_FORMAT (frame)));
      break;
  }
}

static GstFlowReturn
gst_base_ebuttd_overlay_push_frame (GstBaseEbuttdOverlay * overlay,
    GstBuffer * video_frame)
{
  GstVideoFrame frame;

  if (overlay->composition == NULL)
    goto done;

  if (gst_pad_check_reconfigure (overlay->srcpad))
    gst_base_ebuttd_overlay_negotiate (overlay, NULL);

  video_frame = gst_buffer_make_writable (video_frame);

  if (overlay->attach_compo_to_buffer) {
    GST_DEBUG_OBJECT (overlay, "Attaching text overlay image to video buffer");
    gst_buffer_add_video_overlay_composition_meta (video_frame,
        overlay->composition);
    /* FIXME: emulate shaded background box if want_shading=true */
    goto done;
  }

  if (!gst_video_frame_map (&frame, &overlay->info, video_frame,
          GST_MAP_READWRITE))
    goto invalid_frame;

  /* P TAYLOUR */
  /* shaded background box */
  /*if (overlay->want_shading) {
    gint xpos, ypos;

    gst_base_ebuttd_overlay_get_pos (overlay, &xpos, &ypos);

    gst_base_ebuttd_overlay_shade_background (overlay, &frame,
        xpos, xpos + overlay->image_width, ypos, ypos + overlay->image_height);
  }*/

  gst_video_overlay_composition_blend (overlay->composition, &frame);

  gst_video_frame_unmap (&frame);

  if (overlay->layers)
    g_slist_free_full (overlay->layers,
        (GDestroyNotify) gst_base_ebuttd_overlay_layer_free);
  overlay->layers = NULL;

done:

  return gst_pad_push (overlay->srcpad, video_frame);

  /* ERRORS */
invalid_frame:
  {
    gst_buffer_unref (video_frame);
    GST_DEBUG_OBJECT (overlay, "received invalid buffer");
    return GST_FLOW_OK;
  }
}

static GstPadLinkReturn
gst_base_ebuttd_overlay_text_pad_link (GstPad * pad, GstObject * parent,
    GstPad * peer)
{
  GstBaseEbuttdOverlay *overlay;

  overlay = GST_BASE_EBUTTD_OVERLAY (parent);
  if (G_UNLIKELY (!overlay))
    return GST_PAD_LINK_REFUSED;

  GST_DEBUG_OBJECT (overlay, "Text pad linked");

  overlay->text_linked = TRUE;

  return GST_PAD_LINK_OK;
}

static void
gst_base_ebuttd_overlay_text_pad_unlink (GstPad * pad, GstObject * parent)
{
  GstBaseEbuttdOverlay *overlay;

  /* don't use gst_pad_get_parent() here, will deadlock */
  overlay = GST_BASE_EBUTTD_OVERLAY (parent);

  GST_DEBUG_OBJECT (overlay, "Text pad unlinked");

  overlay->text_linked = FALSE;

  gst_segment_init (&overlay->text_segment, GST_FORMAT_UNDEFINED);
}

static gboolean
gst_base_ebuttd_overlay_text_event (GstPad * pad, GstObject * parent,
    GstEvent * event)
{
  gboolean ret = FALSE;
  GstBaseEbuttdOverlay *overlay = NULL;

  overlay = GST_BASE_EBUTTD_OVERLAY (parent);

  GST_LOG_OBJECT (pad, "received event %s", GST_EVENT_TYPE_NAME (event));

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CAPS:
    {
      GstCaps *caps;

      gst_event_parse_caps (event, &caps);
      ret = gst_base_ebuttd_overlay_setcaps_txt (overlay, caps);
      gst_event_unref (event);
      break;
    }
    case GST_EVENT_SEGMENT:
    {
      const GstSegment *segment;

      overlay->text_eos = FALSE;

      gst_event_parse_segment (event, &segment);

      if (segment->format == GST_FORMAT_TIME) {
        GST_BASE_EBUTTD_OVERLAY_LOCK (overlay);
        gst_segment_copy_into (segment, &overlay->text_segment);
        GST_DEBUG_OBJECT (overlay, "TEXT SEGMENT now: %" GST_SEGMENT_FORMAT,
            &overlay->text_segment);
        GST_BASE_EBUTTD_OVERLAY_UNLOCK (overlay);
      } else {
        GST_ELEMENT_WARNING (overlay, STREAM, MUX, (NULL),
            ("received non-TIME newsegment event on text input"));
      }

      gst_event_unref (event);
      ret = TRUE;

      /* wake up the video chain, it might be waiting for a text buffer or
       * a text segment update */
      GST_BASE_EBUTTD_OVERLAY_LOCK (overlay);
      GST_BASE_EBUTTD_OVERLAY_BROADCAST (overlay);
      GST_BASE_EBUTTD_OVERLAY_UNLOCK (overlay);
      break;
    }
    case GST_EVENT_GAP:
    {
      GstClockTime start, duration;

      gst_event_parse_gap (event, &start, &duration);
      if (GST_CLOCK_TIME_IS_VALID (duration))
        start += duration;
      /* we do not expect another buffer until after gap,
       * so that is our position now */
      overlay->text_segment.position = start;

      /* wake up the video chain, it might be waiting for a text buffer or
       * a text segment update */
      GST_BASE_EBUTTD_OVERLAY_LOCK (overlay);
      GST_BASE_EBUTTD_OVERLAY_BROADCAST (overlay);
      GST_BASE_EBUTTD_OVERLAY_UNLOCK (overlay);

      gst_event_unref (event);
      ret = TRUE;
      break;
    }
    case GST_EVENT_FLUSH_STOP:
      GST_BASE_EBUTTD_OVERLAY_LOCK (overlay);
      GST_INFO_OBJECT (overlay, "text flush stop");
      overlay->text_flushing = FALSE;
      overlay->text_eos = FALSE;
      gst_base_ebuttd_overlay_pop_text (overlay);
      gst_segment_init (&overlay->text_segment, GST_FORMAT_TIME);
      GST_BASE_EBUTTD_OVERLAY_UNLOCK (overlay);
      gst_event_unref (event);
      ret = TRUE;
      break;
    case GST_EVENT_FLUSH_START:
      GST_BASE_EBUTTD_OVERLAY_LOCK (overlay);
      GST_INFO_OBJECT (overlay, "text flush start");
      overlay->text_flushing = TRUE;
      GST_BASE_EBUTTD_OVERLAY_BROADCAST (overlay);
      GST_BASE_EBUTTD_OVERLAY_UNLOCK (overlay);
      gst_event_unref (event);
      ret = TRUE;
      break;
    case GST_EVENT_EOS:
      GST_BASE_EBUTTD_OVERLAY_LOCK (overlay);
      overlay->text_eos = TRUE;
      GST_INFO_OBJECT (overlay, "text EOS");
      /* wake up the video chain, it might be waiting for a text buffer or
       * a text segment update */
      GST_BASE_EBUTTD_OVERLAY_BROADCAST (overlay);
      GST_BASE_EBUTTD_OVERLAY_UNLOCK (overlay);
      gst_event_unref (event);
      ret = TRUE;
      break;
    default:
      ret = gst_pad_event_default (pad, parent, event);
      break;
  }

  return ret;
}

static gboolean
gst_base_ebuttd_overlay_video_event (GstPad * pad, GstObject * parent,
    GstEvent * event)
{
  gboolean ret = FALSE;
  GstBaseEbuttdOverlay *overlay = NULL;

  overlay = GST_BASE_EBUTTD_OVERLAY (parent);

  GST_DEBUG_OBJECT (pad, "received event %s", GST_EVENT_TYPE_NAME (event));

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CAPS:
    {
      GstCaps *caps;

      gst_event_parse_caps (event, &caps);
      ret = gst_base_ebuttd_overlay_setcaps (overlay, caps);
      gst_event_unref (event);
      break;
    }
    case GST_EVENT_SEGMENT:
    {
      const GstSegment *segment;

      GST_DEBUG_OBJECT (overlay, "received new segment");

      gst_event_parse_segment (event, &segment);

      if (segment->format == GST_FORMAT_TIME) {
        GST_DEBUG_OBJECT (overlay, "VIDEO SEGMENT now: %" GST_SEGMENT_FORMAT,
            &overlay->segment);

        gst_segment_copy_into (segment, &overlay->segment);
      } else {
        GST_ELEMENT_WARNING (overlay, STREAM, MUX, (NULL),
            ("received non-TIME newsegment event on video input"));
      }

      ret = gst_pad_event_default (pad, parent, event);
      break;
    }
    case GST_EVENT_EOS:
      GST_BASE_EBUTTD_OVERLAY_LOCK (overlay);
      GST_INFO_OBJECT (overlay, "video EOS");
      overlay->video_eos = TRUE;
      GST_BASE_EBUTTD_OVERLAY_UNLOCK (overlay);
      ret = gst_pad_event_default (pad, parent, event);
      break;
    case GST_EVENT_FLUSH_START:
      GST_BASE_EBUTTD_OVERLAY_LOCK (overlay);
      GST_INFO_OBJECT (overlay, "video flush start");
      overlay->video_flushing = TRUE;
      GST_BASE_EBUTTD_OVERLAY_BROADCAST (overlay);
      GST_BASE_EBUTTD_OVERLAY_UNLOCK (overlay);
      ret = gst_pad_event_default (pad, parent, event);
      break;
    case GST_EVENT_FLUSH_STOP:
      GST_BASE_EBUTTD_OVERLAY_LOCK (overlay);
      GST_INFO_OBJECT (overlay, "video flush stop");
      overlay->video_flushing = FALSE;
      overlay->video_eos = FALSE;
      gst_segment_init (&overlay->segment, GST_FORMAT_TIME);
      GST_BASE_EBUTTD_OVERLAY_UNLOCK (overlay);
      ret = gst_pad_event_default (pad, parent, event);
      break;
    default:
      ret = gst_pad_event_default (pad, parent, event);
      break;
  }

  return ret;
}

static gboolean
gst_base_ebuttd_overlay_video_query (GstPad * pad, GstObject * parent,
    GstQuery * query)
{
  gboolean ret = FALSE;
  GstBaseEbuttdOverlay *overlay;

  overlay = GST_BASE_EBUTTD_OVERLAY (parent);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CAPS:
    {
      GstCaps *filter, *caps;

      gst_query_parse_caps (query, &filter);
      caps = gst_base_ebuttd_overlay_get_videosink_caps (pad, overlay, filter);
      gst_query_set_caps_result (query, caps);
      gst_caps_unref (caps);
      ret = TRUE;
      break;
    }
    default:
      ret = gst_pad_query_default (pad, parent, query);
      break;
  }

  return ret;
}

/* Called with lock held */
static void
gst_base_ebuttd_overlay_pop_text (GstBaseEbuttdOverlay * overlay)
{
  g_return_if_fail (GST_IS_BASE_EBUTTD_OVERLAY (overlay));

  if (overlay->text_buffer) {
    GST_DEBUG_OBJECT (overlay, "releasing text buffer %p",
        overlay->text_buffer);
    gst_buffer_unref (overlay->text_buffer);
    overlay->text_buffer = NULL;
  }

  /* Let the text task know we used that buffer */
  GST_BASE_EBUTTD_OVERLAY_BROADCAST (overlay);
}

/* We receive text buffers here. If they are out of segment we just ignore them.
   If the buffer is in our segment we keep it internally except if another one
   is already waiting here, in that case we wait that it gets kicked out */
static GstFlowReturn
gst_base_ebuttd_overlay_text_chain (GstPad * pad, GstObject * parent,
    GstBuffer * buffer)
{
  GstFlowReturn ret = GST_FLOW_OK;
  GstBaseEbuttdOverlay *overlay = NULL;
  gboolean in_seg = FALSE;
  guint64 clip_start = 0, clip_stop = 0;

  overlay = GST_BASE_EBUTTD_OVERLAY (parent);

  GST_BASE_EBUTTD_OVERLAY_LOCK (overlay);

  if (overlay->text_flushing) {
    GST_BASE_EBUTTD_OVERLAY_UNLOCK (overlay);
    ret = GST_FLOW_FLUSHING;
    GST_LOG_OBJECT (overlay, "text flushing");
    goto beach;
  }

  if (overlay->text_eos) {
    GST_BASE_EBUTTD_OVERLAY_UNLOCK (overlay);
    ret = GST_FLOW_EOS;
    GST_LOG_OBJECT (overlay, "text EOS");
    goto beach;
  }

  GST_LOG_OBJECT (overlay, "%" GST_SEGMENT_FORMAT "  BUFFER: ts=%"
      GST_TIME_FORMAT ", end=%" GST_TIME_FORMAT, &overlay->segment,
      GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (buffer)),
      GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (buffer) +
          GST_BUFFER_DURATION (buffer)));

  if (G_LIKELY (GST_BUFFER_TIMESTAMP_IS_VALID (buffer))) {
    GstClockTime stop;

    if (G_LIKELY (GST_BUFFER_DURATION_IS_VALID (buffer)))
      stop = GST_BUFFER_TIMESTAMP (buffer) + GST_BUFFER_DURATION (buffer);
    else
      stop = GST_CLOCK_TIME_NONE;

    in_seg = gst_segment_clip (&overlay->text_segment, GST_FORMAT_TIME,
        GST_BUFFER_TIMESTAMP (buffer), stop, &clip_start, &clip_stop);
  } else {
    in_seg = TRUE;
  }

  if (in_seg) {
    if (GST_BUFFER_TIMESTAMP_IS_VALID (buffer))
      GST_BUFFER_TIMESTAMP (buffer) = clip_start;
    else if (GST_BUFFER_DURATION_IS_VALID (buffer))
      GST_BUFFER_DURATION (buffer) = clip_stop - clip_start;

    /* Wait for the previous buffer to go away */
    while (overlay->text_buffer != NULL) {
      GST_DEBUG ("Pad %s:%s has a buffer queued, waiting",
          GST_DEBUG_PAD_NAME (pad));
      GST_BASE_EBUTTD_OVERLAY_WAIT (overlay);
      GST_DEBUG ("Pad %s:%s resuming", GST_DEBUG_PAD_NAME (pad));
      if (overlay->text_flushing) {
        GST_BASE_EBUTTD_OVERLAY_UNLOCK (overlay);
        ret = GST_FLOW_FLUSHING;
        goto beach;
      }
    }

    if (GST_BUFFER_TIMESTAMP_IS_VALID (buffer))
      overlay->text_segment.position = clip_start;

    overlay->text_buffer = buffer;
    /* That's a new text buffer we need to render */
    overlay->need_render = TRUE;

    /* in case the video chain is waiting for a text buffer, wake it up */
    GST_BASE_EBUTTD_OVERLAY_BROADCAST (overlay);
  }

  GST_BASE_EBUTTD_OVERLAY_UNLOCK (overlay);

beach:

  return ret;
}

gchar *
extract_style_then_remove (gchar * property, gchar ** text)
{
  gchar *style_start;
  gchar *style_end;
  gchar *style;
  gchar *text_before_style;
  gchar *without_mkup;
  /* does the text contain any of the non pango styles? */
  style_start = g_strstr_len (*text, -1, property);
  if (style_start) {
    style = g_strstr_len (style_start, -1, "\"");
    style++;
    style_end = g_strstr_len (style, -1, "\"");
    style = g_strndup (style, style_end - style);       /* This is what we're going to return */

    /* remove markup */
    text_before_style = g_strndup (*text, style_start - *text);
    style_end++;                /* remove " so just have text after style */
    without_mkup = g_strjoin (" ", text_before_style, style_end, NULL);
    g_free (text_before_style);

    g_free (*text);
    *text = without_mkup;

    return style;
  }

  return NULL;                  /* didn't find it */
}

void
add_pango_style (gchar * property, gchar * value, gchar ** text)
{
  gchar *insert_pointer;
  gchar *before;
  gchar *after;
  gchar *text_with_style;

  insert_pointer = g_strstr_len (*text, -1, ">");
  if (insert_pointer) {
    /* remove markup */
    before = g_strndup (*text, insert_pointer - *text);
    after = insert_pointer;

    text_with_style = g_strconcat (before, property, "=\"",
        value, "\"", after, NULL);
    g_free (before);
    // g_free(after);
    g_free (*text);
    *text = text_with_style;
  }
}

void
convert_from_ebutt_to_pango (gchar ** text, GstBaseEbuttdOverlay * overlay)
{
  gchar *font_size_style;       /* font size as a percentage of cell height */
  gdouble font_in_px;
  gdouble cell_height;
  guint cell_res_y;
  gboolean found_non_pango;

  do {
    font_size_style = extract_style_then_remove ("font_size", text);
    if (font_size_style) {
      guint height_in_px;
      gdouble factor;

      if (overlay->cell_resolution_y) {
        cell_res_y = overlay->cell_resolution_y;
      } else {
        cell_res_y = DEFAULT_PROP_CELL_RESOLUTION_Y;
      }

      /* font size = cell_height * factor */
      factor = g_ascii_strtod (font_size_style, NULL);  /* % of cell height */
      factor = factor * 0.01;

      /* convert to absolute (pixels) */
      height_in_px = overlay->height;   /* window height */
      cell_height = height_in_px / (double) cell_res_y;
      font_in_px = cell_height * factor;

      overlay->text_height_px = font_in_px;

#if 0
      /* convert to  1024th point */
      /* converstion factor: px = 4/3 pt assuming 96dpi */
      font_size_int = (int) (font_in_px * (72 / 96.0) * 1024);
      sprintf (font_size_str, "%d", font_size_int);
      /* add to text as pango regonisable font_size */
      add_pango_style ("font_size", &font_size_str, text);
#endif
    }

    /* found all of the non pango styles? */
    if (font_size_style) {
      found_non_pango = TRUE;
    } else {
      found_non_pango = FALSE;
    }
  } while (found_non_pango);
}

void
set_non_pango_markup (gchar ** text, GstBaseEbuttdOverlay * overlay)
{
  gchar *line_padding_style;
  gchar *background_color_style;
  gchar *cell_resolution_x_style;
  gchar *cell_resolution_y_style;
  gchar *multi_row_align_style;
  gchar *text_align_style;
  guint cell_res_x;
  gint win_width;
  gdouble cell_width;
  gdouble factor;
  gboolean found_non_pango;

  do {
    extract_style_then_remove ("foreground", text);
    multi_row_align_style = extract_style_then_remove ("multi_row_align", text);
    text_align_style = extract_style_then_remove ("text_align", text);

    if (text_align_style) {
      if (strcmp (text_align_style, "right") == 0) {
        overlay->halign = GST_BASE_EBUTTD_OVERLAY_HALIGN_RIGHT;
        overlay->line_align = GST_BASE_EBUTTD_OVERLAY_LINE_ALIGN_RIGHT;
      } else if (strcmp (text_align_style, "left") == 0) {
        overlay->halign = GST_BASE_EBUTTD_OVERLAY_HALIGN_LEFT;
        overlay->line_align = GST_BASE_EBUTTD_OVERLAY_LINE_ALIGN_LEFT;
      } else if (strcmp (text_align_style, "center") == 0) {
        overlay->halign = GST_BASE_EBUTTD_OVERLAY_HALIGN_CENTER;
      } else if (strcmp (text_align_style, "start") == 0) {
        /* TODO: check for text direction and adjust accordingly */
        overlay->halign = GST_BASE_EBUTTD_OVERLAY_HALIGN_LEFT;
        overlay->line_align = GST_BASE_EBUTTD_OVERLAY_LINE_ALIGN_LEFT;
      } else if (strcmp (text_align_style, "end") == 0) {
        /* TODO: check for text direction and adjust accordingly */
        overlay->halign = GST_BASE_EBUTTD_OVERLAY_HALIGN_RIGHT;
        overlay->line_align = GST_BASE_EBUTTD_OVERLAY_LINE_ALIGN_RIGHT;
      }

      /* text_align updates automatically */
      /* don't update multirow if it's going to be updated in next clause */
      if (!multi_row_align_style) {
        pango_layout_set_alignment (overlay->layout,
            (PangoAlignment) overlay->line_align);
      }
    } else {
      /* use defult if not supplied. */
      overlay->halign = DEFAULT_PROP_HALIGNMENT;
    }

    if (multi_row_align_style) {
      if (strcmp (multi_row_align_style, "right") == 0) {
        overlay->line_align = GST_BASE_EBUTTD_OVERLAY_LINE_ALIGN_RIGHT;
      } else if (strcmp (multi_row_align_style, "left") == 0) {
        overlay->line_align = GST_BASE_EBUTTD_OVERLAY_LINE_ALIGN_LEFT;
      } else if (strcmp (multi_row_align_style, "center") == 0) {
        overlay->line_align = GST_BASE_EBUTTD_OVERLAY_LINE_ALIGN_CENTER;
      } else if (strcmp (multi_row_align_style, "start") == 0) {
        /* TODO: check for text direction and adjust accordingly */
        overlay->line_align = GST_BASE_EBUTTD_OVERLAY_LINE_ALIGN_LEFT;
      } else if (strcmp (multi_row_align_style, "end") == 0) {
        /* TODO: check for text direction and adjust accordingly */
        overlay->line_align = GST_BASE_EBUTTD_OVERLAY_LINE_ALIGN_RIGHT;
      }
      pango_layout_set_alignment (overlay->layout,
          (PangoAlignment) overlay->line_align);
    } else {
      /* use defult if not supplied. */
      overlay->line_align = DEFAULT_PROP_LINE_ALIGNMENT;
    }

    cell_resolution_x_style =
        extract_style_then_remove ("cell_resolution_x", text);
    if (cell_resolution_x_style) {
      overlay->cell_resolution_x = g_ascii_strtoll (cell_resolution_x_style,
          NULL, 10);
    }

    cell_resolution_y_style =
        extract_style_then_remove ("cell_resolution_y", text);
    if (cell_resolution_y_style) {
      overlay->cell_resolution_y = g_ascii_strtoll (cell_resolution_y_style,
          NULL, 10);
    }

    line_padding_style = extract_style_then_remove ("line_padding", text);
    if (line_padding_style) {
      factor = g_ascii_strtod (line_padding_style, NULL);       /* eg 0.5[c] */

      /* now convert from cell width to cario divice units */
      cell_res_x = overlay->cell_resolution_x;
      if (cell_res_x) {
        win_width = overlay->width;

        /* calculate number of pixels of line_padding required */
        cell_width = win_width / (double) cell_res_x;

        overlay->line_padding = (guint) (cell_width * factor);
      }
    }

    background_color_style = extract_style_then_remove ("background", text);
    if (background_color_style) {
      /* background_color_style should either be:
       * a distribtuionColorType:
       * eg #000000 or #000000FF (RGB or RGBA)
       *
       * or also accept the CSS colours
       * eg "black", "lightgray" etc
       *
       * requrie extra processing step for CSS colors
       */
#if 0
      if (strlen (background_color_style) < 10) {
        gchar *a = "FF";
        gchar *temp;
        /* missing a, assume no transparancy */
        temp = g_strconcat (background_color_style, a, NULL);
        g_free (background_color_style);
        background_color_style = temp;
      }
#endif


      overlay->background_color = background_color_style;
      overlay->want_background = TRUE;  /* if colour is supplied then we need to shade */
    }

    /* keep looping until all of the non pango styles have been removed */
    if (multi_row_align_style || text_align_style
        || cell_resolution_x_style || cell_resolution_y_style
        || line_padding_style || background_color_style) {
      found_non_pango = TRUE;
    } else {
      found_non_pango = FALSE;
    }
  } while (found_non_pango);
}


static gchar *
extract_attribute_value (const gchar * string, const gchar * attr_name)
{
  gchar *pointer1 = NULL;
  gchar *pointer2 = NULL;
  gchar *value = NULL;

  if ((pointer1 = g_strrstr (string, attr_name))) {
    pointer1 += strlen (attr_name);
    while (*pointer1 != '"') ++pointer1;
    pointer2 = ++pointer1;
    while (*pointer2 != '"') ++pointer2;
    value = g_strndup (pointer1, pointer2 - pointer1);
    /*g_print ("Value extracted: %s\n", value);*/
  }
  return value;
}


static GstBaseEbuttdOverlayRegion *
create_new_region (const gchar * description)
{
  GstBaseEbuttdOverlayRegion *r = g_new0 (GstBaseEbuttdOverlayRegion, 1);
  gchar *value = NULL;

  value = extract_attribute_value (description, "id");

  if ((value = extract_attribute_value (description, "origin"))) {
    gchar *c;
    r->origin_x = g_ascii_strtod (value, &c);
    while (!g_ascii_isdigit (*c) && *c != '+' && *c != '-') ++c;
    r->origin_y = g_ascii_strtod (c, NULL);
    /*g_print ("origin_x: %g   origin_y: %g\n", r->origin_x, r->origin_y);*/
    g_free (value);
  }

  if ((value = extract_attribute_value (description, "extent"))) {
    gchar *c;
    r->extent_w = g_ascii_strtod (value, &c);
    r->extent_w = (r->extent_w > 100.0) ? 100.0 : r->extent_w;
    while (!g_ascii_isdigit (*c) && *c != '+' && *c != '-') ++c;
    r->extent_h = g_ascii_strtod (c, NULL);
    r->extent_h = (r->extent_h > 100.0) ? 100.0 : r->extent_h;
    /*g_print ("extent_w: %g   extent_h: %g\n", r->extent_w, r->extent_h);*/
    g_free (value);
  }

  if ((value = extract_attribute_value (description, "display_align"))) {
    if (g_strcmp0 (value, "center") == 0)
      r->display_align = GST_BASE_EBUTTD_OVERLAY_DISPLAY_ALIGN_CENTER;
    else if (g_strcmp0 (value, "after") == 0)
      r->display_align = GST_BASE_EBUTTD_OVERLAY_DISPLAY_ALIGN_AFTER;
    else
      r->display_align = GST_BASE_EBUTTD_OVERLAY_DISPLAY_ALIGN_BEFORE;
    /*g_print ("display_align: %d\n", r->display_align);*/
    g_free (value);
  }

  if ((value = extract_attribute_value (description, "padding"))) {
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
    /*g_print ("padding_start: %g  padding_end: %g  padding_before: %g "
        "padding_after: %g\n", r->padding_start, r->padding_end,
        r->padding_before, r->padding_after);*/
    g_strfreev (decimals);
    g_free (value);
  }

  if ((value = extract_attribute_value (description, "writing_mode"))) {
    if (g_str_has_prefix (value, "rl"))
      r->writing_mode = GST_BASE_EBUTTD_OVERLAY_WRITING_MODE_RLTB;
    else if ((g_strcmp0 (value, "tbrl") == 0) || (g_strcmp0 (value, "tb") == 0))
      r->writing_mode = GST_BASE_EBUTTD_OVERLAY_WRITING_MODE_TBRL;
    else if (g_strcmp0 (value, "tblr") == 0)
      r->writing_mode = GST_BASE_EBUTTD_OVERLAY_WRITING_MODE_TBLR;
    else
      r->writing_mode = GST_BASE_EBUTTD_OVERLAY_WRITING_MODE_LRTB;
    /*g_print ("writing_mode: %d\n", r->writing_mode);*/
    g_free (value);
  }

  if ((value = extract_attribute_value (description, "show_background"))) {
    if (g_strcmp0 (value, "always") == 0)
      r->always_show_background = TRUE;
    /*g_print ("always_show_background: %d\n", r->always_show_background);*/
    g_free (value);
  }

  if ((value = extract_attribute_value (description, "overflow"))) {
    if (g_strcmp0 (value, "hidden") == 0)
      r->clip_contents = TRUE;
    /*g_print ("clip_contents: %d\n", r->clip_contents);*/
    g_free (value);
  }

  return r;
}

static GstBaseEbuttdOverlayStyle *
create_new_style (const gchar * description)
{
  GstBaseEbuttdOverlayStyle *s = g_new0 (GstBaseEbuttdOverlayStyle, 1);
  gchar *value = NULL;

  if ((value = extract_attribute_value (description, "direction"))) {
    if (g_strcmp0 (value, "rtl") == 0)
      s->text_direction = GST_BASE_EBUTTD_OVERLAY_TEXT_DIRECTION_RTL;
    else
      s->text_direction = GST_BASE_EBUTTD_OVERLAY_TEXT_DIRECTION_LTR;
    g_print ("direction: %d\n", s->text_direction);
    g_free (value);
  }

  if ((value = extract_attribute_value (description, "font_family"))) {
    s->font_family = g_strdup (value);
    /*g_print ("s->font_family: %s\n", s->font_family);*/
    g_free (value);
  }

  if ((value = extract_attribute_value (description, "font_size"))) {
    s->font_size = g_ascii_strtod (value, NULL);
    /*g_print ("s->font_size: %g\n", s->font_size);*/
    g_free (value);
  }

  if ((value = extract_attribute_value (description, "line_height"))) {
    if (g_strcmp0 (value, "normal") == 0)
      s->line_height = 0;
    else
      s->line_height = g_ascii_strtod (value, NULL);
    g_free (value);
    /*g_print ("s->line_height:  %g\n",s->line_height);*/
  }

  if ((value = extract_attribute_value (description, "text_align"))) {
    if (g_strcmp0 (value, "center") == 0)
      s->text_align = GST_BASE_EBUTTD_OVERLAY_TEXT_ALIGN_CENTER;
    else if (g_strcmp0 (value, "right") == 0)
      s->text_align = GST_BASE_EBUTTD_OVERLAY_TEXT_ALIGN_RIGHT;
    else if (g_strcmp0 (value, "start") == 0)
      s->text_align = GST_BASE_EBUTTD_OVERLAY_TEXT_ALIGN_START;
    else if (g_strcmp0 (value, "end") == 0)
      s->text_align = GST_BASE_EBUTTD_OVERLAY_TEXT_ALIGN_END;
    else
      s->text_align = GST_BASE_EBUTTD_OVERLAY_TEXT_ALIGN_LEFT;
    g_free (value);
    /*g_print ("s->text_align:  %d\n",s->text_align);*/
  }

  if ((value = extract_attribute_value (description, "foreground"))) {
    s->color = g_strdup (value);
    g_free (value);
  }

  if ((value = extract_attribute_value (description, "background"))) {
    s->bg_color = g_strdup (value);
    g_free (value);
  }

  if ((value = extract_attribute_value (description, "font_style"))) {
    if (g_strcmp0 (value, "italic") == 0)
      s->font_style = GST_BASE_EBUTTD_OVERLAY_FONT_STYLE_ITALIC;
    else
      s->font_style = GST_BASE_EBUTTD_OVERLAY_FONT_STYLE_NORMAL;
    g_free (value);
  }

  if ((value = extract_attribute_value (description, "font_weight"))) {
    if (g_strcmp0 (value, "bold") == 0)
      s->font_weight = GST_BASE_EBUTTD_OVERLAY_FONT_WEIGHT_BOLD;
    else
      s->font_weight = GST_BASE_EBUTTD_OVERLAY_FONT_WEIGHT_NORMAL;
    g_free (value);
  }

  if ((value = extract_attribute_value (description, "underline"))) {
    if (g_strcmp0 (value, "underline") == 0)
      s->text_decoration = GST_BASE_EBUTTD_OVERLAY_TEXT_DECORATION_UNDERLINE;
    else
      s->text_decoration = GST_BASE_EBUTTD_OVERLAY_TEXT_DECORATION_NONE;
    g_free (value);
  }

  if ((value = extract_attribute_value (description, "unicode_bidi"))) {
    if (g_strcmp0 (value, "embed") == 0)
      s->unicode_bidi = GST_BASE_EBUTTD_OVERLAY_UNICODE_BIDI_EMBED;
    else if (g_strcmp0 (value, "bidiOverride") == 0)
      s->unicode_bidi = GST_BASE_EBUTTD_OVERLAY_UNICODE_BIDI_OVERRIDE;
    else
      s->unicode_bidi = GST_BASE_EBUTTD_OVERLAY_UNICODE_BIDI_NORMAL;
    g_free (value);
  }

  if ((value = extract_attribute_value (description, "wrap_option"))) {
    if (g_strcmp0 (value, "wrap") == 0)
      s->wrap = TRUE;
    g_free (value);
  }

  if ((value = extract_attribute_value (description, "multi_row_align"))) {
    if (g_strcmp0 (value, "start") == 0)
      s->mult_row_align = GST_BASE_EBUTTD_OVERLAY_MULTI_ROW_ALIGN_START;
    else if (g_strcmp0 (value, "center") == 0)
      s->mult_row_align = GST_BASE_EBUTTD_OVERLAY_MULTI_ROW_ALIGN_CENTER;
    else if (g_strcmp0 (value, "end") == 0)
      s->mult_row_align = GST_BASE_EBUTTD_OVERLAY_MULTI_ROW_ALIGN_END;
    else
      s->mult_row_align = GST_BASE_EBUTTD_OVERLAY_MULTI_ROW_ALIGN_AUTO;
    g_free (value);
  }

  if ((value = extract_attribute_value (description, "line_padding"))) {
    s->line_padding = g_ascii_strtod (value, NULL);
    g_free (value);
  }

  if ((value = extract_attribute_value (description, "cell_resolution_x"))) {
    s->cellres_x = (guint) g_ascii_strtoull (value, NULL, 10);
    g_free (value);
  } else {
    s->cellres_x = 32U;
  }

  if ((value = extract_attribute_value (description, "cell_resolution_y"))) {
    s->cellres_y = (guint) g_ascii_strtoull (value, NULL, 10);
    g_free (value);
  } else {
    s->cellres_y = 15U;
  }

  return s;
}


static GstBaseEbuttdOverlayRegion *
extract_region (gchar ** text)
{
  gchar **lines = NULL;
  GstBaseEbuttdOverlayRegion *r = NULL;
  gchar *stripped_text = g_strdup ("");
  gint i;

  g_return_val_if_fail (text != NULL, NULL);

  lines = g_strsplit (*text, "\n", 0);
  if ((g_strv_length (lines) > 0) && g_str_has_prefix (lines[0], "<region")) {
    r = create_new_region (lines[0]);
  } else {
    g_print ("Error: region description missing from head of buffer.\n");
  }

  stripped_text = g_strjoinv ("\n", &lines[1]);
  g_free (*text);
  *text = stripped_text;
  g_strfreev (lines);
  return r;
}


static guint
extract_region_info (gchar ** text, GstBaseEbuttdOverlay * overlay)
{
  /* For each <region> tag, extract properties, create a new
   * GstBaseEbuttdOverlayRegion object and strip the region element from
   * text. Return the number of regions extracted. */
  guint n_found = 0U;
  gchar **lines = NULL;
  gchar **line = NULL;
  gchar *stripped_text = g_strdup ("");
  gchar *tmp = NULL;

  lines = g_strsplit (*text, "\n", 0);
  for (line = lines; *line != NULL; ++line) {
    if (!g_str_has_prefix (*line, "<region")) {
      tmp = stripped_text;
      stripped_text = g_strjoin ("\n", stripped_text, *line, NULL);
      g_free (tmp);
      /*g_print ("Found a non-region line in text; stripped text is now: %s\n",
          stripped_text);*/
    } else {
      /*g_print ("Found a region element in text.\n");*/
      GstBaseEbuttdOverlayRegion *r = create_new_region (*line);
      ++n_found;
    }
  }

  g_free (*text);
  *text = stripped_text;
  g_strfreev (lines);
  return n_found;
}


static GstFlowReturn
gst_base_ebuttd_overlay_video_chain (GstPad * pad, GstObject * parent,
    GstBuffer * buffer)
{
  GstBaseEbuttdOverlayClass *klass;
  GstBaseEbuttdOverlay *overlay;
  GstFlowReturn ret = GST_FLOW_OK;
  gboolean in_seg = FALSE;
  guint64 start, stop, clip_start = 0, clip_stop = 0;
  gchar *text = NULL;

  overlay = GST_BASE_EBUTTD_OVERLAY (parent);
  klass = GST_BASE_EBUTTD_OVERLAY_GET_CLASS (overlay);

  if (!GST_BUFFER_TIMESTAMP_IS_VALID (buffer))
    goto missing_timestamp;

  /* ignore buffers that are outside of the current segment */
  start = GST_BUFFER_TIMESTAMP (buffer);

  if (!GST_BUFFER_DURATION_IS_VALID (buffer)) {
    stop = GST_CLOCK_TIME_NONE;
  } else {
    stop = start + GST_BUFFER_DURATION (buffer);
  }

  GST_LOG_OBJECT (overlay, "%" GST_SEGMENT_FORMAT "  BUFFER: ts=%"
      GST_TIME_FORMAT ", end=%" GST_TIME_FORMAT, &overlay->segment,
      GST_TIME_ARGS (start), GST_TIME_ARGS (stop));

  /* segment_clip() will adjust start unconditionally to segment_start if
   * no stop time is provided, so handle this ourselves */
  if (stop == GST_CLOCK_TIME_NONE && start < overlay->segment.start)
    goto out_of_segment;

  in_seg = gst_segment_clip (&overlay->segment, GST_FORMAT_TIME, start, stop,
      &clip_start, &clip_stop);

  if (!in_seg)
    goto out_of_segment;

  /* if the buffer is only partially in the segment, fix up stamps */
  if (clip_start != start || (stop != -1 && clip_stop != stop)) {
    GST_DEBUG_OBJECT (overlay, "clipping buffer timestamp/duration to segment");
    buffer = gst_buffer_make_writable (buffer);
    GST_BUFFER_TIMESTAMP (buffer) = clip_start;
    if (stop != -1)
      GST_BUFFER_DURATION (buffer) = clip_stop - clip_start;
  }

  /* now, after we've done the clipping, fix up end time if there's no
   * duration (we only use those estimated values internally though, we
   * don't want to set bogus values on the buffer itself) */
  if (stop == -1) {
    if (overlay->info.fps_n && overlay->info.fps_d) {
      GST_DEBUG_OBJECT (overlay, "estimating duration based on framerate");
      stop = start + gst_util_uint64_scale_int (GST_SECOND,
          overlay->info.fps_d, overlay->info.fps_n);
    } else {
      GST_LOG_OBJECT (overlay, "no duration, assuming minimal duration");
      stop = start + 1;         /* we need to assume some interval */
    }
  }

  gst_object_sync_values (GST_OBJECT (overlay), GST_BUFFER_TIMESTAMP (buffer));

wait_for_text_buf:

  GST_BASE_EBUTTD_OVERLAY_LOCK (overlay);

  if (overlay->video_flushing)
    goto flushing;

  if (overlay->video_eos)
    goto have_eos;

  if (overlay->silent) {
    GST_BASE_EBUTTD_OVERLAY_UNLOCK (overlay);
    ret = gst_pad_push (overlay->srcpad, buffer);

    /* Update position */
    overlay->segment.position = clip_start;

    return ret;
  }

  /* Text pad not linked, rendering internal text */
  if (!overlay->text_linked) {
    if (klass->get_text) {
      text = klass->get_text (overlay, buffer);
    } else {
      text = g_strdup (overlay->default_text);
    }

    GST_LOG_OBJECT (overlay, "Text pad not linked, rendering default "
        "text: '%s'", GST_STR_NULL (text));

    GST_BASE_EBUTTD_OVERLAY_UNLOCK (overlay);

    if (text != NULL && *text != '\0') {
      /* Render and push */
      gst_base_ebuttd_overlay_render_text (overlay, text, -1);
      ret = gst_base_ebuttd_overlay_push_frame (overlay, buffer);
    } else {
      /* Invalid or empty string */
      ret = gst_pad_push (overlay->srcpad, buffer);
    }
  } else {
    /* Text pad linked, check if we have a text buffer queued */
    if (overlay->text_buffer) {
      gboolean pop_text = FALSE, valid_text_time = TRUE;
      GstClockTime text_start = GST_CLOCK_TIME_NONE;
      GstClockTime text_end = GST_CLOCK_TIME_NONE;
      GstClockTime text_running_time = GST_CLOCK_TIME_NONE;
      GstClockTime text_running_time_end = GST_CLOCK_TIME_NONE;
      GstClockTime vid_running_time, vid_running_time_end;

      /* if the text buffer isn't stamped right, pop it off the
       * queue and display it for the current video frame only */
      if (!GST_BUFFER_TIMESTAMP_IS_VALID (overlay->text_buffer) ||
          !GST_BUFFER_DURATION_IS_VALID (overlay->text_buffer)) {
        GST_WARNING_OBJECT (overlay,
            "Got text buffer with invalid timestamp or duration");
        pop_text = TRUE;
        valid_text_time = FALSE;
      } else {
        text_start = GST_BUFFER_TIMESTAMP (overlay->text_buffer);
        text_end = text_start + GST_BUFFER_DURATION (overlay->text_buffer);
      }

      vid_running_time =
          gst_segment_to_running_time (&overlay->segment, GST_FORMAT_TIME,
          start);
      vid_running_time_end =
          gst_segment_to_running_time (&overlay->segment, GST_FORMAT_TIME,
          stop);

      /* If timestamp and duration are valid */
      if (valid_text_time) {
        text_running_time =
            gst_segment_to_running_time (&overlay->text_segment,
            GST_FORMAT_TIME, text_start);
        text_running_time_end =
            gst_segment_to_running_time (&overlay->text_segment,
            GST_FORMAT_TIME, text_end);
      }

      GST_LOG_OBJECT (overlay, "T: %" GST_TIME_FORMAT " - %" GST_TIME_FORMAT,
          GST_TIME_ARGS (text_running_time),
          GST_TIME_ARGS (text_running_time_end));
      GST_LOG_OBJECT (overlay, "V: %" GST_TIME_FORMAT " - %" GST_TIME_FORMAT,
          GST_TIME_ARGS (vid_running_time),
          GST_TIME_ARGS (vid_running_time_end));

      /* Text too old or in the future */
      if (valid_text_time && text_running_time_end <= vid_running_time) {
        /* text buffer too old, get rid of it and do nothing  */
        GST_LOG_OBJECT (overlay, "text buffer too old, popping");
        pop_text = FALSE;
        gst_base_ebuttd_overlay_pop_text (overlay);
        GST_BASE_EBUTTD_OVERLAY_UNLOCK (overlay);
        goto wait_for_text_buf;
      } else if (valid_text_time && vid_running_time_end <= text_running_time) {
        GST_LOG_OBJECT (overlay, "text in future, pushing video buf");
        GST_BASE_EBUTTD_OVERLAY_UNLOCK (overlay);
        /* Push the video frame */
        ret = gst_pad_push (overlay->srcpad, buffer);
      } else {
        GstMapInfo map;
        gchar *in_text;
        gsize in_size;

        gst_buffer_map (overlay->text_buffer, &map, GST_MAP_READ);
        in_text = (gchar *) map.data;
        in_size = map.size;

        if (in_size > 0) {
          guint n_regions = 0U;
          GstBaseEbuttdOverlayRegion *region;
          GstBaseEbuttdOverlayStyle *style;
          /* g_markup_escape_text() absolutely requires valid UTF8 input, it
           * might crash otherwise. We don't fall back on GST_SUBTITLE_ENCODING
           * here on purpose, this is something that needs fixing upstream */
          if (!g_utf8_validate (in_text, in_size, NULL)) {
            const gchar *end = NULL;

            GST_WARNING_OBJECT (overlay, "received invalid UTF-8");
            in_text = g_strndup (in_text, in_size);
            while (!g_utf8_validate (in_text, in_size, &end) && end)
              *((gchar *) end) = '*';
          }

          /* Get the string */
          if (overlay->have_pango_markup) {
            text = g_strndup (in_text, in_size);
          } else {
            text = g_markup_escape_text (in_text, in_size);
          }

          /* P TAYLOUR */
          /* Extract region descriptions from text. */
          /*n_regions = extract_region_info (&text, overlay);*/
          region = extract_region (&text);
          style = create_new_style (text);

          /* extract non pango styles. Remove markup when done. */
          set_non_pango_markup (&text, overlay);
          /* create styles that need to be converted to pango markup now */
          convert_from_ebutt_to_pango (&text, overlay);

          if (text != NULL && *text != '\0') {
            gint text_len = strlen (text);

            while (text_len > 0 && (text[text_len - 1] == '\n' ||
                    text[text_len - 1] == '\r')) {
              --text_len;
            }
            GST_DEBUG_OBJECT (overlay, "Rendering text '%*s'", text_len, text);
            gst_base_ebuttd_overlay_render_text2 (overlay, text, text_len,
                region, style);
            g_free (region);
            g_free (style);
          } else {
            GST_DEBUG_OBJECT (overlay, "No text to render (empty buffer)");
            gst_base_ebuttd_overlay_render_text (overlay, " ", 1);
          }
          if (in_text != (gchar *) map.data)
            g_free (in_text);
        } else {
          GST_DEBUG_OBJECT (overlay, "No text to render (empty buffer)");
          gst_base_ebuttd_overlay_render_text (overlay, " ", 1);
        }

        gst_buffer_unmap (overlay->text_buffer, &map);

        GST_BASE_EBUTTD_OVERLAY_UNLOCK (overlay);
        ret = gst_base_ebuttd_overlay_push_frame (overlay, buffer);

        if (valid_text_time && text_running_time_end <= vid_running_time_end) {
          GST_LOG_OBJECT (overlay, "text buffer not needed any longer");
          pop_text = TRUE;
        }
      }
      if (pop_text) {
        GST_BASE_EBUTTD_OVERLAY_LOCK (overlay);
        gst_base_ebuttd_overlay_pop_text (overlay);
        GST_BASE_EBUTTD_OVERLAY_UNLOCK (overlay);
      }
    } else {
      gboolean wait_for_text_buf = TRUE;

      if (overlay->text_eos)
        wait_for_text_buf = FALSE;

      if (!overlay->wait_text)
        wait_for_text_buf = FALSE;

      /* Text pad linked, but no text buffer available - what now? */
      if (overlay->text_segment.format == GST_FORMAT_TIME) {
        GstClockTime text_start_running_time, text_position_running_time;
        GstClockTime vid_running_time;

        vid_running_time =
            gst_segment_to_running_time (&overlay->segment, GST_FORMAT_TIME,
            GST_BUFFER_TIMESTAMP (buffer));
        text_start_running_time =
            gst_segment_to_running_time (&overlay->text_segment,
            GST_FORMAT_TIME, overlay->text_segment.start);
        text_position_running_time =
            gst_segment_to_running_time (&overlay->text_segment,
            GST_FORMAT_TIME, overlay->text_segment.position);

        if ((GST_CLOCK_TIME_IS_VALID (text_start_running_time) &&
                vid_running_time < text_start_running_time) ||
            (GST_CLOCK_TIME_IS_VALID (text_position_running_time) &&
                vid_running_time < text_position_running_time)) {
          wait_for_text_buf = FALSE;
        }
      }

      if (wait_for_text_buf) {
        GST_DEBUG_OBJECT (overlay, "no text buffer, need to wait for one");
        GST_BASE_EBUTTD_OVERLAY_WAIT (overlay);
        GST_DEBUG_OBJECT (overlay, "resuming");
        GST_BASE_EBUTTD_OVERLAY_UNLOCK (overlay);
        goto wait_for_text_buf;
      } else {
        GST_BASE_EBUTTD_OVERLAY_UNLOCK (overlay);
        GST_LOG_OBJECT (overlay, "no need to wait for a text buffer");
        ret = gst_pad_push (overlay->srcpad, buffer);
      }
    }
  }

  g_free (text);

  /* Update position */
  overlay->segment.position = clip_start;

  return ret;

missing_timestamp:
  {
    GST_WARNING_OBJECT (overlay, "buffer without timestamp, discarding");
    gst_buffer_unref (buffer);
    return GST_FLOW_OK;
  }

flushing:
  {
    GST_BASE_EBUTTD_OVERLAY_UNLOCK (overlay);
    GST_DEBUG_OBJECT (overlay, "flushing, discarding buffer");
    gst_buffer_unref (buffer);
    return GST_FLOW_FLUSHING;
  }
have_eos:
  {
    GST_BASE_EBUTTD_OVERLAY_UNLOCK (overlay);
    GST_DEBUG_OBJECT (overlay, "eos, discarding buffer");
    gst_buffer_unref (buffer);
    return GST_FLOW_EOS;
  }
out_of_segment:
  {
    GST_DEBUG_OBJECT (overlay, "buffer out of segment, discarding");
    gst_buffer_unref (buffer);
    return GST_FLOW_OK;
  }
}

static GstStateChangeReturn
gst_base_ebuttd_overlay_change_state (GstElement * element,
    GstStateChange transition)
{
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
  GstBaseEbuttdOverlay *overlay = GST_BASE_EBUTTD_OVERLAY (element);

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      GST_BASE_EBUTTD_OVERLAY_LOCK (overlay);
      overlay->text_flushing = TRUE;
      overlay->video_flushing = TRUE;
      /* pop_text will broadcast on the GCond and thus also make the video
       * chain exit if it's waiting for a text buffer */
      gst_base_ebuttd_overlay_pop_text (overlay);
      GST_BASE_EBUTTD_OVERLAY_UNLOCK (overlay);
      break;
    default:
      break;
  }

  ret = parent_class->change_state (element, transition);
  if (ret == GST_STATE_CHANGE_FAILURE)
    return ret;

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      GST_BASE_EBUTTD_OVERLAY_LOCK (overlay);
      overlay->text_flushing = FALSE;
      overlay->video_flushing = FALSE;
      overlay->video_eos = FALSE;
      overlay->text_eos = FALSE;
      gst_segment_init (&overlay->segment, GST_FORMAT_TIME);
      gst_segment_init (&overlay->text_segment, GST_FORMAT_TIME);
      GST_BASE_EBUTTD_OVERLAY_UNLOCK (overlay);
      break;
    default:
      break;
  }

  return ret;
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  if (!gst_element_register (plugin, "ebuttdrender", GST_RANK_NONE,
          GST_TYPE_EBUTTD_OVERLAY)) {
    return FALSE;
  }

  GST_DEBUG_CATEGORY_INIT (ebuttd_render_debug, "ebuttdrender", 0, "EBU-TT-D- renderer");

  return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR, GST_VERSION_MINOR,
    ebuttdrender, "Pango-based text rendering and overlay, supporting EBU-TT_D subtitles.", plugin_init,
    VERSION, "LGPL", "gst-ebuttd-render", "http://www.bbc.co.uk/rd")
