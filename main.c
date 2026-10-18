#include <stdio.h>
#include "io.h"
#include "temperatura.h"
#include "gpio.h"
//#define EXIT_VALUE -999


int main(void) {
        int temperature=0;
        int threshold=0;
        struct gpiochip_info info = {0};
        struct gpio_v2_line_request request = {0};
        struct gpio_v2_line_values values = {0};
        int num_lines;
        int lines[] = {17,18,19};;
        int fd_controller;
        int led_state = 0;
        int desired_state, result;
        int exit_status = 0;

        num_lines = 3;

        //configuration of expection to handle the input
        request.config.flags = GPIO_V2_LINE_FLAG_INPUT | GPIO_V2_LINE_FLAG_BIAS_PULL_UP;
        request.config.num_attrs=1;
	request.config.attrs[0].attr.id = GPIO_V2_LINE_ATTR_ID_FLAGS;
	request.config.attrs[0].attr.flags = GPIO_V2_LINE_FLAG_OUTPUT;
	request.config.attrs[0].mask = 1;


        fd_controller = GPIO_CONNECTION(&info, &request,lines,num_lines);
        if(fd_controller == -1){
                perror("Failed to open GPIO");
		return -1;
	}

        print_menu();

        printf("Enter the threshold: ");
        if (read_integer(&threshold) != 1){
                printf("Input error!\n");
                exit_status = 1;
                goto close;
        }
        //lines --> [17, 18, 19]
        // MASK:
        // 1 1 1 = 7 --> all lines
        // 1 1 0 = 6 --> line 18 & 19
        // 0 0 1 = 1 --> line 17
        // 0 1 0 = 4 --> line 18
        // 1 0 0 = 6 --> line 19


        while(1){
                printf("\nEnter a temperature: ");
                if(read_integer(&temperature) != 1){
                        printf("Input error!\n");
                        exit_status = 2;
                        goto close;
                }
                //if(temperature == EXIT_VALUE) {break;}
                desired_state = update_alarm(temperature, threshold+2,threshold-2,led_state);
                if(desired_state != led_state){
                        result=controller_lines_action(request.fd, &values,desired_state,1,GPIO_V2_LINE_SET_VALUES_IOCTL);
                        if(result == -1){
                                perror("Failed to set GPIO line values");
                                exit_status = 3;
                                goto close;
                        }
                        led_state = desired_state;
                }
                if (is_temperature_high(temperature, threshold)) {
                                printf("TEMPERATURE: Above threshold\n");
                        } else {
                                printf("TEMPERATURE: At or below threshold\n");
                        }
                        if (led_state) {
                                printf("ALARM: Active\n");
                        } else {
                                printf("ALARM: Inactive\n");
                        }
        }

        close:
                if(controller_lines_action(request.fd, &values,0,1,GPIO_V2_LINE_SET_VALUES_IOCTL) == -1){
                        perror("Failed to set GPIO line values");
                        exit_status = 3;
                }
                if(close_fd(fd_controller) == -1){
                        perror("Failed to close GPIO controller");
                        exit_status = -1;
                };
                if(close_fd(request.fd)==-1){
                        perror("Failed to close GPIO line");
                        exit_status = -2;
                }
                printf("Shutting down...\n");
                return exit_status;
}