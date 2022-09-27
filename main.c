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

#define print_error(format,...) fprintf(stderr,"\033[31m" format "\033[39m", __VA_ARGS__)

struct program {
	struct label * main_label;
	struct queue segment_list;
};

void init_opcodes(struct OP * opcodes){

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
	opcodes[i++].arg_types[0] = arg_dirct4;

	//ldm
	opcodes[i].op = 0xD0;
	opcodes[i++].arg_types[0] = arg_dirct4;

	//wmp
	opcodes[i].op = 0xE1;
	opcodes[i++].arg_types[0] = arg_port;

	//rdr
	opcodes[i].op = 0xE2;
	opcodes[i++].arg_types[0] = arg_port;

	//kbi
	opcodes[i++].op = 0xFC;

	//asc
	opcodes[i++].op = 0xFD;

	//CLB to DAC
	for(int k=0xF0;k<=0xF8;k++){
		opcodes[i++].op = k;
	}

	//stc
	opcodes[i++].op = 0xFA;

	//daa
	opcodes[i++].op = 0xFB;
}


int write_data_segment(struct parser_context * context, struct segment * current_segment, char * buffer){
	if(current_segment != 0){
		struct list_head * pos = 0;
		struct statement * stmt = 0;
		int size = 0;

		list_for_each(pos, &current_segment->statements.list){
			stmt = list_entry(pos,struct statement,list);

			if(stmt->type == st_def_str){
				memcpy(buffer, stmt->str,sizeof(char)*stmt->size);
				buffer +=stmt->size;
			}else
			if(stmt->type == st_def_b){
				memcpy(buffer, &stmt->num,sizeof(char));
				buffer += 1;
			}else
			if(stmt->type == st_def_w){
				memcpy(buffer, &stmt->num,sizeof(uint16_t));
				buffer += 2;
			}else
			if(stmt->type == st_inst){
				struct OP inst = context->opcodes[stmt->inst.id];
				uint8_t bin = 0;
				bin = inst.op;

				if(inst.arg_types[0] == arg_reg || inst.arg_types[0] == arg_dirct4){
					bin = bin | ((stmt->inst.p1 & 0xF) << 1);
				}
				memcpy(buffer, &bin,sizeof(bin));
				buffer += sizeof(bin);

				if(inst.arg_types[0] == arg_addr){
					if(stmt->inst.address != 0){
						memcpy(buffer, &stmt->inst.address->address,sizeof(uint16_t));
						buffer += 2;
					}else{
						print_error("Error: %s","Label undefined\n");
					}
				}else
				if(inst.arg_types[1] == arg_addr){
					memcpy(buffer, &stmt->inst.p2,sizeof(uint16_t));
					buffer += 2;
				}else
				if(inst.arg_types[1] == arg_dirct || inst.arg_types[1] == arg_port){
					memcpy(buffer, &stmt->inst.p2,sizeof(uint8_t));
					buffer += 1;
				}
			}
		}
	}
}

int handle_segment(struct parser_context * context, struct segment_context * seg_context, struct program * prg, FILE * f, FILE * of){

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

	uint16_t binary_type = 0x1000;

	switch (new_segment->type)
	{
	case seg_comment:
		fwrite(&binary_type,sizeof(uint16_t),1,of);

		new_segment->base_address = -1; //comments do not exist in application memory
		nexttok(context, f);
		if(cur_tok(context) == tok_eol){ //skip eol
			nexttok(context, f);
		}
		if(cur_tok(context) == tok_str){
			new_segment->string = context->comment_str;
			context->comment_str = 0;
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

		char buffer[UINT16_MAX];
		uint16_t size = 0;
		size = write_data_segment(context, seg_context->current_segment, buffer); // TODO this is bullshit, we can't know that yet

		if(nexttok(context, f) == tok_num){
			new_segment->base_address = context->num_value;
			seg_context->current_address = new_segment->base_address;
			fwrite(&(seg_context->current_address),sizeof(uint16_t),1,of);
			if(nexttok(context, f) == tok_eol){
				break;
			}else{
				goto error;
			}
		}else{
			goto error;
		}

		fwrite(&size, sizeof(uint16_t),1, of);
		fwrite(buffer, sizeof(uint8_t), size, of);

	case seg_input:
		break;	//todo
	default:
		goto error;
	}

	queue_add(&new_segment->list,&prg->segment_list);
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

int parse_label(struct parser_context * context, struct segment_context * seg_context, struct program * prg){
	fprintf(stderr, "Found label: %s address: 0x%x\r\n",context->identifier_str, seg_context->current_address);
	struct label * lab = label_from_identifier(context);
	lab->address = seg_context->current_address;
	lab->valid = 1;

	int len = strlen(context->identifier_str);

	if(strncmp("main",lab->name,len) == 0){
		prg->main_label = lab;
		fprintf(stderr,"Main label found!\r\n");
	}

	//Search for duplicates
	struct list_head * pos;
	struct label * tmp;
	int found = 0;
	list_for_each(pos,&seg_context->current_segment->labels.list){
		tmp = list_entry(pos,struct label,list);
		
		if(strncmp(tmp->name,lab->name,len) == 0){
			found = 1;
		}
	}
	if(found == 0){
		queue_add(&lab->list,&seg_context->current_segment->labels);
	}else
	if(tmp->valid == 0){ //address not yet known? 
		tmp->address = lab->address;
		tmp->valid = 1;
	}else{
		fprintf(stderr,"Error: Label already defined: %s\n",lab->name);
		return -1;
	}

	return 0;
}

int handle_statement(struct parser_context * context, struct segment_context * seg_context, struct program * prg, FILE * f, FILE * of){
	if(cur_tok(context) == tok_label){ //we read a label definition
		parse_label(context,seg_context, prg);
		do{
			nexttok(context, f);
			context->cur_line++;
		}while(cur_tok(context) == tok_eol);//eat empty lines
		handle_statement(context, seg_context, prg, f,of);
	}else if(cur_tok(context) == tok_ident){
		struct statement * st;
		//test definition
		st = parse_definition(context, f);			
		if(st){
			queue_add(&st->list,&seg_context->current_segment->statements);
			seg_context->current_address += st->size;
			nexttok(context, f);
			return 0;
		}else{
			st = parse_instruction(context, context->opcodes, f);
			if(st){
				queue_add(&st->list,&seg_context->current_segment->statements);
				seg_context->current_address += st->size;
			}
		}
	}else{
		print_error("1-Syntax error (line %d): unexpected token %x (%d)\r\n",cur_line(context),context->num_value,cur_tok(context));
		return -1;
	}
	return 0;

error:
	print_error( "2-Syntax error (line %d): unexpected token: %d %s\r\n",cur_line(context),cur_tok(context),context->identifier_str);
	return -1;
}

struct label * get_label(struct queue labels, char * name){
	struct list_head * lab_pos;
	struct label * tmp_lab;

	list_for_each(lab_pos, &labels.list){
		tmp_lab = list_entry(lab_pos, struct label, list);
		if(strcmp(name,tmp_lab->name) == 0){ //todo strncmp
			return tmp_lab;
		}
	}

	return 0;
}

int resolve_label_addresses(struct segment_context * seg_context){
	struct list_head * seg_pos;
	struct statement * tmp_st;
	struct label * tmp_lab;

	if(seg_context->current_segment == 0 || seg_context->current_segment->type != seg_data){
		return 0;
	}

	list_for_each(seg_pos, &seg_context->current_segment->statements.list){ //for every statement in the segment
		tmp_st = list_entry(seg_pos,struct statement,list);

		if(tmp_st->type == st_inst && tmp_st->inst.address != 0 && tmp_st->inst.address->valid == 0){//skip if address is known
			tmp_lab = get_label(seg_context->current_segment->labels, tmp_st->inst.address->name);
			if(tmp_lab == 0){
				print_error("Label not found: %s", tmp_st->inst.address->name);
				return -1;
			}
			tmp_st->inst.address = tmp_lab;
		}
	}

	printf("------------\r\nLabels in this segment:\r\n", NULL);
	struct list_head * label_pos;
	list_for_each(label_pos, &seg_context->current_segment->labels.list){
		tmp_lab = list_entry(label_pos, struct label, list);
		printf("Label Name: %s Address: 0x%x\r\n", tmp_lab->name, tmp_lab->address);
	}

	printf("------------\r\nStatements in this segment:\r\n",NULL);
	struct list_head * st_pos;
	list_for_each(st_pos, &seg_context->current_segment->statements.list){
		tmp_st = list_entry(st_pos, struct statement, list);
		switch(tmp_st->type){
			case st_def_b:
				printf("DB %d", tmp_st->num);
				break;
			case st_def_w:
				printf("DW %d", tmp_st->num);
				break;
			case st_def_str:
				printf("DS %s", tmp_st->str);
				break;
			case st_inst:
				struct instruction inst = tmp_st->inst;
				printf("%s R%d D%d A0x%x", mnemonic_str[inst.id],inst.p1,inst.p2,(inst.address != 0)?inst.address->address:0);
				break;
			default:
				break;
		}
		printf("\r\n");
	}
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

	FILE * output_file = fopen("out.prg","w");
	if(!output_file){
		printf("Could not open output file. Aborting...\n");
		return errno;
	}

	struct OP opcodes[MNEM_MAX] = {{0}};
	init_opcodes(opcodes);

	struct parser_context parse_context;
	memset(parse_context.identifier_str, 0, IDENT_MAX);
	parse_context.comment_str = 0;
	parse_context.num_value = 0;
	parse_context.cur_tok = 0;
	parse_context.cur_line = 0;
	parse_context.opcodes = opcodes;

	struct segment_context segment_context;
	segment_context.current_address = 0;
	segment_context.current_segment = 0;

	static struct program prg;

	INIT_QUEUE(&prg.segment_list);
	prg.main_label = 0;

	nexttok(&parse_context, input_file);

	int error = 0;
	
	while(cur_tok(&parse_context) != tok_eof && error == 0){
		switch(cur_tok(&parse_context)){
			case tok_eof:
				break;
			case tok_eol: //eat empty lines
				nexttok(&parse_context, input_file);
				break;
			case tok_segment:
				resolve_label_addresses(&segment_context);
				error = handle_segment(&parse_context, &segment_context, &prg, input_file, output_file);
				break;
			default:
				error = handle_statement(&parse_context, &segment_context, &prg, input_file, output_file);
				break;
		}
	}

	resolve_label_addresses(&segment_context);

	fclose(input_file);
	fclose(output_file);
	
	return 0;
}
