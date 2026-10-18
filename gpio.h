#ifndef GPIO_H
#define GPIO_H

#include <stdint.h>
#include <linux/gpio.h>

int controller_fd(
    struct gpiochip_info *info
);

int controller_lines_info(
    int fd,
    struct gpio_v2_line_info *line,
    int offset
);
int controller_lines_request(
	int fd 
	,struct gpio_v2_line_request *request 
	,int offset_line[]
	,int num_lines 
);
int controller_lines_action(
	int fd, 
	struct gpio_v2_line_values *values, 
	uint64_t bits, 
	uint64_t mask,
	unsigned long operation
);
int GPIO_CONNECTION(
	struct gpiochip_info *info,
	struct gpio_v2_line_request *request,
	int offset_line[], 
	int num_lines
);
int close_fd(
	int fd
);

#endif