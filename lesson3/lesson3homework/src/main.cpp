#include "modulation.h"
#include "multiplex.h"

#include <iomanip>
#include <iostream>

static void print_bits(const unsigned char *data, int len, const char *name) {
    std::cout << name << ": ";
    for (int i = 0; i < len; ++i) {
        std::cout << (int)(data[i] ? 1 : 0) << " ";
    }
    std::cout << std::endl;
}

static void print_samples(const double *data, int len, const char *name) {
    std::cout << name << ": ";
    for (int i = 0; i < len; ++i) {
        std::cout << std::fixed << std::setprecision(3) << data[i] << " ";
    }
    std::cout << std::endl;
}

int main() {
    unsigned char a[] = {1, 0, 5, 0, 7};
    unsigned char b[] = {0, 3, 1};
    unsigned char c[256] = {0};
    unsigned char a_out[16] = {0};
    unsigned char b_out[16] = {0};

    int mux_len = multiplex(c, 256, a, 5, b, 3);
    int demux_len = demultiplex(a_out, 16, b_out, 16, c, mux_len);

    std::cout << "multiplex bytes = " << mux_len << std::endl;
    std::cout << "demultiplex size = " << demux_len << std::endl;
    print_bits(a_out, 5, "a_out");
    print_bits(b_out, 3, "b_out");

    const int signal_len = 32;
    double cover[signal_len] = {0.0};
    double analog_message[signal_len] = {0.0};
    unsigned char digital_message[signal_len] = {0};

    generate_cover_signal(cover, signal_len);
    simulate_analog_modulation_signal(analog_message, signal_len);
    simulate_digital_modulation_signal(digital_message, signal_len);

    print_samples(cover, 8, "cover");
    print_samples(analog_message, 8, "analog_message");
    print_bits(digital_message, 16, "digital_message");

    generate_cover_signal(cover, signal_len);
    modulate_digital_frequency(cover, signal_len, digital_message, signal_len);
    print_samples(cover, 8, "digital_fsk");

    generate_cover_signal(cover, signal_len);
    modulate_analog_amplitude(cover, signal_len, analog_message, signal_len);
    print_samples(cover, 8, "analog_am");

    generate_cover_signal(cover, signal_len);
    modulate_digital_phase(cover, signal_len, digital_message, signal_len);
    print_samples(cover, 8, "digital_psk");

    return 0;
}
