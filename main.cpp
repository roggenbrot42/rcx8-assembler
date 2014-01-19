#include<stdio.h>
#include<parser.h>
#include<generator.h>
#include<assembler.h>


int main(int argc, char *argv[])
{
    if(argc < 2){
        printf("--------------"
               "\nThis is RCx8Asm by Nicolas Appel (c) 2012"
               "\nUsage: rcx8asm inputfile [outputfile]\n");
    }
    else{
        char * sInputName = argv[1];
        FILE * fInputFile = fopen(sInputName,"r");
        if(!fInputFile)
        {
            printf("File could not be opened. Aborting...");
        }
        else{
            parseInput(fInputFile);
            generateCode();
            assembleBinary();
        }
    }
    return 0;
}
