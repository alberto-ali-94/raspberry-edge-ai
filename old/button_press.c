#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <time.h>
#include "gpio_probe.h"
#include <sys/ioctl.h>
#include <unistd.h>

int main(void){
    struct gpiochip_info info = {0};
    struct gpio_v2_line_request request = {0};
    struct gpio_v2_line_values values = {0};
    int fd_controller, result;
    int button_state = -1;
    int candidate = -1;
    int press = -1;
    int sw_status = 0;
    int counter = 0;
    int button_pressed = 1;
    struct timespec start = {0};
    struct timespec target = {0};
    struct timespec pause = {0};
    pause.tv_nsec = 5000000;
    double elapsed;

    // GPIO18 input with internal pull-up: released = 1, pressed = 0.
    fd_controller = GPIO_CONNECTION(&info, &request,19,1,GPIO_V2_LINE_FLAG_INPUT | GPIO_V2_LINE_FLAG_BIAS_PULL_UP);
        if(fd_controller == -1){
                perror("Failed to open GPIO");
		return -1;
	}
    values.mask = 1;

    printf("Time available to press 3...\n");
    sleep(1);
    printf("Time available to press 2...\n");
    sleep(1);
    printf("Time available to press 1...\n");
    sleep(1);

    while((counter < 4000) && button_pressed){
        //Reading of the value on the GPIO18 pin
        // 1 = released
        // 0 = pressed

        result = ioctl(request.fd,GPIO_V2_LINE_GET_VALUES_IOCTL,&values);
        if(result == -1){
            perror("Failed to read the button");
            sw_status = -1;
            goto closing;
        }
        result = clock_gettime(CLOCK_MONOTONIC, &start);
        if(result == -1){
            perror("Clock reading failure!");
            sw_status = -1;
            goto closing;
        }
        // Convert the requested line's bit to a logical int (0 or 1).
        press = (values.bits & 1) != 0;
        // Establish the initial state without reporting a button event.
        if(button_state == -1){
            button_state = press;
            candidate = press;
            target = start;
        }
        if(candidate != press){
            // Restart the stability interval only when the candidate changes.
            target = start;
            candidate = press;
        }
        elapsed = 
            (start.tv_sec - target.tv_sec) * 1000.0
            + (start.tv_nsec - target.tv_nsec) / 1000000.0;
        if((candidate != button_state) && (elapsed >= 30.0)){
            button_state = candidate;
            if(!button_state){
                printf("Pressed\n");
                button_pressed = !button_pressed
            }
        }
        
        result = nanosleep(&pause, NULL);
        if(result == -1){
            perror("Waiting failure!");
            sw_status = -1;
            goto closing;
        }
        counter ++;
    }


    closing:
        if(close_fd(request.fd) == -1){
            perror("Failed to close request fd");
            sw_status = -1;
        }
        if(close_fd(fd_controller) == -1){
            perror("Failed to close GPIO fd");
            sw_status = -1;
        }


    return sw_status;
}
