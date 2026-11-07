#include "image.h"
#include <cstring>

// ---------------------------------------------------------------------------
// inflate, more or less puff.c: slow bit-at-a-time huffman decoding, but
// short and easy to check. Plenty for sprite sheets.

struct Inflater {
    const uint8_t* in;
    size_t len, pos;
    uint32_t bitbuf;
    int bitcnt;
    std::vector<uint8_t>* out;
    bool err;

    int bits(int need)
    {
        uint32_t val = bitbuf;
        while (bitcnt < need) {
            if (pos >= len) {
                err = true;
                return 0;
            }
            val |= uint32_t(in[pos++]) << bitcnt;
            bitcnt += 8;
        }
        bitbuf = val >> need;
        bitcnt -= need;
        return int(val & ((1u << need) - 1));
    }
};

struct Huffman {
    short count[16];
    short symbol[288];
};

static int decode(Inflater& s, const Huffman& h)
{
    int code = 0, first = 0, index = 0;
    for (int len = 1; len < 16; len++) {
        code |= s.bits(1);
        int count = h.count[len];
        if (code - count < first)
            return h.symbol[index + (code - first)];
        index += count;
        first += count;
        first <<= 1;
        code <<= 1;
    }
    s.err = true;
    return -1;
}

static bool build(Huffman& h, const short* lengths, int n)
{
    memset(h.count, 0, sizeof(h.count));
    for (int i = 0; i < n; i++)
        h.count[lengths[i]]++;
    if (h.count[0] == n)
        return true;

    int left = 1;
    for (int len = 1; len < 16; len++) {
        left <<= 1;
        left -= h.count[len];
        if (left < 0)
            return false;   // over-subscribed
    }

    short offs[16];
    offs[1] = 0;
    for (int len = 1; len < 15; len++)
        offs[len + 1] = offs[len] + h.count[len];
    for (int i = 0; i < n; i++)
        if (lengths[i])
            h.symbol[offs[lengths[i]]++] = short(i);
    return true;
}

static const short len_base[29] = {
    3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
    35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258 };
static const short len_extra[29] = {
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
    3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0 };
static const short dist_base[30] = {
    1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
    257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577 };
static const short dist_extra[30] = {
    0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
    7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13 };

static bool codes(Inflater& s, const Huffman& lencode, const Huffman& distcode)
{
    std::vector<uint8_t>& out = *s.out;
    for (;;) {
        int sym = decode(s, lencode);
        if (s.err || sym < 0)
            return false;
        if (sym < 256) {
            out.push_back(uint8_t(sym));
        } else if (sym == 256) {
            return true;
        } else {
            sym -= 257;
            if (sym >= 29)
                return false;
            int len = len_base[sym] + s.bits(len_extra[sym]);
            int dsym = decode(s, distcode);
            if (dsym < 0 || dsym >= 30)
                return false;
            size_t dist = dist_base[dsym] + s.bits(dist_extra[dsym]);
            if (s.err || dist > out.size())
                return false;
            size_t from = out.size() - dist;
            for (int i = 0; i < len; i++) {
                uint8_t b = out[from + i];
                out.push_back(b);
            }
        }
    }
}

static bool stored(Inflater& s)
{
    // drop the rest of the current byte
    s.bitbuf = 0;
    s.bitcnt = 0;
    if (s.pos + 4 > s.len)
        return false;
    unsigned len = s.in[s.pos] | s.in[s.pos + 1] << 8;
    unsigned nlen = s.in[s.pos + 2] | s.in[s.pos + 3] << 8;
    s.pos += 4;
    if (len != (~nlen & 0xffff) || s.pos + len > s.len)
        return false;
    s.out->insert(s.out->end(), s.in + s.pos, s.in + s.pos + len);
    s.pos += len;
    return true;
}

static bool fixed(Inflater& s)
{
    static Huffman lencode, distcode;
    static bool built;
    if (!built) {
        short l[288];
        for (int i = 0; i < 144; i++) l[i] = 8;
        for (int i = 144; i < 256; i++) l[i] = 9;
        for (int i = 256; i < 280; i++) l[i] = 7;
        for (int i = 280; i < 288; i++) l[i] = 8;
        build(lencode, l, 288);
        for (int i = 0; i < 30; i++) l[i] = 5;
        build(distcode, l, 30);
        built = true;
    }
    return codes(s, lencode, distcode);
}

static bool dynamic(Inflater& s)
{
    static const uint8_t order[19] = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };

    int nlen = s.bits(5) + 257;
    int ndist = s.bits(5) + 1;
    int ncode = s.bits(4) + 4;
    if (nlen > 286 || ndist > 30)
        return false;

    short lengths[320] = {};
    for (int i = 0; i < ncode; i++)
        lengths[order[i]] = short(s.bits(3));

    Huffman lencode, distcode;
    if (!build(lencode, lengths, 19))
        return false;

    int index = 0;
    while (index < nlen + ndist) {
        int sym = decode(s, lencode);
        if (sym < 0)
            return false;
        if (sym < 16) {
            lengths[index++] = short(sym);
            continue;
        }
        short len = 0;
        int rep;
        if (sym == 16) {
            if (index == 0)
                return false;
            len = lengths[index - 1];
            rep = 3 + s.bits(2);
        } else if (sym == 17) {
            rep = 3 + s.bits(3);
        } else {
            rep = 11 + s.bits(7);
        }
        if (index + rep > nlen + ndist)
            return false;
        while (rep--)
            lengths[index++] = len;
    }

    if (lengths[256] == 0)
        return false;
    if (!build(lencode, lengths, nlen) || !build(distcode, lengths + nlen, ndist))
        return false;
    return codes(s, lencode, distcode);
}

bool zlib_inflate(const uint8_t* data, size_t size, std::vector<uint8_t>& out)
{
    if (size < 2 || (data[0] & 0x0f) != 8 || ((data[0] << 8) | data[1]) % 31 != 0 || (data[1] & 0x20))
        return false;

    Inflater s = {};
    s.in = data;
    s.len = size;
    s.pos = 2;
    s.out = &out;

    int last;
    do {
        last = s.bits(1);
        int type = s.bits(2);
        bool ok;
        switch (type) {
        case 0: ok = stored(s); break;
        case 1: ok = fixed(s); break;
        case 2: ok = dynamic(s); break;
        default: ok = false;
        }
        if (!ok || s.err)
            return false;
    } while (!last);
    // not checking the adler32, the png crcs already cover corruption well enough
    return true;
}

