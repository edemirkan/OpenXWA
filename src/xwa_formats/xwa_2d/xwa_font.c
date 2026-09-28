#include "xwa_2d_internal.h"

#include "aeron/asset/abp_font.h"

#define FLIGHT_FONT_FIRST_GLYPH 32
#define FLIGHT_FONT_GLYPH_COUNT 256
#define FLIGHT_FONT_COLUMNS 16
#define FLIGHT_FONT_ROWS 14
#define FLIGHT_FONT_GUTTER 2

typedef struct FlightFontTierDesc {
	int source_index;
	int glyph_size;
	int width_padding;
} FlightFontTierDesc;

static const FlightFontTierDesc flight_font_tiers[3] = {
	{ 1, 16, 2 },
	{ 0, 12, 1 },
	{ 2, 10, 1 },
};

int Xwa2d_DecodeAbpFont(const uint8_t* bytes, size_t size, Xwa2dFontAtlas* out, char* error,
						size_t error_size) {
	if (!out)
		return xwa2d_fail(error, error_size, "invalid ABP output");
	memset(out, 0, sizeof *out);
	AeronDecodedFont font = { 0 };
	AeronDecodeError decode_error = { 0 };
	if (!AeronAbpFont_Decode(bytes, size, &font, &decode_error))
		return xwa2d_fail(error, error_size, "%s", decode_error.message);
	out->rgba = calloc((size_t)font.width * font.height, 4);
	out->glyphs = calloc(font.glyph_count, sizeof *out->glyphs);
	if (!out->rgba || !out->glyphs) {
		AeronDecodedFont_Free(&font);
		Xwa2dFontAtlas_Free(out);
		return xwa2d_fail(error, error_size, "ABP RGBA allocation failed");
	}
	for (uint16_t i = 0; i < font.glyph_count; ++i) {
		const AeronDecodedGlyph* glyph = &font.glyphs[i];
		out->glyphs[i] = (Xwa2dGlyph) { glyph->x, glyph->y, glyph->width, glyph->height, glyph->advance };
		for (uint16_t y = 0; y < glyph->height; ++y) {
			for (uint16_t x = 0; x < glyph->width; ++x) {
				const size_t pixel = (size_t)(glyph->y + y) * font.width + glyph->x + x;
				memset(out->rgba + pixel * 4, 255, 3);
				out->rgba[pixel * 4 + 3] = font.foreground[pixel];
			}
		}
	}
	out->width = font.width;
	out->height = font.height;
	out->cell_width = font.cell_width;
	out->cell_height = font.cell_height;
	out->baseline = font.baseline;
	out->first_char = font.first_char;
	out->glyph_count = font.glyph_count;
	AeronDecodedFont_Free(&font);
	return 1;
}

static int flight_glyph_advance(const Xwa2dFrame* source, int glyph, int size, int padding) {
	const int index = glyph - FLIGHT_FONT_FIRST_GLYPH;
	const int sx = size * (index % FLIGHT_FONT_COLUMNS);
	const int sy = size * (index / FLIGHT_FONT_COLUMNS);
	for (int x = size - 1; x >= 0; x--) {
		for (int y = 0; y < size; y++) {
			if (source->rgba[((size_t)(sy + y) * source->width + sx + x) * 4u + 3] != 0)
				return x + 1 + padding;
		}
	}
	return size / 4 + padding;
}

int Xwa2d_BuildFlightFontTier(const Xwa2dFrameSet* group, int tier, Xwa2dFontAtlas* out, char* error,
							  size_t error_size) {
	if (!group || !out || tier < 0 || tier >= 3 || group->count < 3)
		return xwa2d_fail(error, error_size, "invalid flight font tier input");
	memset(out, 0, sizeof *out);
	const FlightFontTierDesc* desc = &flight_font_tiers[tier];
	const Xwa2dFrame* source = &group->frames[desc->source_index];
	if (source->width < desc->glyph_size * FLIGHT_FONT_COLUMNS ||
		source->height < desc->glyph_size * FLIGHT_FONT_ROWS)
		return xwa2d_fail(error, error_size, "flight font source is too small");
	const int stride = desc->glyph_size + 2 * FLIGHT_FONT_GUTTER;
	out->width = FLIGHT_FONT_COLUMNS * stride;
	out->height = FLIGHT_FONT_ROWS * stride;
	out->cell_width = desc->glyph_size;
	out->cell_height = desc->glyph_size;
	out->baseline = desc->glyph_size;
	out->first_char = 0;
	out->glyph_count = FLIGHT_FONT_GLYPH_COUNT;
	out->rgba = (uint8_t*)calloc((size_t)out->width * out->height, 4u);
	out->glyphs = (Xwa2dGlyph*)calloc(FLIGHT_FONT_GLYPH_COUNT, sizeof *out->glyphs);
	if (!out->rgba || !out->glyphs) {
		Xwa2dFontAtlas_Free(out);
		return xwa2d_fail(error, error_size, "flight font allocation failed");
	}
	for (int glyph = FLIGHT_FONT_FIRST_GLYPH; glyph < FLIGHT_FONT_GLYPH_COUNT; glyph++) {
		const int index = glyph - FLIGHT_FONT_FIRST_GLYPH;
		const int sx = desc->glyph_size * (index % FLIGHT_FONT_COLUMNS);
		const int sy = desc->glyph_size * (index / FLIGHT_FONT_COLUMNS);
		Xwa2dGlyph* metric = &out->glyphs[glyph];
		metric->x = (uint16_t)((index % FLIGHT_FONT_COLUMNS) * stride + FLIGHT_FONT_GUTTER);
		metric->y = (uint16_t)((index / FLIGHT_FONT_COLUMNS) * stride + FLIGHT_FONT_GUTTER);
		metric->width = (uint16_t)desc->glyph_size;
		metric->height = (uint16_t)desc->glyph_size;
		metric->advance =
			(uint16_t)flight_glyph_advance(source, glyph, desc->glyph_size, desc->width_padding);
		for (int y = 0; y < desc->glyph_size; y++) {
			memcpy(out->rgba + ((size_t)(metric->y + y) * out->width + metric->x) * 4u,
				   source->rgba + ((size_t)(sy + y) * source->width + sx) * 4u,
				   (size_t)desc->glyph_size * 4u);
		}
	}
	return 1;
}
