#include <stdio.h>
#include <string.h>
#include <stdlib.h>

char * readLine(FILE *fptr, char *resultLine){
    if(fptr == NULL){
        fprintf(stderr, "Error: file pointer is null");
        return NULL;
    }

    int maxLineLength = 512;
    char *lineBuffer = (char *)malloc(sizeof(char) * maxLineLength);

    if(lineBuffer == NULL){
        fprintf(stderr, "Error: Allocating memory");
        return NULL;
    }

    char ch = getc(fptr);
    int count = 0;

    while((ch != '\n') && (ch != EOF)){
        if(count == maxLineLength){
            maxLineLength += 512;
            lineBuffer = realloc(lineBuffer, maxLineLength);
            if(lineBuffer == NULL){
                fprintf(stderr, "Error: Allocating memory");
                return NULL;
            }
        }
        lineBuffer[count] = ch;
        count++;

        ch = getc(fptr);
    }

    lineBuffer[count] = '\0';
    return lineBuffer;
}
