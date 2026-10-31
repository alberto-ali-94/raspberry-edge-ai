#include <stdio.h>
#include <unistd.h>                     //pipe(), read(), write()
#include <fcntl.h>                      //fcntl(), flags
#include <errno.h>                      //perror()
#include <pthread.h>
#include <poll.h>
#include "io.h"
#include "temperature.h"
#include "gpio.h"


/*
 *****************READING:
errno == EINTR                          La lettura è stata interrotta da un segnale prima di trasferire dati: 
                                        Riprovare
errno == EAGAIN o EWOULDBLOCK           Pipe piena: Al momento non ci sono byte e la pipe è non bloccante

******************WRITING:
errno == EINTR                          Riprovare la scrittura
errno == EAGAIN o EWOULDBLOCK           Pipe piena: contiene già avvisi che possono svegliare il principale

******************
GPIO 26: LED
GPIO 24: EXIT_BUTTON
GPIO 23: ALERT_BUTTON
******************

*/



typedef struct {
    int write_fd;
    double temperature;
    int sequence;
    int stop_requested;
    pthread_mutex_t mutex;

} thread_args_t;

typedef struct {
    double temperature;
    int sequence;
} data_packet;


void *temp_reader(void *arg){
    struct timespec deadline;

    //legge la temperatura continuamente, attende 1 secondo tra una lettura e l'altra
    //non attene con sleep(1) perché se viene premuto il bottone di uscita deve terminare
    //quindi userà pthread_cond_timedwait() che setta una deadline in scadenza assoluta
    //che viene gestita con clock_gettime(clock_monotonic, struttura) e timespec.tv_sec += 1



    

}


int main(){
    int exit_value = 0;
    int pipe_fd[2];                     //pipe_fd[0] --> read
    int num_lines = 3;
    int threshold_ready = 0;
    int flags, saved_error, fd_controller;
    int lines[] = {26,24,23};
    int led_state = 0;
    int exit_status = 0;
    struct pollfd fds[3] = {0};
    input_state input = {0};
    thread_args_t args;
    data_packet snapshot;
    ssize_t n;
    struct gpiochip_info info = {0};
    struct gpio_v2_line_request request = {0};
    struct gpio_v2_line_values values = {0};
    struct gpio_v2_line_event event = {0};

    // Set up of input structure
    init_input(&input);

    // Configuration of expection to handle the input
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


    fd_controller = GPIO_CONNECTION(&info, &request,lines,num_lines);
        if(fd_controller == -1){
            perror("Failed to open GPIO: ");
		return -1;
	}


        //pipe creation
    if(pipe(pipe_fd) == -1){
        perror("Pipe creation failed: ");
        exit_value = 1;
    }

    //get current flags on read end
    flags = fcntl(pipe_fd[0],F_GETFL);
    if (flags == -1){
        printf("Get current pipe_fd[0] flags failed\n");
        exit_value = 1;
    }
    //set O_NONBLOCK flags on read
    if (fcntl(pipe_fd[0],F_SETFL, flags | O_NONBLOCK ) == -1){
        perror("Set O_NONBLOCK flags failed: ");
        exit_value = 1;
    }

    //get current flags on write end
    flags = fcntl(pipe_fd[1],F_GETFL);
    if (flags == -1){
        printf("Get current pipe_fd[0] flags failed\n");
        exit_value = 1;
    }
    //set O_NONBLOCK flags on write
    if (fcntl(pipe_fd[1],F_SETFL, flags | O_NONBLOCK ) == -1){
        perror("Set O_NONBLOCK flags failed: ");
        exit_value = 1;
    }


    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN;
    fds[1].fd = request.fd;
    fds[1].events = POLLIN;
    fds[2].fd = pipe_fd[0];
    fds[2].events = POLLIN;








    n = read(pipe_fd[0],)



    closing:
        return exit_value;

}