#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include "thermistor.h"

int read_adc(int fd, uint8_t *raw){
    ssize_t received, written;
    uint8_t control = 0x00;
    uint8_t local_raw[2];

    written = write(fd, &control, sizeof(control));
    if (written == -1){
        perror("Error in writing control byte");
        return -1;
    } else if (written == 1){
        printf("Control byte transferred successfully\n");
    } else {
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