#include "modulation.h"

#include <cmath>

static const double PI_VALUE = 3.14159265358979323846;
static const double BASE_AMPLITUDE = 1.0;
static const double CARRIER_CYCLE = 8.0;

static double abs_value(double x) {
    return x < 0 ? -x : x;
}

static double max_abs_value(const double *data, int len) {
    double m = 0.0;
    for (int i = 0; i < len; ++i) {
        double v = abs_value(data[i]);
        if (v > m) {
            m = v;
        }
    }
    return m;
}

static double carrier_phase(int index, int total_len, double cycle) {
    if (total_len <= 0) {
        return 0.0;
    }
    return 2.0 * PI_VALUE * cycle * index / total_len;
}

static double carrier_amplitude(const double *cover, int len) {
    double amp = max_abs_value(cover, len);
    if (amp <= 0.000001) {
        amp = BASE_AMPLITUDE;
    }
    return amp;
}

int generate_cover_signal(double *cover, const int size) {
    if (cover == 0 || size <= 0) {
        return -1;
    }

    for (int i = 0; i < size; ++i) {
        double phase = carrier_phase(i, size, CARRIER_CYCLE);
        cover[i] = BASE_AMPLITUDE * std::sin(phase);
    }
    return size;
}

int simulate_digital_modulation_signal(unsigned char *message, const int size) {
    if (message == 0 || size <= 0) {
        return -1;
    }

    for (int i = 0; i < size; ++i) {
        message[i] = (unsigned char)((i / 8) % 2);
    }
    return size;
}

int simulate_analog_modulation_signal(double *message, const int size) {
    if (message == 0 || size <= 0) {
        return -1;
    }

    for (int i = 0; i < size; ++i) {
        double phase = carrier_phase(i, size, 2.0);
        message[i] = 0.8 * std::sin(phase);
    }
    return size;
}

int modulate_digital_frequency(double *cover, const int cover_len,
                               const unsigned char *message, const int msg_len) {
    if (cover == 0 || message == 0 || cover_len <= 0 || msg_len <= 0) {
        return -1;
    }

    double amplitude = carrier_amplitude(cover, cover_len);
    int samples_per_bit = cover_len / msg_len;
    if (samples_per_bit <= 0) {
        samples_per_bit = 1;
    }

    for (int i = 0; i < cover_len; ++i) {
        int bit_index = i / samples_per_bit;
        if (bit_index >= msg_len) {
            bit_index = msg_len - 1;
        }
        double freq = message[bit_index] ? 12.0 : 6.0;
        double phase = carrier_phase(i, cover_len, freq);
        cover[i] = amplitude * std::sin(phase);
    }
    return cover_len;
}

int modulate_analog_frequency(double *cover, const int cover_len,
                              const double *message, const int msg_len) {
    if (cover == 0 || message == 0 || cover_len <= 0 || msg_len <= 0) {
        return -1;
    }

    double amplitude = carrier_amplitude(cover, cover_len);
    double scale = max_abs_value(message, msg_len);
    if (scale <= 0.000001) {
        scale = 1.0;
    }

    for (int i = 0; i < cover_len; ++i) {
        int msg_index = i * msg_len / cover_len;
        if (msg_index >= msg_len) {
            msg_index = msg_len - 1;
        }
        double control = message[msg_index] / scale;
        double freq = 8.0 + 3.0 * control;
        double phase = carrier_phase(i, cover_len, freq);
        cover[i] = amplitude * std::sin(phase);
    }
    return cover_len;
}

int modulate_digital_amplitude(double *cover, const int cover_len,
                               const unsigned char *message, const int msg_len) {
    if (cover == 0 || message == 0 || cover_len <= 0 || msg_len <= 0) {
        return -1;
    }

    double amplitude = carrier_amplitude(cover, cover_len);
    int samples_per_bit = cover_len / msg_len;
    if (samples_per_bit <= 0) {
        samples_per_bit = 1;
    }

    for (int i = 0; i < cover_len; ++i) {
        int bit_index = i / samples_per_bit;
        if (bit_index >= msg_len) {
            bit_index = msg_len - 1;
        }
        double amp = message[bit_index] ? amplitude : amplitude * 0.3;
        double phase = carrier_phase(i, cover_len, CARRIER_CYCLE);
        cover[i] = amp * std::sin(phase);
    }
    return cover_len;
}

int modulate_analog_amplitude(double *cover, const int cover_len,
                              const double *message, const int msg_len) {
    if (cover == 0 || message == 0 || cover_len <= 0 || msg_len <= 0) {
        return -1;
    }

    double amplitude = carrier_amplitude(cover, cover_len);
    double scale = max_abs_value(message, msg_len);
    if (scale <= 0.000001) {
        scale = 1.0;
    }

    for (int i = 0; i < cover_len; ++i) {
        int msg_index = i * msg_len / cover_len;
        if (msg_index >= msg_len) {
            msg_index = msg_len - 1;
        }
        double control = message[msg_index] / scale;
        double phase = carrier_phase(i, cover_len, CARRIER_CYCLE);
        cover[i] = amplitude * (1.0 + 0.5 * control) * std::sin(phase);
    }
    return cover_len;
}

int modulate_digital_phase(double *cover, const int cover_len,
                           const unsigned char *message, const int msg_len) {
    if (cover == 0 || message == 0 || cover_len <= 0 || msg_len <= 0) {
        return -1;
    }

    double amplitude = carrier_amplitude(cover, cover_len);
    int samples_per_bit = cover_len / msg_len;
    if (samples_per_bit <= 0) {
        samples_per_bit = 1;
    }

    for (int i = 0; i < cover_len; ++i) {
        int bit_index = i / samples_per_bit;
        if (bit_index >= msg_len) {
            bit_index = msg_len - 1;
        }
        double extra_phase = message[bit_index] ? PI_VALUE : 0.0;
        double phase = carrier_phase(i, cover_len, CARRIER_CYCLE) + extra_phase;
        cover[i] = amplitude * std::sin(phase);
    }
    return cover_len;
}

int modulate_analog_phase(double *cover, const int cover_len,
                          const double *message, const int msg_len) {
    if (cover == 0 || message == 0 || cover_len <= 0 || msg_len <= 0) {
        return -1;
    }

    double amplitude = carrier_amplitude(cover, cover_len);
    double scale = max_abs_value(message, msg_len);
    if (scale <= 0.000001) {
        scale = 1.0;
    }

    for (int i = 0; i < cover_len; ++i) {
        int msg_index = i * msg_len / cover_len;
        if (msg_index >= msg_len) {
            msg_index = msg_len - 1;
        }
        double control = message[msg_index] / scale;
        double phase = carrier_phase(i, cover_len, CARRIER_CYCLE)
                     + control * PI_VALUE / 2.0;
        cover[i] = amplitude * std::sin(phase);
    }
    return cover_len;
}
