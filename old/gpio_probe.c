#include <fcntl.h>			//open and O_RDONLY
#include <stdio.h>			// printf and perror
#include <stdint.h>			// uint64_t
#include <unistd.h>			//close and sleep
#include <linux/gpio.h>		//GPIO structures and requests
#include <sys/ioctl.h>		//ioctl
#include "gpio_probe.h"

int controller_fd(
	struct gpiochip_info *info
){
	int controller_fd = open("/dev/gpiochip0", O_RDONLY);
	if(controller_fd == -1){
		return -1;
	}
	int result = ioctl(controller_fd, GPIO_GET_CHIPINFO_IOCTL, info);
	if(result == -1){
		close(controller_fd);
		return -1;
	}
	printf("File descriptor assigned: %d\n",controller_fd);
	printf("GPIO info: Name=%s, Label=%s, Lines=%u\n",(*info).name, (*info).label, (*info).lines);
	return controller_fd;
}


int controller_lines_info(
	int fd, 
	struct gpio_v2_line_info *line, 
	int offset
){
	(*line).offset = offset;
	int result = ioctl(fd, GPIO_V2_GET_LINEINFO_IOCTL, line);
	if(result == -1){
		return -1;
	}
	return 0;
}


int controller_lines_request(
	int fd, 
	struct gpio_v2_line_request *request, 
	int offset_line, 
	int num_lines, 
	uint64_t flags
){

	(*request).offsets = offset_line;
	(*request).config.flags = flags;
	(*request).num_lines = num_lines;
	int result = ioctl(fd, GPIO_V2_GET_LINE_IOCTL, request);
	if(result == -1){
		return -1;
	}
	printf("Line file descriptor: %d\n", (*request).fd);
	return 0;
}

int controller_lines_action(
	int fd, 
	struct gpio_v2_line_values *values, 
	uint64_t bits, 
	uint64_t mask
){
	(*values).mask = mask;
	(*values).bits = bits;
	int result = ioctl(fd, GPIO_V2_LINE_SET_VALUES_IOCTL, values);
	if(result == -1){
		return -1;
	}
	return 0;
}

int GPIO_CONNECTION(
	struct gpiochip_info *info,
	struct gpio_v2_line_request *request,
	int offset_line[], 
	int num_lines, 
	uint64_t flags
){
	int fd=controller_fd(info);
	if(fd == -1){
		perror("Error: Opening controller fd");
		return -1;
	}
	if(controller_lines_request(fd,request,offset_line,num_lines,flags) == -1){
		perror("Error: Opening controller line fd");
		close(fd);
		return -1;
	}
	return fd;
}

int close_fd(
	int fd
){
	if(close(fd)==-1){
		return -1;
	}
	return 0;
}