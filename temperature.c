#include "temperature.h"

int is_temperature_high(int temperature, int threshold){
    return (temperature>threshold);
}

int update_alarm(
    int temperature,
    int on_threshold,
    int off_threshold,
    int status
){
    if(!status){
        return (temperature > on_threshold);
    }else{
        return !(temperature <= off_threshold);
    }
}