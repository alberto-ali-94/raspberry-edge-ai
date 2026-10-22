#ifndef IO_H
#define IO_H
#include <stddef.h>

typedef struct {
    char buf[5];
    size_t used;
    int discarding;
} input_state;


void print_menu(void);
void init_input(input_state *state);
int read_input(input_state *state, int *output);
int read_integer(int *p);
int str_to_int(char *str, int *result);
void clear_screen(void);

#endif