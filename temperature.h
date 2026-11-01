#ifndef TEMPERATURE_H
#define TEMPERATURE_H

int is_temperature_high(
    double temperature,
    double threshold
);
int update_alarm(
    double temperature,
    int on_threshold,
    int off_threshold,
    int status
);

#endif
