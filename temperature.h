#ifndef TEMPERATURE_H
#define TEMPERATURE_H

int is_temperature_high(
    int temperature,
    int threshold
);
int update_alarm(
    int temperature,
    int on_threshold,
    int off_threshold,
    int status
);

#endif
