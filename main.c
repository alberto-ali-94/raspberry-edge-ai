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
        input_state input = {0};
        ssize_t n;
        int num_lines = 3;
        int threshold_ready = 0;
        //GPIO 17: LED
        //GPIO 18: EXIT_BUTTON
        //GPIO 19: ALERT_BUTTON
        int lines[] = {17,18,19};
        int led_state = 0;
        int exit_status = 0;

        //set up of input structure
        init_input(&input);

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
        fflush(stdout);
        while(1){
                result = poll(fds, 2, -1);
                if(result == -1){
                        perror("Poll error");
                        exit_status = 1;
                        goto closing;
                }
  //*******************         STDIN branch            *******************
                if(threshold_ready == 0){
                        if(fds[0].revents & POLLERR){
                                printf("Cannot read keyboard input: error on stdin (fd %d). Shutting down.\n", fds[0].fd);
                                exit_status = 1;
                                goto closing;
                        }
                        // stdin is not a valid open descriptor: report the error and shut down.
                        else if(fds[0].revents & POLLNVAL){
                                printf("Cannot read keyboard input: stdin (fd %d) is not open. Shutting down.\n", fds[0].fd);
                                exit_status = 1;
                                goto closing;
                        }
                        // stdin is ready to read: append bytes after those already stored in buf.
                        else if(fds[0].revents & POLLIN){
                                result = read_input(&input, &threshold);
                                if(result == -1){
                                        exit_status = 1;
                                        goto closing;
                                }else if (result == 1){
                                        threshold_ready = 1;
                                        printf("Enter the temperature or press any button:\n");
                                }     
                        }else if(fds[0].revents & POLLHUP){
                                printf("Keyboard input disconnected. Shutting down.\n");
                                exit_status = 1;
                                goto closing;
                        }
                }else{
                        clear_screen();                     
                        if(fds[0].revents & POLLERR){
                                printf("Cannot read keyboard input: error on stdin (fd %d). Shutting down.\n", fds[0].fd);
                                exit_status = 1;
                                goto closing;
                        }
                        // stdin is not a valid open descriptor: report the error and shut down.
                        else if(fds[0].revents & POLLNVAL){
                                printf("Cannot read keyboard input: stdin (fd %d) is not open. Shutting down.\n", fds[0].fd);
                                exit_status = 1;
                                goto closing;
                        }
                        // stdin is ready to read: append bytes after those already stored in buf.
                        else if(fds[0].revents & POLLIN){
                                // read the STDIN with buffer logic and convert into (int)temperature
                                result = read_input(&input, &temperature);
                                if(result == 1){
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
                                        printf("Enter the temperature or press any button:\n");
                                }else if(result == -1){
                                        exit_status = 1;
                                        goto closing;
                                }
                        }
                        // If no earlier stdin branch applies, handle a disconnected input stream by shutting down.
                        else if(fds[0].revents & POLLHUP){
                                printf("Keyboard input disconnected. Shutting down.\n");
                                exit_status = 1;
                                goto closing;
                        }
                }       
                
//*******************           GPIO branch             *******************
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
