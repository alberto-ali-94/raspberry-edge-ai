#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <math.h>
#include "thermistor.h"

int main(void) {
        int exit_status;
        int fd, result;
        uint8_t raw;
        double voltage, Rt, tempK, tempC;
        

        fd = open("/dev/i2c-1",O_RDWR);
        if (fd == -1){
                perror("Error opening bus file descriptor '/dev/i2c-1'\n");
                exit_status = 1;
                goto closing;
        }

        //send to the kernel the request to select the device's address 
        //i'm saying to the kernel that the file descriptor will be associated to the address 0x48
        result = ioctl(fd, I2C_SLAVE, 0x48);

        if(result == -1){
                perror("Error happend while sending the request");
                exit_status = 1;
                goto closing;
        }

        printf("Bus aperto e indirizzo 0x48 selezionato.!\n");


        if (read_adc(fd,&raw) == -1){
                exit_status = 1;
                goto closing;
        }


        voltage = raw / 256.0 * 3.3;
        if ((voltage <= 0.0) || (voltage >= 3.3)){
                printf("Error! Voltage out of ranges");
                exit_status = 1;
                goto closing;
        }
        Rt = 10 * voltage / (3.3 - voltage);
        tempK = 1/(1/(273.15 + 25) + log(Rt/10)/3950.0);
        tempC = tempK - 273.15;
        printf("ADC raw value: %u, Voltage: %.2fV, Temperature: %.2f°C\n",(unsigned int)raw,voltage, tempC);
        exit_status = 0;
        closing:            
                if (fd != -1){
                        if(close(fd) == -1){
                                perror("Error in closure of the file descriptor");
                                exit_status = 1;
                        }
                        
                }
                printf("Shutting down...\n");
                return exit_status;
}
