#include<stdio.h>
#include "stack.h"

int main(int argc, char *argv[])
{
	stack_t stack;
	char c;
	
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
			stack_init(&stack,7);
			printf("stack state: %d\n", stack_is_empty(&stack));
			do{
				c = getchar();
				stack_push(&stack, c);
			}while(c != '.' && !stack_is_full(&stack));
			printf("======================\n");
			while(!stack_is_empty(&stack)){
				printf("%c", stack_pop(&stack));
			}
			stack_destroy(&stack);
		}
	}
	return 0;
}
