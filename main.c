#include<stdio.h>
#include<ctype.h>
#include<string.h>
#include<stdlib.h>
#include<inttypes.h>
#include<stdarg.h>
#include<errno.h>

#include "list.h"
#include "parser.h"
#include "aux.h"

#define IDENT_MAX 255
#define COMMENT_BUFLEN 63
#define NUM_SEGMENT_TYPE 3
#define NUM_DIGITS_DEC 5
#define NUM_DIGITS_BIN 16
#define NUM_DIGITS_HEX 4

#define print_error(format,...) fprintf(stderr,"\033[31m" format "\033[39m", __VA_ARGS__)

void init_opcodes(){
	//skip nop

	int i = 1;

	for(i=1;i<MNEM_MAX;i++){
		opcodes[i].opid = i;
	}
	//jnc
	for(i=1;i<FIM;i++){
		opcodes[i].op = 0x10;
		opcodes[i].arg_types[0] = arg_addr;
	}

	//fim
	opcodes[i].op = 0x20;
	opcodes[i].arg_types[0] = arg_reg;
	opcodes[i++].arg_types[1] = arg_dirct;

	//zer
	opcodes[i].op = 0x21;
	opcodes[i++].arg_types[0] = arg_reg;

	//fin
	opcodes[i].op = 0x30;
	opcodes[i++].arg_types[0] = arg_reg;

	//ftc
	opcodes[i].op = 0x31;
	opcodes[i].arg_types[0] = arg_reg;
	opcodes[i++].arg_types[0] = arg_addr;

	//jun
	opcodes[i].op = 0x40;
	opcodes[i++].arg_types[0] = arg_addr;

	//jms
	opcodes[i].op = 0x50;
	opcodes[i++].arg_types[0] = arg_addr;

	//inc
	opcodes[i].op = 0x60;
	opcodes[i++].arg_types[0] = arg_reg;

	//ino
	opcodes[i].op = 0x61;
	opcodes[i++].arg_types[0] = arg_reg;

	//isz
	opcodes[i].op = 0x70;
	opcodes[i].arg_types[0] = arg_reg;
	opcodes[i++].arg_types[1] = arg_addr;

	//sto
	opcodes[i].op = 0x71;
	opcodes[i].arg_types[0] = arg_reg;
	opcodes[i++].arg_types[1] = arg_addr;

	//add
	opcodes[i].op = 0x80;
	opcodes[i++].arg_types[0] = arg_reg;

	//xor
	opcodes[i].op = 0x81;
	opcodes[i++].arg_types[0] = arg_reg;
	
	//sub
	opcodes[i].op = 0x90;
	opcodes[i++].arg_types[0] = arg_reg;
	
	//sti
	opcodes[i].op = 0x91;
	opcodes[i++].arg_types[0] = arg_reg;

	//ld
	opcodes[i].op = 0xA0;
	opcodes[i++].arg_types[0] = arg_reg;

	//xch
	opcodes[i].op = 0xB0;
	opcodes[i++].arg_types[0] = arg_reg;

	//bbl
	opcodes[i].op = 0xC0;
	opcodes[i++].arg_types[0] = arg_dirct;

	//ldm
	opcodes[i].op = 0xD0;
	opcodes[i++].arg_types[0] = arg_dirct;

	//wmp
	opcodes[i].op = 0xE1;
	opcodes[i++].arg_types[0] = arg_port;

	//rdr
	opcodes[i].op = 0xE2;
	opcodes[i++].arg_types[0] = arg_port;

	//draw the rest of the owl
	for(int k=0xE3;k<=0xFF;k++){
		opcodes[i++].op = k;
	}

}

enum Token {
	tok_eof=-1,
	tok_segment=-2,
	tok_label=-3,
	tok_bol=-4,
	tok_eol=-5,
	tok_num=-6,
	tok_str=-7,
	tok_ident=-8
};

char * segment_str[] = {
	"data", "comment", "input"
};

enum segment_type {
	seg_data,
	seg_comment,
	seg_input,
	seg_max
};

enum statement_type {
	st_def_str,
	st_def_w,
	st_def_b,
	st_inst
};

struct instruction{
	int id;
	uint16_t p1; // 4 Bit parameter
	uint16_t p2; // 8 Bit parameter
	struct label * address;
};

struct statement {
	int type;
	int size;
	union {
		char * str;
		struct instruction inst;
		uint16_t num;
	};
	struct list_head list;
};

struct label {
	uint16_t address;
	char * name;
	struct list_head list;	
};

struct segment {
	int type;
	uint16_t base_address;
	uint16_t length;

	char * string;
	struct queue statements;
	struct queue labels;
	
	struct list_head list;	
};

struct program {
	struct label * main_label;
	struct queue segment_list;
};


static char identifier_str[IDENT_MAX] = {0};
static char * comment_str = 0;
static int num_value = 0;
static int cur_tok = 0;
static int cur_line = 0;
static struct program prg;

static uint16_t current_address = 0;
struct segment * current_segment = 0;

int gettok(FILE * f){
	static int last_char = tok_bol;
	int ident_index=0;
	num_value = 0;

	if(last_char == tok_bol){
		last_char = fgetc(f);
	}

	//Skip any whitespaces
	while(isblank(last_char)){
		last_char = fgetc(f);
	}

	//Skip any comments until the line end
	if(last_char == '#'){
		do
		{
			last_char = fgetc(f);
		} while (last_char != EOF && last_char != '\n' && last_char != '\r');
	}

	if((last_char == '\n' || last_char == '\r') && last_char != EOF){
		do{
			last_char = fgetc(f);
		}while((last_char == '\n' || last_char == '\r') && last_char != EOF);
		cur_line++;
		return tok_eol;
	}

	//Parse segment line
	if(last_char == '.'){
		last_char = fgetc(f);
		while(isalpha(last_char) && ident_index < IDENT_MAX){
			identifier_str[ident_index++] = (char)last_char;
			last_char = fgetc(f);
		}
		identifier_str[ident_index] = '\0';
		return tok_segment;
	}

	//read a string
	if(last_char == '\"'){
		comment_str = (char*) calloc(COMMENT_BUFLEN,sizeof(char));
		memset(comment_str,0,COMMENT_BUFLEN);
		//comment_str[0] = (char) last_char;
		int i=0;
		size_t len = COMMENT_BUFLEN;
		do{
			last_char = fgetc(f);
			if(isprint(last_char)){
				comment_str[i] = (char) last_char;
				i++;
			}else{
				continue;
			}
			if(i > len-2){
				len *= 2;
				comment_str = (char*) realloc(comment_str,len);
			}
		}while(i < len-1 && last_char != '\"');
		comment_str[i-1] = '\0';
		last_char = ' ';
		return tok_str;
	}

	//parse number
	if(last_char == '0'){
		last_char = fgetc(f);
		//hexadecimal
		if(last_char == 'x'){
			last_char=fgetc(f);
			for(int i=0;i<NUM_DIGITS_HEX;i++){
				if(isdigit(last_char)){
					num_value = num_value << 4;
					num_value += last_char - '0';
				}else if (isalpha(last_char))
				{
					num_value = num_value << 4;
					num_value += last_char + 10 - 'A'; //better check case
				}else{
					break;
				}
				last_char = fgetc(f);
			}
			return tok_num;
		}
		else if (last_char == 'b'){
			last_char = fgetc(f);
			for(int i=0;i<NUM_DIGITS_BIN;i++){
				if(last_char=='1'||last_char=='0'){
					num_value = (num_value << 1) + last_char - '0';
				}else{
					break;
				}
				last_char = fgetc(f);
			}
			return tok_num;
		}
	}
	if(isdigit(last_char)){
		for(int i=0;isdigit(last_char) && i<NUM_DIGITS_DEC;i++){
			num_value = num_value*10 + last_char - '0';
			last_char = fgetc(f);
		}
		return tok_num;
	}

	//read instruction, labels, ...
	if(isalpha(last_char)){
		identifier_str[ident_index++] = (char)last_char;

		while(isalnum((last_char=fgetc(f))) //lol abfuck, fixme
				&& ident_index < IDENT_MAX){
			identifier_str[ident_index++] = (char)last_char;
		}

		identifier_str[ident_index] = '\0';

		if(last_char == ':'){
			last_char = fgetc(f);
			return tok_label;
		}else{
			return tok_ident;
		}
	}

	if(last_char == EOF)
		return tok_eof;

	int tmp = last_char;
	last_char = fgetc(f);
	return tmp;
}

static int nexttok(FILE * f){
	return cur_tok = gettok(f);
}

int write_segment(FILE * f, FILE * of){
	if(current_segment != 0){
		struct list_head * pos = 0;
		struct statement * stmt = 0;

		list_for_each(pos, &current_segment->statements.list){
			stmt = list_entry(pos,struct statement,list);

			if(stmt->type == st_def_str){
				fwrite(stmt->str,sizeof(char),stmt->size,of);
			}else
			if(stmt->type == st_def_b){
				fwrite(&stmt->num,sizeof(char),1,of);
			}else
			if(stmt->type == st_def_w){
				fwrite(&stmt->num,sizeof(uint16_t),1,of);
			}else
			if(stmt->type == st_inst){
				struct OP inst = opcodes[stmt->inst.id];
				uint8_t bin = 0;
				bin = inst.op;

				if(inst.arg_types[0] == arg_reg || inst.arg_types[0] == arg_dirct){
					bin = bin | ((stmt->inst.p1 & 0xF) << 1);
				}
				fwrite(&bin,sizeof(bin),1,of);

				if(inst.arg_types[0] == arg_addr){
					if(stmt->inst.address != 0){
						fwrite(&stmt->inst.address->address,sizeof(uint16_t),1,of);
					}else{
						print_error("Error: %s","Label undefined\n");
					}
				}else
				if(inst.arg_types[1] == arg_addr){
					fwrite(&stmt->inst.p2,sizeof(uint16_t),1,of);
				}else
				if(inst.arg_types[1] == arg_dirct || inst.arg_types[1] == arg_port){
					fwrite(&stmt->inst.p2,sizeof(uint8_t),1,of);
				}
			}
		}
	}
}

int handle_segment(FILE * f, FILE * of){

	write_segment(f,of);

	fprintf(stderr, "Parsing segment: ");
	struct segment * new_segment = (struct segment *) malloc(sizeof(struct segment));
	new_segment->length = 0;
	int type = 0;

	new_segment->type = -1;
	for(int i=0;i<seg_max;i++){
		if(strncmp(identifier_str,segment_str[i],IDENT_MAX) == 0){
			new_segment->type = i;
			break;
		}
	}

	uint16_t binary_type = 0x1000;

	switch (new_segment->type)
	{
	case seg_comment:
		fwrite(&binary_type,sizeof(uint16_t),1,of);

		new_segment->base_address = -1; //comments do not exist in application memory
		nexttok(f);
		if(cur_tok == tok_eol){
			nexttok(f);
		}
		if(cur_tok == tok_str){
			new_segment->string = comment_str;
			comment_str = 0;
			new_segment->length = strlen(new_segment->string);

			fwrite(&new_segment->length,sizeof(uint16_t),1,of);
			fwrite(new_segment->string,sizeof(char),new_segment->length,of);
		}else{
			goto error;
		}
		break;
	case seg_data:
		binary_type = 0x2000;
		fwrite(&binary_type,sizeof(uint16_t),1,of);

		if(nexttok(f) == tok_num){
			new_segment->base_address = num_value;
			current_address = new_segment->base_address;
			fwrite(&current_address,sizeof(uint16_t),1,of);
			if(nexttok(f) == tok_eol){
				break;
			}else{
				goto error;
			}
		}else{
			goto error;
		}
	case seg_input:
		break;	
	default:
		goto error;
	}

	queue_add(&new_segment->list,&prg.segment_list);
	current_segment = new_segment;
	INIT_QUEUE(&current_segment->labels);
	INIT_QUEUE(&current_segment->statements);
	fprintf(stderr,".%s segment found,base: 0x%x length: %d\r\n", identifier_str,new_segment->base_address,new_segment->length);
	nexttok(f);

	return 0;

error:
	print_error( "Syntax error: unexpected token: %d %s\r\n",cur_tok,identifier_str);
	free(new_segment);
	return -1;
}

int parse_label(){
	fprintf(stderr, "Found label: %s address: 0x%x\r\n",identifier_str,current_address);
	struct label * lab = (struct label *) malloc(sizeof(struct label));
	lab->address = current_address;

	int len = strlen(identifier_str);
	lab->name = (char *) calloc(len, sizeof(char));
	memcpy(lab->name,identifier_str,len);

	if(strncmp("main",lab->name,len) == 0){
		prg.main_label = lab;
		fprintf(stderr,"Main label found!\r\n");
	}

	//Search for duplicates
	struct list_head * pos;
	struct label * tmp;
	int found = 0;
	list_for_each(pos,&current_segment->labels.list){
		tmp = list_entry(pos,struct label,list);
		
		if(strncmp(tmp->name,lab->name,len) == 0){
			found = 1;
		}
	}
	if(found == 0){
		queue_add(&lab->list,&current_segment->labels);
	}else{
		fprintf(stderr,"Error: Label already defined: %s\n",lab->name);
		return -1;
	}

	return 0;
}

struct statement * parse_definition(FILE * f, FILE * of){
	struct statement * st = 0;
	for(int i=0;i<DEF_MAX;i++){
		if(strncmp(strupr(identifier_str),def_str[i],2) == 0){
			fprintf(stderr,"Definition type %s ",identifier_str);
			nexttok(f);
			st = (struct statement*) malloc(sizeof(struct statement));
			if(i == 0 && cur_tok == tok_str){
				st->type = st_def_str;
				st->str = comment_str;
				st->size = strlen(st->str);
				fprintf(stderr,"%s\r\n",st->str);
				break;
			}else if(i == 1 && cur_tok == tok_num){
				st->type = st_def_b;
				st->num = num_value & 0xFF;
				st->size = 1;
				fprintf(stderr,"%d\r\n",st->num);
				break;
			}else if(i == 2 && cur_tok == tok_num){
				st->type = st_def_w;
				st->num = num_value & 0xFFFF;
				st->size = 2;
				fprintf(stderr,"%d\r\n",st->num);
				break;
			}else{
				free(st);
				st = 0;
				print_error("Syntax error (line %d): unkown token %c(%d)\r\n",cur_line,cur_tok,cur_tok);
			}
		}
	}
	return st;
}

struct statement * parse_instruction(FILE * f, FILE * of){
	struct statement * st = 0;
	//test instruction
	for(int i=0;i<MNEM_MAX;i++){
		//todo case sensitivity!!!
		if(strncmp(strupr(identifier_str),mnemonic_str[i],3) == 0){
			fprintf(stderr,"Instruction type %s ",identifier_str);
			st = (struct statement*) malloc(sizeof(struct statement));
			st->type = st_inst;
			st->inst.id = i;
			st->size = 1;
			break;
		}
	}
	nexttok(f);
	if(st){
		int num_args = 0;
		struct OP * opcode = &opcodes[st->inst.id];
		if(opcode->arg_types[0] != arg_none){
			num_args = 1;
			if(opcode->arg_types[1] != arg_none){
				num_args = 2;
			}
		}
		for(int i=0;i<num_args;i++){
			if(cur_tok == tok_eol || cur_tok == tok_eof){
				print_error("Syntax error (line %d): Expected argument, got nothing\r\n",cur_line);
				break;
			}else 
			if(opcode->arg_types[i] == arg_dirct &&	cur_tok == tok_num){
				fprintf(stderr,"%d", num_value);
				st->num = num_value;
				st->size += 1;
			}else
			if((opcode->arg_types[i] == arg_reg ||
				opcode->arg_types[i] == arg_port) &&
				cur_tok == tok_ident)
			{
				fprintf(stderr,"%s ",identifier_str);
				st->size += 2;
			}else
			if((opcode->arg_types[i] == arg_addr) &&
				cur_tok == tok_ident)
			{
				fprintf(stderr, "%s",identifier_str);
				st->size += 3;
			}
			else{
				print_error( "Syntax error (line %d): Unexpected token %c(%d)\r\n", cur_line,cur_tok, cur_tok);
			}
			nexttok(f);
		}
		fprintf(stderr,"\r\n");
	}

	return st;
}

int handle_statement(FILE * f, FILE * of){
	if(cur_tok == tok_label){ //we read a label definition
		parse_label();
		do{
			nexttok(f);
		}while(cur_tok == tok_eol);
		handle_statement(f,of);
	}else if(cur_tok == tok_ident){
		struct statement * st;
		//test definition
		st = parse_definition(f,of);			
		if(st){
			queue_add(&st->list,&current_segment->statements);
			current_address += st->size;
			nexttok(f);
			return 0;
		}else{
			st = parse_instruction(f,of);
			if(st){
				queue_add(&st->list,&current_segment->statements);
				current_address += st->size;
			}
		}
	}else{
		print_error("Syntax error (line %d): Unexpected token %c(%d)\r\n",cur_line,cur_tok,cur_tok);
	}
	return 0;

error:
	print_error( "Syntax error (line %d): unexpected token: %d %s\r\n",cur_line,cur_tok,identifier_str);
	return -1;
}

int main(int argc, char *argv[])
{	
	if(argc < 2){
		printf("--------------"
		"\nThis is RCx8Asm by Nicolas Appel (c) 2012--2022"
		"\nUsage: rcx8asm inputfile [outputfile]\n");

		return 0;
	}
	char * input_name = argv[1];
	FILE * input_file = fopen(input_name,"r");
	if(!input_file)
	{
		printf("File could not be opened. Aborting...\n");
		return errno;
	}

	init_opcodes();
	cur_line = 0;
	current_address = 0;
	current_segment = 0;

	INIT_QUEUE(&prg.segment_list);
	prg.main_label = 0;

	FILE * output_file = fopen("out.prg","w");
	if(!output_file){
		printf("Could not open output file. Aborting...\n");
		return errno;
	}

	nexttok(input_file);
	
	while(cur_tok != tok_eof){
		switch(cur_tok){
			case tok_eof:
				break;
			case tok_eol: //eat empty lines
				nexttok(input_file);
				break;
			case tok_segment:
				handle_segment(input_file,output_file);
				break;
			default:
				handle_statement(input_file,output_file);
				break;
		}
	}

	fclose(input_file);
	fclose(output_file);
	
	return 0;
}
