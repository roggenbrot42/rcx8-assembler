#include<stdio.h>
#include<ctype.h>
#include<string.h>
#include<stdlib.h>
#include<strings.h>

#include "list.h"
#include "parser.h"
#include "auxiliary.h"

#define COMMENT_BUFLEN 63

int gettok(struct parser_context *context, FILE * f){
	int ident_index=0;
	context->num_value = 0;

	//the line counter advances when the first token behind a line end is read,
	//so errors reported at the end of a line still show that line
	if(context->cur_tok == tok_eol){
		context->cur_line++;
	}

	if(context->last_char == tok_bol){
		context->last_char = fgetc(f);
	}

	//Skip any whitespaces
	while(isblank(context->last_char)){
		context->last_char = fgetc(f);
	}

	//Skip any comments until the line end
	if(context->last_char == '#'){
		do
		{
			context->last_char = fgetc(f);
		} while (context->last_char != EOF && context->last_char != '\n' && context->last_char != '\r');
	}

	//Skip carriage return
	if(context->last_char == '\r'){
		context->last_char = fgetc(f);
	}

	//Count lines
	if(context->last_char == '\n'){
		context->last_char = fgetc(f);
		return tok_eol;
	}

	//Parse segment line
	if(context->last_char == '.'){
		context->last_char = fgetc(f);
		while(isalpha(context->last_char) && ident_index < IDENT_MAX){
			context->identifier_str[ident_index++] = (char)context->last_char;
			context->last_char = fgetc(f);
		}
		context->identifier_str[ident_index] = '\0';
		return tok_segment;
	}

	//read a string
	if(context->last_char == '\"'){
		context->comment_str = (char*) calloc(COMMENT_BUFLEN,sizeof(char));
		memset(context->comment_str,0,COMMENT_BUFLEN);
		//comment_str[0] = (char) context->last_char;
		int i=0;
		size_t len = COMMENT_BUFLEN;
		do{
			context->last_char = fgetc(f);
			if(context->last_char == EOF){
				print_error("Syntax error (line %d): unterminated string\r\n",context->cur_line);
				free(context->comment_str);
				context->comment_str = 0;
				return tok_eof;
			}
			if(isprint(context->last_char) || context->last_char == '\n' || context->last_char == '\t'){
				context->comment_str[i] = (char) context->last_char;
				i++;
				if(context->last_char == '\n'){
					context->cur_line++;
				}
			}else{
				continue;
			}
			if(i > len-2){
				len *= 2;
				context->comment_str = (char*) realloc(context->comment_str,len);
			}
		}while(i < len-1 && context->last_char != '\"');
		context->comment_str[i-1] = '\0';
		context->last_char = ' ';
		return tok_str;
	}

	//parse number
	if(context->last_char == '0'){
		context->last_char = fgetc(f);
		//hexadecimal
		if(context->last_char == 'x'){
			context->last_char=fgetc(f);
			for(int i=0;i<NUM_DIGITS_HEX;i++){
				if(isxdigit(context->last_char)){
					context->num_value = context->num_value << 4;
					context->num_value += isdigit(context->last_char) ? context->last_char - '0' : toupper(context->last_char) + 10 - 'A';
				}else{
					break;
				}
				context->last_char = fgetc(f);
			}
			return tok_num;
		}
		else if (context->last_char == 'b'){
			context->last_char = fgetc(f);
			for(int i=0;i<NUM_DIGITS_BIN;i++){
				if(context->last_char=='1'||context->last_char=='0'){
					context->num_value = (context->num_value << 1) + context->last_char - '0';
				}else{
					break;
				}
				context->last_char = fgetc(f);
			}
			return tok_num;
		}
		//plain decimal starting with 0 (or just "0")
		for(int i=0;isdigit(context->last_char) && i<NUM_DIGITS_DEC-1;i++){
			context->num_value = context->num_value*10 + context->last_char - '0';
			context->last_char = fgetc(f);
		}
		return tok_num;
	}
	if(isdigit(context->last_char)){
		for(int i=0;isdigit(context->last_char) && i<NUM_DIGITS_DEC;i++){
			context->num_value = context->num_value*10 + context->last_char - '0';
			context->last_char = fgetc(f);
		}
		return tok_num;
	}

	//read instruction, labels, ...
	if(isalpha(context->last_char)){
		context->identifier_str[ident_index++] = (char)context->last_char;

		while((isalnum(context->last_char=fgetc(f)) || context->last_char == '_')
				&& ident_index < IDENT_MAX - 1){
			context->identifier_str[ident_index++] = (char)context->last_char;
		}

		context->identifier_str[ident_index] = '\0';

		if(context->last_char == ':'){
			context->last_char = fgetc(f);
			return tok_label;
		}else{
			return tok_ident;
		}
	}

	if(context->last_char == EOF)
		return tok_eof;

	int tmp = context->last_char;
	context->last_char = fgetc(f);
	return tmp;
}

int nexttok(struct parser_context * context, FILE * f){
	return context->cur_tok = gettok(context, f);
}

int cur_tok(struct parser_context * context){
	return context->cur_tok;
}

int cur_line(struct parser_context * context){
	return context->cur_line;
}

static struct expr * new_expr(struct parser_context * context){
	struct expr * e = (struct expr *) calloc(1, sizeof(struct expr));
	e->line = cur_line(context);
	return e;
}

//Parses a sum of numbers and at most one label. The first label may have been consumed already.
static int parse_sum(struct parser_context * context, FILE * f, struct expr * e, int have_label){
	int sign = 1;
	int need_item = !have_label;

	for(;;){
		if(need_item){
			while(cur_tok(context) == '+' || cur_tok(context) == '-'){ //unary signs
				if(cur_tok(context) == '-'){
					sign = -sign;
				}
				nexttok(context, f);
			}
			if(cur_tok(context) == tok_num){
				e->value += sign * context->num_value;
				nexttok(context, f);
			}else
			if(cur_tok(context) == tok_ident){
				if(e->name != 0 || sign < 0){
					print_error("Syntax error (line %d): only one positive label allowed in an expression\r\n",cur_line(context));
					return -1;
				}
				e->name = strdup(context->identifier_str);
				nexttok(context, f);
			}else{
				print_error("Syntax error (line %d): expected a number or label, got %c(%d)\r\n",cur_line(context),cur_tok(context),cur_tok(context));
				return -1;
			}
			need_item = 0;
			sign = 1;
		}else
		if(cur_tok(context) == '+' || cur_tok(context) == '-'){
			sign = (cur_tok(context) == '-') ? -1 : 1;
			nexttok(context, f);
			need_item = 1;
		}else{
			break;
		}
	}

	return 0;
}

// expr := [ 'lo' | 'hi' ] '(' sum ')' | sum
struct expr * parse_expr(struct parser_context * context, FILE * f){
	struct expr * e = new_expr(context);

	if(cur_tok(context) == tok_ident &&
		(strcasecmp(context->identifier_str, "lo") == 0 || strcasecmp(context->identifier_str, "hi") == 0)){
		char saved[IDENT_MAX];
		strcpy(saved, context->identifier_str);
		nexttok(context, f);
		if(cur_tok(context) == '('){
			e->func = (strcasecmp(saved, "lo") == 0) ? expr_lo : expr_hi;
			nexttok(context, f);
			if(parse_sum(context, f, e, 0) != 0){
				return 0;
			}
			if(cur_tok(context) != ')'){
				print_error("Syntax error (line %d): expected ')'\r\n",cur_line(context));
				return 0;
			}
			nexttok(context, f);
			return e;
		}
		//just a label that happens to be called lo or hi
		e->name = strdup(saved);
		return parse_sum(context, f, e, 1) == 0 ? e : 0;
	}

	return parse_sum(context, f, e, 0) == 0 ? e : 0;
}

static struct statement * new_statement(struct parser_context * context, int type, int size){
	struct statement * st = (struct statement *) calloc(1, sizeof(struct statement));
	st->type = type;
	st->size = size;
	st->line = cur_line(context);
	return st;
}

static void add_statement(struct segment_context * seg_context, struct statement * st){
	queue_add(&st->list,&seg_context->current_segment->statements);
	seg_context->current_address += st->size;
}

//ds "text"[, ...] zero terminated, db number|"text"[, ...], dw number[, ...]
int parse_definition(struct parser_context * context, struct segment_context * seg_context, FILE * f){
	int kind = -1;
	for(int i=0;i<DEF_MAX;i++){
		if(strcmp(strupr(context->identifier_str),def_str[i]) == 0){
			kind = i;
		}
	}
	if(kind < 0){
		return -1;
	}

	do{
		struct statement * st;
		nexttok(context, f); //skip mnemonic or comma

		if(cur_tok(context) == tok_str){
			if(kind == 2){ //dw
				print_error("Syntax error (line %d): strings are not allowed in dw\r\n",cur_line(context));
				return -1;
			}
			st = new_statement(context, st_def_str, 0);
			st->str = context->comment_str;
			context->comment_str = 0;
			st->size = strlen(st->str) + (kind == 0 ? 1 : 0); //only ds is zero terminated
			nexttok(context, f);
		}else{
			if(kind == 0){
				print_error("Syntax error (line %d): ds expects a string\r\n",cur_line(context));
				return -1;
			}
			st = new_statement(context, kind == 1 ? st_def_b : st_def_w, kind == 1 ? 1 : 2);
			st->expr = parse_expr(context, f);
			if(st->expr == 0){
				free(st);
				return -1;
			}
		}
		add_statement(seg_context, st);
	}while(cur_tok(context) == ',');

	return 0;
}

struct label * label_from_identifier(struct parser_context * context){
	struct label * lab = (struct label *) malloc(sizeof(struct label));
	lab->address = 0;
	lab->valid = 0; //invalid address

	int len = strlen(context->identifier_str);
	lab->name = (char *) calloc(len + 1, sizeof(char));
	memcpy(lab->name,context->identifier_str,len);

	return lab;
}

static int parse_number_or_expr(struct parser_context * context, FILE * f, struct instruction * inst){
	inst->expr = parse_expr(context, f);
	return inst->expr == 0 ? -1 : 0;
}

struct statement * parse_instruction(struct parser_context * context, struct OP * opcodes, FILE * f){
	struct statement * st = 0;
	//test instruction
	for(int i=0;i<MNEM_MAX;i++){
		if(strcmp(strupr(context->identifier_str),mnemonic_str[i]) == 0){
			st = new_statement(context, st_inst, 1);
			st->inst.id = i;
			break;
		}
	}
	if(!st){
		print_error("Syntax error (line %d): unknown instruction %s\r\n",cur_line(context),context->identifier_str);
		return 0;
	}
	nexttok(context, f);

	int num_args = 0;
	struct OP * opcode = &opcodes[st->inst.id];
	if(opcode->arg_types[0] != arg_none){
		num_args = 1;
		if(opcode->arg_types[1] != arg_none){
			num_args = 2;
		}
	}
	for(int i=0;i<num_args;i++){
		int type = opcode->arg_types[i];

		if(cur_tok(context) == tok_eol || cur_tok(context) == tok_eof){
			print_error("Syntax error (line %d): Expected argument, got nothing\r\n",cur_line(context));
			free(st);
			return 0;
		}

		if(type == arg_reg){
			unsigned int reg;
			int n = 0;
			if(cur_tok(context) != tok_ident ||
				sscanf(strupr(context->identifier_str), "R%u%n", &reg, &n) != 1 ||
				context->identifier_str[n] != '\0' || reg > 7){
				print_error("Syntax error (line %d): expected a register R0-R7\r\n",cur_line(context));
				free(st);
				return 0;
			}
			st->inst.p1 = reg;
			nexttok(context, f);
		}else{
			//port numbers may also be written as P5
			unsigned int port;
			int n = 0;
			if(type == arg_port && cur_tok(context) == tok_ident &&
				sscanf(strupr(context->identifier_str), "P%u%n", &port, &n) == 1 &&
				context->identifier_str[n] == '\0'){
				st->inst.expr = new_expr(context);
				st->inst.expr->value = port;
				nexttok(context, f);
			}else
			if(parse_number_or_expr(context, f, &st->inst) != 0){
				free(st);
				return 0;
			}
			if(type == arg_dirct || type == arg_port){
				st->size += 1;
			}else
			if(type == arg_addr){
				st->size += 2;
			}
		}
	}

	return st;
}

int parse_label(struct parser_context * context, struct segment_context * seg_context){
	struct label * lab = label_from_identifier(context);
	lab->address = seg_context->current_address;
	lab->valid = 1;

	//Search for duplicates
	struct list_head * pos;
	struct label * tmp;
	list_for_each(pos,&seg_context->current_segment->labels.list){
		tmp = list_entry(pos,struct label,list);

		if(strcmp(tmp->name,lab->name) == 0){
			print_error("Error (line %d): Label already defined: %s\n",cur_line(context),lab->name);
			free(lab->name);
			free(lab);
			return -1;
		}
	}
	queue_add(&lab->list,&seg_context->current_segment->labels);

	return 0;
}

static int is_definition(const char * ident){
	for(int i=0;i<DEF_MAX;i++){
		if(strcmp(strupr((char*)ident),def_str[i]) == 0){
			return 1;
		}
	}
	return 0;
}

int parse_statement(struct parser_context * context, struct segment_context * seg_context, FILE * f){
	if(seg_context->current_segment == 0){
		print_error("Syntax error (line %d): statement outside of a segment\r\n",cur_line(context));
		return -1;
	}

	if(cur_tok(context) == tok_label){ //we read a label definition
		if(parse_label(context,seg_context) != 0){
			return -1;
		}
		do{
			nexttok(context, f);
		}while(cur_tok(context) == tok_eol);//eat empty lines
		if(cur_tok(context) == tok_segment || cur_tok(context) == tok_eof){
			return 0; //label at the end of a segment
		}
		return parse_statement(context, seg_context, f);
	}else if(cur_tok(context) == tok_ident){
		if(is_definition(context->identifier_str)){
			return parse_definition(context, seg_context, f);
		}
		struct statement * st = parse_instruction(context, context->opcodes, f);
		if(!st){
			return -1;
		}
		add_statement(seg_context, st);
	}else{
		print_error("Syntax error (line %d): unexpected token %x (%d)\r\n",cur_line(context),context->num_value,cur_tok(context));
		return -1;
	}
	return 0;
}

int parse_segment(struct parser_context * context, struct segment_context * seg_context, FILE * f){

	struct segment * new_segment = (struct segment *) malloc(sizeof(struct segment));
	new_segment->length = 0;
	int type = 0;

	new_segment->type = -1;
	for(int i=0;i<seg_max;i++){ //identify segment type from identifier string
		if(strncmp(context->identifier_str,segment_str[i],IDENT_MAX) == 0){
			new_segment->type = i;
			break;
		}
	}

	switch (new_segment->type)
	{
	case seg_comment:
		new_segment->base_address = -1; //comments do not exist in application memory
		nexttok(context, f);
		if(cur_tok(context) == tok_eol){ //skip eol
			nexttok(context, f);
		}
		if(cur_tok(context) == tok_str){
			new_segment->string = context->comment_str;
			context->comment_str = 0;
			new_segment->length = strlen(new_segment->string);

		}else{
			goto error;
		}
		break;
	case seg_data:
		if(nexttok(context, f) == tok_num){
			new_segment->base_address = context->num_value;
			seg_context->current_address = new_segment->base_address;
			if(nexttok(context, f) == tok_eol){
				break;
			}else{
				goto error;
			}
		}else{
			goto error;
		}

	case seg_input:
		break;	//todo
	default:
		goto error;
	}

	// queue_add(&new_segment->list,&prg->segment_list);
	seg_context->current_segment = new_segment;
	INIT_QUEUE(&seg_context->current_segment->labels);
	INIT_QUEUE(&seg_context->current_segment->statements);
	nexttok(context, f);

	return 0;

error:
	print_error( "Syntax error: unexpected token: %d %s\r\n",cur_tok(context),context->identifier_str);
	free(new_segment);
	return -1;
}