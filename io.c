#include "io.h"
#include <stdio.h>

void print_menu(void){
    printf("========================================\n");
    printf("\tRASPBERRY CONTROL CENTER\t\n");
    printf("========================================\n\n");
}

int read_integer(int *p){
    return scanf("%d",p);
}