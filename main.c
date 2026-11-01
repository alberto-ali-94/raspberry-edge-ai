//POSIX INTERFACES WITH "-std=c11" GCC ATTRIBUTE 
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>                     //_Exit(), EXIT_FAILURE
#include <unistd.h>                     //pipe(), read(), write()
#include <fcntl.h>                      //fcntl(), flags
#include <errno.h>                      //perror()
#include <pthread.h>
#include <poll.h>
#include <string.h>                     //strerror()
#include <sys/ioctl.h>
#include <time.h>
#include <linux/i2c-dev.h>
#include "io.h"
#include "temperature.h"
#include "gpio.h"
#include "thermistor.h"

/*
******************
GPIO 19: LED
GPIO 24: EXIT_BUTTON
GPIO 23: ALERT_BUTTON
******************
*/

typedef struct {
    int FD_ADC;
    int write_fd;
    double temperature;
    unsigned long sequence;
    int stop_requested;
    pthread_mutex_t mutex;
    pthread_cond_t condition;
    int error_exit;
} thread_args_t;

typedef struct {
    double temperature;
    unsigned long sequence;
} data_packet;

void *temp_reader(void *arg){
    struct timespec deadline;
    thread_args_t *local_arg = arg;
    int fd_adc, rc;
    int worker_error_code = 0;
    int error_detail = 0;
    uint8_t raw;
    double tempC;
    int local_stop_request;
    unsigned char notification = 1;
    ssize_t written;

    fd_adc = local_arg->FD_ADC;
    while(1){        
        rc=pthread_mutex_lock(&(local_arg->mutex));
        if(rc != 0){
            worker_error_code = 65;
            goto synchronization_error;
        }
        local_stop_request = local_arg->stop_requested;
        rc = pthread_mutex_unlock(&(local_arg->mutex));
        if(rc != 0){
            worker_error_code = 66;
            goto synchronization_error;
        }
        if(local_stop_request){
            break;
        }
        if (read_adc(fd_adc, &raw) == -1){
            worker_error_code = 60;
            goto worker_error;
        }
        if(adc_to_celsius(raw, &tempC) == -1){
            worker_error_code = 61;
            goto worker_error;
        }

        rc=pthread_mutex_lock(&(local_arg->mutex));
        if(rc != 0){
            worker_error_code = 70;
            goto synchronization_error;
        }
        local_arg->temperature = tempC;
        local_arg->sequence += 1;
        rc = pthread_mutex_unlock(&(local_arg->mutex));
        if(rc != 0){
            worker_error_code = 71;
            goto synchronization_error;
        }
        written = write(local_arg->write_fd, &notification, sizeof(notification));
        while ((written == -1) && (errno == EINTR)){
            written = write(local_arg->write_fd, &notification, sizeof(notification));
        }
        if(written == 1){
            //printf("[Thread] Alert written\n");
        }
        else if ((written == -1) && ((errno == EAGAIN) || (errno == EWOULDBLOCK))){
            printf("[Thread] Nothing written, already pending alerts\n");
        }
        else if (written == -1){
            error_detail = errno;
            worker_error_code = 62;
            goto worker_error;
        }
        else {
            fprintf(stderr, "[Thread] Notification write transferred %zd bytes; expected 1\n", written);
            worker_error_code = 63;
            goto worker_error;
        }
        
        if (clock_gettime(CLOCK_MONOTONIC,&deadline) == -1){
            error_detail = errno;
            worker_error_code = 64;
            goto worker_error;
        }
        deadline.tv_sec += 2;
        
        rc=pthread_mutex_lock(&(local_arg->mutex));
        if(rc != 0){
            worker_error_code = 72;
            goto synchronization_error;
        }
        while(!(local_arg->stop_requested)){
            rc = pthread_cond_timedwait(&local_arg-> condition, &local_arg->mutex, &deadline);
            if (rc == ETIMEDOUT){
                break;
            } else if (rc != 0){
                worker_error_code = 67;
                goto synchronization_error;
            }
        }
        rc = pthread_mutex_unlock(&(local_arg->mutex));
        if(rc != 0){
            worker_error_code = 73;
            goto synchronization_error;
        }

    }
    printf("[Thread] Shutting down...\n");
    return local_arg;

worker_error:
    fprintf(stderr, "[Thread] Error code: %d\n", worker_error_code);
    if (error_detail != 0) fprintf(stderr, "[Thread] Cause: %s\n", strerror(error_detail));
    // This path is reached only when the mutex is not held.
    rc = pthread_mutex_lock(&local_arg->mutex);
    if (rc != 0){
        worker_error_code = 68;
        goto synchronization_error;
    }
    local_arg->error_exit = worker_error_code;
    rc = pthread_mutex_unlock(&local_arg->mutex);
    if (rc != 0){
        worker_error_code = 69;
        goto synchronization_error;
    }
    return NULL;

synchronization_error:
    // Do not publish data using unreliable synchronization.
    fprintf(stderr, "[Thread] Synchronization error %d: %s\n", worker_error_code, strerror(rc));
    _Exit(worker_error_code);
}

int main(){
    int exit_value = 0;
    int worker_error_code = 0;
    //File descriptors
    int pipe_fd[2] = {-1, -1};
    struct pollfd fds[3] = {0};
    int fd_controller = -1, fd_adc = -1;
    //GPIO configuration                    
    int num_lines = 3;
    int lines[] = {19,24,23};
    int led_state = 0;
    struct gpiochip_info info = {0};
    struct gpio_v2_line_request request = {0};
    struct gpio_v2_line_values values = {0};
    struct gpio_v2_line_event event = {0};
    //ADC/thermistor configuration
    //pipe()
    int flags;
    //Thread
    int rc;
    pthread_condattr_t attributes;
    pthread_t worker;
    int worker_started = 0;
    // Resource initialization flags: these are not synchronization primitives.
    int mutex_ready = 0;
    int condition_ready = 0;
    int attributes_ready = 0;
    void *worker_result = NULL;
    //Poll
    int ready;
    unsigned char notification;
    //Others
    int desired_state;
    int threshold = 0;
    int threshold_ready = 0;
    int new_sequence = 0;
    input_state input = {0};
    thread_args_t args;
    data_packet snapshot = {0};
    ssize_t n;

    // Set up of input structure
    init_input(&input);
    request.fd = -1;

    //=========================== CONFIGURATION OF GPIO CONNECTION ===========================
    request.config.flags = 
    GPIO_V2_LINE_FLAG_INPUT | // Configure the requested GPIO line as an input.
    GPIO_V2_LINE_FLAG_BIAS_PULL_UP | // Enable the internal pull-up resistor, keeping the input high when the button is open.
    GPIO_V2_LINE_FLAG_EDGE_FALLING; // Trigger an event when the GPIO changes from high to low.
    
    //Number of extra configuration attributes attached to the line
    request.config.num_attrs=2;
	request.config.attrs[0].attr.id = GPIO_V2_LINE_ATTR_ID_FLAGS;
	request.config.attrs[0].attr.flags = GPIO_V2_LINE_FLAG_OUTPUT;
	request.config.attrs[0].mask = 1; //first element in the array "lines"
    // Set up debounce of buttons at 30 ms
    request.config.attrs[1].attr.id = GPIO_V2_LINE_ATTR_ID_DEBOUNCE;
    request.config.attrs[1].attr.debounce_period_us = 30000; //micro seconds = 30 ms
    request.config.attrs[1].mask = 6;

    //=========================== OPENING GPIO CONNECTION ===========================
    fd_controller = GPIO_CONNECTION(&info, &request,lines,num_lines);
    if(fd_controller == -1){
        fprintf(stderr, "GPIO controller or line request setup failed\n");
        return 10;
    }

    //=========================== OPENING I2C CONNECTION WITH ADC ===========================
    fd_adc = open("/dev/i2c-1",O_RDWR);
        if (fd_adc == -1){
            fprintf(stderr, "Opening /dev/i2c-1 failed: %s\n", strerror(errno));
            exit_value = 11;
            goto closing;
        }

    //send to the kernel the request to select the device's address 
    //i'm saying to the kernel that the file descriptor will be associated to the address 0x48
    if((ioctl(fd_adc, I2C_SLAVE, 0x48)) == -1){
            fprintf(stderr, "Selecting I2C device 0x48 failed: %s\n", strerror(errno));
            exit_value = 12;
            goto closing;
    }

    //=========================== PIPE CREATION AND CONFIG ===========================

    if(pipe(pipe_fd) == -1){
        fprintf(stderr, "Creating notification pipe failed: %s\n", strerror(errno));
        exit_value = 13;
        goto closing;
    }
    // READ CONFIG
    flags = fcntl(pipe_fd[0],F_GETFL);
    if (flags == -1){
        fprintf(stderr, "Reading notification pipe read-end flags failed: %s\n", strerror(errno));
        exit_value = 14;
        goto closing;
    }
    if (fcntl(pipe_fd[0],F_SETFL, flags | O_NONBLOCK ) == -1){
        fprintf(stderr, "Enabling nonblocking mode on pipe read end failed: %s\n", strerror(errno));
        exit_value = 15;
        goto closing;
    }
    // WRITE CONFIG
    flags = fcntl(pipe_fd[1],F_GETFL);
    if (flags == -1){
        fprintf(stderr, "Reading notification pipe write-end flags failed: %s\n", strerror(errno));
        exit_value = 16;
        goto closing;
    }
    if (fcntl(pipe_fd[1],F_SETFL, flags | O_NONBLOCK ) == -1){
        fprintf(stderr, "Enabling nonblocking mode on pipe write end failed: %s\n", strerror(errno));
        exit_value = 17;
        goto closing;
    }

    //=========================== POLL CONFIG ===========================
    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN;
    fds[1].fd = request.fd;
    fds[1].events = POLLIN;
    fds[2].fd = pipe_fd[0];
    fds[2].events = POLLIN;

    //=========================== THREAD CONFIG AND RUN ===========================
    rc = pthread_mutex_init(&args.mutex, NULL);
    if (rc != 0){
        fprintf(stderr, "Initializing shared mutex failed: %s\n", strerror(rc));
        exit_value = 18;
        goto closing;
    }
    mutex_ready = 1;
    args.sequence = 0;
    args.stop_requested = 0;
    args.temperature = 0;
    args.error_exit = 0;
    args.write_fd = pipe_fd[1];
    args.FD_ADC = fd_adc;

    snapshot.sequence = 0;
    snapshot.temperature = 0;

    rc = pthread_condattr_init(&attributes);
    if (rc != 0){
        fprintf(stderr, "Initializing condition attributes failed: %s\n", strerror(rc));
        exit_value = 19;
        goto closing;
    }
    attributes_ready = 1;
    rc = pthread_condattr_setclock(&attributes, CLOCK_MONOTONIC);
    if (rc != 0){
        fprintf(stderr, "Selecting condition CLOCK_MONOTONIC failed: %s\n", strerror(rc));
        exit_value = 20;
        goto closing;
    }
    rc = pthread_cond_init(&args.condition, &attributes);
    if (rc != 0){
        fprintf(stderr, "Initializing condition variable failed: %s\n", strerror(rc));
        exit_value = 21;
        goto closing;
    }
    condition_ready = 1;
    rc = pthread_condattr_destroy(&attributes);
    if (rc != 0){
        fprintf(stderr, "Releasing condition attributes during startup failed: %s\n", strerror(rc));
        exit_value = 22;
        goto closing;
    }
    attributes_ready = 0;

    rc = pthread_create(&worker, NULL, temp_reader, &args);
    if (rc != 0){
        fprintf(stderr, "Creating acquisition worker failed: %s\n", strerror(rc));
        exit_value = 23;
        goto closing;
    }
    worker_started = 1;

    //=========================== POLL RUN ===========================

    print_menu();
    printf("Enter the threshold: ");
    fflush(stdout);

    while(1){
        // Also clear events for descriptors excluded from this iteration.
        for (int i = 0; i < 3; ++i){
            fds[i].revents = 0;
        }

        ready = poll(fds, 3, -1);
        if (ready == -1 && errno == EINTR){
            continue;
        }
        if (ready == -1){
            fprintf(stderr, "Waiting for monitored events with poll failed: %s\n", strerror(errno));
            exit_value = 30;
            goto closing;
        }

        // Copy the worker error under the mutex before deciding whether to exit.
        rc = pthread_mutex_lock(&args.mutex);
        if (rc != 0){
            fprintf(stderr, "Main could not lock shared state; process shutdown required: %s\n", strerror(rc));
            return 31;
        }
        worker_error_code = args.error_exit;
        rc = pthread_mutex_unlock(&args.mutex);
        if (rc != 0){
            fprintf(stderr, "Main could not unlock shared state; process shutdown required: %s\n", strerror(rc));
            return 32;
        }
        if (worker_error_code != 0){
            exit_value = worker_error_code;
            goto closing;
        }
        if (ready == 0){
            exit_value = 2;
            goto closing;
        }

        //Button handling
        //gpio branch
        if(fds[1].revents & POLLERR){
            fprintf(stderr, "GPIO descriptor reported POLLERR\n");
            exit_value = 33;
            goto closing;  
        }else if(fds[1].revents & POLLNVAL){
            fprintf(stderr, "GPIO descriptor reported POLLNVAL\n");
            exit_value = 34;
            goto closing;
        }else if(fds[1].revents & POLLIN){ //Incoming GPIO event
            do {
                n = read(request.fd, &event, sizeof(event));
            } while (n == -1 && errno == EINTR);
            if(n == -1){
                fprintf(stderr, "Reading GPIO event failed: %s\n", strerror(errno));
                exit_value = 35;
                goto closing;
            }   
            if(n == (ssize_t)sizeof(event)){ //Complete GPIO event read
                //Acquire the mutex and set the thread exit condition
                if((event.offset == 24) && (event.id == GPIO_V2_LINE_EVENT_FALLING_EDGE)){ //Read from the exit button on GPIO24
                        exit_value = 0;
                        goto closing;
                }
                //Read from the alert button on GPIO23 - manually switch the LED on or off
                if((event.offset == 23) && (event.id == GPIO_V2_LINE_EVENT_FALLING_EDGE)){
                    led_state = !led_state;
                    rc=controller_lines_action(request.fd, &values,led_state,1,GPIO_V2_LINE_SET_VALUES_IOCTL);
                    if(rc == -1){
                            fprintf(stderr, "Writing manual LED state failed: %s\n", strerror(errno));
                            exit_value = 38;
                            goto closing;
                    }
                    desired_state = led_state;
                    //Manually switch the LED on or off
                    if(led_state){
                        printf("ALARM: Manually activated\n");
                    }else{
                        printf("ALARM: Manually deactivated\n");
                    }     
                }
            } else { //Incomplete GPIO event read
                fprintf(stderr, "[E036] GPIO event: received %zd bytes, expected %zu\n", n, sizeof event);
                exit_value = 36;
                goto closing;
            }
        }else if(fds[1].revents & POLLHUP){
            fprintf(stderr, "GPIO descriptor reported POLLHUP\n");
            exit_value = 37;
            goto closing;
        }

        //Read threshold   
        // stdin is not a valid open descriptor: report the error and shut down.
        if(fds[0].revents & POLLERR){
            fprintf(stderr, "Keyboard descriptor reported POLLERR\n");
            exit_value = 39;
            goto closing;  
        }else if(fds[0].revents & POLLNVAL){
                fprintf(stderr, "Keyboard descriptor reported POLLNVAL\n");
                exit_value = 40;
                goto closing;
        }
        // stdin is ready to read: append bytes after those already stored in buf.
        else if(fds[0].revents & POLLIN){
            rc = read_input(&input, &threshold);
            if(rc == -1){
                    fprintf(stderr, "Keyboard read failed or reached EOF; see input diagnostic\n");
                    exit_value = 41;
                    goto closing;
            }else if ((rc == 1)){
                    threshold_ready = 1;
                    printf("You can enter a new threshold\n");
            } 
        }
        else if(fds[0].revents & POLLHUP){
            fprintf(stderr, "Keyboard disconnected (POLLHUP)\n");
            exit_value = 42;
            goto closing;
        }

        // Consume notifications even before threshold entry; automatic LED control waits for the threshold.
        //pipeline branch
        if(fds[2].revents & POLLERR){
            fprintf(stderr, "Notification pipe reported POLLERR\n");
            exit_value = 43;
            goto closing;
        }// fds[2] is not a valid open descriptor: report the error and shut down.
        else if(fds[2].revents & POLLNVAL){
                fprintf(stderr, "Notification pipe reported POLLNVAL\n");
                exit_value = 44;
                goto closing;
        }
        // Notifications may remain even with HUP: read them before recognizing EOF.
        // One read per iteration limits the work spent on the pipe.
        else if(fds[2].revents & (POLLIN | POLLHUP)){
            do {
                n = read(pipe_fd[0], &notification, sizeof(notification));
            } while (n == -1 && errno == EINTR);
            //Data available in the pipe
            if (n>0){
                rc=pthread_mutex_lock(&(args.mutex));
                if(rc != 0){
                    fprintf(stderr, "Main could not lock shared state; process shutdown required: %s\n", strerror(rc));
                    return 48;
                }
                if(args.sequence != snapshot.sequence){
                    snapshot.temperature = args.temperature;
                    snapshot.sequence = args.sequence;
                    new_sequence = 1;
                }
                rc = pthread_mutex_unlock(&(args.mutex));
                if(rc != 0){
                    fprintf(stderr, "Main could not unlock shared state; process shutdown required: %s\n", strerror(rc));
                    return 49;
                }
                //New sample: check the LED state and change it if necessary
                if(threshold_ready && new_sequence){
                    desired_state = update_alarm(snapshot.temperature, (double)threshold,(double)threshold,led_state);
                    if(desired_state != led_state){
                        rc=controller_lines_action(request.fd, &values,desired_state,1,GPIO_V2_LINE_SET_VALUES_IOCTL);
                        if(rc == -1){
                                fprintf(stderr, "Writing automatic LED state failed: %s\n", strerror(errno));
                                exit_value = 45;
                                goto closing;
                        }
                        led_state = desired_state;
                        if(is_temperature_high(snapshot.temperature, (double)threshold)){
                            printf("[MAIN] TEMPERATURE: %.2f°C above threshold.\n", snapshot.temperature);
                        }else{
                            printf("[MAIN] TEMPERATURE: %.2f°C at or below threshold\n", snapshot.temperature);
                        }
                        if(led_state){
                            printf("[MAIN] ALARM: Active\n");
                        }else{
                            printf("[MAIN] ALARM: Inactive\n");
                        }  
                    }
                    
                }
                new_sequence = 0;              
            } 
            //No notification available; this is normal
            else if (n == -1 && ((errno == EAGAIN)|| (errno == EWOULDBLOCK))){
                printf("[MAIN] No new reading available from the worker\n");
            } else if (n == 0){
                fprintf(stderr, "Notification pipe reached EOF unexpectedly\n");
                exit_value = 46;
                goto closing;
            } else {
                fprintf(stderr, "Reading notification pipe failed: %s\n", strerror(errno));
                exit_value = 47;
                goto closing;
            }
        }
    }

    closing:
        if (worker_started){
            rc = pthread_mutex_lock(&args.mutex);
            if (rc != 0){
                fprintf(stderr, "Main could not lock mutex to request stop: %s\n", strerror(rc));
                return exit_value != 0 ? exit_value : 80;
            }
            args.stop_requested = 1;
            rc = pthread_cond_broadcast(&args.condition);
            if (rc != 0){
                fprintf(stderr, "Broadcasting stop notification failed: %s\n", strerror(rc));
                if (exit_value == 0) exit_value = 81;
            }
            rc = pthread_mutex_unlock(&args.mutex);
            if (rc != 0){
                fprintf(stderr, "Main could not unlock mutex after requesting stop: %s\n", strerror(rc));
                return exit_value != 0 ? exit_value : 82;
            }
            rc = pthread_join(worker, &worker_result);
            if (rc != 0){
                fprintf(stderr, "Joining worker failed; worker termination is not confirmed: %s\n", strerror(rc));
                // Worker termination is not confirmed: do not destroy resources.
                return exit_value != 0 ? exit_value : 83;
            }
            worker_started = 0;
            if (worker_result != &args){
                // After join, the worker has terminated: reading without the mutex is allowed.
                worker_error_code = args.error_exit != 0 ? args.error_exit : 84;
                fprintf(stderr, "[Main] Worker error code: %d\n", worker_error_code);
                // If the timeout only revealed the failure, preserve the actual cause.
                if (exit_value == 0 || exit_value == 2){
                    exit_value = worker_error_code;
                }
            }
        }

        if (condition_ready){
            rc = pthread_cond_destroy(&args.condition);
            if (rc != 0){
                fprintf(stderr, "Destroying condition variable failed: %s\n", strerror(rc));
                if (exit_value == 0) exit_value = 85;
            } else {
                condition_ready = 0;
            }
        }
        if (attributes_ready){
            rc = pthread_condattr_destroy(&attributes);
            if (rc != 0){
                fprintf(stderr, "Destroying remaining condition attributes failed: %s\n", strerror(rc));
                if (exit_value == 0) exit_value = 86;
            } else {
                attributes_ready = 0;
            }
        }
        if (mutex_ready && !condition_ready){
            rc = pthread_mutex_destroy(&args.mutex);
            if (rc != 0){
                fprintf(stderr, "Destroying shared mutex failed: %s\n", strerror(rc));
                if (exit_value == 0) exit_value = 87;
            } else {
                mutex_ready = 0;
            }
        }

        // Do not retry close: on Linux, the descriptor may already have been released.
        for (int i = 0; i < 2; ++i){
            if (pipe_fd[i] != -1){
                if (close(pipe_fd[i]) == -1){
                    int close_code = i == 0 ? 88 : 89;
                    fprintf(stderr, "Closing pipe end %d failed: %s\n", i, strerror(errno));
                    if (exit_value == 0) exit_value = close_code;
                }
                pipe_fd[i] = -1;
            }
        }
        if (fd_adc != -1){
            if (close(fd_adc) == -1){
                fprintf(stderr, "Closing ADC descriptor failed: %s\n", strerror(errno));
                if (exit_value == 0) exit_value = 90;
            }
            fd_adc = -1;
        }
        if (request.fd != -1){
            if (controller_lines_action(request.fd, &values, 0, 1,
                                       GPIO_V2_LINE_SET_VALUES_IOCTL) == -1){
                fprintf(stderr, "Turning LED off during shutdown failed: %s\n", strerror(errno));
                if (exit_value == 0) exit_value = 91;
            }
            if (close_fd(request.fd) == -1){
                fprintf(stderr, "Closing GPIO line descriptor failed: %s\n", strerror(errno));
                if (exit_value == 0) exit_value = 92;
            }
            request.fd = -1;
        }
        if (fd_controller != -1){
            if (close_fd(fd_controller) == -1){
                fprintf(stderr, "Closing GPIO controller descriptor failed: %s\n", strerror(errno));
                if (exit_value == 0) exit_value = 93;
            }
            fd_controller = -1;
        }
        return exit_value;
}