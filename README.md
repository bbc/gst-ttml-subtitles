## GStreamer TTML subtitling package

** The elements and library in this project provide a means for GStreamer pipelines to parse and render [TTML](http://www.w3.org/TR/ttaf1-dfxp/) subtitles. More specifically, the code handles the [EBU-TT-D](https://tech.ebu.ch/ebu-tt) profile of TTML designed for the distribution of subtitles over IP. **

### Getting Started

##### Install GStreamer
In order to run this code, you will need a recent clone of the git masters of the various GStreamer packages (gstreamer, gst-plugins-base, gst-plugins-good, gst-plugins-bad, gst-libav) installed, as recent patches to dashdemux and qtdemux are needed by the subtitle elements.

Also, as the code calls upon the pango and cairo libraries in order to render subtitles, you will need these installed on your system.

##### Clone this repository

```
$ git clone https://github.com/bbc/gst-ttml-subtitles
```

##### Build the subtitling code

In order to build the subtitling code, you will need to ensure that pkg-config can find the installed GStreamer packages. This requires that the directory containing the GStreamer \*.pc files is either (a) in one of the normal directories that pkg-config searches for \*.pc files (_/usr/lib/pkgconfig_, _/usr/share/pkgconfig_, _/usr/local/lib/pkgconfig_ and _/usr/local/share/pkgconfig_), or (b) listed in the PKG_CONFIG_PATH environment variable.

Enter the directory containing the subtitling code and run the following:

```
$ ./autogen.sh --prefix=<prefix used for GStreamer packages>
$ make && make install
```

This should install the subtitling elements in the same location as the other GStreamer elements. To check that they have been correctly installed, run the following:

```
$ gst-inspect-1.0 ttmlparse
$ gst-inspect-1.0 ttmlrender
```

If they are present, the above commands should result in information about the ttmlparse and ttmlrender elements being output to the terminal.

##### Test the code

BBC R&D has created an MPEG DASH [test stream](http://rdmedia.bbc.co.uk/dash/ondemand/elephants_dream/) containing an EBU-TT-D subtitle component, which can be used to test that the elements are installed and working.

To test the code, simply run:

```
$ gst-play-1.0 http://rdmedia.bbc.co.uk/dash/ondemand/elephants_dream/1/client_manifest-all.mpd
```

You should see Elephants Dream play with subtitles rendered on top of the video.

---

### Code overview
The TTML subtitling code follows the subtitling approach used currently in GStreamer, in which subtitle parsing and rendering are handled by different elements. The subtitle parser element is called _ttmlparse_, and the rendering element is called _ttmlrender_.

#### Parser element (ttmlparse)
The ttmlparse parser element is an expanded version of the [subparse](http://gstreamer.freedesktop.org/data/doc/gstreamer/head/gst-plugins-base-plugins/html/gst-plugins-base-plugins-subparse.html) element, which is the standard element within gst-plugins-base that parses subtitles of different formats.

ttmlparse parses the input TTML file into an tree structure, and it resolves the styles and regions referenced by the elements within that tree. From this tree, the parser determines the resulting _scenes_: the periods of time over which a static set of subtitles should be displayed. For example, the following subtitle file:

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

The existing method of interchange between subtitle parser and renderer in GStreamer is _pango-markup_, in which styling information is included inline with the subtitle text using a markup syntax. Unfortunately, pango-markup does not support all of the layout and styling options available in TTML; therefore, a different exchange format is used in this code.

This code contains a library that defines different types that can be used to describe the layout and styling of a scene. The main types are shown below:

Type | Description
-----|------------
GstSubtitleElement|Describes an inline text element resulting from a `span` or `br`.
GstSubtitleBlock|Describes a block of text resulting from a `div` or `p` element; contains one or more GstSubtitleElements.
GstSubtitleRegion|Describes an on-screen region into which subtitles are visible; contans zero or more GstSubtitleBlocks.
GstSubtitleStyleSet|Describes the styling options that should be applied to an element; all three of types above have a GstSubtitleStyleSet associated with them.

For each scene, the parser creates objects of the above types to describe the layout of its components and attaches it as metadata (using a metadata type, GstSubtitleMeta, defined in the library) to the GstBuffer that holds the text from that scene. Within the GstBuffer, the text associated with each GstSubtitleElement sits within its own GstMemory and is indexed by a field in the corresponding GstSubtitleElement structure.

The figure below shows the types created for a simple subtitle file that contains a single scene:

![Submeta](file:///C:/tmp/submeta.png)

#### Renderer element (ttmlrender)

The render element, ttmlrender, is a modified version of the GStreamer [textoverlay](http://gstreamer.freedesktop.org/data/doc/gstreamer/head/gst-plugins-base-plugins/html/gst-plugins-base-plugins-textoverlay.html) element, with most of the modifications being made to the GstBaseTextOverlay class. It calls upon the pango and cairo libraries the render the scene described by each GstBuffer and its attached metadata.

---

### Current limitations
The code in this package provides reasonably complete support for EBU-TT-D features. However, it does currently have a couple of limitations:

* It supports only the rendering of text that flows left-to-right and top-to-bottom.
* Alpha values on foreground colours are ignored. Internally, ttmlrender creates a string marked up with pango-markup to render the text in each block and, unfortunately, colours in pango-markup can be specified by RGB values only. Note that this limitation does not apply to background colours: ttmlrender renders background rectangles directly using cairo, which supports transparency. Also note that the limitation on foreground colours shouldn't be much of an issue in practice, since it rather defeats the purpose of subtitles to make their text semi-transparent and, hence, more difficult to read!

---

### Disclaimer
BBC COPYRIGHT &copy; 2015

COMMERCIAL USE OR INTEGRATION OF THIS SOFTWARE INTO OTHER SOFTWARE MAY REQUIRE YOU TO TAKE LICENSES OR PAY RIGHTS FROM THIRD PARTIES. YOU SHOULD ALWAYS CHECK FOR ANY LIMITATIONS BEFORE INTEGRATING THIS SOFTWARE AND BEFORE USING IT COMMERCIALLY ON END-USER OR B2B PRODUCTS OR SERVICES.

THIS SOFTWARE IS PROVIDED 'AS IS', AT NO COST, WITHOUT ANY WARRANTIES INCLUDING WARRANTIES OF MERCHANTABILITY, FITNESS OR NON-INFRINGEMENT. IN NO EVENT SHALL THE BBC BE LIABLE FOR ANY SPECIAL, INDIRECT, CONSEQUENTIAL OR OTHER DAMAGES RESULTING FROM THE LOSS OF DATA, USE OR LOSS OF PROFITS AND GOODWILL WHETHER IN CONTRACT OR TORT UNDER ANY THEORY OF LOSS, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE, AND WHETHER THE PARTIES WERE INFORMED OR AWARE OF THE POSSIBILITY OF SUCH LOSS OR LOSSES.

THIS SOFTWARE IS DISTRIBUTED UNDER THE TERMS OF THE LESSER GPL LICENCE.
