#ifndef THERMISTOR_H
#define THERMISTOR_H

#include <stdint.h>

int read_adc(int fd, uint8_t *raw);

#endif