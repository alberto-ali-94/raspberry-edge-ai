#include "io.h"
#include <stdio.h>
#include <stdlib.h>

void print_menu(void){
    printf("========================================\n");
    printf("\tRASPBERRY CONTROL CENTER\t\n");
    printf("========================================\n\n");
}

int read_integer(int *p){
    return scanf("%d",p);
}

int str_to_int(char *str){
    long i;
    char *eob;

    i = strtol(str, &eob, 10);
    // Compare the two pointers: if they point to the same byte, eob did not move and no conversion occurred.
    if (str == eob){ 
        printf("No Number found!\n");
        return -1;
    }else{
        return (int)i;
    }
}

void clear_screen(void) {
    printf("\033[2J\033[H");
    fflush(stdout);
    // \033     --> ESC character, alert the terminal the beginning of a control sequence
    // [2J      --> clear the visible area
    // [H       --> bring the cursor in the upper right corner
}