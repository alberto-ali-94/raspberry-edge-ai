#ifndef THERMISTOR_H
#define THERMISTOR_H

#include <stdint.h>

int read_adc(int fd, uint8_t *raw);
int adc_to_celsius(uint8_t raw, double *celsius);

#endif