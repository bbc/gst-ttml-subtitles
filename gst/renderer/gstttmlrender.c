/* GStreamer
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
 * Copyright (C) <2003> David Schleef <ds@schleef.org>
 * Copyright (C) <2006> Julien Moutte <julien@moutte.net>
 * Copyright (C) <2006> Zeeshan Ali <zeeshan.ali@nokia.com>
 * Copyright (C) <2006-2008> Tim-Philipp Müller <tim centricular net>
 * Copyright (C) <2009> Young-Ho Cha <ganadist@gmail.com>
 * Copyright (C) <2015> British Broadcasting Corporation <dash@rd.bbc.co.uk>
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

#include "gstttmlrender.h"
#include "gsttextoverlay.h"
#include <string.h>
#include <math.h>

/* FIXME:
 *  - use proper strides and offset for I420
 *  - if text is wider than the video picture, it does not get
 *    clipped properly during blitting (if wrapping is disabled)
 */

GST_DEBUG_CATEGORY_STATIC (ttmlrender);

#define DEFAULT_PROP_TEXT 	""
#define DEFAULT_PROP_SHADING	FALSE
#define DEFAULT_PROP_VALIGNMENT	GST_TTML_RENDER_VALIGN_BASELINE
#define DEFAULT_PROP_HALIGNMENT	GST_TTML_RENDER_HALIGN_CENTER
#define DEFAULT_PROP_XPAD	25
#define DEFAULT_PROP_YPAD	25
#define DEFAULT_PROP_DELTAX	0
#define DEFAULT_PROP_DELTAY	0
#define DEFAULT_PROP_XPOS       0.5
#define DEFAULT_PROP_YPOS       0.5
#define DEFAULT_PROP_WRAP_MODE  GST_TTML_RENDER_WRAP_MODE_WORD_CHAR
#define DEFAULT_PROP_FONT_DESC	""
#define DEFAULT_PROP_SILENT	FALSE
#define DEFAULT_PROP_LINE_ALIGNMENT GST_TTML_RENDER_LINE_ALIGN_CENTER
#define DEFAULT_PROP_WAIT_TEXT	TRUE
#define DEFAULT_PROP_AUTO_ADJUST_SIZE TRUE
#define DEFAULT_PROP_VERTICAL_RENDER  FALSE
#define DEFAULT_PROP_COLOR      0xffffffff
#define DEFAULT_PROP_OUTLINE_COLOR 0xff000000

#define DEFAULT_PROP_SHADING_VALUE    80

#define MINIMUM_OUTLINE_OFFSET 1.0
#define DEFAULT_SCALE_BASIS    640

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
  PROP_LAST
};

#define VIDEO_FORMATS GST_VIDEO_OVERLAY_COMPOSITION_BLEND_FORMATS

#define TTML_RENDER_CAPS GST_VIDEO_CAPS_MAKE (VIDEO_FORMATS)

#define TTML_RENDER_ALL_CAPS TTML_RENDER_CAPS ";" \
    GST_VIDEO_CAPS_MAKE_WITH_FEATURES ("ANY", GST_VIDEO_FORMATS_ALL)

static GstStaticCaps sw_template_caps =
GST_STATIC_CAPS (TTML_RENDER_CAPS);

static GstStaticPadTemplate src_template_factory =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (TTML_RENDER_ALL_CAPS)
    );

static GstStaticPadTemplate video_sink_template_factory =
GST_STATIC_PAD_TEMPLATE ("video_sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (TTML_RENDER_ALL_CAPS)
    );

#define GST_TYPE_TTML_RENDER_VALIGN (gst_ttml_render_valign_get_type())
static GType
gst_ttml_render_valign_get_type (void)
{
  static GType ttml_render_valign_type = 0;
  static const GEnumValue ttml_render_valign[] = {
    {GST_TTML_RENDER_VALIGN_BASELINE, "baseline", "baseline"},
    {GST_TTML_RENDER_VALIGN_BOTTOM, "bottom", "bottom"},
    {GST_TTML_RENDER_VALIGN_TOP, "top", "top"},
    {GST_TTML_RENDER_VALIGN_POS, "position", "position"},
    {GST_TTML_RENDER_VALIGN_CENTER, "center", "center"},
    {0, NULL, NULL},
  };

  if (!ttml_render_valign_type) {
    ttml_render_valign_type =
        g_enum_register_static ("GstTtmlRenderVAlign",
        ttml_render_valign);
  }
  return ttml_render_valign_type;
}

#define GST_TYPE_TTML_RENDER_HALIGN (gst_ttml_render_halign_get_type())
static GType
gst_ttml_render_halign_get_type (void)
{
  static GType ttml_render_halign_type = 0;
  static const GEnumValue ttml_render_halign[] = {
    {GST_TTML_RENDER_HALIGN_LEFT, "left", "left"},
    {GST_TTML_RENDER_HALIGN_CENTER, "center", "center"},
    {GST_TTML_RENDER_HALIGN_RIGHT, "right", "right"},
    {GST_TTML_RENDER_HALIGN_POS, "position", "position"},
    {0, NULL, NULL},
  };

  if (!ttml_render_halign_type) {
    ttml_render_halign_type =
        g_enum_register_static ("GstTtmlRenderHAlign",
        ttml_render_halign);
  }
  return ttml_render_halign_type;
}


#define GST_TYPE_TTML_RENDER_WRAP_MODE (gst_ttml_render_wrap_mode_get_type())
static GType
gst_ttml_render_wrap_mode_get_type (void)
{
  static GType ttml_render_wrap_mode_type = 0;
  static const GEnumValue ttml_render_wrap_mode[] = {
    {GST_TTML_RENDER_WRAP_MODE_NONE, "none", "none"},
    {GST_TTML_RENDER_WRAP_MODE_WORD, "word", "word"},
    {GST_TTML_RENDER_WRAP_MODE_CHAR, "char", "char"},
    {GST_TTML_RENDER_WRAP_MODE_WORD_CHAR, "wordchar", "wordchar"},
    {0, NULL, NULL},
  };

  if (!ttml_render_wrap_mode_type) {
    ttml_render_wrap_mode_type =
        g_enum_register_static ("GstTtmlRenderWrapMode",
        ttml_render_wrap_mode);
  }
  return ttml_render_wrap_mode_type;
}

#define GST_TYPE_TTML_RENDER_LINE_ALIGN (gst_ttml_render_line_align_get_type())
static GType
gst_ttml_render_line_align_get_type (void)
{
  static GType ttml_render_line_align_type = 0;
  static const GEnumValue ttml_render_line_align[] = {
    {GST_TTML_RENDER_LINE_ALIGN_LEFT, "left", "left"},
    {GST_TTML_RENDER_LINE_ALIGN_CENTER, "center", "center"},
    {GST_TTML_RENDER_LINE_ALIGN_RIGHT, "right", "right"},
    {0, NULL, NULL}
  };

  if (!ttml_render_line_align_type) {
    ttml_render_line_align_type =
        g_enum_register_static ("GstTtmlRenderLineAlign",
        ttml_render_line_align);
  }
  return ttml_render_line_align_type;
}

#define GST_TTML_RENDER_GET_LOCK(ov) (&GST_TTML_RENDER (ov)->lock)
#define GST_TTML_RENDER_GET_COND(ov) (&GST_TTML_RENDER (ov)->cond)
#define GST_TTML_RENDER_LOCK(ov)     (g_mutex_lock (GST_TTML_RENDER_GET_LOCK (ov)))
#define GST_TTML_RENDER_UNLOCK(ov)   (g_mutex_unlock (GST_TTML_RENDER_GET_LOCK (ov)))
#define GST_TTML_RENDER_WAIT(ov)     (g_cond_wait (GST_TTML_RENDER_GET_COND (ov), GST_TTML_RENDER_GET_LOCK (ov)))
#define GST_TTML_RENDER_SIGNAL(ov)   (g_cond_signal (GST_TTML_RENDER_GET_COND (ov)))
#define GST_TTML_RENDER_BROADCAST(ov)(g_cond_broadcast (GST_TTML_RENDER_GET_COND (ov)))

static GstElementClass *parent_class = NULL;
static void gst_ttml_render_base_init (gpointer g_class);
static void gst_ttml_render_class_init (GstTtmlRenderClass * klass);
static void gst_ttml_render_init (GstTtmlRender * render,
    GstTtmlRenderClass * klass);

static GstStateChangeReturn gst_ttml_render_change_state (GstElement *
    element, GstStateChange transition);

static GstCaps *gst_ttml_render_get_videosink_caps (GstPad * pad,
    GstTtmlRender * render, GstCaps * filter);
static GstCaps *gst_ttml_render_get_src_caps (GstPad * pad,
    GstTtmlRender * render, GstCaps * filter);
static gboolean gst_ttml_render_setcaps (GstTtmlRender * render,
    GstCaps * caps);
static gboolean gst_ttml_render_setcaps_txt (GstTtmlRender * render,
    GstCaps * caps);
static gboolean gst_ttml_render_src_event (GstPad * pad,
    GstObject * parent, GstEvent * event);
static gboolean gst_ttml_render_src_query (GstPad * pad,
    GstObject * parent, GstQuery * query);

static gboolean gst_ttml_render_video_event (GstPad * pad,
    GstObject * parent, GstEvent * event);
static gboolean gst_ttml_render_video_query (GstPad * pad,
    GstObject * parent, GstQuery * query);
static GstFlowReturn gst_ttml_render_video_chain (GstPad * pad,
    GstObject * parent, GstBuffer * buffer);

static gboolean gst_ttml_render_text_event (GstPad * pad,
    GstObject * parent, GstEvent * event);
static GstFlowReturn gst_ttml_render_text_chain (GstPad * pad,
    GstObject * parent, GstBuffer * buffer);
static GstPadLinkReturn gst_ttml_render_text_pad_link (GstPad * pad,
    GstObject * parent, GstPad * peer);
static void gst_ttml_render_text_pad_unlink (GstPad * pad,
    GstObject * parent);
static void gst_ttml_render_pop_text (GstTtmlRender * render);
static void gst_ttml_render_update_render_mode (GstTtmlRender *
    render);

static void gst_ttml_render_finalize (GObject * object);
static void gst_ttml_render_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_ttml_render_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static void
gst_ttml_render_adjust_values_with_fontdesc (GstTtmlRender * render,
    PangoFontDescription * desc);
static gboolean gst_ttml_render_can_handle_caps (GstCaps * incaps);


static gboolean gst_text_overlay_filter_foreground_attr (PangoAttribute * attr,
    gpointer data);

static GstTtmlRenderRenderedImage * rendered_image_new (GstBuffer * image,
    gint x, gint y, guint width, guint height);
static GstTtmlRenderRenderedImage * rendered_image_new_empty ();

GType
gst_ttml_render_get_type (void)
{
  static GType type = 0;

  if (g_once_init_enter ((gsize *) & type)) {
    static const GTypeInfo info = {
      sizeof (GstTtmlRenderClass),
      (GBaseInitFunc) gst_ttml_render_base_init,
      NULL,
      (GClassInitFunc) gst_ttml_render_class_init,
      NULL,
      NULL,
      sizeof (GstTtmlRender),
      0,
      (GInstanceInitFunc) gst_ttml_render_init,
    };

    g_once_init_leave ((gsize *) & type,
        g_type_register_static (GST_TYPE_ELEMENT, "GstTtmlRender", &info,
            0));
  }

  return type;
}

static gchar *
gst_ttml_render_get_text (GstTtmlRender * render,
    GstBuffer * video_frame)
{
  return g_strdup (render->default_text);
}

static void
gst_ttml_render_base_init (gpointer g_class)
{
  GstTtmlRenderClass *klass = GST_TTML_RENDER_CLASS (g_class);
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
gst_ttml_render_class_init (GstTtmlRenderClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  parent_class = g_type_class_peek_parent (klass);

  gobject_class->finalize = gst_ttml_render_finalize;
  gobject_class->set_property = gst_ttml_render_set_property;
  gobject_class->get_property = gst_ttml_render_get_property;

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&src_template_factory));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&video_sink_template_factory));

  gstelement_class->change_state =
      GST_DEBUG_FUNCPTR (gst_ttml_render_change_state);

  klass->pango_lock = g_slice_new (GMutex);
  g_mutex_init (klass->pango_lock);

  klass->get_text = gst_ttml_render_get_text;

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
          "Vertical alignment of the text", GST_TYPE_TTML_RENDER_VALIGN,
          DEFAULT_PROP_VALIGNMENT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_HALIGNMENT,
      g_param_spec_enum ("halignment", "horizontal alignment",
          "Horizontal alignment of the text", GST_TYPE_TTML_RENDER_HALIGN,
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
   * GstTtmlRender:xpos:
   *
   * Horizontal position of the rendered text when using positioned alignment.
   */
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_XPOS,
      g_param_spec_double ("xpos", "horizontal position",
          "Horizontal position when using position alignment", 0, 1.0,
          DEFAULT_PROP_XPOS,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
  /**
   * GstTtmlRender:ypos:
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
          GST_TYPE_TTML_RENDER_WRAP_MODE, DEFAULT_PROP_WRAP_MODE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_FONT_DESC,
      g_param_spec_string ("font-desc", "font description",
          "Pango font description of font to be used for rendering. "
          "See documentation of pango_font_description_from_string "
          "for syntax.", DEFAULT_PROP_FONT_DESC,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  /**
   * GstTtmlRender:color:
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
   * GstTtmlRender:line-alignment:
   *
   * Alignment of text lines relative to each other (for multi-line text)
   */
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_LINE_ALIGNMENT,
      g_param_spec_enum ("line-alignment", "line alignment",
          "Alignment of text lines relative to each other.",
          GST_TYPE_TTML_RENDER_LINE_ALIGN, DEFAULT_PROP_LINE_ALIGNMENT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  /**
   * GstTtmlRender:silent:
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
   * GstTtmlRender:wait-text:
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
}

static void
gst_ttml_render_finalize (GObject * object)
{
  GstTtmlRender *render = GST_TTML_RENDER (object);

  g_free (render->default_text);

  if (render->composition) {
    gst_video_overlay_composition_unref (render->composition);
    render->composition = NULL;
  }

  if (render->compositions) {
    g_list_free_full (render->compositions,
        (GDestroyNotify) gst_video_overlay_composition_unref);
    render->compositions = NULL;
  }

  if (render->text_image) {
    gst_buffer_unref (render->text_image);
    render->text_image = NULL;
  }

  if (render->layout) {
    g_object_unref (render->layout);
    render->layout = NULL;
  }

  if (render->text_buffer) {
    gst_buffer_unref (render->text_buffer);
    render->text_buffer = NULL;
  }

  g_mutex_clear (&render->lock);
  g_cond_clear (&render->cond);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_ttml_render_init (GstTtmlRender * render,
    GstTtmlRenderClass * klass)
{
  GstPadTemplate *template;
  PangoFontDescription *desc;

  /* video sink */
  template = gst_static_pad_template_get (&video_sink_template_factory);
  render->video_sinkpad = gst_pad_new_from_template (template, "video_sink");
  gst_object_unref (template);
  gst_pad_set_event_function (render->video_sinkpad,
      GST_DEBUG_FUNCPTR (gst_ttml_render_video_event));
  gst_pad_set_chain_function (render->video_sinkpad,
      GST_DEBUG_FUNCPTR (gst_ttml_render_video_chain));
  gst_pad_set_query_function (render->video_sinkpad,
      GST_DEBUG_FUNCPTR (gst_ttml_render_video_query));
  GST_PAD_SET_PROXY_ALLOCATION (render->video_sinkpad);
  gst_element_add_pad (GST_ELEMENT (render), render->video_sinkpad);

  template =
      gst_element_class_get_pad_template (GST_ELEMENT_CLASS (klass),
      "text_sink");
  if (template) {
    /* text sink */
    render->text_sinkpad = gst_pad_new_from_template (template, "text_sink");

    gst_pad_set_event_function (render->text_sinkpad,
        GST_DEBUG_FUNCPTR (gst_ttml_render_text_event));
    gst_pad_set_chain_function (render->text_sinkpad,
        GST_DEBUG_FUNCPTR (gst_ttml_render_text_chain));
    gst_pad_set_link_function (render->text_sinkpad,
        GST_DEBUG_FUNCPTR (gst_ttml_render_text_pad_link));
    gst_pad_set_unlink_function (render->text_sinkpad,
        GST_DEBUG_FUNCPTR (gst_ttml_render_text_pad_unlink));
    gst_element_add_pad (GST_ELEMENT (render), render->text_sinkpad);
  }

  /* (video) source */
  template = gst_static_pad_template_get (&src_template_factory);
  render->srcpad = gst_pad_new_from_template (template, "src");
  gst_object_unref (template);
  gst_pad_set_event_function (render->srcpad,
      GST_DEBUG_FUNCPTR (gst_ttml_render_src_event));
  gst_pad_set_query_function (render->srcpad,
      GST_DEBUG_FUNCPTR (gst_ttml_render_src_query));
  gst_element_add_pad (GST_ELEMENT (render), render->srcpad);

  g_mutex_lock (GST_TTML_RENDER_GET_CLASS (render)->pango_lock);
  render->line_align = DEFAULT_PROP_LINE_ALIGNMENT;
  render->layout =
      pango_layout_new (GST_TTML_RENDER_GET_CLASS
      (render)->pango_context);
  desc =
      pango_context_get_font_description (GST_TTML_RENDER_GET_CLASS
      (render)->pango_context);
  gst_ttml_render_adjust_values_with_fontdesc (render, desc);

  render->color = DEFAULT_PROP_COLOR;
  render->outline_color = DEFAULT_PROP_OUTLINE_COLOR;
  render->halign = DEFAULT_PROP_HALIGNMENT;
  render->valign = DEFAULT_PROP_VALIGNMENT;
  render->xpad = DEFAULT_PROP_XPAD;
  render->ypad = DEFAULT_PROP_YPAD;
  render->deltax = DEFAULT_PROP_DELTAX;
  render->deltay = DEFAULT_PROP_DELTAY;
  render->xpos = DEFAULT_PROP_XPOS;
  render->ypos = DEFAULT_PROP_YPOS;

  render->wrap_mode = DEFAULT_PROP_WRAP_MODE;

  render->want_shading = DEFAULT_PROP_SHADING;
  render->shading_value = DEFAULT_PROP_SHADING_VALUE;
  render->silent = DEFAULT_PROP_SILENT;
  render->wait_text = DEFAULT_PROP_WAIT_TEXT;
  render->auto_adjust_size = DEFAULT_PROP_AUTO_ADJUST_SIZE;

  render->default_text = g_strdup (DEFAULT_PROP_TEXT);
  render->need_render = TRUE;
  render->text_image = NULL;
  render->use_vertical_render = DEFAULT_PROP_VERTICAL_RENDER;
  gst_ttml_render_update_render_mode (render);

  render->text_buffer = NULL;
  render->text_linked = FALSE;

  render->compositions = NULL;

  g_mutex_init (&render->lock);
  g_cond_init (&render->cond);
  gst_segment_init (&render->segment, GST_FORMAT_TIME);
  g_mutex_unlock (GST_TTML_RENDER_GET_CLASS (render)->pango_lock);
}


static void
gst_ttml_render_update_wrap_mode (GstTtmlRender * render)
{
  if (render->wrap_mode == GST_TTML_RENDER_WRAP_MODE_NONE) {
    GST_DEBUG_OBJECT (render, "Set wrap mode NONE");
    pango_layout_set_width (render->layout, -1);
  } else {
    int width;

    if (render->auto_adjust_size) {
      width = DEFAULT_SCALE_BASIS * PANGO_SCALE;
      if (render->use_vertical_render) {
        width = width * (render->height - render->ypad * 2) / render->width;
      }
    } else {
      width =
          (render->use_vertical_render ? render->height : render->width) *
          PANGO_SCALE;
    }

    GST_DEBUG_OBJECT (render, "Set layout width %d", render->width);
    GST_DEBUG_OBJECT (render, "Set wrap mode    %d", render->wrap_mode);
    pango_layout_set_width (render->layout, width);
    pango_layout_set_wrap (render->layout, (PangoWrapMode) render->wrap_mode);
  }
}

static void
gst_ttml_render_update_render_mode (GstTtmlRender * render)
{
  PangoMatrix matrix = PANGO_MATRIX_INIT;
  PangoContext *context = pango_layout_get_context (render->layout);

  if (render->use_vertical_render) {
    pango_matrix_rotate (&matrix, -90);
    pango_context_set_base_gravity (context, PANGO_GRAVITY_AUTO);
    pango_context_set_matrix (context, &matrix);
    pango_layout_set_alignment (render->layout, PANGO_ALIGN_LEFT);
  } else {
    pango_context_set_base_gravity (context, PANGO_GRAVITY_SOUTH);
    pango_context_set_matrix (context, &matrix);
    pango_layout_set_alignment (render->layout,
        (PangoAlignment) render->line_align);
  }
}

static gboolean
gst_ttml_render_setcaps_txt (GstTtmlRender * render, GstCaps * caps)
{
  GstStructure *structure;
  const gchar *format;

  structure = gst_caps_get_structure (caps, 0);
  format = gst_structure_get_string (structure, "format");
  render->have_pango_markup = format && (strcmp (format, "pango-markup") == 0);

  return TRUE;
}

/* only negotiate/query video render composition support for now */
static gboolean
gst_ttml_render_negotiate (GstTtmlRender * render, GstCaps * caps)
{
  GstQuery *query;
  gboolean attach = FALSE;
  gboolean caps_has_meta = TRUE;
  gboolean ret;
  GstCapsFeatures *f;
  GstCaps *original_caps;
  gboolean original_has_meta = FALSE;
  gboolean allocation_ret = TRUE;

  GST_DEBUG_OBJECT (render, "performing negotiation");

  if (!caps)
    caps = gst_pad_get_current_caps (render->video_sinkpad);
  else
    gst_caps_ref (caps);

  if (!caps || gst_caps_is_empty (caps))
    goto no_format;

  original_caps = caps;

  /* Try to use the render meta if possible */
  f = gst_caps_get_features (caps, 0);

  /* if the caps doesn't have the render meta, we query if downstream
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

    ret = gst_pad_peer_query_accept_caps (render->srcpad, overlay_caps);
    GST_DEBUG_OBJECT (render, "Downstream accepts the render meta: %d", ret);
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
  GST_DEBUG_OBJECT (render, "Using caps %" GST_PTR_FORMAT, caps);
  ret = gst_pad_set_caps (render->srcpad, caps);

  if (ret) {
    /* find supported meta */
    query = gst_query_new_allocation (caps, FALSE);

    if (!gst_pad_peer_query (render->srcpad, query)) {
      /* no problem, we use the query defaults */
      GST_DEBUG_OBJECT (render, "ALLOCATION query failed");
      allocation_ret = FALSE;
    }

    if (caps_has_meta && gst_query_find_allocation_meta (query,
            GST_VIDEO_OVERLAY_COMPOSITION_META_API_TYPE, NULL))
      attach = TRUE;

    gst_query_unref (query);
  }

  render->attach_compo_to_buffer = attach;

  if (!allocation_ret && render->video_flushing) {
    ret = FALSE;
  } else if (original_caps && !original_has_meta && !attach) {
    if (caps_has_meta) {
      /* Some elements (fakesink) claim to accept the meta on caps but won't
         put it in the allocation query result, this leads below
         check to fail. Prevent this by removing the meta from caps */
      gst_caps_unref (caps);
      caps = gst_caps_ref (original_caps);
      ret = gst_pad_set_caps (render->srcpad, caps);
      if (ret && !gst_ttml_render_can_handle_caps (caps))
        ret = FALSE;
    }
  }

  if (!ret) {
    GST_DEBUG_OBJECT (render, "negotiation failed, schedule reconfigure");
    gst_pad_mark_reconfigure (render->srcpad);
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
gst_ttml_render_can_handle_caps (GstCaps * incaps)
{
  gboolean ret;
  GstCaps *caps;
  static GstStaticCaps static_caps = GST_STATIC_CAPS (TTML_RENDER_CAPS);

  caps = gst_static_caps_get (&static_caps);
  ret = gst_caps_is_subset (incaps, caps);
  gst_caps_unref (caps);

  return ret;
}

static gboolean
gst_ttml_render_setcaps (GstTtmlRender * render, GstCaps * caps)
{
  GstVideoInfo info;
  gboolean ret = FALSE;

  if (!gst_video_info_from_caps (&info, caps))
    goto invalid_caps;

  render->info = info;
  render->format = GST_VIDEO_INFO_FORMAT (&info);
  render->width = GST_VIDEO_INFO_WIDTH (&info);
  render->height = GST_VIDEO_INFO_HEIGHT (&info);

  ret = gst_ttml_render_negotiate (render, caps);

  GST_TTML_RENDER_LOCK (render);
  g_mutex_lock (GST_TTML_RENDER_GET_CLASS (render)->pango_lock);
  if (!render->attach_compo_to_buffer &&
      !gst_ttml_render_can_handle_caps (caps)) {
    GST_DEBUG_OBJECT (render, "unsupported caps %" GST_PTR_FORMAT, caps);
    ret = FALSE;
  }

  gst_ttml_render_update_wrap_mode (render);
  g_mutex_unlock (GST_TTML_RENDER_GET_CLASS (render)->pango_lock);
  GST_TTML_RENDER_UNLOCK (render);

  return ret;

  /* ERRORS */
invalid_caps:
  {
    GST_DEBUG_OBJECT (render, "could not parse caps");
    return FALSE;
  }
}

static void
gst_ttml_render_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstTtmlRender *render = GST_TTML_RENDER (object);

  GST_TTML_RENDER_LOCK (render);
  switch (prop_id) {
    case PROP_TEXT:
      g_free (render->default_text);
      render->default_text = g_value_dup_string (value);
      render->need_render = TRUE;
      break;
    case PROP_SHADING:
      render->want_shading = g_value_get_boolean (value);
      break;
    case PROP_XPAD:
      render->xpad = g_value_get_int (value);
      break;
    case PROP_YPAD:
      render->ypad = g_value_get_int (value);
      break;
    case PROP_DELTAX:
      render->deltax = g_value_get_int (value);
      break;
    case PROP_DELTAY:
      render->deltay = g_value_get_int (value);
      break;
    case PROP_XPOS:
      render->xpos = g_value_get_double (value);
      break;
    case PROP_YPOS:
      render->ypos = g_value_get_double (value);
      break;
    case PROP_VALIGNMENT:
      render->valign = g_value_get_enum (value);
      break;
    case PROP_HALIGNMENT:
      render->halign = g_value_get_enum (value);
      break;
    case PROP_WRAP_MODE:
      render->wrap_mode = g_value_get_enum (value);
      g_mutex_lock (GST_TTML_RENDER_GET_CLASS (render)->pango_lock);
      gst_ttml_render_update_wrap_mode (render);
      g_mutex_unlock (GST_TTML_RENDER_GET_CLASS (render)->pango_lock);
      break;
    case PROP_FONT_DESC:
    {
      PangoFontDescription *desc;
      const gchar *fontdesc_str;

      fontdesc_str = g_value_get_string (value);
      g_mutex_lock (GST_TTML_RENDER_GET_CLASS (render)->pango_lock);
      desc = pango_font_description_from_string (fontdesc_str);
      if (desc) {
        GST_LOG_OBJECT (render, "font description set: %s", fontdesc_str);
        pango_layout_set_font_description (render->layout, desc);
        gst_ttml_render_adjust_values_with_fontdesc (render, desc);
        pango_font_description_free (desc);
      } else {
        GST_WARNING_OBJECT (render, "font description parse failed: %s",
            fontdesc_str);
      }
      g_mutex_unlock (GST_TTML_RENDER_GET_CLASS (render)->pango_lock);
      break;
    }
    case PROP_COLOR:
      render->color = g_value_get_uint (value);
      break;
    case PROP_OUTLINE_COLOR:
      render->outline_color = g_value_get_uint (value);
      break;
    case PROP_SILENT:
      render->silent = g_value_get_boolean (value);
      break;
    case PROP_LINE_ALIGNMENT:
      render->line_align = g_value_get_enum (value);
      g_mutex_lock (GST_TTML_RENDER_GET_CLASS (render)->pango_lock);
      pango_layout_set_alignment (render->layout,
          (PangoAlignment) render->line_align);
      g_mutex_unlock (GST_TTML_RENDER_GET_CLASS (render)->pango_lock);
      break;
    case PROP_WAIT_TEXT:
      render->wait_text = g_value_get_boolean (value);
      break;
    case PROP_AUTO_ADJUST_SIZE:
      render->auto_adjust_size = g_value_get_boolean (value);
      render->need_render = TRUE;
      break;
    case PROP_VERTICAL_RENDER:
      render->use_vertical_render = g_value_get_boolean (value);
      g_mutex_lock (GST_TTML_RENDER_GET_CLASS (render)->pango_lock);
      gst_ttml_render_update_render_mode (render);
      g_mutex_unlock (GST_TTML_RENDER_GET_CLASS (render)->pango_lock);
      render->need_render = TRUE;
      break;
    case PROP_SHADING_VALUE:
      render->shading_value = g_value_get_uint (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

  render->need_render = TRUE;
  GST_TTML_RENDER_UNLOCK (render);
}

static void
gst_ttml_render_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstTtmlRender *render = GST_TTML_RENDER (object);

  GST_TTML_RENDER_LOCK (render);
  switch (prop_id) {
    case PROP_TEXT:
      g_value_set_string (value, render->default_text);
      break;
    case PROP_SHADING:
      g_value_set_boolean (value, render->want_shading);
      break;
    case PROP_XPAD:
      g_value_set_int (value, render->xpad);
      break;
    case PROP_YPAD:
      g_value_set_int (value, render->ypad);
      break;
    case PROP_DELTAX:
      g_value_set_int (value, render->deltax);
      break;
    case PROP_DELTAY:
      g_value_set_int (value, render->deltay);
      break;
    case PROP_XPOS:
      g_value_set_double (value, render->xpos);
      break;
    case PROP_YPOS:
      g_value_set_double (value, render->ypos);
      break;
    case PROP_VALIGNMENT:
      g_value_set_enum (value, render->valign);
      break;
    case PROP_HALIGNMENT:
      g_value_set_enum (value, render->halign);
      break;
    case PROP_WRAP_MODE:
      g_value_set_enum (value, render->wrap_mode);
      break;
    case PROP_SILENT:
      g_value_set_boolean (value, render->silent);
      break;
    case PROP_LINE_ALIGNMENT:
      g_value_set_enum (value, render->line_align);
      break;
    case PROP_WAIT_TEXT:
      g_value_set_boolean (value, render->wait_text);
      break;
    case PROP_AUTO_ADJUST_SIZE:
      g_value_set_boolean (value, render->auto_adjust_size);
      break;
    case PROP_VERTICAL_RENDER:
      g_value_set_boolean (value, render->use_vertical_render);
      break;
    case PROP_COLOR:
      g_value_set_uint (value, render->color);
      break;
    case PROP_OUTLINE_COLOR:
      g_value_set_uint (value, render->outline_color);
      break;
    case PROP_SHADING_VALUE:
      g_value_set_uint (value, render->shading_value);
      break;
    case PROP_FONT_DESC:
    {
      const PangoFontDescription *desc;

      g_mutex_lock (GST_TTML_RENDER_GET_CLASS (render)->pango_lock);
      desc = pango_layout_get_font_description (render->layout);
      if (!desc)
        g_value_set_string (value, "");
      else {
        g_value_take_string (value, pango_font_description_to_string (desc));
      }
      g_mutex_unlock (GST_TTML_RENDER_GET_CLASS (render)->pango_lock);
      break;
    }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

  render->need_render = TRUE;
  GST_TTML_RENDER_UNLOCK (render);
}

static gboolean
gst_ttml_render_src_query (GstPad * pad, GstObject * parent,
    GstQuery * query)
{
  gboolean ret = FALSE;
  GstTtmlRender *render;

  render = GST_TTML_RENDER (parent);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CAPS:
    {
      GstCaps *filter, *caps;

      gst_query_parse_caps (query, &filter);
      caps = gst_ttml_render_get_src_caps (pad, render, filter);
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
gst_ttml_render_src_event (GstPad * pad, GstObject * parent,
    GstEvent * event)
{
  GstTtmlRender *render;
  gboolean ret;

  render = GST_TTML_RENDER (parent);

  if (render->text_linked) {
    gst_event_ref (event);
    ret = gst_pad_push_event (render->video_sinkpad, event);
    gst_pad_push_event (render->text_sinkpad, event);
  } else {
    ret = gst_pad_push_event (render->video_sinkpad, event);
  }

  return ret;
}

/**
 * gst_ttml_render_add_feature_and_intersect:
 *
 * Creates a new #GstCaps containing the (given caps +
 * given caps feature) + (given caps intersected by the
 * given filter).
 *
 * Returns: the new #GstCaps
 */
static GstCaps *
gst_ttml_render_add_feature_and_intersect (GstCaps * caps,
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
 * gst_ttml_render_intersect_by_feature:
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
gst_ttml_render_intersect_by_feature (GstCaps * caps,
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
gst_ttml_render_get_videosink_caps (GstPad * pad,
    GstTtmlRender * render, GstCaps * filter)
{
  GstPad *srcpad = render->srcpad;
  GstCaps *peer_caps = NULL, *caps = NULL, *overlay_filter = NULL;

  if (G_UNLIKELY (!render))
    return gst_pad_get_pad_template_caps (pad);

  if (filter) {
    /* filter caps + composition feature + filter caps
     * filtered by the software caps. */
    GstCaps *sw_caps = gst_static_caps_get (&sw_template_caps);
    overlay_filter = gst_ttml_render_add_feature_and_intersect (filter,
        GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION, sw_caps);
    gst_caps_unref (sw_caps);

    GST_DEBUG_OBJECT (render, "render filter %" GST_PTR_FORMAT,
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
      caps = gst_ttml_render_intersect_by_feature (peer_caps,
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

  GST_DEBUG_OBJECT (render, "returning  %" GST_PTR_FORMAT, caps);

  return caps;
}

static GstCaps *
gst_ttml_render_get_src_caps (GstPad * pad, GstTtmlRender * render,
    GstCaps * filter)
{
  GstPad *sinkpad = render->video_sinkpad;
  GstCaps *peer_caps = NULL, *caps = NULL, *overlay_filter = NULL;

  if (G_UNLIKELY (!render))
    return gst_pad_get_pad_template_caps (pad);

  if (filter) {
    /* duplicate filter caps which contains the composition into one version
     * with the meta and one without. Filter the other caps by the software
     * caps */
    GstCaps *sw_caps = gst_static_caps_get (&sw_template_caps);
    overlay_filter =
        gst_ttml_render_intersect_by_feature (filter,
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
      caps = gst_ttml_render_add_feature_and_intersect (peer_caps,
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
  GST_DEBUG_OBJECT (render, "returning  %" GST_PTR_FORMAT, caps);

  return caps;
}

static void
gst_ttml_render_adjust_values_with_fontdesc (GstTtmlRender * render,
    PangoFontDescription * desc)
{
  gint font_size = pango_font_description_get_size (desc) / PANGO_SCALE;
  render->shadow_offset = (double) (font_size) / 13.0;
  render->outline_offset = (double) (font_size) / 15.0;
  if (render->outline_offset < MINIMUM_OUTLINE_OFFSET)
    render->outline_offset = MINIMUM_OUTLINE_OFFSET;
}

static void
gst_ttml_render_get_pos (GstTtmlRender * render,
    gint * xpos, gint * ypos)
{
  gint width, height;
  GstTtmlRenderVAlign valign;
  GstTtmlRenderHAlign halign;

  width = render->image_width;
  height = render->image_height;

  if (render->use_vertical_render)
    halign = GST_TTML_RENDER_HALIGN_RIGHT;
  else
    halign = render->halign;

  switch (halign) {
    case GST_TTML_RENDER_HALIGN_LEFT:
      *xpos = render->xpad;
      break;
    case GST_TTML_RENDER_HALIGN_CENTER:
      *xpos = (render->width - width) / 2;
      break;
    case GST_TTML_RENDER_HALIGN_RIGHT:
      *xpos = render->width - width - render->xpad;
      break;
    case GST_TTML_RENDER_HALIGN_POS:
      *xpos = (gint) (render->width * render->xpos) - width / 2;
      *xpos = CLAMP (*xpos, 0, render->width - width);
      if (*xpos < 0)
        *xpos = 0;
      break;
    default:
      *xpos = 0;
  }
  *xpos += render->deltax;

  if (render->use_vertical_render)
    valign = GST_TTML_RENDER_VALIGN_TOP;
  else
    valign = render->valign;

  switch (valign) {
    case GST_TTML_RENDER_VALIGN_BOTTOM:
      *ypos = render->height - height - render->ypad;
      break;
    case GST_TTML_RENDER_VALIGN_BASELINE:
      *ypos = render->height - (height + render->ypad);
      break;
    case GST_TTML_RENDER_VALIGN_TOP:
      *ypos = render->ypad;
      break;
    case GST_TTML_RENDER_VALIGN_POS:
      *ypos = (gint) (render->height * render->ypos) - height / 2;
      *ypos = CLAMP (*ypos, 0, render->height - height);
      break;
    case GST_TTML_RENDER_VALIGN_CENTER:
      *ypos = (render->height - height) / 2;
      break;
    default:
      *ypos = render->ypad;
      break;
  }
  *ypos += render->deltay;
}

static inline void
gst_ttml_render_set_composition (GstTtmlRender * render)
{
  gint xpos, ypos;
  GstVideoOverlayRectangle *rectangle;

  gst_ttml_render_get_pos (render, &xpos, &ypos);

  if (render->text_image) {
    g_assert (gst_buffer_is_writable (render->text_image));
    gst_buffer_add_video_meta (render->text_image, GST_VIDEO_FRAME_FLAG_NONE,
        GST_VIDEO_OVERLAY_COMPOSITION_FORMAT_RGB,
        render->image_width, render->image_height);
    rectangle = gst_video_overlay_rectangle_new_raw (render->text_image,
        xpos, ypos, render->image_width, render->image_height,
        GST_VIDEO_OVERLAY_FORMAT_FLAG_PREMULTIPLIED_ALPHA);

    if (render->composition)
      gst_video_overlay_composition_unref (render->composition);

    render->composition = gst_video_overlay_composition_new (rectangle);
  } else if (render->composition) {
    gst_video_overlay_composition_unref (render->composition);
    render->composition = NULL;
  }
}


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
gst_ttml_render_render_pangocairo (GstTtmlRender * render,
    const gchar * string, gint textlen)
{
  cairo_t *cr;
  cairo_surface_t *surface;
  PangoRectangle ink_rect, logical_rect;
  cairo_matrix_t cairo_matrix;
  int width, height;
  double scalef = 1.0;
  double a, r, g, b;
  GstBuffer *buffer;
  GstMapInfo map;

  GST_CAT_DEBUG (ttmlrender, "Input string: %s", string);
  g_mutex_lock (GST_TTML_RENDER_GET_CLASS (render)->pango_lock);

  if (render->auto_adjust_size) {
    /* 640 pixel is default */
    scalef = (double) (render->width) / DEFAULT_SCALE_BASIS;
  }
  pango_layout_set_width (render->layout, -1);
  /* set text on pango layout */
  pango_layout_set_markup (render->layout, string, textlen);

  /* get subtitle image size */
  pango_layout_get_pixel_extents (render->layout, &ink_rect, &logical_rect);
  width = (logical_rect.width + render->shadow_offset) * scalef;

  if (width + render->deltax >
      (render->use_vertical_render ? render->height : render->width)) {
    /*
     * subtitle image width is larger then render width
     * so rearrange render wrap mode.
     */
    gst_ttml_render_update_wrap_mode (render);
    pango_layout_get_pixel_extents (render->layout, &ink_rect, &logical_rect);
    width = render->width;
  }

  height =
      (logical_rect.height + logical_rect.y + render->shadow_offset) * scalef;
  if (height > render->height) {
    height = render->height;
  }
  if (render->use_vertical_render) {
    PangoRectangle rect;
    PangoContext *context;
    PangoMatrix matrix = PANGO_MATRIX_INIT;
    int tmp;

    context = pango_layout_get_context (render->layout);

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

  /* reallocate render buffer */
  buffer = gst_buffer_new_allocate (NULL, 4 * width * height, NULL);
  gst_buffer_replace (&render->text_image, buffer);
  gst_buffer_unref (buffer);

  gst_buffer_map (buffer, &map, GST_MAP_READWRITE);
  surface = cairo_image_surface_create_for_data (map.data,
      CAIRO_FORMAT_ARGB32, width, height, width * 4);
  cr = cairo_create (surface);

  /* clear surface */
  cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
  cairo_paint (cr);

  cairo_set_operator (cr, CAIRO_OPERATOR_OVER);

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

  a = (render->outline_color >> 24) & 0xff;
  r = (render->outline_color >> 16) & 0xff;
  g = (render->outline_color >> 8) & 0xff;
  b = (render->outline_color >> 0) & 0xff;

  /* draw outline text */
  cairo_save (cr);
  cairo_set_source_rgba (cr, r / 255.0, g / 255.0, b / 255.0, a / 255.0);
  cairo_set_line_width (cr, render->outline_offset);
  pango_cairo_layout_path (cr, render->layout);
  cairo_stroke (cr);
  cairo_restore (cr);

  a = (render->color >> 24) & 0xff;
  r = (render->color >> 16) & 0xff;
  g = (render->color >> 8) & 0xff;
  b = (render->color >> 0) & 0xff;

  /* draw text */
  cairo_save (cr);
  cairo_set_source_rgba (cr, r / 255.0, g / 255.0, b / 255.0, a / 255.0);
  pango_cairo_show_layout (cr, render->layout);
  cairo_restore (cr);

  cairo_destroy (cr);
  cairo_surface_destroy (surface);
  gst_buffer_unmap (buffer, &map);
  render->image_width = width;
  render->image_height = height;
  render->baseline_y = ink_rect.y;
  g_mutex_unlock (GST_TTML_RENDER_GET_CLASS (render)->pango_lock);

  gst_ttml_render_set_composition (render);
}


static inline void
gst_ttml_render_shade_planar_Y (GstTtmlRender * render,
    GstVideoFrame * dest, gint x0, gint x1, gint y0, gint y1)
{
  gint i, j, dest_stride;
  guint8 *dest_ptr;

  dest_stride = dest->info.stride[0];
  dest_ptr = dest->data[0];

  for (i = y0; i < y1; ++i) {
    for (j = x0; j < x1; ++j) {
      gint y = dest_ptr[(i * dest_stride) + j] - render->shading_value;

      dest_ptr[(i * dest_stride) + j] = CLAMP (y, 0, 255);
    }
  }
}

static inline void
gst_ttml_render_shade_packed_Y (GstTtmlRender * render,
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
      y = dest_ptr[y_pos] - render->shading_value;

      dest_ptr[y_pos] = CLAMP (y, 0, 255);
    }
  }
}

#define gst_ttml_render_shade_BGRx gst_ttml_render_shade_xRGB
#define gst_ttml_render_shade_RGBx gst_ttml_render_shade_xRGB
#define gst_ttml_render_shade_xBGR gst_ttml_render_shade_xRGB
static inline void
gst_ttml_render_shade_xRGB (GstTtmlRender * render,
    GstVideoFrame * dest, gint x0, gint x1, gint y0, gint y1)
{
  gint i, j;
  guint8 *dest_ptr;

  dest_ptr = dest->data[0];

  for (i = y0; i < y1; i++) {
    for (j = x0; j < x1; j++) {
      gint y, y_pos, k;

      y_pos = (i * 4 * render->width) + j * 4;
      for (k = 0; k < 4; k++) {
        y = dest_ptr[y_pos + k] - render->shading_value;
        dest_ptr[y_pos + k] = CLAMP (y, 0, 255);
      }
    }
  }
}

/* FIXME: orcify */
static void
gst_ttml_render_shade_rgb24 (GstTtmlRender * render,
    GstVideoFrame * frame, gint x0, gint x1, gint y0, gint y1)
{
  const int pstride = 3;
  gint y, x, stride, shading_val, tmp;
  guint8 *p;

  shading_val = -render->shading_value;
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
gst_ttml_render_shade_IYU1 (GstTtmlRender * render,
    GstVideoFrame * frame, gint x0, gint x1, gint y0, gint y1)
{
  gint y, x, stride, shading_val, tmp;
  guint8 *p;

  shading_val = -render->shading_value;
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
gst_ttml_render_shade_##name (GstTtmlRender * render, GstVideoFrame * dest, \
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
      y_pos = (i * 4 * render->width) + j * 4;\
      for (k = OFFSET; k < 3+OFFSET; k++) {\
        y = dest_ptr[y_pos + k] - render->shading_value;\
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
gst_ttml_render_render_text (GstTtmlRender * render,
    const gchar * text, gint textlen)
{
  gchar *string;

  if (!render->need_render) {
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
  gst_ttml_render_render_pangocairo (render, string, textlen);

  g_free (string);

  render->need_render = FALSE;
}


/* FIXME: should probably be relative to width/height (adjusted for PAR) */
#define BOX_XPAD  6
#define BOX_YPAD  6

static void
gst_ttml_render_shade_background (GstTtmlRender * render,
    GstVideoFrame * frame, gint x0, gint x1, gint y0, gint y1)
{
  x0 = CLAMP (x0 - BOX_XPAD, 0, render->width);
  x1 = CLAMP (x1 + BOX_XPAD, 0, render->width);

  y0 = CLAMP (y0 - BOX_YPAD, 0, render->height);
  y1 = CLAMP (y1 + BOX_YPAD, 0, render->height);

  switch (render->format) {
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
      gst_ttml_render_shade_planar_Y (render, frame, x0, x1, y0, y1);
      break;
    case GST_VIDEO_FORMAT_AYUV:
    case GST_VIDEO_FORMAT_UYVY:
    case GST_VIDEO_FORMAT_YUY2:
    case GST_VIDEO_FORMAT_v308:
      gst_ttml_render_shade_packed_Y (render, frame, x0, x1, y0, y1);
      break;
    case GST_VIDEO_FORMAT_xRGB:
      gst_ttml_render_shade_xRGB (render, frame, x0, x1, y0, y1);
      break;
    case GST_VIDEO_FORMAT_xBGR:
      gst_ttml_render_shade_xBGR (render, frame, x0, x1, y0, y1);
      break;
    case GST_VIDEO_FORMAT_BGRx:
      gst_ttml_render_shade_BGRx (render, frame, x0, x1, y0, y1);
      break;
    case GST_VIDEO_FORMAT_RGBx:
      gst_ttml_render_shade_RGBx (render, frame, x0, x1, y0, y1);
      break;
    case GST_VIDEO_FORMAT_ARGB:
      gst_ttml_render_shade_ARGB (render, frame, x0, x1, y0, y1);
      break;
    case GST_VIDEO_FORMAT_ABGR:
      gst_ttml_render_shade_ABGR (render, frame, x0, x1, y0, y1);
      break;
    case GST_VIDEO_FORMAT_RGBA:
      gst_ttml_render_shade_RGBA (render, frame, x0, x1, y0, y1);
      break;
    case GST_VIDEO_FORMAT_BGRA:
      gst_ttml_render_shade_BGRA (render, frame, x0, x1, y0, y1);
      break;
    case GST_VIDEO_FORMAT_BGR:
    case GST_VIDEO_FORMAT_RGB:
      gst_ttml_render_shade_rgb24 (render, frame, x0, x1, y0, y1);
      break;
    case GST_VIDEO_FORMAT_IYU1:
      gst_ttml_render_shade_IYU1 (render, frame, x0, x1, y0, y1);
      break;
    default:
      GST_FIXME_OBJECT (render, "implement background shading for format %s",
          gst_video_format_to_string (GST_VIDEO_FRAME_FORMAT (frame)));
      break;
  }
}

static GstFlowReturn
gst_ttml_render_push_frame (GstTtmlRender * render,
    GstBuffer * video_frame)
{
  GstVideoFrame frame;
  GList *compositions = render->compositions;

  if (compositions == NULL) {
    GST_CAT_DEBUG (ttmlrender, "No compositions.");
    goto done;
  }

  if (gst_pad_check_reconfigure (render->srcpad))
    gst_ttml_render_negotiate (render, NULL);

  video_frame = gst_buffer_make_writable (video_frame);

  if (render->attach_compo_to_buffer) {
    GST_DEBUG_OBJECT (render, "Attaching text render images to video buffer");
    gst_buffer_add_video_overlay_composition_meta (video_frame,
        render->composition);
    /* FIXME: emulate shaded background box if want_shading=true */
    goto done;
  }

  if (!gst_video_frame_map (&frame, &render->info, video_frame,
          GST_MAP_READWRITE))
    goto invalid_frame;

  while (compositions) {
    GstVideoOverlayComposition *composition = compositions->data;
    gst_video_overlay_composition_blend (composition, &frame);
    compositions = compositions->next;
  }

  gst_video_frame_unmap (&frame);

done:

  return gst_pad_push (render->srcpad, video_frame);

  /* ERRORS */
invalid_frame:
  {
    gst_buffer_unref (video_frame);
    GST_DEBUG_OBJECT (render, "received invalid buffer");
    return GST_FLOW_OK;
  }
}

static GstPadLinkReturn
gst_ttml_render_text_pad_link (GstPad * pad, GstObject * parent,
    GstPad * peer)
{
  GstTtmlRender *render;

  render = GST_TTML_RENDER (parent);
  if (G_UNLIKELY (!render))
    return GST_PAD_LINK_REFUSED;

  GST_DEBUG_OBJECT (render, "Text pad linked");

  render->text_linked = TRUE;

  return GST_PAD_LINK_OK;
}

static void
gst_ttml_render_text_pad_unlink (GstPad * pad, GstObject * parent)
{
  GstTtmlRender *render;

  /* don't use gst_pad_get_parent() here, will deadlock */
  render = GST_TTML_RENDER (parent);

  GST_DEBUG_OBJECT (render, "Text pad unlinked");

  render->text_linked = FALSE;

  gst_segment_init (&render->text_segment, GST_FORMAT_UNDEFINED);
}

static gboolean
gst_ttml_render_text_event (GstPad * pad, GstObject * parent,
    GstEvent * event)
{
  gboolean ret = FALSE;
  GstTtmlRender *render = NULL;

  render = GST_TTML_RENDER (parent);

  GST_LOG_OBJECT (pad, "received event %s", GST_EVENT_TYPE_NAME (event));

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CAPS:
    {
      GstCaps *caps;

      gst_event_parse_caps (event, &caps);
      ret = gst_ttml_render_setcaps_txt (render, caps);
      gst_event_unref (event);
      break;
    }
    case GST_EVENT_SEGMENT:
    {
      const GstSegment *segment;

      render->text_eos = FALSE;

      gst_event_parse_segment (event, &segment);

      if (segment->format == GST_FORMAT_TIME) {
        GST_TTML_RENDER_LOCK (render);
        gst_segment_copy_into (segment, &render->text_segment);
        GST_DEBUG_OBJECT (render, "TEXT SEGMENT now: %" GST_SEGMENT_FORMAT,
            &render->text_segment);
        GST_TTML_RENDER_UNLOCK (render);
      } else {
        GST_ELEMENT_WARNING (render, STREAM, MUX, (NULL),
            ("received non-TIME newsegment event on text input"));
      }

      gst_event_unref (event);
      ret = TRUE;

      /* wake up the video chain, it might be waiting for a text buffer or
       * a text segment update */
      GST_TTML_RENDER_LOCK (render);
      GST_TTML_RENDER_BROADCAST (render);
      GST_TTML_RENDER_UNLOCK (render);
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
      render->text_segment.position = start;

      /* wake up the video chain, it might be waiting for a text buffer or
       * a text segment update */
      GST_TTML_RENDER_LOCK (render);
      GST_TTML_RENDER_BROADCAST (render);
      GST_TTML_RENDER_UNLOCK (render);

      gst_event_unref (event);
      ret = TRUE;
      break;
    }
    case GST_EVENT_FLUSH_STOP:
      GST_TTML_RENDER_LOCK (render);
      GST_INFO_OBJECT (render, "text flush stop");
      render->text_flushing = FALSE;
      render->text_eos = FALSE;
      gst_ttml_render_pop_text (render);
      gst_segment_init (&render->text_segment, GST_FORMAT_TIME);
      GST_TTML_RENDER_UNLOCK (render);
      gst_event_unref (event);
      ret = TRUE;
      break;
    case GST_EVENT_FLUSH_START:
      GST_TTML_RENDER_LOCK (render);
      GST_INFO_OBJECT (render, "text flush start");
      render->text_flushing = TRUE;
      GST_TTML_RENDER_BROADCAST (render);
      GST_TTML_RENDER_UNLOCK (render);
      gst_event_unref (event);
      ret = TRUE;
      break;
    case GST_EVENT_EOS:
      GST_TTML_RENDER_LOCK (render);
      render->text_eos = TRUE;
      GST_INFO_OBJECT (render, "text EOS");
      /* wake up the video chain, it might be waiting for a text buffer or
       * a text segment update */
      GST_TTML_RENDER_BROADCAST (render);
      GST_TTML_RENDER_UNLOCK (render);
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
gst_ttml_render_video_event (GstPad * pad, GstObject * parent,
    GstEvent * event)
{
  gboolean ret = FALSE;
  GstTtmlRender *render = NULL;

  render = GST_TTML_RENDER (parent);

  GST_DEBUG_OBJECT (pad, "received event %s", GST_EVENT_TYPE_NAME (event));

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CAPS:
    {
      GstCaps *caps;

      gst_event_parse_caps (event, &caps);
      ret = gst_ttml_render_setcaps (render, caps);
      gst_event_unref (event);
      break;
    }
    case GST_EVENT_SEGMENT:
    {
      const GstSegment *segment;

      GST_DEBUG_OBJECT (render, "received new segment");

      gst_event_parse_segment (event, &segment);

      if (segment->format == GST_FORMAT_TIME) {
        GST_DEBUG_OBJECT (render, "VIDEO SEGMENT now: %" GST_SEGMENT_FORMAT,
            &render->segment);

        gst_segment_copy_into (segment, &render->segment);
      } else {
        GST_ELEMENT_WARNING (render, STREAM, MUX, (NULL),
            ("received non-TIME newsegment event on video input"));
      }

      ret = gst_pad_event_default (pad, parent, event);
      break;
    }
    case GST_EVENT_EOS:
      GST_TTML_RENDER_LOCK (render);
      GST_INFO_OBJECT (render, "video EOS");
      render->video_eos = TRUE;
      GST_TTML_RENDER_UNLOCK (render);
      ret = gst_pad_event_default (pad, parent, event);
      break;
    case GST_EVENT_FLUSH_START:
      GST_TTML_RENDER_LOCK (render);
      GST_INFO_OBJECT (render, "video flush start");
      render->video_flushing = TRUE;
      GST_TTML_RENDER_BROADCAST (render);
      GST_TTML_RENDER_UNLOCK (render);
      ret = gst_pad_event_default (pad, parent, event);
      break;
    case GST_EVENT_FLUSH_STOP:
      GST_TTML_RENDER_LOCK (render);
      GST_INFO_OBJECT (render, "video flush stop");
      render->video_flushing = FALSE;
      render->video_eos = FALSE;
      gst_segment_init (&render->segment, GST_FORMAT_TIME);
      GST_TTML_RENDER_UNLOCK (render);
      ret = gst_pad_event_default (pad, parent, event);
      break;
    default:
      ret = gst_pad_event_default (pad, parent, event);
      break;
  }

  return ret;
}

static gboolean
gst_ttml_render_video_query (GstPad * pad, GstObject * parent,
    GstQuery * query)
{
  gboolean ret = FALSE;
  GstTtmlRender *render;

  render = GST_TTML_RENDER (parent);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CAPS:
    {
      GstCaps *filter, *caps;

      gst_query_parse_caps (query, &filter);
      caps = gst_ttml_render_get_videosink_caps (pad, render, filter);
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
gst_ttml_render_pop_text (GstTtmlRender * render)
{
  g_return_if_fail (GST_IS_TTML_RENDER (render));

  if (render->text_buffer) {
    GST_DEBUG_OBJECT (render, "releasing text buffer %p",
        render->text_buffer);
    gst_buffer_unref (render->text_buffer);
    render->text_buffer = NULL;
  }

  /* Let the text task know we used that buffer */
  GST_TTML_RENDER_BROADCAST (render);
}

/* We receive text buffers here. If they are out of segment we just ignore them.
   If the buffer is in our segment we keep it internally except if another one
   is already waiting here, in that case we wait that it gets kicked out */
static GstFlowReturn
gst_ttml_render_text_chain (GstPad * pad, GstObject * parent,
    GstBuffer * buffer)
{
  GstFlowReturn ret = GST_FLOW_OK;
  GstTtmlRender *render = NULL;
  gboolean in_seg = FALSE;
  guint64 clip_start = 0, clip_stop = 0;

  render = GST_TTML_RENDER (parent);

  GST_TTML_RENDER_LOCK (render);

  if (render->text_flushing) {
    GST_TTML_RENDER_UNLOCK (render);
    ret = GST_FLOW_FLUSHING;
    GST_LOG_OBJECT (render, "text flushing");
    goto beach;
  }

  if (render->text_eos) {
    GST_TTML_RENDER_UNLOCK (render);
    ret = GST_FLOW_EOS;
    GST_LOG_OBJECT (render, "text EOS");
    goto beach;
  }

  GST_LOG_OBJECT (render, "%" GST_SEGMENT_FORMAT "  BUFFER: ts=%"
      GST_TIME_FORMAT ", end=%" GST_TIME_FORMAT, &render->segment,
      GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (buffer)),
      GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (buffer) +
          GST_BUFFER_DURATION (buffer)));

  if (G_LIKELY (GST_BUFFER_TIMESTAMP_IS_VALID (buffer))) {
    GstClockTime stop;

    if (G_LIKELY (GST_BUFFER_DURATION_IS_VALID (buffer)))
      stop = GST_BUFFER_TIMESTAMP (buffer) + GST_BUFFER_DURATION (buffer);
    else
      stop = GST_CLOCK_TIME_NONE;

    in_seg = gst_segment_clip (&render->text_segment, GST_FORMAT_TIME,
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
    while (render->text_buffer != NULL) {
      GST_DEBUG ("Pad %s:%s has a buffer queued, waiting",
          GST_DEBUG_PAD_NAME (pad));
      GST_TTML_RENDER_WAIT (render);
      GST_DEBUG ("Pad %s:%s resuming", GST_DEBUG_PAD_NAME (pad));
      if (render->text_flushing) {
        GST_TTML_RENDER_UNLOCK (render);
        ret = GST_FLOW_FLUSHING;
        goto beach;
      }
    }

    if (GST_BUFFER_TIMESTAMP_IS_VALID (buffer))
      render->text_segment.position = clip_start;

    render->text_buffer = buffer;
    /* That's a new text buffer we need to render */
    render->need_render = TRUE;

    /* in case the video chain is waiting for a text buffer, wake it up */
    GST_TTML_RENDER_BROADCAST (render);
  }

  GST_TTML_RENDER_UNLOCK (render);

beach:

  return ret;
}


static gchar *
color_to_rgb_string (GstSubtitleColor color)
{
  return g_strdup_printf ("#%02x%02x%02x", color.r, color.g, color.b);
}


static GstBuffer *
draw_rectangle (guint width, guint height, GstSubtitleColor color)
{
  GstMapInfo map;
  cairo_surface_t *surface;
  cairo_t *cairo_state;
  GstBuffer *buffer = gst_buffer_new_allocate (NULL, 4 * width * height, NULL);

  gst_buffer_map (buffer, &map, GST_MAP_READWRITE);
  surface = cairo_image_surface_create_for_data (map.data,
      CAIRO_FORMAT_ARGB32, width, height, width * 4);
  cairo_state = cairo_create (surface);

  /* clear surface */
  cairo_set_operator (cairo_state, CAIRO_OPERATOR_CLEAR);
  cairo_paint (cairo_state);
  cairo_set_operator (cairo_state, CAIRO_OPERATOR_OVER);

  cairo_save (cairo_state);
  cairo_set_source_rgba (cairo_state, color.r/255.0, color.g/255.0,
      color.b/255.0, color.a/255.0);
  cairo_paint (cairo_state);
  cairo_restore (cairo_state);
  cairo_destroy (cairo_state);
  cairo_surface_destroy (surface);
  gst_buffer_unmap (buffer, &map);

  return buffer;
}


typedef struct {
  guint first_char;
  guint last_char;
} TextRange;

static void
text_range_free (TextRange * range)
{
  g_slice_free (TextRange, range);
}

static gchar *
generate_marked_up_string (GstTtmlRender * render,
    GPtrArray * elements, GstBuffer * text_buf, GPtrArray ** text_ranges)
{
  GstSubtitleElement *element;
  GstMemory *mem;
  GstMapInfo map;
  gchar *buf_text, *joined_text, *old_text;
  gchar *fgcolor, *font_size, *font_family, *font_style, *font_weight,
        *underline;
  guint total_text_length = 0U;
  guint i;

  joined_text = g_strdup ("");

  if (*text_ranges != NULL)
    g_ptr_array_unref (*text_ranges);
  *text_ranges = g_ptr_array_new_full (elements->len,
      (GDestroyNotify) text_range_free);

  for (i = 0; i < elements->len; ++i) {
    TextRange *range = g_slice_new0 (TextRange);
    element = g_ptr_array_index (elements, i);
    mem = gst_buffer_get_memory (text_buf, element->text_index);
    if (!mem || !gst_memory_map (mem, &map, GST_MAP_READ)) {
      GST_CAT_ERROR (ttmlrender, "Failed to access element memory.");
      g_slice_free (TextRange, range);
      continue;
    }

    buf_text = g_strndup ((const gchar *)map.data, map.size);
    if (!g_utf8_validate (buf_text, -1, NULL)) {
      GST_CAT_ERROR (ttmlrender, "Text in buffer us not valid UTF-8");
      gst_memory_unmap (mem, &map);
      gst_memory_unref (mem);
    }
    GST_CAT_DEBUG (ttmlrender, "Text from buffer is: %s", buf_text);

    range->first_char = total_text_length;

    fgcolor = color_to_rgb_string (element->style.color);
    /* XXX: Should we round the pixel font size? */
    font_size = g_strdup_printf ("%u",
        (guint) (element->style.font_size * render->height));
    font_family = (g_strcmp0 (element->style.font_family, "default") == 0) ?
      "Monospace" : element->style.font_family;
    font_style = (element->style.font_style == GST_SUBTITLE_FONT_STYLE_NORMAL) ?
      "normal" : "italic";
    font_weight =
      (element->style.font_weight == GST_SUBTITLE_FONT_WEIGHT_NORMAL) ?
      "normal" : "bold";
    underline = (element->style.text_decoration
        == GST_SUBTITLE_TEXT_DECORATION_UNDERLINE) ? "single" : "none";

    old_text = joined_text;
    joined_text = g_strconcat (joined_text,
        "<span "
          "fgcolor=\"", fgcolor, "\" ",
          "font=\"", font_size, "px\" ",
          "font_family=\"", font_family, "\" ",
          "font_style=\"", font_style, "\" ",
          "font_weight=\"", font_weight, "\" ",
          "underline=\"", underline, "\" ",
        ">", buf_text, "</span>", NULL);
    GST_CAT_DEBUG (ttmlrender, "Joined text is now: %s", joined_text);

    total_text_length += strlen (buf_text);
    range->last_char = total_text_length - 1;
    GST_CAT_DEBUG (ttmlrender, "First character index: %u; last character  "
        "index: %u", range->first_char, range->last_char);
    g_ptr_array_insert (*text_ranges, i, range);

    if (old_text) g_free (old_text);
    g_free (buf_text);
    g_free (fgcolor);
    g_free (font_size);
    gst_memory_unmap (mem, &map);
    gst_memory_unref (mem);
  }

  return joined_text;
}


static GstTtmlRenderRenderedText *
draw_text (GstTtmlRender * render, const gchar * text, guint max_width,
    PangoAlignment alignment, guint line_height, guint max_font_size,
    gboolean wrap)
{
  GstTtmlRenderClass *class;
  GstTtmlRenderRenderedText *ret;
  cairo_surface_t *surface, *cropped_surface;
  cairo_t *cairo_state, *cropped_state;
  GstMapInfo map;
  PangoRectangle logical_rect;
  gdouble cur_height;
  gint spacing = 0;
  guint buf_width, buf_height;
  gdouble cur_spacing;
  gdouble padding;
  guint vertical_offset;
  gint stride;

  ret = g_slice_new0 (GstTtmlRenderRenderedText);
  ret->text_image = rendered_image_new_empty ();

  class = GST_TTML_RENDER_GET_CLASS (render);
  ret->layout = pango_layout_new (class->pango_context);

  pango_layout_set_markup (ret->layout, text, strlen (text));
  GST_CAT_DEBUG (ttmlrender, "Layout text: %s",
      pango_layout_get_text (ret->layout));
  if (wrap) {
    pango_layout_set_width (ret->layout, max_width * PANGO_SCALE);
    pango_layout_set_wrap (ret->layout, PANGO_WRAP_WORD_CHAR);
  } else {
    pango_layout_set_width (ret->layout, -1);
  }

  pango_layout_set_alignment (ret->layout, alignment);
  pango_layout_get_pixel_extents (ret->layout, NULL, &logical_rect);

  /* XXX: Is this the best way to do it? Could we alternatively find the
   * extents of the first line? */
  /* XXX: This will only really work if PangoLayout has spaced all lines by the
   * same amount, which might not be the case if there are multiple spans with
   * different sized fonts - need to test. */
  cur_height = (gdouble)logical_rect.height
    / pango_layout_get_line_count (ret->layout);
  cur_spacing = cur_height - (gdouble)max_font_size;
  padding = (line_height - max_font_size)/2.0;
  spacing =
    (gint) round ((gdouble)line_height - (gdouble)max_font_size - cur_spacing);

  /* Offset text downwards by 0.1 * max_font_size is to ensure that text looks
   * optically in the correct position relative to it's background box. Without
   * this downward shift, the text looks too high. */
  vertical_offset =
    (guint) round ((padding - cur_spacing) + (0.1 * max_font_size));
  GST_CAT_LOG (ttmlrender, "offset: %g   spacing: %d", cur_spacing,
      spacing);
  GST_CAT_LOG (ttmlrender, "Requested line_height: %u", line_height);
  pango_layout_set_spacing (ret->layout, PANGO_SCALE * spacing);
  GST_CAT_LOG (ttmlrender, "Line spacing set to %d",
      pango_layout_get_spacing (ret->layout) / PANGO_SCALE);

  pango_layout_get_pixel_extents (ret->layout, NULL, &logical_rect);
  GST_CAT_DEBUG (ttmlrender, "logical_rect.x: %d   logical_rect.y: %d   "
      "logical_rect.width: %d   logical_rect.height: %d", logical_rect.x,
      logical_rect.y, logical_rect.width, logical_rect.height);

  /* Create surface for pango layout to render into. */
  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
      (logical_rect.x + logical_rect.width),
      (logical_rect.y + logical_rect.height));
  cairo_state = cairo_create (surface);
  cairo_set_operator (cairo_state, CAIRO_OPERATOR_CLEAR);
  cairo_paint (cairo_state);
  cairo_set_operator (cairo_state, CAIRO_OPERATOR_OVER);

  /* Render layout. */
  cairo_save (cairo_state);
  pango_cairo_show_layout (cairo_state, ret->layout);
  cairo_restore (cairo_state);

  buf_width = logical_rect.width;
  buf_height = logical_rect.height + vertical_offset;
  GST_CAT_DEBUG (ttmlrender, "Output buffer width: %u  height: %u",
      buf_width, buf_height);

  /* Depending on whether the text is wrapped and its alignment, the image
   * created by rendering a PangoLayout will contain more than just the
   * rendered text: it may also contain blankspace around the rendered text.
   * The following code crops blankspace from around the rendered text,
   * returning only the rendered text itself in a GstBuffer. */
  /* TODO: move into a separate function? */
  ret->text_image->image =
    gst_buffer_new_allocate (NULL, 4 * buf_width * buf_height, NULL);
  gst_buffer_memset (ret->text_image->image, 0, 0U, 4 * buf_width * buf_height);
  gst_buffer_map (ret->text_image->image, &map, GST_MAP_READWRITE);

  stride = cairo_format_stride_for_width (CAIRO_FORMAT_ARGB32, buf_width);
  cropped_surface =
    cairo_image_surface_create_for_data (
        map.data + (vertical_offset * stride), CAIRO_FORMAT_ARGB32, buf_width,
        buf_height, stride);
  cropped_state = cairo_create (cropped_surface);
  cairo_set_source_surface (cropped_state, surface, -logical_rect.x,
      -logical_rect.y);
  cairo_rectangle (cropped_state, 0, 0, logical_rect.width,
      logical_rect.height);
  cairo_fill (cropped_state);

  cairo_destroy (cairo_state);
  cairo_surface_destroy (surface);
  cairo_destroy (cropped_state);
  cairo_surface_destroy (cropped_surface);
  gst_buffer_unmap (ret->text_image->image, &map);

  ret->text_image->width = buf_width;
  ret->text_image->height = buf_height;
  ret->horiz_offset = logical_rect.x;

  return ret;
}


/* If any of an array of elements has line wrapping enabled, return TRUE. */
static gboolean
is_wrapped (GPtrArray * elements)
{
  GstSubtitleElement *element;
  guint i;

  for (i = 0; i < elements->len; ++i) {
    element = g_ptr_array_index (elements, i);
    if (element->style.wrap_option == GST_SUBTITLE_WRAPPING_ON)
      return TRUE;
  }

  return FALSE;
}


/* Return the maximum font size used in an array of elements. */
static gdouble
get_max_font_size (GPtrArray * elements)
{
  GstSubtitleElement *element;
  guint i;
  gdouble max_size = 0.0;

  for (i = 0; i < elements->len; ++i) {
    element = g_ptr_array_index (elements, i);
    if (element->style.font_size > max_size)
      max_size = element->style.font_size;
  }

  return max_size;
}


static GstTtmlRenderRenderedImage *
rendered_image_new (GstBuffer * image, gint x, gint y, guint width,
    guint height)
{
  GstTtmlRenderRenderedImage *ret;

  ret = g_slice_new0 (GstTtmlRenderRenderedImage);
  /*gst_mini_object_init (GST_MINI_OBJECT_CAST (element), 0,
      rendered_image_get_type (), NULL, NULL,
      (GstMiniObjectFreeFunction) rendered_image_free);*/

  ret->image = image;
  ret->x = x;
  ret->y = y;
  ret->width = width;
  ret->height = height;

  return ret;
}

static GstTtmlRenderRenderedImage *
rendered_image_new_empty ()
{
  return rendered_image_new (NULL, 0, 0, 0, 0);
}


static GstTtmlRenderRenderedImage *
rendered_image_copy (GstTtmlRenderRenderedImage * image)
{
  GstTtmlRenderRenderedImage *ret
    = g_slice_new0 (GstTtmlRenderRenderedImage);

  ret->image = gst_buffer_ref (image->image);
  ret->x = image->x;
  ret->y = image->y;
  ret->width = image->width;
  ret->height = image->height;

  return ret;
}


static void
rendered_image_free (GstTtmlRenderRenderedImage * image)
{
  if (!image) return;
  gst_buffer_unref (image->image);
  g_slice_free (GstTtmlRenderRenderedImage, image);
}


static void
output_image (const GstTtmlRenderRenderedImage * image, const gchar * filename)
{
  GstMapInfo map;
  cairo_surface_t *surface;
  cairo_t *cairo_state;

  printf ("Outputting image with following dimensions:  x:%u  y:%u  width:%u "
          "height:%u\n", image->x, image->y, image->width, image->height);

  gst_buffer_map (image->image, &map, GST_MAP_READ);
  surface = cairo_image_surface_create_for_data (map.data,
          CAIRO_FORMAT_ARGB32, image->width, image->height,
          cairo_format_stride_for_width (CAIRO_FORMAT_ARGB32, image->width));
  cairo_state = cairo_create (surface);

  cairo_surface_write_to_png(surface, filename);
  cairo_destroy(cairo_state);
  cairo_surface_destroy(surface);
  gst_buffer_unmap (image->image, &map);
}


/* The order of arguments is significant: @image2 will be rendered on top of
 * @image1. */
static GstTtmlRenderRenderedImage *
rendered_image_combine (GstTtmlRenderRenderedImage * image1,
    GstTtmlRenderRenderedImage * image2)
{
  GstTtmlRenderRenderedImage *ret;
  GstMapInfo map1, map2, map_dest;
  cairo_surface_t *sfc1, *sfc2, *sfc_dest;
  cairo_t *state_dest;

  if (image1 && !image2)
    return rendered_image_copy (image1);
  if (image2 && !image1)
    return rendered_image_copy (image2);

  ret = g_slice_new0 (GstTtmlRenderRenderedImage);

  /* Work out dimensions of combined image. */
  ret->x = MIN (image1->x, image2->x);
  ret->y = MIN (image1->y, image2->y);
  ret->width = MAX (image1->x + image1->width, image2->x + image2->width)
    - ret->x;
  ret->height = MAX (image1->y + image1->height, image2->y + image2->height)
    - ret->y;

  GST_CAT_LOG (ttmlrender, "Dimensions of combined image:  x:%u  y:%u  "
      "width:%u  height:%u", ret->x, ret->y, ret->width, ret->height);

  /* Create cairo_surface from src images. */
  gst_buffer_map (image1->image, &map1, GST_MAP_READ);
  sfc1 = cairo_image_surface_create_for_data (
      map1.data, CAIRO_FORMAT_ARGB32, image1->width, image1->height,
      cairo_format_stride_for_width (CAIRO_FORMAT_ARGB32, image1->width));

  gst_buffer_map (image2->image, &map2, GST_MAP_READ);
  sfc2 = cairo_image_surface_create_for_data (
      map2.data, CAIRO_FORMAT_ARGB32, image2->width, image2->height,
      cairo_format_stride_for_width (CAIRO_FORMAT_ARGB32, image2->width));

  /* Create cairo_surface for resultant image. */
  ret->image = gst_buffer_new_allocate (NULL, 4 * ret->width * ret->height,
      NULL);
  gst_buffer_memset (ret->image, 0, 0U, 4 * ret->width * ret->height);
  gst_buffer_map (ret->image, &map_dest, GST_MAP_READWRITE);
  sfc_dest = cairo_image_surface_create_for_data (
      map_dest.data, CAIRO_FORMAT_ARGB32, ret->width, ret->height,
      cairo_format_stride_for_width (CAIRO_FORMAT_ARGB32, ret->width));
  state_dest = cairo_create (sfc_dest);

  /* Blend image1 into destination surface. */
  cairo_set_source_surface (state_dest, sfc1, image1->x - ret->x,
      image1->y - ret->y);
  cairo_rectangle (state_dest, image1->x - ret->x, image1->y - ret->y,
      image1->width, image1->height);
  cairo_fill (state_dest);

  /* Blend image2 into destination surface. */
  cairo_set_source_surface (state_dest, sfc2, image2->x - ret->x,
      image2->y - ret->y);
  cairo_rectangle (state_dest, image2->x - ret->x, image2->y - ret->y,
      image2->width, image2->height);
  cairo_fill (state_dest);

  /* Return destination image. */
  cairo_destroy (state_dest);
  cairo_surface_destroy (sfc1);
  cairo_surface_destroy (sfc2);
  cairo_surface_destroy (sfc_dest);
  gst_buffer_unmap (image1->image, &map1);
  gst_buffer_unmap (image2->image, &map2);
  gst_buffer_unmap (ret->image, &map_dest);

  return ret;
}


static GstTtmlRenderRenderedImage *
rendered_image_crop (GstTtmlRenderRenderedImage * image, gint x, gint y,
    guint width, guint height)
{
  GstTtmlRenderRenderedImage *ret;
  GstMapInfo map_src, map_dest;
  cairo_surface_t *sfc_src, *sfc_dest;
  cairo_t *state_dest;

  if ((x <= image->x) && (y <= image->y) && (width >= image->width)
      && (height >= image->height))
    return rendered_image_copy (image);

  /* TODO: Handle case where crop rectangle doesn't intersect image. */

  ret = g_slice_new0 (GstTtmlRenderRenderedImage);

  ret->x = MAX (image->x, x);
  ret->y = MAX (image->y, y);
  ret->width = MIN ((image->x + image->width) - ret->x, (x + width) - ret->x);
  ret->height = MIN ((image->y + image->height) - ret->y,
      (y + height) - ret->y);

  GST_CAT_LOG (ttmlrender, "Dimensions of cropped image:  x:%u  y:%u  "
      "width:%u  height:%u", ret->x, ret->y, ret->width, ret->height);

  /* Create cairo_surface from src image. */
  gst_buffer_map (image->image, &map_src, GST_MAP_READ);
  sfc_src = cairo_image_surface_create_for_data (
      map_src.data, CAIRO_FORMAT_ARGB32, image->width, image->height,
      cairo_format_stride_for_width (CAIRO_FORMAT_ARGB32, image->width));

  /* Create cairo_surface for cropped image. */
  ret->image = gst_buffer_new_allocate (NULL, 4 * ret->width * ret->height,
      NULL);
  gst_buffer_memset (ret->image, 0, 0U, 4 * ret->width * ret->height);
  gst_buffer_map (ret->image, &map_dest, GST_MAP_READWRITE);
  sfc_dest = cairo_image_surface_create_for_data (
      map_dest.data, CAIRO_FORMAT_ARGB32, ret->width, ret->height,
      cairo_format_stride_for_width (CAIRO_FORMAT_ARGB32, ret->width));
  state_dest = cairo_create (sfc_dest);

  /* Copy section of image1 into destination surface. */
  cairo_set_source_surface (state_dest, sfc_src, (image->x - ret->x),
      (image->y - ret->y));
  cairo_rectangle (state_dest, 0, 0, ret->width, ret->height);
  cairo_fill (state_dest);

  cairo_destroy (state_dest);
  cairo_surface_destroy (sfc_src);
  cairo_surface_destroy (sfc_dest);
  gst_buffer_unmap (image->image, &map_src);
  gst_buffer_unmap (ret->image, &map_dest);

  return ret;
}


static gboolean
color_is_transparent (GstSubtitleColor * color)
{
  return (color->a == 0);
}


/* Render the background rectangles to be placed behind each element. */
static GstTtmlRenderRenderedImage *
render_element_backgrounds (GPtrArray * elements, GPtrArray * char_ranges,
    PangoLayout * layout, guint origin_x, guint origin_y, guint line_height,
    guint line_padding, guint horiz_offset)
{
  gint first_line, last_line, cur_line;
  guint padding;
  PangoLayoutLine *line;
  PangoRectangle first_char_pos, last_char_pos, line_extents;
  TextRange *range;
  GstSubtitleElement *element;
  guint rect_width;
  GstBuffer *rectangle;
  guint first_char_start, last_char_end;
  guint i;
  GstTtmlRenderRenderedImage *ret = NULL;

  for (i = 0; i < char_ranges->len; ++i) {
    range = g_ptr_array_index (char_ranges, i);
    element = g_ptr_array_index (elements, i);

    GST_CAT_LOG (ttmlrender, "First char index: %u   Last char index: %u",
        range->first_char, range->last_char);
    pango_layout_index_to_pos (layout, range->first_char, &first_char_pos);
    pango_layout_index_to_pos (layout, range->last_char, &last_char_pos);
    pango_layout_index_to_line_x (layout, range->first_char, 1,
        &first_line, NULL);
    pango_layout_index_to_line_x (layout, range->last_char, 0,
        &last_line, NULL);

    /* XXX: Or could leave everything in Pango units until later? */
    first_char_start = PANGO_PIXELS (first_char_pos.x) - horiz_offset;
    last_char_end = PANGO_PIXELS (last_char_pos.x + last_char_pos.width)
      - horiz_offset;

    GST_CAT_LOG (ttmlrender, "First char start: %u  Last char end: %u",
        first_char_start, last_char_end);
    GST_CAT_LOG (ttmlrender, "First line: %u  Last line: %u", first_line,
        last_line);

    for (cur_line = first_line; cur_line <= last_line; ++cur_line) {
      guint line_start, line_end;
      guint area_start, area_end;
      gint first_char_index;
      PangoRectangle line_pos;
      padding = 0;

      line = pango_layout_get_line (layout, cur_line);
      pango_layout_line_get_pixel_extents (line, NULL, &line_extents);

      pango_layout_line_x_to_index (line, 0, &first_char_index, NULL);
      pango_layout_index_to_pos (layout, first_char_index, &line_pos);
      GST_CAT_LOG (ttmlrender, "First char index:%d  position_X:%d  "
          "position_Y:%d", first_char_index, PANGO_PIXELS (line_pos.x),
          PANGO_PIXELS (line_pos.y));

      line_start = PANGO_PIXELS (line_pos.x) - horiz_offset;
      line_end = (PANGO_PIXELS (line_pos.x) + line_extents.width)
        - horiz_offset;

      GST_CAT_LOG (ttmlrender, "line_extents.x:%d  line_extents.y:%d  "
          "line_extents.width:%d  line_extents.height:%d", line_extents.x,
          line_extents.y, line_extents.width, line_extents.height);
      GST_CAT_LOG (ttmlrender, "cur_line:%u  line start:%u  line end:%u "
          "first_char_start: %u  last_char_end: %u", cur_line, line_start,
          line_end, first_char_start, last_char_end);

      if ((cur_line == first_line) && (first_char_start != line_start)) {
        area_start = first_char_start + line_padding;
        GST_CAT_LOG (ttmlrender,
            "First line, but there is preceding text in line.");
      } else {
        GST_CAT_LOG (ttmlrender,
            "Area contains first text on the line; adding padding...");
        ++padding;
        area_start = line_start;
      }

      if ((cur_line == last_line) && (last_char_end != line_end)) {
        GST_CAT_LOG (ttmlrender,
            "Last line, but there is following text in line.");
        area_end = last_char_end + line_padding;
      } else {
        GST_CAT_LOG (ttmlrender,
            "Area contains last text on the line; adding padding...");
        ++padding;
        area_end = line_end + (2 * line_padding);
      }

      rect_width = (area_end - area_start);

      /* <br>s will result in zero-width rectangle */
      if (rect_width > 0 && !color_is_transparent (&element->style.bg_color)) {
        GstTtmlRenderRenderedImage *image, *tmp;
        rectangle = draw_rectangle (rect_width, line_height,
            element->style.bg_color);
        image = rendered_image_new (rectangle, origin_x + area_start,
            origin_y + (cur_line * line_height), rect_width, line_height);
        tmp = ret;
        ret = rendered_image_combine (ret, image);
        if (tmp) rendered_image_free (tmp);
        rendered_image_free (image);
      }
    }
  }

  return ret;
}


static PangoAlignment
get_alignment (GstSubtitleStyleSet * style)
{
  PangoAlignment align = PANGO_ALIGN_LEFT;

  switch (style->multi_row_align) {
    case GST_SUBTITLE_MULTI_ROW_ALIGN_START:
      align = PANGO_ALIGN_LEFT;
      break;
    case GST_SUBTITLE_MULTI_ROW_ALIGN_CENTER:
      align = PANGO_ALIGN_CENTER;
      break;
    case GST_SUBTITLE_MULTI_ROW_ALIGN_END:
      align = PANGO_ALIGN_RIGHT;
      break;
    case GST_SUBTITLE_MULTI_ROW_ALIGN_AUTO:
      switch (style->text_align) {
        case GST_SUBTITLE_TEXT_ALIGN_START:
        case GST_SUBTITLE_TEXT_ALIGN_LEFT:
          align = PANGO_ALIGN_LEFT;
          break;
        case GST_SUBTITLE_TEXT_ALIGN_CENTER:
          align = PANGO_ALIGN_CENTER;
          break;
        case GST_SUBTITLE_TEXT_ALIGN_END:
        case GST_SUBTITLE_TEXT_ALIGN_RIGHT:
          align = PANGO_ALIGN_RIGHT;
          break;
        default:
          GST_CAT_ERROR (ttmlrender, "Illegal TextAlign value (%d)",
              style->text_align);
          break;
      }
      break;
    default:
      GST_CAT_ERROR (ttmlrender, "Illegal MultiRowAlign value (%d)",
          style->multi_row_align);
      break;
  }
  return align;
}


static GstTtmlRenderRenderedImage *
stitch_blocks (GList * blocks)
{
  guint vert_offset = 0;
  GList *block_entry;
  GstTtmlRenderRenderedImage *ret = NULL;

  for (block_entry = g_list_first (blocks); block_entry;
      block_entry = block_entry->next) {
    GstTtmlRenderRenderedImage *block, *tmp;
    block = (GstTtmlRenderRenderedImage *)block_entry->data;
    tmp = ret;

    block->y += vert_offset;
    GST_CAT_LOG (ttmlrender, "Rendering block at vertical offset %u",
        vert_offset);
    vert_offset = block->y + block->height;
    ret = rendered_image_combine (ret, block);
    if (tmp) rendered_image_free (tmp);
  }

  GST_CAT_LOG (ttmlrender, "Height of stitched image: %u", ret->height);
  ret->image = gst_buffer_make_writable (ret->image);
  return ret;
}


static void
rendered_text_free (GstTtmlRenderRenderedText * text)
{
  if (text->text_image)
    rendered_image_free (text->text_image);
  if (text->layout)
    g_object_unref (text->layout);
  g_slice_free (GstTtmlRenderRenderedText, text);
}


static GstTtmlRenderRenderedImage *
render_text_block (GstTtmlRender * render, GstSubtitleBlock * block,
    GstBuffer * text_buf, guint width, gboolean overflow)
{
  GPtrArray *char_ranges = NULL;
  gchar *marked_up_string;
  PangoAlignment alignment;
  guint max_font_size;
  guint line_padding;
  gint text_offset = 0;
  GstTtmlRenderRenderedText *rendered_text;
  GstTtmlRenderRenderedImage *backgrounds = NULL;
  GstTtmlRenderRenderedImage *ret;

  /* Join text from elements to form a single marked-up string. */
  marked_up_string = generate_marked_up_string (render, block->elements,
      text_buf, &char_ranges);

  max_font_size = (guint) (get_max_font_size (block->elements)
      * render->height);
  GST_CAT_DEBUG (ttmlrender, "Max font size: %u", max_font_size);

  line_padding = (guint) (block->style.line_padding * render->width);
  alignment = get_alignment (&block->style);

  /* Render text to buffer. */
  rendered_text = draw_text (render, marked_up_string,
      (width - (2 * line_padding)), alignment,
      (guint) (block->style.line_height * max_font_size), max_font_size,
      is_wrapped (block->elements));

  switch (block->style.text_align) {
    case GST_SUBTITLE_TEXT_ALIGN_START:
    case GST_SUBTITLE_TEXT_ALIGN_LEFT:
      text_offset = line_padding;
      break;
    case GST_SUBTITLE_TEXT_ALIGN_CENTER:
      text_offset = ((gint)width - rendered_text->text_image->width);
      text_offset /= 2;
      break;
    case GST_SUBTITLE_TEXT_ALIGN_END:
    case GST_SUBTITLE_TEXT_ALIGN_RIGHT:
      text_offset = (gint)width
        - (rendered_text->text_image->width + line_padding);
      break;
  }

  rendered_text->text_image->x = text_offset;

  /* Render background rectangles, if any. */
  backgrounds = render_element_backgrounds (block->elements, char_ranges,
      rendered_text->layout, text_offset - line_padding, 0,
      (guint) (block->style.line_height * max_font_size), line_padding,
      rendered_text->horiz_offset);

  /* Render block background, if non-transparent. */
  if (!color_is_transparent (&block->style.bg_color)) {
    GstTtmlRenderRenderedImage *block_background;
    GstTtmlRenderRenderedImage *tmp = backgrounds;

    GstBuffer *block_bg_image = draw_rectangle (width, backgrounds->height,
        block->style.bg_color);
    block_background = rendered_image_new (block_bg_image, 0, 0, width,
        backgrounds->height);
    backgrounds = rendered_image_combine (block_background, backgrounds);
    rendered_image_free (tmp);
    rendered_image_free (block_background);
  }

  /* Combine text and background images. */
  ret = rendered_image_combine (backgrounds, rendered_text->text_image);
  rendered_image_free (backgrounds);
  rendered_text_free (rendered_text);

  g_free (marked_up_string);
  g_ptr_array_unref (char_ranges);
  GST_CAT_DEBUG (ttmlrender, "block width: %u   block height: %u",
      ret->width, ret->height);
  return ret;
}


static GstVideoOverlayComposition *
gst_ttml_render_compose_overlay (GstTtmlRenderRenderedImage * image)
{
  GstVideoOverlayRectangle *rectangle;
  GstBuffer *buf = gst_buffer_copy (image->image);
  GstVideoOverlayComposition *ret = NULL;

  gst_buffer_add_video_meta (buf, GST_VIDEO_FRAME_FLAG_NONE,
      GST_VIDEO_OVERLAY_COMPOSITION_FORMAT_RGB, image->width, image->height);

  rectangle = gst_video_overlay_rectangle_new_raw (buf, image->x, image->y,
      image->width, image->height,
      GST_VIDEO_OVERLAY_FORMAT_FLAG_PREMULTIPLIED_ALPHA);

  ret = gst_video_overlay_composition_new (rectangle);
  return ret;
}


static GstVideoOverlayComposition *
render_text_area (GstTtmlRender * render, GstSubtitleArea * area,
  GstBuffer * text_buf)
{
  GList *blocks = NULL;
  guint area_x, area_y, area_width, area_height;
  guint window_x, window_y, window_width, window_height;
  guint padding_start, padding_end, padding_before, padding_after;
  GstTtmlRenderRenderedImage *area_image = NULL;
  GstVideoOverlayComposition *ret = NULL;

  area_width = (guint) (round (area->style.extent_w * render->width));
  area_height = (guint) (round (area->style.extent_h * render->height));
  area_x = (guint) (round (area->style.origin_x * render->width));
  area_y = (guint) (round (area->style.origin_y * render->height));

  padding_start = (guint) (round (area->style.padding_start * render->width));
  padding_end = (guint) (round (area->style.padding_end * render->width));
  padding_before =
    (guint) (round (area->style.padding_before * render->height));
  padding_after = (guint) (round (area->style.padding_after * render->height));

  /* "window" here refers to the section of the area that we're allowed to
   * render into. i.e., the area minus padding. */
  window_x = area_x + padding_start;
  window_y = area_y + padding_before;
  window_width = area_width - (padding_start + padding_end);
  window_height = area_height - (padding_before + padding_after);

  GST_CAT_DEBUG (ttmlrender,
      "Padding: start: %u  end: %u  before: %u  after: %u",
      padding_start, padding_end, padding_before, padding_after);

  /* Render region background, if non-transparent. */
  if (!color_is_transparent (&area->style.bg_color)) {
    GstBuffer *bg_rect;

    bg_rect = draw_rectangle (area_width, area_height, area->style.bg_color);
    area_image = rendered_image_new (bg_rect, area_x, area_y, area_width,
        area_height);
  }

  if (area->blocks) {
    GstTtmlRenderRenderedImage *blocks_image, *tmp;
    guint i;

    /* Render each block and append to list. */
    for (i = 0; i < area->blocks->len; ++i) {
      GstSubtitleBlock *block;
      GstTtmlRenderRenderedImage *rendered_block;

      block = g_ptr_array_index (area->blocks, i);
      rendered_block = render_text_block (render, block, text_buf,
          window_width, TRUE);

      blocks = g_list_append (blocks, rendered_block);
    }
    blocks_image = stitch_blocks (blocks);
    g_list_free_full (blocks, (GDestroyNotify) rendered_image_free);
    blocks_image->x += window_x;

    switch (area->style.display_align) {
      case GST_SUBTITLE_DISPLAY_ALIGN_BEFORE:
        blocks_image->y = window_y;
        break;
      case GST_SUBTITLE_DISPLAY_ALIGN_CENTER:
        blocks_image->y = area_y + ((gint)((area_height + padding_before)
              - (padding_after + blocks_image->height)))/2;
        break;
      case GST_SUBTITLE_DISPLAY_ALIGN_AFTER:
        blocks_image->y = (area_y + area_height)
          - (padding_after + blocks_image->height);
        break;
    }

    if ((area->style.overflow == GST_SUBTITLE_OVERFLOW_MODE_HIDDEN)
        && ((blocks_image->height > window_height)
          || (blocks_image->width > window_width))) {
      GstTtmlRenderRenderedImage *tmp = blocks_image;
      blocks_image = rendered_image_crop (blocks_image, window_x, window_y,
          window_width, window_height);
      rendered_image_free (tmp);
    }

    tmp = area_image;
    area_image = rendered_image_combine (area_image, blocks_image);
    if (tmp) rendered_image_free (tmp);
    rendered_image_free (blocks_image);
  }

  GST_CAT_DEBUG (ttmlrender, "Height of rendered area: %u",
      area_image->height);

  ret = gst_ttml_render_compose_overlay (area_image);
  rendered_image_free (area_image);
  return ret;
}


static GstFlowReturn
gst_ttml_render_video_chain (GstPad * pad, GstObject * parent,
    GstBuffer * buffer)
{
  GstTtmlRenderClass *klass;
  GstTtmlRender *render;
  GstFlowReturn ret = GST_FLOW_OK;
  gboolean in_seg = FALSE;
  guint64 start, stop, clip_start = 0, clip_stop = 0;
  gchar *text = NULL;

  render = GST_TTML_RENDER (parent);
  klass = GST_TTML_RENDER_GET_CLASS (render);

  if (!GST_BUFFER_TIMESTAMP_IS_VALID (buffer))
    goto missing_timestamp;

  /* ignore buffers that are outside of the current segment */
  start = GST_BUFFER_TIMESTAMP (buffer);

  if (!GST_BUFFER_DURATION_IS_VALID (buffer)) {
    stop = GST_CLOCK_TIME_NONE;
  } else {
    stop = start + GST_BUFFER_DURATION (buffer);
  }

  GST_LOG_OBJECT (render, "%" GST_SEGMENT_FORMAT "  BUFFER: ts=%"
      GST_TIME_FORMAT ", end=%" GST_TIME_FORMAT, &render->segment,
      GST_TIME_ARGS (start), GST_TIME_ARGS (stop));

  /* segment_clip() will adjust start unconditionally to segment_start if
   * no stop time is provided, so handle this ourselves */
  if (stop == GST_CLOCK_TIME_NONE && start < render->segment.start)
    goto out_of_segment;

  in_seg = gst_segment_clip (&render->segment, GST_FORMAT_TIME, start, stop,
      &clip_start, &clip_stop);

  if (!in_seg)
    goto out_of_segment;

  /* if the buffer is only partially in the segment, fix up stamps */
  if (clip_start != start || (stop != -1 && clip_stop != stop)) {
    GST_DEBUG_OBJECT (render, "clipping buffer timestamp/duration to segment");
    buffer = gst_buffer_make_writable (buffer);
    GST_BUFFER_TIMESTAMP (buffer) = clip_start;
    if (stop != -1)
      GST_BUFFER_DURATION (buffer) = clip_stop - clip_start;
  }

  /* now, after we've done the clipping, fix up end time if there's no
   * duration (we only use those estimated values internally though, we
   * don't want to set bogus values on the buffer itself) */
  if (stop == -1) {
    if (render->info.fps_n && render->info.fps_d) {
      GST_DEBUG_OBJECT (render, "estimating duration based on framerate");
      stop = start + gst_util_uint64_scale_int (GST_SECOND,
          render->info.fps_d, render->info.fps_n);
    } else {
      GST_LOG_OBJECT (render, "no duration, assuming minimal duration");
      stop = start + 1;         /* we need to assume some interval */
    }
  }

  gst_object_sync_values (GST_OBJECT (render), GST_BUFFER_TIMESTAMP (buffer));

wait_for_text_buf:

  GST_TTML_RENDER_LOCK (render);

  if (render->video_flushing)
    goto flushing;

  if (render->video_eos)
    goto have_eos;

  if (render->silent) {
    GST_TTML_RENDER_UNLOCK (render);
    ret = gst_pad_push (render->srcpad, buffer);

    /* Update position */
    render->segment.position = clip_start;

    return ret;
  }

  /* Text pad not linked, rendering internal text */
  if (!render->text_linked) {
    if (klass->get_text) {
      text = klass->get_text (render, buffer);
    } else {
      text = g_strdup (render->default_text);
    }

    GST_LOG_OBJECT (render, "Text pad not linked, rendering default "
        "text: '%s'", GST_STR_NULL (text));

    GST_TTML_RENDER_UNLOCK (render);

    if (text != NULL && *text != '\0') {
      /* Render and push */
      gst_ttml_render_render_text (render, text, -1);
      ret = gst_ttml_render_push_frame (render, buffer);
    } else {
      /* Invalid or empty string */
      ret = gst_pad_push (render->srcpad, buffer);
    }
  } else {
    /* Text pad linked, check if we have a text buffer queued */
    if (render->text_buffer) {
      gboolean pop_text = FALSE, valid_text_time = TRUE;
      GstClockTime text_start = GST_CLOCK_TIME_NONE;
      GstClockTime text_end = GST_CLOCK_TIME_NONE;
      GstClockTime text_running_time = GST_CLOCK_TIME_NONE;
      GstClockTime text_running_time_end = GST_CLOCK_TIME_NONE;
      GstClockTime vid_running_time, vid_running_time_end;

      /* if the text buffer isn't stamped right, pop it off the
       * queue and display it for the current video frame only */
      if (!GST_BUFFER_TIMESTAMP_IS_VALID (render->text_buffer) ||
          !GST_BUFFER_DURATION_IS_VALID (render->text_buffer)) {
        GST_WARNING_OBJECT (render,
            "Got text buffer with invalid timestamp or duration");
        pop_text = TRUE;
        valid_text_time = FALSE;
      } else {
        text_start = GST_BUFFER_TIMESTAMP (render->text_buffer);
        text_end = text_start + GST_BUFFER_DURATION (render->text_buffer);
      }

      vid_running_time =
          gst_segment_to_running_time (&render->segment, GST_FORMAT_TIME,
          start);
      vid_running_time_end =
          gst_segment_to_running_time (&render->segment, GST_FORMAT_TIME,
          stop);

      /* If timestamp and duration are valid */
      if (valid_text_time) {
        text_running_time =
            gst_segment_to_running_time (&render->text_segment,
            GST_FORMAT_TIME, text_start);
        text_running_time_end =
            gst_segment_to_running_time (&render->text_segment,
            GST_FORMAT_TIME, text_end);
      }

      GST_LOG_OBJECT (render, "T: %" GST_TIME_FORMAT " - %" GST_TIME_FORMAT,
          GST_TIME_ARGS (text_running_time),
          GST_TIME_ARGS (text_running_time_end));
      GST_LOG_OBJECT (render, "V: %" GST_TIME_FORMAT " - %" GST_TIME_FORMAT,
          GST_TIME_ARGS (vid_running_time),
          GST_TIME_ARGS (vid_running_time_end));

      /* Text too old or in the future */
      if (valid_text_time && text_running_time_end <= vid_running_time) {
        /* text buffer too old, get rid of it and do nothing  */
        GST_LOG_OBJECT (render, "text buffer too old, popping");
        pop_text = FALSE;
        gst_ttml_render_pop_text (render);
        GST_TTML_RENDER_UNLOCK (render);
        goto wait_for_text_buf;
      } else if (valid_text_time && vid_running_time_end <= text_running_time) {
        GST_LOG_OBJECT (render, "text in future, pushing video buf");
        GST_TTML_RENDER_UNLOCK (render);
        /* Push the video frame */
        ret = gst_pad_push (render->srcpad, buffer);
      } else {
        if (render->need_render) {
          GstSubtitleArea *area = NULL;
          GstSubtitleMeta *subtitle_meta = NULL;
          guint i;

          if (render->compositions) {
            g_list_free_full (render->compositions,
                (GDestroyNotify) gst_video_overlay_composition_unref);
            render->compositions = NULL;
          }

          subtitle_meta = gst_buffer_get_subtitle_meta (render->text_buffer);
          g_assert (subtitle_meta != NULL);

          for (i = 0; i < subtitle_meta->areas->len; ++i) {
            GstVideoOverlayComposition *composition;
            area = g_ptr_array_index (subtitle_meta->areas, i);
            g_assert (area != NULL);
            composition = render_text_area (render, area,
                render->text_buffer);
            render->compositions = g_list_append (render->compositions,
                composition);
          }
          render->need_render = FALSE;
        }

        GST_TTML_RENDER_UNLOCK (render);
        ret = gst_ttml_render_push_frame (render, buffer);

        if (valid_text_time && text_running_time_end <= vid_running_time_end) {
          GST_LOG_OBJECT (render, "text buffer not needed any longer");
          pop_text = TRUE;
        }
      }
      if (pop_text) {
        GST_TTML_RENDER_LOCK (render);
        gst_ttml_render_pop_text (render);
        GST_TTML_RENDER_UNLOCK (render);
      }
    } else {
      gboolean wait_for_text_buf = TRUE;

      if (render->text_eos)
        wait_for_text_buf = FALSE;

      if (!render->wait_text)
        wait_for_text_buf = FALSE;

      /* Text pad linked, but no text buffer available - what now? */
      if (render->text_segment.format == GST_FORMAT_TIME) {
        GstClockTime text_start_running_time, text_position_running_time;
        GstClockTime vid_running_time;

        vid_running_time =
            gst_segment_to_running_time (&render->segment, GST_FORMAT_TIME,
            GST_BUFFER_TIMESTAMP (buffer));
        text_start_running_time =
            gst_segment_to_running_time (&render->text_segment,
            GST_FORMAT_TIME, render->text_segment.start);
        text_position_running_time =
            gst_segment_to_running_time (&render->text_segment,
            GST_FORMAT_TIME, render->text_segment.position);

        if ((GST_CLOCK_TIME_IS_VALID (text_start_running_time) &&
                vid_running_time < text_start_running_time) ||
            (GST_CLOCK_TIME_IS_VALID (text_position_running_time) &&
                vid_running_time < text_position_running_time)) {
          wait_for_text_buf = FALSE;
        }
      }

      if (wait_for_text_buf) {
        GST_DEBUG_OBJECT (render, "no text buffer, need to wait for one");
        GST_TTML_RENDER_WAIT (render);
        GST_DEBUG_OBJECT (render, "resuming");
        GST_TTML_RENDER_UNLOCK (render);
        goto wait_for_text_buf;
      } else {
        GST_TTML_RENDER_UNLOCK (render);
        GST_LOG_OBJECT (render, "no need to wait for a text buffer");
        ret = gst_pad_push (render->srcpad, buffer);
      }
    }
  }

  g_free (text);

  /* Update position */
  render->segment.position = clip_start;

  return ret;

missing_timestamp:
  {
    GST_WARNING_OBJECT (render, "buffer without timestamp, discarding");
    gst_buffer_unref (buffer);
    return GST_FLOW_OK;
  }

flushing:
  {
    GST_TTML_RENDER_UNLOCK (render);
    GST_DEBUG_OBJECT (render, "flushing, discarding buffer");
    gst_buffer_unref (buffer);
    return GST_FLOW_FLUSHING;
  }
have_eos:
  {
    GST_TTML_RENDER_UNLOCK (render);
    GST_DEBUG_OBJECT (render, "eos, discarding buffer");
    gst_buffer_unref (buffer);
    return GST_FLOW_EOS;
  }
out_of_segment:
  {
    GST_DEBUG_OBJECT (render, "buffer out of segment, discarding");
    gst_buffer_unref (buffer);
    return GST_FLOW_OK;
  }
}

static GstStateChangeReturn
gst_ttml_render_change_state (GstElement * element,
    GstStateChange transition)
{
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
  GstTtmlRender *render = GST_TTML_RENDER (element);

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      GST_TTML_RENDER_LOCK (render);
      render->text_flushing = TRUE;
      render->video_flushing = TRUE;
      /* pop_text will broadcast on the GCond and thus also make the video
       * chain exit if it's waiting for a text buffer */
      gst_ttml_render_pop_text (render);
      GST_TTML_RENDER_UNLOCK (render);
      break;
    default:
      break;
  }

  ret = parent_class->change_state (element, transition);
  if (ret == GST_STATE_CHANGE_FAILURE)
    return ret;

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      GST_TTML_RENDER_LOCK (render);
      render->text_flushing = FALSE;
      render->video_flushing = FALSE;
      render->video_eos = FALSE;
      render->text_eos = FALSE;
      gst_segment_init (&render->segment, GST_FORMAT_TIME);
      gst_segment_init (&render->text_segment, GST_FORMAT_TIME);
      GST_TTML_RENDER_UNLOCK (render);
      break;
    default:
      break;
  }

  return ret;
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  GST_DEBUG_CATEGORY_INIT (ttmlrender, "ttmlrender", 0, "TTML renderer");

  if (!gst_element_register (plugin, "ttmlrender", GST_RANK_PRIMARY,
          GST_TYPE_TEXT_OVERLAY)) {
    return FALSE;
  }

  return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR, GST_VERSION_MINOR,
    ttmlrender, "Pango-based text rendering and render, supporting the "
    "EBU-TT-D profile of TTML.", plugin_init,
    VERSION, "LGPL", "gst-ttml-render", "http://www.bbc.co.uk/rd")
