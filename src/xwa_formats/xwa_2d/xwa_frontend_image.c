#include "aeron/asset/bmp.h"
#include "xwa_2d_internal.h"

#define CBM_DESC_SIZE 36u
#define CBM_IMAGE_HEADER_SIZE 2084u

int Xwa2d_DecodeCbm(const uint8_t* bytes, size_t size, Xwa2dFrameSet* out, char* error, size_t error_size) {
	if (!bytes || !out || size < CBM_DESC_SIZE)
		return xwa2d_fail(error, error_size, "invalid CBM input");
	memset(out, 0, sizeof *out);
	int frame_count = xwa2d_i32(bytes);
	if (frame_count <= 0 || frame_count > 4096)
		return xwa2d_fail(error, error_size, "invalid CBM frame count");
	size_t offset = CBM_DESC_SIZE;
	for (int i = 0; i < frame_count; i++) {
		if (size - offset < CBM_IMAGE_HEADER_SIZE)
			goto malformed;
		const uint8_t* header = bytes + offset;
		int width = xwa2d_i32(header);
		int height = xwa2d_i32(header + 4);
		int compressed = xwa2d_i32(header + 8);
		int pixel_count = xwa2d_i32(header + 12);
		int bounds_right = xwa2d_i32(header + 24);
		int bounds_bottom = xwa2d_i32(header + 28);
		offset += CBM_IMAGE_HEADER_SIZE;
		if (width <= 0 || height <= 0 || pixel_count <= 0 || (size_t)pixel_count > size - offset)
			goto malformed;
		Xwa2dFrame frame = { 0 };
		if (!Xwa2d_DecodeIndexedFrame(bytes + offset, (size_t)pixel_count, compressed, width, height,
									  bounds_right, bounds_bottom, header + 1060, &frame, error, error_size))
			goto malformed;
		frame.frame_index = i;
		frame.sprite_id = -1;
		if (!xwa2d_append_frame(out, &frame)) {
			free(frame.rgba);
			goto oom;
		}
		offset += (size_t)pixel_count;
	}
	return 1;

oom:
	Xwa2dFrameSet_Free(out);
	return xwa2d_fail(error, error_size, "CBM allocation failed");
malformed:
	Xwa2dFrameSet_Free(out);
	return xwa2d_fail(error, error_size, "malformed CBM resource");
}

int Xwa2d_DecodeBmp(const uint8_t* bytes, size_t size, Xwa2dFrameSet* out, char* error, size_t error_size) {
	if (!out)
		return xwa2d_fail(error, error_size, "invalid BMP output");
	memset(out, 0, sizeof *out);
	AeronIndexedFrame decoded = { 0 };
	AeronDecodeError decode_error = { 0 };
	if (!AeronBmp_Decode(bytes, size, &decoded, &decode_error))
		return xwa2d_fail(error, error_size, "%s", decode_error.message);
	Xwa2dFrame frame = { 0 };
	frame.width = decoded.width;
	frame.height = decoded.height;
	frame.sprite_id = -1;
	const size_t pixels = (size_t)decoded.width * decoded.height;
	frame.rgba = calloc(pixels, 4);
	if (!frame.rgba)
		goto oom;
	/* XWA's frontend BMP draw policy keys palette index zero. */
	for (size_t i = 0; i < pixels; ++i) {
		const uint8_t index = decoded.indices[i];
		if (!index)
			continue;
		memcpy(frame.rgba + i * 4, decoded.palette[index], 3);
		frame.rgba[i * 4 + 3] = decoded.coverage[i];
	}
	if (!xwa2d_append_frame(out, &frame))
		goto oom;
	AeronIndexedFrame_Free(&decoded);
	return 1;
oom:
	free(frame.rgba);
	AeronIndexedFrame_Free(&decoded);
	return xwa2d_fail(error, error_size, "BMP RGBA allocation failed");
}
