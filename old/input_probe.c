#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

int main(void){
    char buffer[64];
    char *eob;
    long r;

    printf("Inserisci il testo: ");
    if (fgets(buffer,sizeof(buffer),stdin) == NULL){
        printf("\nNo string reeived");
        return 1;
    }
    printf("String received: [%s]", buffer);

    r = strtol(buffer, &eob, 10);
    //confronto i due puntatori, se puntano allo stesso byte allora oeb non si è mosso e non c'è stata conversione
    if (buffer == eob){ 
        printf("No Number found!\n");
    }else{
        char *cursor;
        cursor = eob;
        //printf("CIAOOOO %c",*cursor);
        while ( (*cursor) !='\0' && (isspace((unsigned char)*cursor) != 0)){
            cursor = cursor+1;
        }
        if((*cursor) =='\0'){
            printf("OK\n");
            printf("Stringa: [%s],\n EOB: [%s]\n Result: %ld\n", buffer, eob,r);
        }else{
            printf("KO");
        }


        
    }
    return 0;
}