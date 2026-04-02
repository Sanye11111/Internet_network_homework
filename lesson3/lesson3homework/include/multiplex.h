#ifndef MULTIPLEX_H
#define MULTIPLEX_H

int multiplex(unsigned char *c, const int c_size,
              const unsigned char *a, const int a_len,
              const unsigned char *b, const int b_len);

int demultiplex(unsigned char *a, const int a_size,
                unsigned char *b, const int b_size,
                const unsigned char *c, const int c_len);

#endif
