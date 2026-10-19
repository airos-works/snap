/*
 * Copyright 2026, air/OS contributors.
 * Distributed under the terms of the MIT License.
 */


#include "TextTexture.h"

#include <Bitmap.h>
#include <View.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <utility>
#include <vector>


namespace snap {


namespace {


// A B_RGB32 bitmap with a view to draw into it, black to begin with. The
// bitmap owns the view (it deletes its views with its offscreen window).
class Canvas {
public:
	Canvas(int width, int height)
		:
		fBitmap(BRect(0, 0, width - 1, height - 1), B_BITMAP_ACCEPTS_VIEWS,
			B_RGB32),
		fView(new BView(fBitmap.Bounds(), "canvas", B_FOLLOW_NONE,
			B_WILL_DRAW))
	{
		fBitmap.AddChild(fView);
		fBitmap.Lock();
		fView->SetLowColor(0, 0, 0);
		fView->FillRect(fView->Bounds(), B_SOLID_LOW);
		fView->SetHighColor(255, 255, 255);
		fView->SetDrawingMode(B_OP_OVER);
	}

	~Canvas()
	{
		if (fBitmap.IsLocked())
			fBitmap.Unlock();
	}

	BView& View() { return *fView; }

	// Waits for app_server to finish drawing, so the bits are complete.
	BBitmap& Finish()
	{
		fView->Sync();
		fBitmap.Unlock();
		return fBitmap;
	}

private:
	BBitmap		fBitmap;
	BView*		fView;
};


void
Upload(Texture& texture, const BBitmap& bitmap)
{
	const BRect bounds = bitmap.Bounds();
	texture.Allocate(bounds.IntegerWidth() + 1, bounds.IntegerHeight() + 1,
		GL_RGBA8);
	texture.UploadBGRA(bitmap.Bits(), bitmap.BytesPerRow());
}


}	// namespace


// #pragma mark - TextTexture


TextTexture::TextTexture()
	:
	fFontSize(0),
	fWidth(0),
	fHeight(0)
{
}


status_t
TextTexture::SetTo(const char* text, const BFont& font)
{
	if (fTexture.IsValid() && fText == text && fFontSize == font.Size())
		return B_OK;

	font_height height;
	font.GetHeight(&height);
	// A pixel of room around the text, so the filtering at the texture's
	// edges never cuts into antialiased glyph edges.
	const int width = (int)ceilf(font.StringWidth(text)) + 4;
	const int lineHeight = (int)ceilf(height.ascent + height.descent) + 4;

	Canvas canvas(width, lineHeight);
	canvas.View().SetFont(&font);
	canvas.View().DrawString(text, BPoint(2, 2 + ceilf(height.ascent)));
	Upload(fTexture, canvas.Finish());

	fText = text;
	fFontSize = font.Size();
	fWidth = width;
	fHeight = lineHeight;
	return B_OK;
}


// #pragma mark -


int32
MakeGlyphRamp(Texture& texture)
{
	// Characters with a wide range of ink, from a dot to a block.
	const char* candidates = " .'`,:;-~_\"^+=<>!|/\\()[]{}?1il7rcvxzjtf"
		"suneaoCJLTYZXUQ0OSmwqpdbkhgy*#%&8$@BMWN";
	const int count = (int)strlen(candidates);

	BFont font(be_fixed_font);
	font.SetSize(28);
	font_height height;
	font.GetHeight(&height);
	const int cellWidth = (int)ceilf(font.StringWidth("M"));
	const int cellHeight = (int)ceilf(height.ascent + height.descent);

	// Draw every candidate once, and weigh its ink.
	Canvas all(cellWidth * count, cellHeight);
	all.View().SetFont(&font);
	for (int i = 0; i < count; i++) {
		const char glyph[2] = { candidates[i], '\0' };
		all.View().DrawString(glyph, BPoint(i * cellWidth, ceilf(height.ascent)));
	}
	BBitmap& weighed = all.Finish();
	std::vector<std::pair<float, char> > ink;
	for (int i = 0; i < count; i++) {
		float sum = 0;
		for (int y = 0; y < cellHeight; y++) {
			const uint8* row = (const uint8*)weighed.Bits()
				+ y * weighed.BytesPerRow() + i * cellWidth * 4;
			for (int x = 0; x < cellWidth; x++)
				sum += row[x * 4 + 2];
		}
		ink.push_back(std::make_pair(sum, candidates[i]));
	}
	std::sort(ink.begin(), ink.end());

	// Keep 16 glyphs spread evenly over the range of ink.
	const int32 kRampLength = 16;
	std::vector<char> ramp;
	const float lightest = ink.front().first;
	const float heaviest = ink.back().first;
	size_t next = 0;
	for (int32 step = 0; step < kRampLength; step++) {
		const float wanted = lightest
			+ (heaviest - lightest) * step / (kRampLength - 1);
		while (next + 1 < ink.size() && ink[next].first < wanted)
			next++;
		ramp.push_back(ink[next].second);
		if (next + 1 < ink.size())
			next++;
	}

	Canvas canvas(cellWidth * kRampLength, cellHeight);
	canvas.View().SetFont(&font);
	for (int32 i = 0; i < kRampLength; i++) {
		const char glyph[2] = { ramp[i], '\0' };
		canvas.View().DrawString(glyph,
			BPoint(i * cellWidth, ceilf(height.ascent)));
	}
	Upload(texture, canvas.Finish());
	return kRampLength;
}


}	// namespace snap
