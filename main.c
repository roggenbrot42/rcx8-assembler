#include<stdio.h>
#include<ctype.h>
#include<string.h>
#include<stdlib.h>
#include<inttypes.h>
#include<stdarg.h>
#include<errno.h>

#include "list.h"
#include "parser.h"
#include "auxiliary.h"
#include "binary.c"

#define print_error(format,...) fprintf(stderr,"\033[31m" format "\033[39m", __VA_ARGS__)

#define new(type) (type *) malloc(sizeof(type))

struct program {
	struct label * main_label;
	struct OP * opcodes;
	struct queue segment_list;
};

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

int finalize_segment(struct segment_context * seg_context, struct program * prg){
	struct list_head * seg_pos;
	struct statement * tmp_st;
	struct label * tmp_lab;

	if(seg_context == 0){
		return -1;
	}

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
		if(strcmp("main", tmp_lab->name)){
			prg->main_label = tmp_lab;
		}
		printf("Label Name: %s Address: 0x%x\r\n", tmp_lab->name, tmp_lab->address);
	}

	printf("------------\r\nStatements in this segment:\r\n",NULL);
	struct list_head * st_pos;
	uint16_t running_address = seg_context->current_segment->base_address;
	list_for_each(st_pos, &seg_context->current_segment->statements.list){
		tmp_st = list_entry(st_pos, struct statement, list);
		switch(tmp_st->type){
			case st_def_b:
				printf("0x%x DB %d", running_address, tmp_st->num);
				break;
			case st_def_w:
				printf("0x%x DW %d", running_address, tmp_st->num);
				break;
			case st_def_str:
				printf("0x%x DS %s", running_address, tmp_st->str);
				break;
			case st_inst:
				struct instruction inst = tmp_st->inst;
				printf("0x%x %s R%d D%d A0x%x", running_address, mnemonic_str[inst.id],inst.p1,inst.p2,(inst.address != 0)?inst.address->address:0);
				break;
			default:
				break;
		}
		printf("\r\n");
		running_address += tmp_st->size;
	}

	return 0;
}

int write_data_segment(struct program prg, struct segment * current_segment, FILE * of){
	if(current_segment != 0){
		struct list_head * pos = 0;
		struct statement * stmt = 0;
		int size = 0;
		const int zero = 0;

		list_for_each(pos, &current_segment->statements.list){
			stmt = list_entry(pos,struct statement,list);

			if(stmt->type == st_def_str){
				fwrite(stmt->str,sizeof(char),stmt->size,of);
				size +=stmt->size;
			}else
			if(stmt->type == st_def_b){
				fwrite(&stmt->num,sizeof(char),1,of);
				size += 1;
			}else
			if(stmt->type == st_def_w){
				fwrite(&stmt->num,sizeof(uint16_t),1,of);
				size += 2;
			}else
			if(stmt->type == st_inst){
				struct OP inst = prg.opcodes[stmt->inst.id];
				uint8_t bin = 0;
				bin = inst.op;

				if(inst.arg_types[0] == arg_reg || inst.arg_types[0] == arg_dirct4){
					bin = bin | ((stmt->inst.p1 & 0xF) << 1);
				}
				fwrite(&bin,sizeof(bin),1,of);
				size += sizeof(bin);

				if(inst.arg_types[0] == arg_addr){
					if(stmt->inst.address != 0){
						fwrite(&stmt->inst.address->address,sizeof(uint16_t),1,of);
						size += 2;
					}else{
						print_error("Error: %s","Label undefined\n");
					}
				}else
				if(inst.arg_types[1] == arg_addr){
					fwrite(&stmt->inst.p2,sizeof(uint16_t),1,of);
					size += 2;
				}else
				if(inst.arg_types[1] == arg_dirct || inst.arg_types[1] == arg_port){
					fwrite(&stmt->inst.p2,sizeof(uint8_t),1,of);
					size += 1;
				}
			}
		}

		if(size % 2 != 0){
			fwrite(&zero, sizeof(uint8_t), 1, of);
		}
	}

	return 0;
}

int write_start_address(struct program prg, FILE * of){
	uint16_t start_addr = 0x7000;
	fwrite(&start_addr, sizeof(uint16_t),1,of);
	fwrite(prg.main_label,sizeof(uint16_t),1,of);
	
	return 0;
}

void print_segments(struct list_head * list){
	struct list_head * pos;
	struct segment * seg_iter;

	fprintf(stderr, "\n----------------\nCurrent segments:...\n");

	list_for_each(pos, list){
		seg_iter = list_entry(pos, struct segment, list);
		fprintf(stderr, "Segment type %d %x\n", seg_iter->type, seg_iter->base_address);
	}
	
	fprintf(stderr, "------------\n\n");
}

int write_segments(struct program prg, struct list_head * list, FILE * of){
	struct list_head * pos;
	struct segment * seg_iter;
	const int zero = 0;
	int error = 0;

	list_for_each(pos, list){
		seg_iter = list_entry(pos, struct segment, list);
		fprintf(stderr, "Writing segment type %d %x\n", seg_iter->type, seg_iter->base_address);

		switch(seg_iter->type){
			case seg_comment:
				fwrite(seg_iter->string,sizeof(char), seg_iter->length, of);
				if(seg_iter->length % 2 != 0){
					fwrite(&zero,sizeof(char), 1, of);
				}
				break;
			case seg_data:
				error = write_data_segment(prg, seg_iter, of);
				break;
			default:
				error = -1;
				break;
		}
		if(error != 0){
			print_error("Unkown segment error: %d\n", seg_iter->type);
			return -1;
		}
	}

	return 0;
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

	struct segment_context segment_context = {0};

	static struct program prg = {0};
	prg.opcodes = opcodes;
	INIT_QUEUE(&prg.segment_list);
	prg.main_label = 0;

	nexttok(&parse_context, input_file);

	int error = 0;
	
	while(cur_tok(&parse_context) != tok_eof && error == 0){
		switch(cur_tok(&parse_context)){
			case tok_eof://never happens
				break;
			case tok_eol: //eat empty lines
				nexttok(&parse_context, input_file);
				break;
			case tok_segment:
				if(segment_context.current_segment){ //finalize segment if any
					finalize_segment(&segment_context, &prg);
				}
				memset(&segment_context,0,sizeof(struct segment_context));
				error = parse_segment(&parse_context, &segment_context, input_file);
				queue_add(&segment_context.current_segment->list,&prg.segment_list);
				break;
			default:
				error = parse_statement(&parse_context, &segment_context, input_file);
				break;
		}
	}

	finalize_segment(&segment_context, &prg);

	//write program
	//1. write start address
	write_start_address(prg, output_file);

	//2. write segments
	write_segments(prg, &prg.segment_list.list, output_file);
	//3. write eof block
	uint16_t eofblock = 0x0;
	fwrite(&eofblock, sizeof(uint16_t), 1, output_file);

	if(error != 0){
		print_error("Something failed: %d. Exiting...\n",error);
		goto out;
	}

out:
	fclose(input_file);
	fclose(output_file);
	
	return 0;
}
