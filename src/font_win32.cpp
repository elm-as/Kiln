#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cstdio>
#include <vector>
#include "render.h"

// Let GDI rasterize the glyphs into a DIB, then upload that as the atlas.
// Keep smooth off for anything drawn on the canvas, AA looks wrong once scaled up.
Font font_create(const char* face, int px, bool smooth)
{
    Font f = {};

    HDC dc = CreateCompatibleDC(0);
    HFONT hf = CreateFontA(-px, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                           OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, smooth ? ANTIALIASED_QUALITY : NONANTIALIASED_QUALITY,
                           DEFAULT_PITCH, face);
    HGDIOBJ old_font = SelectObject(dc, hf);

    TEXTMETRICA tm;
    GetTextMetricsA(dc, &tm);
    f.height = tm.tmHeight;

    const int W = 256;
    int x = 1, y = 1;
    for (int c = 32; c < 256; c++) {
        uint32_t cp = glyph_codepoint(c);
        if (glyph_slot(cp) != c)
            continue;
        wchar_t ch = (wchar_t)cp;
        SIZE sz;
        GetTextExtentPoint32W(dc, &ch, 1, &sz);
        if (x + sz.cx + 1 > W) {
            x = 1;
            y += f.height + 1;
        }
        f.glyphs[c] = { (short)x, (short)y, (short)sz.cx };
        x += sz.cx + 1;
    }
    int H = y + f.height + 1;

    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
    bi.bmiHeader.biWidth = W;
    bi.bmiHeader.biHeight = -H;    // top-down
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    void* bits = 0;
    HBITMAP bmp = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, 0, 0);
    if (!bmp || !bits) {
        SelectObject(dc, old_font);
        if (bmp)
            DeleteObject(bmp);
        DeleteObject(hf);
        DeleteDC(dc);
        return f;
    }
    HGDIOBJ old_bmp = SelectObject(dc, bmp);

    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(255, 255, 255));
    for (int c = 32; c < 256; c++) {
        if (!f.glyphs[c].w)
            continue;
        wchar_t ch = (wchar_t)glyph_codepoint(c);
        TextOutW(dc, f.glyphs[c].x, f.glyphs[c].y, &ch, 1);
    }
    GdiFlush();

    std::vector<uint8_t> rgba(size_t(W) * H * 4);
    const uint8_t* src = (const uint8_t*)bits;
    for (int i = 0; i < W * H; i++) {
        rgba[i * 4 + 0] = 255;
        rgba[i * 4 + 1] = 255;
        rgba[i * 4 + 2] = 255;
        rgba[i * 4 + 3] = src[i * 4];
    }
    f.tex = texture_create(W, H, rgba.data());

    SelectObject(dc, old_bmp);
    SelectObject(dc, old_font);
    DeleteObject(bmp);
    DeleteObject(hf);
    DeleteDC(dc);

    if (!f.height)
        fprintf(stderr, "font_create: '%s' %dpx came out empty\n", face, px);
    return f;
}

void font_free(Font* f)
{
    texture_free(&f->tex);
}
