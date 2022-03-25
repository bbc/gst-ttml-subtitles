#include <gst/gst.h>
#include <SubtitlesParserFactory.h>
#include <gst/subtitle/subtitle.h>

GST_DEBUG_CATEGORY_STATIC(ttmlparse);

namespace SubtitleParserUtils
{
	typedef std::vector<timedText::Subtitle::const_shared_ptr> SubtitleConstList;
	static const int maxFontFamilyNameLength = 128;

	/* Represents a scene consisting of one or more text elements that
		* should be visible over a specific period of time. */
	class Scene {
	public:
		Scene() = delete;
		Scene(SubtitleConstList& cues) = delete;

		Scene(SubtitleConstList&& cues) : subtitleCues(std::move(cues)) {};

		void setStartNs(int64_t timeUs) { begin = timeUs; }
		void setEndNs(int64_t timeUs) { end = timeUs; }

		int64_t getStartNs() const { return begin; }
		int64_t getEndNs() const { return end; }

		//create data structures & attach them to GstBuffer
		GstBuffer* fillAndGetBuffer();

	private:
		//matches <region>
		GstSubtitleRegion* getRegionByCue(std::map<std::string, GstSubtitleRegion*>& gstSubRegionsById, const timedText::Subtitle::const_shared_ptr& cue);

		//matches <p>
		GstSubtitleBlock* createBlock(const timedText::Subtitle::const_shared_ptr& cue);

		//matches <span>
		GstSubtitleElement* createElement(const timedText::TextSpan& span, uint64_t cellColumns, uint64_t cellRows);

		guint addTextToBuffer(const gchar* text);
		
		GstBuffer* attachMetadata(const std::map<std::string, GstSubtitleRegion*>& regionsMap);

	private:
		GstClockTime begin = GST_CLOCK_TIME_NONE;
		GstClockTime end = GST_CLOCK_TIME_NONE;
		//timed text cues
		SubtitleConstList subtitleCues;
		GstBuffer* buf = nullptr;
		guint currBrTextIndexInGstBuffer = -1;
	};

	class ScenesHandler
	{
	public:
		ScenesHandler() = delete;
		ScenesHandler(const timedText::SubtitlesPool& pool) : pool(pool), trackId(pool.getCurrentTrackIndex()) {}

		clc_Result createScenes();

		std::vector<GstBuffer*> getScenesBuffersList();

	private:

		SubtitleConstList getCuesAtTimeUs(int64_t timeUs);
		
		int64_t findNextTransition(int64_t timestamp);

	private:
		//reference to Parser pool
		const timedText::SubtitlesPool& pool;
		std::vector<std::unique_ptr<Scene>> scenes;
		const size_t trackId;
	};
}