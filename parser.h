#ifndef PARSER_H
#define PARSER_H

#include<inttypes.h>

#define IDENT_MAX 255
#define NUM_SEGMENT_TYPE 3
#define NUM_DIGITS_DEC 5
#define NUM_DIGITS_BIN 16
#define NUM_DIGITS_HEX 4

static char * mnemonic_str[] = {
	"NOP", "JZ", "JNZ", "JC", "JNC", "JT", "JNT", "FIM", "ZER", 
	"FIN", "FTC", "JUN", "JMS",	"INC", "INO", "ISZ", "STO",
	"ADD", "XOR", "SUB", "STI", "LD",	"XCH", "BBL", "LDM",
	"WMP", "RDR", "KBI", "ASC", "CLB", "CLC", "IAC", "CMC",
	"CMA", "RAL", "RAR", "TCC", "DAC", "STC", "DAA", "HLT"
};

enum mnemonic_id {
	NOP, JZ, JNZ, JC, JNC, JT, JNT, FIM, ZER, 
	FIN, FTC, JUN, JMS,	INC, INO, ISZ, STO,
	ADD, XOR, SUB, STI, LD,	XCH, BBL, LDM,
	WMP, RDR, KBI, ASC, CLB, CLC, IAC, CMC,
	CMA, RAL, RAR, TCC, DAC, STC, DAA, HLT,MNEM_MAX
};

enum arg_types{
    arg_none,
    arg_reg,
    arg_dirct,
    arg_cnd, //unused
    arg_addr,
    arg_ptr,
    arg_port,
    num_argst
};

struct OP{
    uint8_t opid;
    uint8_t op; //op binary
    uint8_t arg_types[2];
};

static char * def_str[] = {
	"DS", "DB", "DW"
};

enum def_id {
	DEF_STR, DEF_WORD, DEF_BYTE, DEF_MAX
};

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

static char * segment_str[] = {
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

struct parser_context {
	char identifier_str[IDENT_MAX];
	char * comment_str;
	int num_value,
		cur_tok,
		cur_line;
};

struct segment_context {
	uint16_t current_address;
	struct segment * current_segment;
};

int gettok(struct parser_context *context, FILE * f);
int nexttok(struct parser_context *context, FILE * f);
int cur_tok(struct parser_context *context);
int cur_line(struct parser_context *context);

struct statement * parse_definition(struct parser_context * context, FILE * f);
struct statement * parse_instruction(struct parser_context * context, struct OP *, FILE * f);

#endif // PARSER_H
