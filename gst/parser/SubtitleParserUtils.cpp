#include "SubtitleParserUtils.h"

using namespace SubtitleParserUtils;

namespace {
	namespace StyleUtils {
		GstSubtitleColor ARGBColorToGstSubtileColor(uint32_t colorARGB)
		{
			guint8 a = colorARGB >> 24;
			guint8 r = (colorARGB >> 16) & 0xFF;
			guint8 g = (colorARGB >> 8) & 0xFF;
			guint8 b = colorARGB & 0xFF;

			return { r, g, b, a };
		}

		gboolean colorIsTransparent(const GstSubtitleColor* color)
		{
			if(!color)
				return FALSE;
			else
				return (color->a == 0);
		}

		/* TODO: Implement actual blending of colors. */
		GstSubtitleColor blendColors(GstSubtitleColor color1, GstSubtitleColor color2)
		{
			if(colorIsTransparent(&color2))
				return color1;
			else
				return color2;
		}

		void updateRegionStyleSet(GstSubtitleStyleSet* style_set, const timedText::RegionStyle& tt_style_set)
		{
			if(tt_style_set.backgroundColorARGB != 0)
				style_set->background_color = StyleUtils::ARGBColorToGstSubtileColor(tt_style_set.backgroundColorARGB);


			if(tt_style_set.origin.x.unit == timedText::LengthUnit::percentage)
			{
				style_set->origin_x = tt_style_set.origin.x.value / 100.0;
				style_set->origin_y = tt_style_set.origin.y.value / 100.0;
			}		


			if(tt_style_set.extent.x.unit == timedText::LengthUnit::percentage)
			{
				style_set->extent_w = tt_style_set.extent.x.value / 100.0;
				if((style_set->origin_x + style_set->extent_w) > 1.0) {
					style_set->extent_w = 1.0 - style_set->origin_x;
				}

				style_set->extent_h = tt_style_set.extent.y.value / 100.0;
				if((style_set->origin_y + style_set->extent_h) > 1.0) {
					style_set->extent_h = 1.0 - style_set->origin_y;
				}
			}


			if(tt_style_set.displayAlign == timedText::DisplayAlign::center)
				style_set->display_align = GST_SUBTITLE_DISPLAY_ALIGN_CENTER;
			if(tt_style_set.displayAlign == timedText::DisplayAlign::after)
				style_set->display_align = GST_SUBTITLE_DISPLAY_ALIGN_AFTER;
			else
				style_set->display_align = GST_SUBTITLE_DISPLAY_ALIGN_BEFORE;


			if(tt_style_set.padding.left.unit == timedText::LengthUnit::percentage)
			{
				style_set->padding_before = tt_style_set.padding.top.value / 100.0;
				style_set->padding_end = tt_style_set.padding.right.value / 100.0;
				style_set->padding_after = tt_style_set.padding.bottom.value / 100.0;
				style_set->padding_start = tt_style_set.padding.left.value / 100.0;

				/* Padding values in TTML files are relative to the region width & height;
				 * make them relative to the overall display width & height like all other
				 * dimensions. */
				style_set->padding_before *= style_set->extent_h;
				style_set->padding_after *= style_set->extent_h;
				style_set->padding_end *= style_set->extent_w;
				style_set->padding_start *= style_set->extent_w;
			}


			if(tt_style_set.writingMode == timedText::WritingMode::rl)
				style_set->writing_mode = GST_SUBTITLE_WRITING_MODE_RLTB;
			else if(tt_style_set.writingMode == timedText::WritingMode::tbrl
				|| tt_style_set.writingMode == timedText::WritingMode::tb)
				style_set->writing_mode = GST_SUBTITLE_WRITING_MODE_TBRL;
			else if(tt_style_set.writingMode == timedText::WritingMode::tblr)
				style_set->writing_mode = GST_SUBTITLE_WRITING_MODE_TBLR;
			else
				style_set->writing_mode = GST_SUBTITLE_WRITING_MODE_LRTB;


			if(tt_style_set.isBackgroundAlwaysShown)
				style_set->show_background = GST_SUBTITLE_BACKGROUND_MODE_ALWAYS;
			else
				style_set->show_background = GST_SUBTITLE_BACKGROUND_MODE_WHEN_ACTIVE;


			if(tt_style_set.isOverflowClipped)
				style_set->overflow = GST_SUBTITLE_OVERFLOW_MODE_HIDDEN;
			else
				style_set->overflow = GST_SUBTITLE_OVERFLOW_MODE_VISIBLE;
		}

		void updateBlockStyleSet(GstSubtitleStyleSet* style_set, const timedText::PStyle& tt_style_set,
			uint64_t cellColumns, uint64_t cellRows)
		{
			style_set->unicode_bidi = GST_SUBTITLE_UNICODE_BIDI_NORMAL;

			
			if(tt_style_set.ebuttsMultiRowAlign == timedText::MultiRowAlign::start)
				style_set->multi_row_align = GST_SUBTITLE_MULTI_ROW_ALIGN_START;
			else if(tt_style_set.ebuttsMultiRowAlign == timedText::MultiRowAlign::center)
				style_set->multi_row_align = GST_SUBTITLE_MULTI_ROW_ALIGN_CENTER;
			else if(tt_style_set.ebuttsMultiRowAlign == timedText::MultiRowAlign::end)
				style_set->multi_row_align = GST_SUBTITLE_MULTI_ROW_ALIGN_END;
			else
				style_set->multi_row_align = GST_SUBTITLE_MULTI_ROW_ALIGN_AUTO;


			if(tt_style_set.backgroundColorARGB != 0)
				style_set->background_color = StyleUtils::ARGBColorToGstSubtileColor(tt_style_set.backgroundColorARGB);


			if(!(tt_style_set.ebuttsLinePadding == 0))
			{
				style_set->line_padding = tt_style_set.ebuttsLinePadding.value;
				style_set->line_padding *= (1.0 / cellColumns);
			}


			if(!(tt_style_set.lineHeight == 0) && tt_style_set.lineHeight.unit == timedText::LengthUnit::percentage)
				style_set->line_height = tt_style_set.lineHeight.value / 100.0;


			if(tt_style_set.textAlign == timedText::TextAlign::left)
				style_set->text_align = GST_SUBTITLE_TEXT_ALIGN_LEFT;
			else if(tt_style_set.textAlign == timedText::TextAlign::center)
				style_set->text_align = GST_SUBTITLE_TEXT_ALIGN_CENTER;
			else if(tt_style_set.textAlign == timedText::TextAlign::right)
				style_set->text_align = GST_SUBTITLE_TEXT_ALIGN_RIGHT;
			else if(tt_style_set.textAlign == timedText::TextAlign::end)
				style_set->text_align = GST_SUBTITLE_TEXT_ALIGN_END;
			else
				style_set->text_align = GST_SUBTITLE_TEXT_ALIGN_START;
		}

		void updateElementStyleSet(GstSubtitleStyleSet* style_set, const timedText::SpanStyle& tt_style_set,
			uint64_t cellColumns, uint64_t cellRows)
		{
			//hardcode			
			style_set->unicode_bidi = GST_SUBTITLE_UNICODE_BIDI_NORMAL;


			if(tt_style_set.fontFamily != "default")
			{
				if(tt_style_set.fontFamily.size() <= maxFontFamilyNameLength) {
					g_free(style_set->font_family); //why?					
					style_set->font_family = g_strdup(tt_style_set.fontFamily.c_str());
				}
				else {
					GST_CAT_WARNING(ttmlparse,
						"Ignoring font family name as it's overly long.");
				}

			}


			if(tt_style_set.backgroundColorARGB != 0)
				style_set->background_color = StyleUtils::ARGBColorToGstSubtileColor(tt_style_set.backgroundColorARGB);


			if(tt_style_set.fontSize.horizontal.unit == timedText::LengthUnit::percentage &&
				tt_style_set.fontSize.horizontal.value != 0)
			{
				style_set->font_size = tt_style_set.fontSize.horizontal.value / 100.0;
			}
			style_set->font_size *= (1.0 / cellRows);


			if(tt_style_set.fontStyle == timedText::FontStyle::italic)
				style_set->font_style = GST_SUBTITLE_FONT_STYLE_ITALIC;
			else
				style_set->font_style = GST_SUBTITLE_FONT_STYLE_NORMAL;

			if(tt_style_set.isFontBold)
				style_set->font_weight = GST_SUBTITLE_FONT_WEIGHT_BOLD;
			else
				style_set->font_weight = GST_SUBTITLE_FONT_WEIGHT_NORMAL;

			if(tt_style_set.textDecoration.isUnderline)
				style_set->text_decoration = GST_SUBTITLE_TEXT_DECORATION_UNDERLINE;
			else
				style_set->text_decoration = GST_SUBTITLE_TEXT_DECORATION_NONE;

			if(tt_style_set.isWordWrapped)
				style_set->wrap_option = GST_SUBTITLE_WRAPPING_ON;
			else
				style_set->wrap_option = GST_SUBTITLE_WRAPPING_OFF;


			if(tt_style_set.colorARGB != 0xffffffff)
				style_set->color = StyleUtils::ARGBColorToGstSubtileColor(tt_style_set.colorARGB);
		}
	}
}


/* Scene */

GstSubtitleRegion* Scene::getRegionByCue(std::map<std::string, GstSubtitleRegion*>& gstSubRegionsById, const timedText::Subtitle::const_shared_ptr& cue)
{
	std::string id = cue->region.id;
	GstSubtitleRegion* region;
	if(gstSubRegionsById.find(id) == gstSubRegionsById.end())
	{
		//create region						
		GstSubtitleStyleSet* region_style;

		region_style = gst_subtitle_style_set_new();
		StyleUtils::updateRegionStyleSet(region_style, cue->region);
		region = gst_subtitle_region_new(region_style);

		//save region
		gstSubRegionsById[id] = region;
	}

	return gstSubRegionsById[id];
}

GstSubtitleBlock* Scene::createBlock(const timedText::Subtitle::const_shared_ptr& cue)
{
	//<region>, <div> and <p> color blending
	GstSubtitleColor blockColor, divColor;
	blockColor = StyleUtils::ARGBColorToGstSubtileColor(cue->region.backgroundColorARGB);
	divColor = StyleUtils::ARGBColorToGstSubtileColor(cue->divTag.backgroundColorARGB);
	blockColor = StyleUtils::blendColors(blockColor, divColor);

	GstSubtitleColor pColor;
	pColor = StyleUtils::ARGBColorToGstSubtileColor(cue->pTag.backgroundColorARGB);
	blockColor = StyleUtils::blendColors(blockColor, divColor);

	//create block
	GstSubtitleStyleSet* block_style;
	block_style = gst_subtitle_style_set_new();
	StyleUtils::updateBlockStyleSet(block_style, cue->pTag, cue->cellColumns,
		cue->cellRows);

	GstSubtitleBlock* block = NULL;
	block_style->background_color = blockColor;
	block = gst_subtitle_block_new(block_style);
	g_assert(block != NULL);

	//create elements
	for(const auto& span : cue->spanList)
	{
		GstSubtitleElement* currElement = createElement(span, cue->cellColumns, cue->cellRows);
		//add element to block
		gst_subtitle_block_add_element(block, currElement);

	}

	return block;
}

GstSubtitleElement* Scene::createElement(const timedText::TextSpan& span, uint64_t cellColumns, uint64_t cellRows)
{
	GstSubtitleStyleSet* element_style = NULL;
	guint buffer_index;
	GstSubtitleElement* element = NULL;

	element_style = gst_subtitle_style_set_new();
	StyleUtils::updateElementStyleSet(element_style, span.style,
		cellColumns, cellRows);

	if(!span.newLine)
		buffer_index = addTextToBuffer(span.text.c_str());
	else
	{
		std::ostringstream ssTextWithNewLine;
		ssTextWithNewLine << span.text << std::endl;
		buffer_index = addTextToBuffer(ssTextWithNewLine.str().c_str());
	}

	element = gst_subtitle_element_new(element_style, buffer_index, false);

	return element;
}

guint Scene::addTextToBuffer(const gchar* text)
{
	GstMemory* mem;
	GstMapInfo map;
	guint ret;

	mem = gst_allocator_alloc(NULL, strlen(text) + 1, NULL);
	if(!gst_memory_map(mem, &map, GST_MAP_WRITE))
		GST_CAT_ERROR(ttmlparse, "Failed to map memory.");

	g_strlcpy((gchar*)map.data, text, map.size);
	GST_CAT_DEBUG(ttmlparse, "Inserted following text into buffer: %s",
		(gchar*)map.data);
	gst_memory_unmap(mem, &map);

	ret = gst_buffer_n_memory(this->buf);
	gst_buffer_insert_memory(this->buf, -1, mem);
	return ret;
}

GstBuffer* Scene::attachMetadata(const std::map<std::string, GstSubtitleRegion*>& regionsMap)
{
	GstSubtitleMeta* meta;

	if(buf != nullptr && !regionsMap.empty())
	{
		meta = (GstSubtitleMeta*)gst_buffer_add_meta(this->buf,
			GST_SUBTITLE_META_INFO, NULL);

		GPtrArray* gstRegions = g_ptr_array_new_with_free_func(
			(GDestroyNotify)gst_subtitle_region_unref);

		for(auto region : regionsMap)
			g_ptr_array_add(gstRegions, region.second);

		meta->regions = gstRegions;
	}

	return this->buf;
}

GstBuffer* Scene::fillAndGetBuffer()
{
	this->buf = gst_buffer_new();
	GST_BUFFER_PTS(this->buf) = this->begin;
	GST_BUFFER_DURATION(this->buf) = (this->end - this->begin);

	std::map<std::string, GstSubtitleRegion*> gstSubRegions;
	for(const auto cue : this->subtitleCues)
	{
		GstSubtitleRegion* region = getRegionByCue(gstSubRegions, cue);
		GstSubtitleBlock* currentBlock = createBlock(cue);

		//add block to region
		gst_subtitle_region_add_block(region, currentBlock);
	}

	return attachMetadata(gstSubRegions);
}


/* ScenesHandler */

SubtitleConstList ScenesHandler::getCuesAtTimeUs(int64_t timeUs)
{
	SubtitleConstList cues;
	auto currTrackId = this->pool.getCurrentTrackIndex();
	for(int cueIndex = 0; cueIndex < pool.getNumberOfCues(this->trackId); ++cueIndex)
	{
		auto cue = this->pool.getCueAtIndex(this->trackId, cueIndex);
		if(cue->startUs <= timeUs && timeUs < cue->endUs())
			cues.push_back(cue);
	}

	return cues;
}

int64_t ScenesHandler::findNextTransition(int64_t timeUs)
{
	int64_t nextTransitionUs = std::numeric_limits<int64_t>::max();
	for(int cueIndex = 0; cueIndex < this->pool.getNumberOfCues(this->trackId); ++cueIndex)
	{
		auto cue = this->pool.getCueAtIndex(this->trackId, cueIndex);
		if(cue->startUs < nextTransitionUs && cue->startUs > timeUs)		
			nextTransitionUs = cue->startUs;

		if(cue->endUs() < nextTransitionUs && cue->endUs() > timeUs)		
			nextTransitionUs = cue->endUs();		
	}

	return nextTransitionUs;
}

clc_Result ScenesHandler::createScenes()
{
	int64_t timeUs = -1;
	while((timeUs = findNextTransition(timeUs)) != std::numeric_limits<int64_t>::max())
	{
		auto lastSceneIt = this->scenes.rbegin();
		if(this->scenes.size() > 0 && ((*lastSceneIt)->getEndNs() == GST_CLOCK_TIME_NONE))
			(*lastSceneIt)->setEndNs(timeUs * 1000.0);

		SubtitleConstList cues = getCuesAtTimeUs(timeUs);
		if(!cues.empty())
		{
			auto curScene = std::make_unique<Scene>(std::move(cues));
			curScene->setStartNs(timeUs * 1000.0);
			scenes.push_back(std::move(curScene));
		}
	}

	if(this->scenes.empty())
		return CLC_FAIL;

	return CLC_SUCCESS;
}

std::vector<GstBuffer*> ScenesHandler::getScenesBuffersList()
{
	std::vector<GstBuffer*> bufferList;
	for(const auto& scene : this->scenes)
	{
		GstBuffer* buf = scene->fillAndGetBuffer();
		if(buf != nullptr)
			bufferList.push_back(buf);
	}
	
	return bufferList;
}