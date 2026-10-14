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

#define INCLUDE_DEPTH_MAX 16
#define PATH_MAX_LEN 4096

struct program {
	struct label * main_label;
	struct OP * opcodes;
	struct queue segment_list;
};

//labels are global to the program, so search every data segment
struct label * get_label(struct program * prg, const char * name){
	struct list_head * seg_pos;
	struct list_head * lab_pos;
	struct segment * seg;
	struct label * tmp_lab;

	list_for_each(seg_pos, &prg->segment_list.list){
		seg = list_entry(seg_pos, struct segment, list);
		if(seg->type != seg_data){
			continue;
		}
		list_for_each(lab_pos, &seg->labels.list){
			tmp_lab = list_entry(lab_pos, struct label, list);
			if(strcmp(name,tmp_lab->name) == 0){
				return tmp_lab;
			}
		}
	}

	return 0;
}

//the same label name must not be used in two segments
int check_duplicate_labels(struct program * prg){
	struct list_head * seg_pos;
	struct list_head * lab_pos;
	struct segment * seg;
	struct label * lab;
	int error = 0;

	list_for_each(seg_pos, &prg->segment_list.list){
		seg = list_entry(seg_pos, struct segment, list);
		if(seg->type != seg_data){
			continue;
		}
		list_for_each(lab_pos, &seg->labels.list){
			lab = list_entry(lab_pos, struct label, list);
			if(get_label(prg, lab->name) != lab){
				print_error("Error: Label already defined in another segment: %s\n", lab->name);
				error = -1;
			}
		}
	}

	return error;
}

//Evaluates an expression and checks that it fits into [min,max]
int resolve_expr(struct program * prg, struct expr * e, int32_t min, int32_t max, int32_t * out){
	int32_t v = e->value;

	if(e->name != 0){
		struct label * lab = get_label(prg, e->name);
		if(lab == 0){
			print_error("Error (line %d): Label not found: %s\n", e->line, e->name);
			return -1;
		}
		v += lab->address;
	}

	if(e->func == expr_lo){
		v &= 0xFF;
	}else
	if(e->func == expr_hi){
		v = (v >> 8) & 0xFF;
	}

	if(v < min || v > max){
		print_error("Error (line %d): value %d is out of range (%d to %d)\n", e->line, v, min, max);
		return -1;
	}

	*out = v;
	return 0;
}

int finalize_segment(struct segment * seg, struct program * prg){
	struct list_head * seg_pos;
	struct statement * st;
	int32_t v;

	if(seg == 0){
		return -1;
	}

	if(seg->type != seg_data){
		return 0;
	}

	list_for_each(seg_pos, &seg->statements.list){
		st = list_entry(seg_pos,struct statement,list);

		switch(st->type){
		case st_def_b:
			if(resolve_expr(prg, st->expr, -128, 255, &v) != 0) return -1;
			st->num = v & 0xFF;
			break;
		case st_def_w:
			if(resolve_expr(prg, st->expr, -32768, 65535, &v) != 0) return -1;
			st->num = v & 0xFFFF;
			break;
		case st_inst:
			for(int i=0;i<2;i++){
				int type = prg->opcodes[st->inst.id].arg_types[i];
				if(type == arg_dirct4){
					if(resolve_expr(prg, st->inst.expr, 0, 15, &v) != 0) return -1;
					st->inst.p1 = v;
				}else
				if(type == arg_dirct){
					if(resolve_expr(prg, st->inst.expr, -128, 255, &v) != 0) return -1;
					st->inst.p2 = v & 0xFF;
				}else
				if(type == arg_port){
					if(resolve_expr(prg, st->inst.expr, 0, 255, &v) != 0) return -1;
					st->inst.p2 = v;
				}else
				if(type == arg_addr){
					if(resolve_expr(prg, st->inst.expr, 0, 65535, &v) != 0) return -1;
					st->inst.p2 = v;
				}
			}
			break;
		default:
			break;
		}
	}

	return 0;
}

//Binary format (all values little endian, see RCX8 loader):
//  0x7000 <pc>                           program start address
//  0x1000 <len> <bytes>                  comment block
//  0x2000 <offset> <len> <bytes>         data block
//  0x0000                                end of file
static void write_u16(uint16_t v, FILE * of){
	fputc(v & 0xFF, of);
	fputc(v >> 8, of);
}

//Writes the machine code of one resolved statement to out (stmt->size bytes)
static void encode_statement(struct program * prg, struct statement * stmt, uint8_t * out){
	switch(stmt->type){
	case st_def_str:
		memcpy(out, stmt->str, stmt->size);
		break;
	case st_def_b:
		out[0] = stmt->num & 0xFF;
		break;
	case st_def_w:
		out[0] = stmt->num & 0xFF;
		out[1] = stmt->num >> 8;
		break;
	case st_inst: {
		struct OP inst = prg->opcodes[stmt->inst.id];
		uint8_t bin = inst.op;
		int n = 1;

		if(inst.arg_types[0] == arg_reg){
			bin |= (stmt->inst.p1 & 0x7) << 1;
		}else
		if(inst.arg_types[0] == arg_dirct4){
			bin |= stmt->inst.p1 & 0xF;
		}
		out[0] = bin;

		if(inst.arg_types[0] == arg_addr || inst.arg_types[1] == arg_addr){
			out[n++] = stmt->inst.p2 & 0xFF;
			out[n++] = stmt->inst.p2 >> 8;
		}else
		if(inst.arg_types[1] == arg_dirct || inst.arg_types[0] == arg_port){
			out[n++] = stmt->inst.p2 & 0xFF;
		}
		break;
	}
	}
}

int write_data_segment(struct program * prg, struct segment * current_segment, FILE * of){
	struct list_head * pos = 0;
	struct statement * stmt = 0;
	uint32_t size = 0;

	list_for_each(pos, &current_segment->statements.list){
		size += list_entry(pos,struct statement,list)->size;
	}
	if(size > 0xFFFF){
		print_error("Error: data segment at 0x%x is too large (%u bytes)\n", current_segment->base_address, size);
		return -1;
	}

	write_u16(0x2000, of);
	write_u16(current_segment->base_address, of);
	write_u16(size, of);

	list_for_each(pos, &current_segment->statements.list){
		stmt = list_entry(pos,struct statement,list);
		uint8_t * buf = (uint8_t *) calloc(stmt->size + 1, 1);
		encode_statement(prg, stmt, buf);
		fwrite(buf, 1, stmt->size, of);
		free(buf);
	}

	return 0;
}

int write_start_address(struct program * prg, FILE * of){
	write_u16(0x7000, of);
	write_u16(prg->main_label->address, of);

	return 0;
}

int write_segments(struct program * prg, FILE * of){
	struct list_head * pos;
	struct segment * seg_iter;
	int error = 0;

	list_for_each(pos, &prg->segment_list.list){
		seg_iter = list_entry(pos, struct segment, list);

		switch(seg_iter->type){
			case seg_comment:
				write_u16(0x1000, of);
				write_u16(seg_iter->length + 1, of); //including the terminating zero
				fwrite(seg_iter->string,sizeof(char), seg_iter->length + 1, of);
				break;
			case seg_data:
				error = write_data_segment(prg, seg_iter, of);
				break;
			default:
				print_error("Unsupported segment type %d\n", seg_iter->type);
				error = -1;
				break;
		}
		if(error != 0){
			return -1;
		}
	}

	return 0;
}

static void print_expr(struct expr * e, FILE * lf){
	if(e->func != expr_plain){
		fprintf(lf, "%s(", e->func == expr_lo ? "lo" : "hi");
	}
	if(e->name != 0){
		fprintf(lf, "%s", e->name);
		if(e->value != 0){
			fprintf(lf, "%+d", e->value);
		}
	}else{
		fprintf(lf, "%d", e->value);
	}
	if(e->func != expr_plain){
		fprintf(lf, ")");
	}
}

static void print_statement(struct program * prg, struct statement * st, FILE * lf){
	switch(st->type){
	case st_def_b:
		fprintf(lf, "db "); print_expr(st->expr, lf);
		break;
	case st_def_w:
		fprintf(lf, "dw "); print_expr(st->expr, lf);
		break;
	case st_def_str:
		fprintf(lf, "d%c \"%.*s\"", st->size > (int) strlen(st->str) ? 's' : 'b', (int) strlen(st->str), st->str);
		break;
	case st_inst: {
		struct OP op = prg->opcodes[st->inst.id];
		fprintf(lf, "%s", mnemonic_str[st->inst.id]);
		for(int i=0;i<2;i++){
			if(op.arg_types[i] == arg_reg){
				fprintf(lf, " R%d", st->inst.p1);
			}else
			if(op.arg_types[i] != arg_none){
				fprintf(lf, " ");
				print_expr(st->inst.expr, lf);
			}
		}
		break;
	}
	}
}

static int compare_labels(const void * a, const void * b){
	const struct label * la = *(struct label * const *) a;
	const struct label * lb = *(struct label * const *) b;
	return (int) la->address - (int) lb->address;
}

int write_listing(struct program * prg, FILE * lf){
	struct list_head * seg_pos;
	struct list_head * pos;
	struct segment * seg;
	int count = 0;

	list_for_each(seg_pos, &prg->segment_list.list){
		seg = list_entry(seg_pos, struct segment, list);
		if(seg->type == seg_data){
			count += seg->labels.len;
		}
	}

	struct label ** labels = (struct label **) malloc(sizeof(struct label *) * (count + 1));
	int n = 0;
	list_for_each(seg_pos, &prg->segment_list.list){
		seg = list_entry(seg_pos, struct segment, list);
		if(seg->type != seg_data){
			continue;
		}
		list_for_each(pos, &seg->labels.list){
			labels[n++] = list_entry(pos, struct label, list);
		}
	}
	qsort(labels, n, sizeof(struct label *), compare_labels);

	fprintf(lf, "Symbols:\n");
	for(int i=0;i<n;i++){
		fprintf(lf, "  %04X  %s\n", labels[i]->address, labels[i]->name);
	}
	free(labels);

	list_for_each(seg_pos, &prg->segment_list.list){
		seg = list_entry(seg_pos, struct segment, list);
		if(seg->type == seg_comment){
			fprintf(lf, "\n.comment (%d bytes)\n", seg->length);
			continue;
		}
		if(seg->type != seg_data){
			continue;
		}

		fprintf(lf, "\n.data 0x%04X\n", seg->base_address);
		uint16_t address = seg->base_address;
		list_for_each(pos, &seg->statements.list){
			struct statement * st = list_entry(pos, struct statement, list);
			struct list_head * lp;
			list_for_each(lp, &seg->labels.list){
				struct label * lab = list_entry(lp, struct label, list);
				if(lab->address == address){
					fprintf(lf, "%s:\n", lab->name);
				}
			}

			uint8_t * buf = (uint8_t *) calloc(st->size + 1, 1);
			encode_statement(prg, st, buf);
			fprintf(lf, "  %04X  ", address);
			for(int i=0;i<6;i++){
				if(i < st->size){
					fprintf(lf, "%02X ", buf[i]);
				}else{
					fprintf(lf, "   ");
				}
			}
			fprintf(lf, st->size > 6 ? "... " : "    ");
			free(buf);

			print_statement(prg, st, lf);
			fprintf(lf, "\n");
			address += st->size;
		}
		//labels behind the last statement
		struct list_head * lp;
		list_for_each(lp, &seg->labels.list){
			struct label * lab = list_entry(lp, struct label, list);
			if(lab->address == address){
				fprintf(lf, "%s:\n", lab->name);
			}
		}
	}

	return 0;
}

//Parses one source file (and everything it includes) into prg
int parse_file(struct parser_context * context, struct segment_context * seg_context,
		struct program * prg, const char * path, int depth){
	if(depth > INCLUDE_DEPTH_MAX){
		print_error("Error: includes nested too deeply (%s)\n", path);
		return -1;
	}

	FILE * input_file = fopen(path, "r");
	if(!input_file){
		print_error("Error: could not open %s: %s\n", path, strerror(errno));
		return -1;
	}

	//save the lexer state of the including file
	int saved_char = context->last_char;
	int saved_line = context->cur_line;
	int saved_tok = context->cur_tok;
	const char * saved_name = context->filename;
	context->last_char = tok_bol;
	context->cur_line = 1;
	context->filename = path;
	context->cur_tok = 0;

	int error = 0;
	nexttok(context, input_file);

	while(cur_tok(context) != tok_eof && error == 0){
		switch(cur_tok(context)){
			case tok_eof://never happens
				break;
			case tok_eol: //eat empty lines
				nexttok(context, input_file);
				break;
			case tok_segment:
				if(strcmp(context->identifier_str, "include") == 0){
					if(nexttok(context, input_file) != tok_str){
						print_error("Syntax error (%s line %d): .include expects a file name in quotes\n", path, cur_line(context));
						error = -1;
						break;
					}
					char full[PATH_MAX_LEN];
					const char * slash = strrchr(path, '/');
					if(context->comment_str[0] != '/' && slash != 0){ //relative to the including file
						snprintf(full, sizeof(full), "%.*s/%s", (int)(slash - path), path, context->comment_str);
					}else{
						snprintf(full, sizeof(full), "%s", context->comment_str);
					}
					free(context->comment_str);
					context->comment_str = 0;
					error = parse_file(context, seg_context, prg, full, depth + 1);
					if(error == 0){
						nexttok(context, input_file);
					}
					break;
				}
				memset(seg_context,0,sizeof(struct segment_context));
				error = parse_segment(context, seg_context, input_file);
				if(error == 0){
					queue_add(&seg_context->current_segment->list,&prg->segment_list);
				}
				break;
			default:
				error = parse_statement(context, seg_context, input_file);
				break;
		}
	}

	if(error != 0){
		print_error("  in %s\n", path);
	}

	fclose(input_file);
	context->last_char = saved_char;
	context->cur_line = saved_line;
	context->cur_tok = saved_tok;
	context->filename = saved_name;

	return error;
}

int main(int argc, char *argv[])
{
	const char * input_name = 0;
	const char * output_name = 0;
	const char * listing_name = 0;

	for(int i=1;i<argc;i++){
		if(strcmp(argv[i], "-l") == 0 && i + 1 < argc){
			listing_name = argv[++i];
		}else
		if(input_name == 0){
			input_name = argv[i];
		}else
		if(output_name == 0){
			output_name = argv[i];
		}else{
			input_name = 0;
			break;
		}
	}
	if(input_name == 0){
		printf("--------------"
		"\nThis is RCx8Asm by Nicolas Appel (c) 2012--2022"
		"\nUsage: rcx8asm inputfile [outputfile] [-l listingfile]\n"
		"\nA listing file name of - writes the listing to stdout.\n");

		return 0;
	}
	if(output_name == 0){
		output_name = "out.prg";
	}

	struct OP opcodes[MNEM_MAX] = {{0}};
	init_opcodes(opcodes);

	struct parser_context parse_context;
	memset(&parse_context, 0, sizeof(parse_context));
	parse_context.opcodes = opcodes;

	struct segment_context segment_context = {0};

	static struct program prg = {0};
	prg.opcodes = opcodes;
	INIT_QUEUE(&prg.segment_list);
	prg.main_label = 0;

	int error = parse_file(&parse_context, &segment_context, &prg, input_name, 0);

	//resolve labels once every segment is known
	struct list_head * seg_pos;
	if(error == 0){
		error = check_duplicate_labels(&prg);
	}
	list_for_each(seg_pos, &prg.segment_list.list){
		if(error == 0){
			error = finalize_segment(list_entry(seg_pos, struct segment, list), &prg);
		}
	}

	if(error == 0){
		prg.main_label = get_label(&prg, "main");
		if(prg.main_label == 0){
			print_error("Error: %s","no 'main' label found\n");
			error = -1;
		}
	}

	if(error != 0){
		print_error("Assembly failed: %d. Exiting...\n",error);
		return 1;
	}

	FILE * output_file = fopen(output_name,"wb");
	if(!output_file){
		printf("Could not open output file. Aborting...\n");
		return errno;
	}

	//write program
	//1. write start address
	write_start_address(&prg, output_file);

	//2. write segments
	error = write_segments(&prg, output_file);

	//3. write eof block
	write_u16(0x0000, output_file);

	fclose(output_file);
	if(error != 0){
		print_error("Writing output failed: %d\n", error);
		remove(output_name);
		return 1;
	}

	if(listing_name != 0){
		FILE * lf = strcmp(listing_name, "-") == 0 ? stdout : fopen(listing_name, "w");
		if(!lf){
			printf("Could not open listing file %s\n", listing_name);
			return 1;
		}
		write_listing(&prg, lf);
		if(lf != stdout){
			fclose(lf);
		}
	}

	return 0;
}
