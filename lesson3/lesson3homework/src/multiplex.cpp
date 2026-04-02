#include "multiplex.h"

#include <vector>

static unsigned char bit_value(unsigned char x) {
    return x ? 1 : 0;
}

static void write_int(std::vector<unsigned char> &out, int value) {
    out.push_back((unsigned char)(value & 0xff));
    out.push_back((unsigned char)((value >> 8) & 0xff));
    out.push_back((unsigned char)((value >> 16) & 0xff));
    out.push_back((unsigned char)((value >> 24) & 0xff));
}

static int read_int(const unsigned char *data, int len, int &pos) {
    if (pos + 4 > len) {
        return -1;
    }

    int value = 0;
    value |= (int)data[pos];
    value |= ((int)data[pos + 1] << 8);
    value |= ((int)data[pos + 2] << 16);
    value |= ((int)data[pos + 3] << 24);
    pos += 4;
    return value;
}

static bool same_data(const std::vector<unsigned char> &x,
                      const std::vector<unsigned char> &y) {
    if (x.size() != y.size()) {
        return false;
    }

    for (size_t i = 0; i < x.size(); ++i) {
        if (bit_value(x[i]) != bit_value(y[i])) {
            return false;
        }
    }
    return true;
}

int multiplex(unsigned char *c, const int c_size,
              const unsigned char *a, const int a_len,
              const unsigned char *b, const int b_len) {
    if (c == 0 || a == 0 || b == 0 || c_size < 0 || a_len < 0 || b_len < 0) {
        return -1;
    }

    std::vector<unsigned char> out;
    write_int(out, a_len);
    write_int(out, b_len);

    std::vector<unsigned char> stat_tdm;
    int ia = 0;
    int ib = 0;
    while (ia < a_len || ib < b_len) {
        if (ia < a_len) {
            stat_tdm.push_back((unsigned char)(bit_value(a[ia]) | 0));
            ++ia;
        }
        if (ib < b_len) {
            stat_tdm.push_back((unsigned char)(bit_value(b[ib]) | 2));
            ++ib;
        }
    }
    write_int(out, (int)stat_tdm.size());
    out.insert(out.end(), stat_tdm.begin(), stat_tdm.end());

    std::vector<unsigned char> sync_tdm;
    int max_len = (a_len > b_len) ? a_len : b_len;
    for (int i = 0; i < max_len; ++i) {
        sync_tdm.push_back((i < a_len) ? bit_value(a[i]) : 0);
        sync_tdm.push_back((i < b_len) ? bit_value(b[i]) : 0);
    }
    write_int(out, (int)sync_tdm.size());
    out.insert(out.end(), sync_tdm.begin(), sync_tdm.end());

    std::vector<unsigned char> fdm;
    for (int i = 0; i < max_len; ++i) {
        unsigned char av = (i < a_len) ? bit_value(a[i]) : 0;
        unsigned char bv = (i < b_len) ? bit_value(b[i]) : 0;
        fdm.push_back((unsigned char)(av | (bv << 1)));
    }
    write_int(out, (int)fdm.size());
    out.insert(out.end(), fdm.begin(), fdm.end());

    std::vector<unsigned char> cdm;
    for (int i = 0; i < max_len; ++i) {
        int av = (i < a_len) ? bit_value(a[i]) : 0;
        int bv = (i < b_len) ? bit_value(b[i]) : 0;
        int chip1 = av + bv;
        int chip2 = av - bv;
        cdm.push_back((unsigned char)(chip1 + 2));
        cdm.push_back((unsigned char)(chip2 + 2));
    }
    write_int(out, (int)cdm.size());
    out.insert(out.end(), cdm.begin(), cdm.end());

    if ((int)out.size() > c_size) {
        return -1;
    }

    for (size_t i = 0; i < out.size(); ++i) {
        c[i] = out[i];
    }
    return (int)out.size();
}

int demultiplex(unsigned char *a, const int a_size,
                unsigned char *b, const int b_size,
                const unsigned char *c, const int c_len) {
    if (a == 0 || b == 0 || c == 0 || a_size < 0 || b_size < 0 || c_len < 0) {
        return -1;
    }

    int pos = 0;
    int a_len = read_int(c, c_len, pos);
    int b_len = read_int(c, c_len, pos);
    if (a_len < 0 || b_len < 0 || a_len > a_size || b_len > b_size) {
        return -1;
    }

    std::vector<unsigned char> a1(a_len, 0), b1(b_len, 0);
    std::vector<unsigned char> a2(a_len, 0), b2(b_len, 0);
    std::vector<unsigned char> a3(a_len, 0), b3(b_len, 0);
    std::vector<unsigned char> a4(a_len, 0), b4(b_len, 0);

    int len1 = read_int(c, c_len, pos);
    if (len1 < 0 || pos + len1 > c_len) {
        return -1;
    }
    int pa = 0;
    int pb = 0;
    for (int i = 0; i < len1; ++i) {
        unsigned char x = c[pos + i];
        if ((x & 2) == 0) {
            if (pa >= a_len) {
                return -1;
            }
            a1[pa++] = (unsigned char)(x & 1);
        } else {
            if (pb >= b_len) {
                return -1;
            }
            b1[pb++] = (unsigned char)(x & 1);
        }
    }
    pos += len1;
    if (pa != a_len || pb != b_len) {
        return -1;
    }

    int len2 = read_int(c, c_len, pos);
    if (len2 < 0 || pos + len2 > c_len || (len2 % 2) != 0) {
        return -1;
    }
    for (int i = 0; i < len2 / 2; ++i) {
        if (i < a_len) {
            a2[i] = bit_value(c[pos + i * 2]);
        }
        if (i < b_len) {
            b2[i] = bit_value(c[pos + i * 2 + 1]);
        }
    }
    pos += len2;

    int len3 = read_int(c, c_len, pos);
    if (len3 < 0 || pos + len3 > c_len) {
        return -1;
    }
    for (int i = 0; i < len3; ++i) {
        unsigned char x = c[pos + i];
        if (i < a_len) {
            a3[i] = (unsigned char)(x & 1);
        }
        if (i < b_len) {
            b3[i] = (unsigned char)((x >> 1) & 1);
        }
    }
    pos += len3;

    int len4 = read_int(c, c_len, pos);
    if (len4 < 0 || pos + len4 > c_len || (len4 % 2) != 0) {
        return -1;
    }
    for (int i = 0; i < len4 / 2; ++i) {
        int chip1 = (int)c[pos + i * 2] - 2;
        int chip2 = (int)c[pos + i * 2 + 1] - 2;
        int av = (chip1 + chip2) / 2;
        int bv = (chip1 - chip2) / 2;
        if (i < a_len) {
            a4[i] = (unsigned char)(av ? 1 : 0);
        }
        if (i < b_len) {
            b4[i] = (unsigned char)(bv ? 1 : 0);
        }
    }
    pos += len4;

    if (!same_data(a1, a2) || !same_data(a1, a3) || !same_data(a1, a4)) {
        return -1;
    }
    if (!same_data(b1, b2) || !same_data(b1, b3) || !same_data(b1, b4)) {
        return -1;
    }

    for (int i = 0; i < a_len; ++i) {
        a[i] = bit_value(a1[i]);
    }
    for (int i = 0; i < b_len; ++i) {
        b[i] = bit_value(b1[i]);
    }

    return a_len + b_len;
}
