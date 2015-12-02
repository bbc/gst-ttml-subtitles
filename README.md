## GStreamer TTML subtitling package

**The elements and library in this project provide a means for GStreamer pipelines to parse and render [TTML](http://www.w3.org/TR/ttaf1-dfxp/) subtitles. More specifically, the code handles the [EBU-TT-D](https://tech.ebu.ch/ebu-tt) profile of TTML designed for the distribution of subtitles over IP.**

### Getting Started

##### Install dependencies
In order to run this code, you will need the following packages installed:
* libxml2
* pango
* cairo
* Up-to-date versions of the GStreamer git masters for the following GStreamer modules:
  * gstreamer
  * gst-plugins-base
  * gst-plugins-good
  * gst-plugins-bad
  * gst-libav

Note that it is not sufficient to have the latest stable releases of the GStreamer modules installed; you must use the git masters, as the TTML subtitling elements depend upon recent additions that have not yet made it into the latest stable releases. For instructions on how to build GStreamer modules from source see [Building GStreamer modules](#building-gstreamer-modules) below.

##### Clone this repository

```
$ git clone https://github.com/bbc/gst-ttml-subtitles.git
```

##### Build the subtitling code

In order to build the subtitling code, you will need to ensure that pkg-config (which is used in the build process) can find the installed GStreamer modules built from the git masters. This requires that the directory containing the \*.pc files for these modules is listed in the PKG_CONFIG_PATH environment variable. If the GStreamer modules were installed in a non-system location by passing a --prefix parameter to their configure scripts, their \*.pc files will usually be in &lt;prefix&gt;/lib/pkgconfig, which should be added to PKG_CONFIG_PATH, as follows:

```
$ export PKG_CONFIG_PATH=<prefix>/lib/pkgconfig
```

Run the following to build and install the subtitling elements:

```
$ cd gst-ttml-subtitles
$ ./autogen.sh
$ ./configure --prefix=<prefix used when building GStreamer modules>
$ make && make install
```

This should install the two subtitling elements - ttmlparse and ttmlrender - in the same location as the other GStreamer elements.

##### Check the elements have been successfully installed
To check that the elements have been correctly installed, first ensure that GStreamer can find its plugins and registry:

```
export GST_PLUGIN_SYSTEM_PATH=<prefix>/DASH/lib/gstreamer-1.0
export GST_PLUGIN_PATH=<prefix>/DASH/lib/gstreamer-1.0
export GST_REGISTRY=<prefix>/var/gstreamer-1.0/<registry filename>
```

Where &lt;registry filename&gt; is usually something like *registry-i686.xml*.

Then check that GStreamer can successfully load the TTML subtitling elements, as follows:

```
$ gst-inspect-1.0 ttmlparse
$ gst-inspect-1.0 ttmlrender
```

If they are present, the above commands should result in information about the ttmlparse and ttmlrender elements being output to the terminal.

### Using the elements

The TTML subtitling elements can be used to present EBU-TT-D subtitles delivered via two routes:
1. EBU-TT-D subtitles delivered as part of an MPEG DASH stream (i.e., in-band).
2. EBU-TT-D subtitles encapsulated in a single XML file (i.e., out-of-band).

##### DASH stream

If GStreamer is used to play a DASH stream that contains an EBU-TT-D subtitle component, it should autoplug the correct elements together so that the subtitles are displayed.

BBC R&D has created an MPEG DASH [test stream](http://rdmedia.bbc.co.uk/dash/ondemand/elephants_dream/) containing an EBU-TT-D subtitle component, which can be used to demonstrate the operation of the TTML subtitling elements.

To play this stream, simply run:

```
$ gst-play-1.0 http://rdmedia.bbc.co.uk/dash/ondemand/elephants_dream/1/client_manifest-all.mpd
```

You should see Elephants Dream play with subtitles rendered on top of the video.

##### Single XML file

If subtitles for a piece of content are carried out-of-band in a single XML file then the pipeline needs to be manually constructed. For example, presenting an EBU-TT-D subtitle file over an mp4 file containing h264 video and AAC audio would require the following command line:

```
$ gst-launch-1.0 filesrc location=<media file location> ! video/quicktime ! qtdemux name=q ttmlrender name=r q. ! queue ! h264parse ! avdec_h264 ! autovideoconvert ! r.video_sink filesrc location=<subtitle file location> blocksize=16777216 ! queue ! ttmlparse ! r.text_sink r. ! ximagesink q. ! queue ! aacparse ! avdec_aac ! audioconvert ! alsasink
```

Note the blocksize argument given to the filesrc handling the subtitle file. This is necessary for larger XML files because the ttmlparse element currently expects to be passed complete XML files in each input buffer; if filesrc uses a blocksize less than the size of the input XML file, it will split the XML file into multiple smaller chunks, which will cause ttmlparse to fail.

---

### Code overview
The TTML subtitling code follows the subtitling approach used currently in GStreamer, in which subtitle parsing and rendering are handled by different elements. The subtitle parser element is called _ttmlparse_, and the renderer element is called _ttmlrender_.

#### Parser element (ttmlparse)
The ttmlparse parser element is an expanded version of the [subparse](http://gstreamer.freedesktop.org/data/doc/gstreamer/head/gst-plugins-base-plugins/html/gst-plugins-base-plugins-subparse.html) element, which is the standard element within gst-plugins-base that parses subtitles of different formats.

ttmlparse parses the input TTML file into a tree structure, and it resolves the styles and regions referenced by the elements within that tree. From this tree, the parser determines the resulting _scenes_: the periods of time over which a static set of subtitles should be displayed. For example, the following subtitle file:

```
[...]
<body>
    <div region="r0">
        <p xml:id="sub0" begin="00:00:02" end="00:00:08">First subtitle</p>
        <p xml:id="sub1" begin="00:00:06" end="00:00:10">Second subtitle</p>
    </div>
</body>
[...]

```

...would result in three scenes being generated:
* The first, visible from 2s to 6s, in which only the first paragraph is displayed;
* The second, visible from 6s to 8s, in which both the first and second paragraphs are displayed;
* The third, visible from 8s to 10s, in which only the second paragraph is displayed.

Once the parser has worked out all of the scenes that would result from the input file, it creates a GstBuffer for each scene containing all the  information that the downstream renderer would need in order to present that scene. Finally, it pushes these buffers to the downstream renderer.

(Note: a scene is analagous to an _Intermediate Synchronic Document_ in TTML terminology.)

#### Interchange format
In TTML, text may be rendered simultaneously into multiple different on-screen regions with different layout options, and the different elements within that text can have their own styling associated with them (colours, fonts, etc.). For each scene the parser needs to pass all of this layout and styling information to the renderer, along with the actual text to be rendered.

The existing method of interchange between subtitle parser and renderer in GStreamer is _pango-markup_, in which styling information is included inline with the subtitle text using a markup syntax. Unfortunately, pango-markup does not support all of the layout and styling options available in TTML, therefore a different exchange format is used in this code.

This code contains a library that defines different types that can be used to describe the layout and styling of a scene. The main types are shown below:

Type | Description
-----|------------
GstSubtitleElement|Describes an inline text element resulting from a `span` or `br`.
GstSubtitleBlock|Describes a block of text resulting from a `p` element. Contains one or more GstSubtitleElements.
GstSubtitleRegion|Describes an on-screen region into which subtitles may be rendered, corresponding to a TTML `region` element. Contans zero or more GstSubtitleBlocks.
GstSubtitleStyleSet|Describes the styling options that should be applied to an element. All three of the types above have a GstSubtitleStyleSet associated with them.

For each scene, the parser creates objects of the above types to describe the layout of its components and attaches them as metadata (using a metadata type, GstSubtitleMeta, also defined in the library) to the GstBuffer that holds the text from that scene. Within the GstBuffer, the text associated with each GstSubtitleElement sits within its own GstMemory and is indexed by a field in the corresponding GstSubtitleElement structure.

The figure below shows the types created for a simple subtitle file that contains a single scene:

![Submeta](/docs/images/submeta.png)

#### Renderer element (ttmlrender)

The render element, ttmlrender, is a modified version of the GStreamer [textoverlay](http://gstreamer.freedesktop.org/data/doc/gstreamer/head/gst-plugins-base-plugins/html/gst-plugins-base-plugins-textoverlay.html) element, with most of the modifications being made to the GstBaseTextOverlay class. It calls upon the pango and cairo libraries the render the scene described by each GstBuffer and its attached metadata.

---

### Current limitations
The code in this package provides reasonably complete support for EBU-TT-D. However, it does currently have a few limitations:

* It supports only the rendering of text that flows left-to-right, top-to-bottom, and it does not support bi-directional text.
* Alpha values on foreground colours are supported only if the version of pango that the code is built against is at least 1.38.0. If it is built against an earlier version of pango, alpha channels on foreground colours will be ignored and they will be rendered fully opaque.
* As noted above, ttmlparse currently only works when each buffer passed to it contains a complete XML document; this will always be the case when subtitles are in-band within a DASH stream, but might not be the case when they are passed as an out-of-band XML file. (What ttmlparse probably needs is something like a [GstAdapter](http://gstreamer.freedesktop.org/data/doc/gstreamer/head/gstreamer-libs/html/GstAdapter.html) attached to its sink pad so that it can accumulate input data and process it only when a complete XML file has been received.)

Finally, it should be stressed again that this code currently supports only those TTML features that are included in EBU-TT-D specification; it does not currently handle the full range of features available in TTML.

---

### Building GStreamer modules
If building GStreamer modules from source, they should be built in the following order:
* gstreamer
* gst-plugins-base
* gst-plugins-good
* gst-plugins-bad
* gst-libav

It is suggested that these modules, once built, are installed under a different directory than that used for system packages (e.g., not under /usr on a Linux system); this avoids files from installed system GStreamer packages being overwritten by files from the modules built from source. Files associated with built modules will be installed under a non-system directory if its path is passed as a --prefix option to the configure script for each module, as shown below.

If a prefix is being used in this way, the PKG_CONFIG_PATH environment variable should be updated as follows to ensure that the build system can find headers and libraries associated with these installed modules:

```
$ export PKG_CONFIG_PATH=<install directory>/lib/pkgconfig
```

The process for building each module is the same:

1. Obtain the source code.
```
git clone git://anongit.freedesktop.org/gstreamer/<module name>
```

2. Configure and build the module.
```
cd <module name>
./autogen.sh
./configure --prefix=<install directory>
make
```

3. Install the module.
```
make install
```

---

### Disclaimer
BBC COPYRIGHT &copy; 2015

COMMERCIAL USE OR INTEGRATION OF THIS SOFTWARE INTO OTHER SOFTWARE MAY REQUIRE YOU TO TAKE LICENSES OR PAY RIGHTS FROM THIRD PARTIES. YOU SHOULD ALWAYS CHECK FOR ANY LIMITATIONS BEFORE INTEGRATING THIS SOFTWARE AND BEFORE USING IT COMMERCIALLY ON END-USER OR B2B PRODUCTS OR SERVICES.

THIS SOFTWARE IS PROVIDED 'AS IS', AT NO COST, WITHOUT ANY WARRANTIES INCLUDING WARRANTIES OF MERCHANTABILITY, FITNESS OR NON-INFRINGEMENT. IN NO EVENT SHALL THE BBC BE LIABLE FOR ANY SPECIAL, INDIRECT, CONSEQUENTIAL OR OTHER DAMAGES RESULTING FROM THE LOSS OF DATA, USE OR LOSS OF PROFITS AND GOODWILL WHETHER IN CONTRACT OR TORT UNDER ANY THEORY OF LOSS, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE, AND WHETHER THE PARTIES WERE INFORMED OR AWARE OF THE POSSIBILITY OF SUCH LOSS OR LOSSES.

THIS SOFTWARE IS DISTRIBUTED UNDER THE TERMS OF THE LESSER GPL LICENCE.
