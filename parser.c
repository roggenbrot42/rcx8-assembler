#include<stdio.h>
#include<ctype.h>
#include<string.h>
#include<stdlib.h>

#include "list.h"
#include "parser.h"
#include "auxiliary.h"

#define COMMENT_BUFLEN 63

int gettok(struct parser_context *context, FILE * f){
	static int last_char = tok_bol;
	int ident_index=0;
	context->num_value = 0;

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

	//Skip carriage return
	if(last_char == '\r'){
		last_char = fgetc(f);
	}

	//Count lines
	if(last_char == '\n'){
		last_char = fgetc(f);
		context->cur_line++;
		return tok_eol;
	}

	//Parse segment line
	if(last_char == '.'){
		last_char = fgetc(f);
		while(isalpha(last_char) && ident_index < IDENT_MAX){
			context->identifier_str[ident_index++] = (char)last_char;
			last_char = fgetc(f);
		}
		context->identifier_str[ident_index] = '\0';
		return tok_segment;
	}

	//read a string
	if(last_char == '\"'){
		context->comment_str = (char*) calloc(COMMENT_BUFLEN,sizeof(char));
		memset(context->comment_str,0,COMMENT_BUFLEN);
		//comment_str[0] = (char) last_char;
		int i=0;
		size_t len = COMMENT_BUFLEN;
		do{
			last_char = fgetc(f);
			if(last_char == EOF){
				print_error("Syntax error (line %d): unterminated string\r\n",context->cur_line);
				free(context->comment_str);
				context->comment_str = 0;
				return tok_eof;
			}
			if(isprint(last_char) || last_char == '\n'){
				context->comment_str[i] = (char) last_char;
				i++;
				if(last_char == '\n'){
					context->cur_line++;
				}
			}else{
				continue;
			}
			if(i > len-2){
				len *= 2;
				context->comment_str = (char*) realloc(context->comment_str,len);
			}
		}while(i < len-1 && last_char != '\"');
		context->comment_str[i-1] = '\0';
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
				if(isxdigit(last_char)){
					context->num_value = context->num_value << 4;
					context->num_value += isdigit(last_char) ? last_char - '0' : toupper(last_char) + 10 - 'A';
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
					context->num_value = (context->num_value << 1) + last_char - '0';
				}else{
					break;
				}
				last_char = fgetc(f);
			}
			return tok_num;
		}
		//plain decimal starting with 0 (or just "0")
		for(int i=0;isdigit(last_char) && i<NUM_DIGITS_DEC-1;i++){
			context->num_value = context->num_value*10 + last_char - '0';
			last_char = fgetc(f);
		}
		return tok_num;
	}
	if(isdigit(last_char)){
		for(int i=0;isdigit(last_char) && i<NUM_DIGITS_DEC;i++){
			context->num_value = context->num_value*10 + last_char - '0';
			last_char = fgetc(f);
		}
		return tok_num;
	}

	//read instruction, labels, ...
	if(isalpha(last_char)){
		context->identifier_str[ident_index++] = (char)last_char;

		while((isalnum(last_char=fgetc(f)) || last_char == '_')
				&& ident_index < IDENT_MAX - 1){
			context->identifier_str[ident_index++] = (char)last_char;
		}

		context->identifier_str[ident_index] = '\0';

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

int nexttok(struct parser_context * context, FILE * f){
	return context->cur_tok = gettok(context, f);
}

int cur_tok(struct parser_context * context){
	return context->cur_tok;
}

int cur_line(struct parser_context * context){
	return context->cur_line;
}

struct statement * parse_definition(struct parser_context * context, FILE * f){
	struct statement * st = 0;
	for(int i=0;i<DEF_MAX;i++){
		if(strcmp(strupr(context->identifier_str),def_str[i]) == 0){
			fprintf(stderr,"%d: Definition type %s ",context->cur_line,context->identifier_str);
			nexttok(context, f);
			st = (struct statement*) malloc(sizeof(struct statement));
			if(i == 0 && cur_tok(context) == tok_str){
				st->type = st_def_str;
				st->str = context->comment_str;
				st->size = strlen(st->str) + 1; //zero terminated
				fprintf(stderr,"%s\r\n",st->str);
				break;
			}else if(i == 1 && cur_tok(context) == tok_num){
				st->type = st_def_b;
				st->num = context->num_value & 0xFF;
				st->size = 1;
				fprintf(stderr,"%d\r\n",st->num);
				break;
			}else if(i == 2 && cur_tok(context) == tok_num){
				st->type = st_def_w;
				st->num = context->num_value & 0xFFFF;
				st->size = 2;
				fprintf(stderr,"%d\r\n",st->num);
				break;
			}else{
				free(st);
				st = 0;
				print_error("Syntax error (line %d): unkown token %c(%d)\r\n",cur_line(context),cur_tok(context),cur_tok(context));
			}
		}
	}
	return st;
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

struct statement * parse_instruction(struct parser_context * context, struct OP * opcodes, FILE * f){
	struct statement * st = 0;
	//test instruction
	for(int i=0;i<MNEM_MAX;i++){
		//todo case sensitivity!!!
		if(strcmp(strupr(context->identifier_str),mnemonic_str[i]) == 0){
			fprintf(stderr,"%d: Instruction type %s ",context->cur_line, context->identifier_str);
			st = (struct statement*) malloc(sizeof(struct statement));
			st->type = st_inst;
			st->inst.id = i;
			st->size = 1;
			break;
		}
	}
	if(!st){
		print_error("Syntax error (line %d): unknown instruction %s\r\n",cur_line(context),context->identifier_str);
		return 0;
	}
	nexttok(context, f);
	if(st){
		st->inst.p1 = 0;
		st->inst.p2 = 0;
		st->inst.address = 0;
		int num_args = 0;
		struct OP * opcode = &opcodes[st->inst.id];
		if(opcode->arg_types[0] != arg_none){
			num_args = 1;
			if(opcode->arg_types[1] != arg_none){
				num_args = 2;
			}
		}
		for(int i=0;i<num_args;i++){
			if(cur_tok(context) == tok_eol || cur_tok(context) == tok_eof){
				print_error("Syntax error (line %d): Expected argument, got nothing\r\n",cur_line(context));
				free(st);
				return 0;
			}else
			if(opcode->arg_types[i] == arg_reg && cur_tok(context) == tok_ident){
				fprintf(stderr, "%s ", context->identifier_str);
				if(sscanf(strupr(context->identifier_str), "R%hu", &st->inst.p1) != 1 || st->inst.p1 > 7){
					print_error("Syntax error (line %d): invalid register %s (R0-R7)\r\n",cur_line(context),context->identifier_str);
					free(st);
					return 0;
				}
			}else
			if(opcode->arg_types[i] == arg_dirct4 && cur_tok(context) == tok_num){
				fprintf(stderr, "%d ", context->num_value);
				if(context->num_value > 0xF){
					print_error("Syntax error (line %d): value %d does not fit in 4 bits\r\n",cur_line(context),context->num_value);
					free(st);
					return 0;
				}
				st->inst.p1 = context->num_value;
			}else 
			if(opcode->arg_types[i] == arg_dirct &&	cur_tok(context) == tok_num){
				fprintf(stderr,"%d ", context->num_value);
				if(context->num_value > 0xFF){
					print_error("Syntax error (line %d): value %d does not fit in 8 bits\r\n",cur_line(context),context->num_value);
					free(st);
					return 0;
				}
				st->inst.p2 = context->num_value;
				st->size += 1;
			}else
			if(opcode->arg_types[i] == arg_port && cur_tok(context) == tok_ident)
			{
				fprintf(stderr,"%s ",context->identifier_str);
				if(sscanf(strupr(context->identifier_str), "P%hu",&st->inst.p2) != 1 || st->inst.p2 > 255){
					print_error("Syntax error (line %d): invalid port %s (P0-P255)\r\n",cur_line(context),context->identifier_str);
					free(st);
					return 0;
				}
				st->size += 1;
			}else
			if((opcode->arg_types[i] == arg_addr) &&
				cur_tok(context) == tok_ident)
			{
				fprintf(stderr, "%s ",context->identifier_str);
				struct label * lab = label_from_identifier(context);
				st->inst.address = lab;
				st->size += 2;
			}
			else{
				print_error( "Syntax error (line %d): Unexpected token %c(%d)\r\n", cur_line(context),cur_tok(context), cur_tok(context));
				free(st);
				return 0;
			}
			nexttok(context, f);
		}
		fprintf(stderr," size:%d\r\n",st->size);
	}

	return st;
}

int parse_label(struct parser_context * context, struct segment_context * seg_context){
	fprintf(stderr, "Found label: %s address: 0x%x\r\n",context->identifier_str, seg_context->current_address);
	struct label * lab = label_from_identifier(context);
	lab->address = seg_context->current_address;
	lab->valid = 1;

	if(strcmp("main",lab->name) == 0){
		fprintf(stderr,"Main label found!\r\n");
	}

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
		struct statement * st;
		if(is_definition(context->identifier_str)){
			st = parse_definition(context, f);
			if(!st){
				return -1;
			}
			nexttok(context, f);
		}else{
			st = parse_instruction(context, context->opcodes, f);
			if(!st){
				return -1;
			}
		}
		queue_add(&st->list,&seg_context->current_segment->statements);
		seg_context->current_address += st->size;
	}else{
		print_error("Syntax error (line %d): unexpected token %x (%d)\r\n",cur_line(context),context->num_value,cur_tok(context));
		return -1;
	}
	return 0;
}

int parse_segment(struct parser_context * context, struct segment_context * seg_context, FILE * f){

	fprintf(stderr, "Parsing segment: ");
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
	fprintf(stderr,".%s segment found,base: 0x%x length: %d\r\n", context->identifier_str,new_segment->base_address,new_segment->length);
	nexttok(context, f);

	return 0;

error:
	print_error( "Syntax error: unexpected token: %d %s\r\n",cur_tok(context),context->identifier_str);
	free(new_segment);
	return -1;
}