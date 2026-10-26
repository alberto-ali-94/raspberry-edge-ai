#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <math.h>
#include "thermistor.h"

int read_adc(int fd, uint8_t *raw){
    ssize_t received, written;
    uint8_t control = 0x00;
    uint8_t local_raw[2];

    written = write(fd, &control, sizeof(control));
    if (written == -1){
        perror("Error in writing control byte");
        return -1;
    } else if (written != 1){
        printf("Control byte not transferred\n");
        return -1;
    }

    received = read(fd, local_raw, sizeof(local_raw));
    if (received == -1){
        perror("Error in reading bytes");
        return -1;
    } else if(received != 2){
        printf("Received %zd bytes instead of 2\n", received);
        return -1;
    }
    *raw = local_raw[1]; 
    return 0;    
}


int adc_to_celsius(uint8_t raw, double *celsius){
    double local_celsius, voltage, Rt, kelvin;

    voltage = raw / 256.0 * 3.3;
    if ((voltage <= 0.0) || (voltage >= 3.3)){
            printf("Error! Voltage out of ranges");
            return -1;
    }
    Rt = 10 * voltage / (3.3 - voltage);
    kelvin = 1/(1/(273.15 + 25) + log(Rt/10)/3950.0);
    local_celsius = kelvin - 273.15;
    *celsius = local_celsius;
    return 0;

}