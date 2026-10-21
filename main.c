#include <stdio.h>
#include <unistd.h>
#include <poll.h>
#include "io.h"
#include "temperature.h"
#include "gpio.h"

int main(void) {
        int temperature=0;
        int threshold=0;
        struct gpiochip_info info = {0};
        struct gpio_v2_line_request request = {0};
        struct gpio_v2_line_values values = {0};
        struct gpio_v2_line_event event = {0};
        struct pollfd fds[2] = {0};
        int fd_controller, desired_state, result;
        char buf[5];
        ssize_t n;
        size_t ctr;
        size_t used = 0;
        int flg = 0;
        int num_lines = 3;
        //GPIO 17: LED
        //GPIO 18: EXIT_BUTTON
        //GPIO 19: ALERT_BUTTON
        int lines[] = {17,18,19};
        int led_state = 0;
        int exit_status = 0;


        //configuration of expection to handle the input
        request.config.flags = 
        GPIO_V2_LINE_FLAG_INPUT | // Configure the requested GPIO line as an input.
        GPIO_V2_LINE_FLAG_BIAS_PULL_UP | // Enable the internal pull-up resistor, keeping the input high when the button is open.
        GPIO_V2_LINE_FLAG_EDGE_FALLING; // Trigger an event when the GPIO changes from high to low.

        //Number of extra configuration attributes attached to the line
        request.config.num_attrs=1;
	request.config.attrs[0].attr.id = GPIO_V2_LINE_ATTR_ID_FLAGS;
	request.config.attrs[0].attr.flags = GPIO_V2_LINE_FLAG_OUTPUT;
	request.config.attrs[0].mask = 1; //first element in the array "lines"


        fd_controller = GPIO_CONNECTION(&info, &request,lines,num_lines);
        if(fd_controller == -1){
                perror("Failed to open GPIO");
		return -1;
	}

        // set up of poll() parameters
        fds[0].fd = STDIN_FILENO;
        fds[0].events = POLLIN;
        fds[1].fd = request.fd;
        fds[1].events = POLLIN;

        print_menu();

        printf("Enter the threshold: ");
        if (read_integer(&threshold) != 1){
                printf("Input error!\n");
                exit_status = 1;
                goto closing;
        }
        
        while(1){
                printf("Enter the temperature or press any button:\n");
                result = poll(fds, 2, -1);
                if(result == -1){
                        perror("Poll error");
                        exit_status = 1;
                        goto closing;
                }
                clear_screen();
                //*******************STDIN branch*******************
                if(fds[0].revents & POLLERR){
                        printf("Error on File Descriptor [%d]\n", fds[0].fd);
                        exit_status = 1;
                        goto closing;
                }else if(fds[0].revents & POLLNVAL){
                        printf("Errore: File Descriptor [%d] not open\n", fds[0].fd);
                        exit_status = 1;
                        goto closing;
                }else if((fds[0].revents & POLLIN)){
                        n = read(0, &buf[used], sizeof(buf)-used-1);
                        if(n > 0){
                                used += n;
                                buf[used] = '\0';
                                ctr = 0;
                                while(ctr < used){
                                        if(buf[ctr] == '\n'){
                                                flg = 1;
                                        }
                                        ctr++;
                                }
                                if(flg){
                                        // Temperature and LED control
                                        temperature = str_to_int(buf);
                                        if(temperature == -1){
                                                exit_status = 3;
                                                goto closing;
                                        }
                                        desired_state = update_alarm(temperature, threshold+1,threshold-1,led_state);
                                        if(desired_state != led_state){
                                                result=controller_lines_action(request.fd, &values,desired_state,1,GPIO_V2_LINE_SET_VALUES_IOCTL);
                                                if(result == -1){
                                                        perror("Failed to set GPIO line values");
                                                        exit_status = 3;
                                                        goto closing;
                                                }
                                                led_state = desired_state;
                                        }
                                        if(is_temperature_high(temperature, threshold)){
                                                printf("TEMPERATURE: Above threshold\n");
                                        }else{
                                                printf("TEMPERATURE: At or below threshold\n");
                                        }
                                        if(led_state){
                                                printf("ALARM: Active\n");
                                        }else{
                                                printf("ALARM: Inactive\n");
                                        }
                                        used=0;
                                        flg=0;
                                } else {
                                        printf("\nNo newline in this block\n");
                                        printf("Current lenght: %zu\n",used);
                                }           
                        }
                if(n == -1){
                        perror("Error in read()");
                        exit_status = 1;
                        goto closing;
                }
                if(n == 0){
                        printf("No input received from STDIN. Closing\n");
                        exit_status = 0;
                        goto closing;
                }
                }else if(fds[0].revents & POLLHUP){
                        printf("Error: End of input");
                        exit_status = 1;
                        goto closing;
                }

                
                //*******************GPIO branch*******************
                if(fds[1].revents & POLLERR){
                        printf("Error on File Descriptor [%d]\n",fds[1].fd);
                        exit_status = 1;
                        goto closing;
                }else if(fds[1].revents & POLLNVAL){
                        printf("Errore: File Descriptor [%d] not open\n", fds[1].fd);
                        exit_status = 1;
                        goto closing;
                }else if(fds[1].revents & POLLIN){
                        n = read(request.fd, &event, sizeof event);
                        if(n == -1){
                                perror("Error in read()");
                                exit_status = -3;
                                goto closing;
                        }
                        if(n == (ssize_t)sizeof event){
                                if((event.offset == 18) && (event.id == GPIO_V2_LINE_EVENT_FALLING_EDGE)){
                                        printf("Exit button pressed!\n");
                                        exit_status = 0;
                                        goto closing;
                                }
                                if((event.offset == 19) && (event.id == GPIO_V2_LINE_EVENT_FALLING_EDGE)){
                                        led_state = !led_state;
                                        result=controller_lines_action(request.fd, &values,led_state,1,GPIO_V2_LINE_SET_VALUES_IOCTL);
                                        if(result == -1){
                                                perror("Failed to set GPIO line values");
                                                exit_status = 3;
                                                goto closing;
                                        }
                                        desired_state = led_state;
                                        if(led_state){
                                                printf("ALARM: Manually activated\n");
                                        }else{
                                                printf("ALARM: Manually deactivated\n");
                                        }     
                                }
                        }
                        else{
                                printf("Incomplete GPIO event: received %zd bytes, expected %zu\n",
                                        n, sizeof event);
                                exit_status = -3;
                                goto closing;
                        }
                }else if(fds[1].revents & POLLHUP){
                       printf("Error: End of input");
                        exit_status = 1;
                        goto closing;
                }
                
        }
        /*if(used >= (sizeof(buf)-1)){
                printf("Buffer limit reached!\n");
                exit_status = 1;
                goto closing;
        }*/


        closing:
                if(controller_lines_action(request.fd, &values,0,1,GPIO_V2_LINE_SET_VALUES_IOCTL) == -1){
                        perror("Failed to set GPIO line values");
                        if(exit_status == 0){
                                exit_status = 3;
                        }
                }
                if(close_fd(request.fd)==-1){
                        perror("Failed to close GPIO line");
                        if(exit_status == 0){
                                exit_status = -2;
                        }
                }
                if(close_fd(fd_controller) == -1){
                        perror("Failed to close GPIO controller");
                        if(exit_status == 0){
                                exit_status = -1;
                        }
                }
                printf("Shutting down...\n");
                return exit_status;
}
