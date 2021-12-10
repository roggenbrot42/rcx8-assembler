#include<ctype.h>
#include"parser.h"

#define STACK_SIZE 50
#define LINEBUF_SIZE 100
#define NUM_SYMS 9
#define NUM_CHARS 128

static struct token tokens[20];
static struct symbol symbols[100];
static long handlers[NUM_SYMS]
static int cur_token;
static int num_symbols;

int pda_init(struct pda *pda, int q0, int Z, int fin)
{
	pda->st = (stack_t*) malloc(sizeof(stack_t));
	
	if(pda->st == NULL) 
		return -1;
	if(stack_init(pda->st, STACK_SIZE) < 0) 
		return -1;
	
	stack_push(pda->st, Z);
	pda->q = q0;
	pda->fin = fin;
	
	return 0;
}

static int syntax_error(){
	return -1;
}

static int add_to_token(char c)
{
	if(tokens[cur_token].chars < 15){
		tokens[cur_token].content[++chars] = c;
	}
}

static int handle_q0(struct pda *pda, char c, int stack_top)
{
	int stack_top;
	
	stack_top = ;
	
	switch(stack_top){
	case -1:
		return -1;
	case LIN:
		if(isalpha(c)){
			add_to_token(c);
			tokens[cur_token].type = 0;
			stack_push(pda->st,LIN);
			pda->q = 1;
		} else if(isspace(c)){
			stack_push(pda->st,MNE);
			tokens[cur_token].type = 1;
		}else if(c == '\n'){
			pda->q = pda->fin;
		}
		else if(c == '#'){
			stack_push(pda->st, COMM);
		}
		else{
			printf("Syntax error\n");
		}
		break;
	case MNE:
		if(isupper(c)){
			add_to_token(c);
			stack_push(pda->st, MNE);
		}else if(c == ' '){
			cur_token++;
			stack_push(pda->st, OP);
		}else{
			printf("Syntax error\n");
		}
		break;
}

static int handle_q1(struct pda *pda, char c){
	

static int check_char(struct pda *pda, char c)
{
	switch(pda->q){
	case -1:
		return -1;
	case 0:
		handle_q0(pda,c,stack_pop(pda->st));
		break;
	case 1:
		handle_q1(pda,c,stack_pop(pda->st));
		break;
	}
}

static int parse_line(struct pda *pda, char * line)
{
	char * lp = line;
	int ret;
	
	while(isprint(*lp)){
		ret = check_char(pda, *lp);
		lp++;
	}
}
	
int parseInput(FILE * fin)
{
	char * buf[LINEBUF_SIZE];
	struct pda *pda;
	
	token_head.next = &token_head;
	token_head.prev = token_head.next;
	
	symbol_head.next = &symbol_head;
	symbol_head.prev = symbol_head.next;
	
	pda = malloc(sizeof(*pda));
	if(!pda) return -1;
	
	if(pda_init(pda,0,LIN,6) < 0) return -1;
	
	while(!feof(fin)){
		fgets(buf, LINEBUF_SIZE, fin);
		parse_line(pda, buf);
	}
}
