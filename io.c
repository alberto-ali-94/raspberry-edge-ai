#include "io.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void print_menu(void){
    printf("========================================\n");
    printf("\tRASPBERRY CONTROL CENTER\t\n");
    printf("========================================\n\n");
}

void init_input(input_state *state){
    state->used = 0;
    state->discarding = 0;
    state->buf[0] = '\0';
}

int read_input(input_state *state, int *output){
    size_t ctr = 0;
    int flg = 0;
    ssize_t n;
    int exit_state;

    n = read(0, &((*state).buf[(*state).used]), sizeof((*state).buf)-(*state).used-1);
        if(n > 0){
            (*state).used += n;
            ctr = 0;
            while(ctr < (*state).used){
                if((*state).buf[ctr] == '\n'){
                        flg = 1;
                }
                ctr++;
            }
            (*state).buf[(*state).used] = '\0';

            if(flg && !(*state).discarding){
                // Ramo ok: l'input è andato a buon fine, non devo fare altro e posso uscire dalla funzione 
                exit_state = str_to_int((*state).buf, output);
                if (exit_state == -1){
                    (*state).used = 0;
                    return 0;
                }
                (*state).used = 0; 
                return 1;
            }
            // The rejected line has not ended: discard this block and stay in discard mode.
            else if(!flg && (*state).discarding){
                printf("Discarding the rest of the invalid temperature line...\n");
                (*state).used = 0;
                return 0;
            }
            // The rejected line has reached its newline: reset the state for a new line.
            else if(flg && (*state).discarding){
                printf("Invalid temperature line fully discarded. Ready for a new temperature.\n");
                (*state).discarding = 0;
                flg = 0;
                (*state).used = 0; 
                return 0;
            }
            // No newline yet, but space remains: keep the accumulated bytes for the next read.
            else if((*state).used < (sizeof((*state).buf) - 1)){
                printf("Temperature line incomplete: waiting for the remaining characters or Enter.\n");
                return 0;
            }
            else {
                (*state).discarding = 1;
                (*state).used = 0; 
                printf("Invalid temperature: enter at most 3 characters before pressing Enter. Discarding this line.\n");
                return 0;
            }
                        
        }else if(n == -1){
            perror("Error in read()");
            return -1;
        }else{
            printf("End of keyboard input (EOF). Shutting down.\n");
            return -1;
        }   
}

int read_integer(int *p){
    return scanf("%d",p);
}

int str_to_int(char *str, int *result){
    long i;
    char *eob;

    i = strtol(str, &eob, 10);
    // Compare the two pointers: if they point to the same byte, eob did not move and no conversion occurred.
    if (str == eob){ 
        printf("No Number found!\n");
        return -1;
    }else if (*eob != '\n'){
       printf("Invalid temperature: unexpected character after the number.\n");
       return -1; 
    }
    else {
        *result = (int)i;
        return 0;
    }
}

void clear_screen(void) {
    printf("\033[2J\033[H");
    fflush(stdout);
    // \033     --> ESC character, alert the terminal the beginning of a control sequence
    // [2J      --> clear the visible area
    // [H       --> bring the cursor in the upper right corner
}