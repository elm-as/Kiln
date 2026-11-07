#include "image.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "fs.h"

static uint32_t be32(const uint8_t* p)
{
    return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3];
}

static uint8_t paeth(int a, int b, int c)
{
    int p = a + b - c, pa = abs(p - a), pb = abs(p - b), pc = abs(p - c);
    return (pa <= pb && pa <= pc) ? uint8_t(a) : (pb <= pc ? uint8_t(b) : uint8_t(c));
}

uint8_t* png_decode(const uint8_t* d, size_t size, int* out_w, int* out_h, const char* name)
{
    static const uint8_t sig[8] = { 137, 80, 78, 71, 13, 10, 26, 10 };
    if (size < 8 || memcmp(d, sig, 8) != 0) {
        fprintf(stderr, "%s: not a png\n", name);
        return 0;
    }

    int w = 0, h = 0, depth = 0, ctype = 0, interlace = 0;
    uint8_t palette[256][4];
    memset(palette, 255, sizeof(palette));
    bool has_trns = false;
    uint16_t trns[3] = {};
    std::vector<uint8_t> idat;

    size_t pos = 8;
    while (pos + 12 <= size) {
        uint32_t len = be32(d + pos);
        const uint8_t* type = d + pos + 4;
        const uint8_t* body = d + pos + 8;
        if (pos + 12 + len > size) {
            fprintf(stderr, "%s: truncated\n", name);
            return 0;
        }
        if (!memcmp(type, "IHDR", 4) && len >= 13) {
            w = (int)be32(body);
            h = (int)be32(body + 4);
            depth = body[8];
            ctype = body[9];
            interlace = body[12];
        } else if (!memcmp(type, "PLTE", 4)) {
            for (uint32_t i = 0; i < len / 3 && i < 256; i++) {
                palette[i][0] = body[i * 3];
                palette[i][1] = body[i * 3 + 1];
                palette[i][2] = body[i * 3 + 2];
            }
        } else if (!memcmp(type, "tRNS", 4)) {
            has_trns = true;
            if (ctype == 3) {
                for (uint32_t i = 0; i < len && i < 256; i++)
                    palette[i][3] = body[i];
            } else {
                for (uint32_t i = 0; i < 3 && i * 2 + 1 < len; i++)
                    trns[i] = uint16_t(body[i * 2] << 8 | body[i * 2 + 1]);
            }
        } else if (!memcmp(type, "IDAT", 4)) {
            idat.insert(idat.end(), body, body + len);
        } else if (!memcmp(type, "IEND", 4)) {
            break;
        }
        pos += 12 + len;
    }

    int channels = 0;
    switch (ctype) {
    case 0: case 3: channels = 1; break;
    case 2: channels = 3; break;
    case 4: channels = 2; break;
    case 6: channels = 4; break;
    }
    if (w <= 0 || h <= 0 || !channels || (depth != 1 && depth != 2 && depth != 4 && depth != 8 && depth != 16)) {
        fprintf(stderr, "%s: unsupported png (type %d, depth %d)\n", name, ctype, depth);
        return 0;
    }
    if (interlace) {
        fprintf(stderr, "%s: interlaced png, save it again without interlacing\n", name);
        return 0;
    }

    std::vector<uint8_t> raw;
    raw.reserve(size_t(h) * (w * channels * depth / 8 + 2));
    if (!zlib_inflate(idat.data(), idat.size(), raw)) {
        fprintf(stderr, "%s: bad compressed data\n", name);
        return 0;
    }

    int bits_pp = channels * depth;
    size_t stride = (size_t(w) * bits_pp + 7) / 8;
    int bpp = bits_pp >= 8 ? bits_pp / 8 : 1;
    if (raw.size() < size_t(h) * (stride + 1)) {
        fprintf(stderr, "%s: not enough image data\n", name);
        return 0;
    }

    // undo the per-row filters in place, row data follows its filter byte
    std::vector<uint8_t> img(size_t(h) * stride);
    for (int y = 0; y < h; y++) {
        const uint8_t* src = &raw[y * (stride + 1)];
        uint8_t* row = &img[y * stride];
        const uint8_t* prev = y ? &img[(y - 1) * stride] : 0;
        int filter = *src++;
        for (size_t i = 0; i < stride; i++) {
            int a = i >= (size_t)bpp ? row[i - bpp] : 0;
            int b = prev ? prev[i] : 0;
            int c = (prev && i >= (size_t)bpp) ? prev[i - bpp] : 0;
            switch (filter) {
            case 0: row[i] = src[i]; break;
            case 1: row[i] = uint8_t(src[i] + a); break;
            case 2: row[i] = uint8_t(src[i] + b); break;
            case 3: row[i] = uint8_t(src[i] + ((a + b) >> 1)); break;
            case 4: row[i] = uint8_t(src[i] + paeth(a, b, c)); break;
            default:
                fprintf(stderr, "%s: bad filter %d\n", name, filter);
                return 0;
            }
        }
    }

    auto sample = [&](const uint8_t* row, int x, int c) -> int {
        if (depth == 8) return row[x * channels + c];
        if (depth == 16) return row[(x * channels + c) * 2] << 8 | row[(x * channels + c) * 2 + 1];
        int bit = x * depth;    // sub-byte depths only happen with one channel
        return (row[bit >> 3] >> (8 - depth - (bit & 7))) & ((1 << depth) - 1);
    };
    int maxval = (1 << depth) - 1;
    auto to8 = [&](int v) { return uint8_t(depth == 16 ? v >> 8 : v * 255 / maxval); };

    uint8_t* out = (uint8_t*)malloc(size_t(w) * h * 4);
    if (!out)
        return 0;
    for (int y = 0; y < h; y++) {
        const uint8_t* row = &img[y * stride];
        for (int x = 0; x < w; x++) {
            uint8_t* p = out + (size_t(y) * w + x) * 4;
            switch (ctype) {
            case 0: {
                int v = sample(row, x, 0);
                p[0] = p[1] = p[2] = to8(v);
                p[3] = (has_trns && v == trns[0]) ? 0 : 255;
                break;
            }
            case 2: {
                int r = sample(row, x, 0), g = sample(row, x, 1), b = sample(row, x, 2);
                p[0] = to8(r); p[1] = to8(g); p[2] = to8(b);
                p[3] = (has_trns && r == trns[0] && g == trns[1] && b == trns[2]) ? 0 : 255;
                break;
            }
            case 3:
                memcpy(p, palette[sample(row, x, 0) & 255], 4);
                break;
            case 4:
                p[0] = p[1] = p[2] = to8(sample(row, x, 0));
                p[3] = to8(sample(row, x, 1));
                break;
            case 6:
                for (int c = 0; c < 4; c++)
                    p[c] = to8(sample(row, x, c));
                break;
            }
        }
    }

    *out_w = w;
    *out_h = h;
    return out;
}

static uint32_t crc_table[256];

static uint32_t crc32(uint32_t crc, const uint8_t* p, size_t n)
{
    if (!crc_table[1]) {
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t c = i;
            for (int k = 0; k < 8; k++)
                c = c & 1 ? 0xedb88320 ^ (c >> 1) : c >> 1;
            crc_table[i] = c;
        }
    }
    crc = ~crc;
    while (n--)
        crc = crc_table[(crc ^ *p++) & 255] ^ (crc >> 8);
    return ~crc;
}

static void put_be32(std::vector<uint8_t>& v, uint32_t x)
{
    v.push_back(uint8_t(x >> 24));
    v.push_back(uint8_t(x >> 16));
    v.push_back(uint8_t(x >> 8));
    v.push_back(uint8_t(x));
}

static void chunk(std::vector<uint8_t>& file, const char* type, const std::vector<uint8_t>& body)
{
    put_be32(file, (uint32_t)body.size());
    size_t start = file.size();
    file.insert(file.end(), type, type + 4);
    file.insert(file.end(), body.begin(), body.end());
    put_be32(file, crc32(0, &file[start], file.size() - start));
}

// Stored deflate blocks, no compression at all. It's for screenshots and
// atlas previews, size doesn't matter.
bool png_write(const char* path, const uint8_t* rgba, int w, int h)
{
    std::vector<uint8_t> raw;
    raw.reserve(size_t(h) * (w * 4 + 1));
    for (int y = 0; y < h; y++) {
        raw.push_back(0);
        raw.insert(raw.end(), rgba + size_t(y) * w * 4, rgba + size_t(y + 1) * w * 4);
    }

    std::vector<uint8_t> z = { 0x78, 0x01 };
    size_t pos = 0;
    do {
        size_t n = raw.size() - pos;
        if (n > 65535) n = 65535;
        z.push_back(pos + n == raw.size() ? 1 : 0);
        z.push_back(uint8_t(n)); z.push_back(uint8_t(n >> 8));
        z.push_back(uint8_t(~n)); z.push_back(uint8_t(~n >> 8));
        z.insert(z.end(), raw.begin() + pos, raw.begin() + pos + n);
        pos += n;
    } while (pos < raw.size());

    uint32_t a = 1, b = 0;
    for (uint8_t c : raw) {
        a = (a + c) % 65521;
        b = (b + a) % 65521;
    }
    put_be32(z, b << 16 | a);

    std::vector<uint8_t> file = { 137, 80, 78, 71, 13, 10, 26, 10 };
    std::vector<uint8_t> ihdr;
    put_be32(ihdr, w);
    put_be32(ihdr, h);
    ihdr.insert(ihdr.end(), { 8, 6, 0, 0, 0 });
    chunk(file, "IHDR", ihdr);
    chunk(file, "IDAT", z);
    chunk(file, "IEND", {});
    return write_file(path, file.data(), file.size());
}

// ---------------------------------------------------------------------------
// tga: truecolor 24/32 bit, raw or RLE, which is what every paint program writes

uint8_t* tga_decode(const uint8_t* d, size_t size, int* out_w, int* out_h, const char* name)
{
    if (size < 18) {
        fprintf(stderr, "%s: not a tga\n", name);
        return 0;
    }

    int id_len = d[0], cmap = d[1], type = d[2];
    int w = d[12] | d[13] << 8, h = d[14] | d[15] << 8;
    int bpp = d[16], desc = d[17];

    if (cmap || (type != 2 && type != 10) || (bpp != 24 && bpp != 32) || w <= 0 || h <= 0) {
        fprintf(stderr, "%s: unsupported tga (type %d, %d bpp)\n", name, type, bpp);
        return 0;
    }

    int n = bpp / 8;
    size_t pos = 18 + id_len;
    size_t count = size_t(w) * h;
    uint8_t* out = (uint8_t*)malloc(count * 4);
    if (!out)
        return 0;

    auto read_px = [&](uint8_t* dst) {
        if (pos + n > size)
            return false;
        dst[0] = d[pos + 2]; dst[1] = d[pos + 1]; dst[2] = d[pos];
        dst[3] = n == 4 ? d[pos + 3] : 255;
        pos += n;
        return true;
    };

    bool ok = true;
    size_t i = 0;
    if (type == 2) {
        for (; i < count && ok; i++)
            ok = read_px(out + i * 4);
    } else {
        while (i < count && ok) {
            if (pos >= size) { ok = false; break; }
            int packet = d[pos++];
            int len = (packet & 0x7f) + 1;
            if (i + len > count)
                len = (int)(count - i);
            if (packet & 0x80) {
                uint8_t px[4];
                ok = read_px(px);
                for (int k = 0; k < len; k++)
                    memcpy(out + (i + k) * 4, px, 4);
            } else {
                for (int k = 0; k < len && ok; k++)
                    ok = read_px(out + (i + k) * 4);
            }
            i += len;
        }
    }

    if (!ok) {
        fprintf(stderr, "%s: truncated\n", name);
        free(out);
        return 0;
    }

    // bit 5 set means rows are already top-down
    if (!(desc & 0x20)) {
        std::vector<uint8_t> tmp(size_t(w) * 4);
        for (int y = 0; y < h / 2; y++) {
            uint8_t* a = out + size_t(y) * w * 4;
            uint8_t* b = out + size_t(h - 1 - y) * w * 4;
            memcpy(tmp.data(), a, tmp.size());
            memcpy(a, b, tmp.size());
            memcpy(b, tmp.data(), tmp.size());
        }
    }

    *out_w = w;
    *out_h = h;
    return out;
}

uint8_t* image_decode(const uint8_t* data, size_t size, int* w, int* h, const char* name)
{
    if (size >= 8 && data[0] == 137 && data[1] == 'P' && data[2] == 'N' && data[3] == 'G')
        return png_decode(data, size, w, h, name);
    return tga_decode(data, size, w, h, name);
}

uint8_t* image_load(const char* path, int* w, int* h)
{
    std::vector<uint8_t> data;
    if (!read_file(path, data)) {
        fprintf(stderr, "%s: can't read\n", path);
        return 0;
    }
    return image_decode(data.data(), data.size(), w, h, path);
}
