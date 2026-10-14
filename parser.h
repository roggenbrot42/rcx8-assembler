#ifndef PARSER_H
#define PARSER_H

#include<inttypes.h>
#include "abi.h"

#define IDENT_MAX 255
#define NUM_SEGMENT_TYPE 3
#define NUM_DIGITS_DEC 5
#define NUM_DIGITS_BIN 16
#define NUM_DIGITS_HEX 4

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

enum statement_type {
	st_def_str,
	st_def_w,
	st_def_b,
	st_inst
};

enum expr_func {
	expr_plain,
	expr_lo,
	expr_hi
};

// [lo|hi]( [label] +/- constants ), evaluated once all labels are known
struct expr {
	char * name;	// label name or 0
	int32_t value;	// constant part, final value after resolving
	int func;
	int line;
};

struct instruction{
	int id;
	uint16_t p1; // register or 4 Bit parameter
	uint16_t p2; // 8 Bit parameter, port or 16 Bit address
	struct expr * expr; // unresolved value for the non register argument
};

struct statement {
	int type;
	int size;
	union {
		char * str;
		struct instruction inst;
		uint16_t num;
	};
	struct expr * expr; // unresolved value of db/dw
	int line;
	struct list_head list;
};

struct label {
	uint16_t address;
	char valid;
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
		cur_line,
		last_char;
	const char * filename;
	struct OP * opcodes;
};

struct segment_context {
	uint16_t current_address;
	struct segment * current_segment;
};

int gettok(struct parser_context *context, FILE * f);
int nexttok(struct parser_context *context, FILE * f);
int cur_tok(struct parser_context *context);
int cur_line(struct parser_context *context);

struct expr * parse_expr(struct parser_context * context, FILE * f);
int parse_definition(struct parser_context * context, struct segment_context * seg_context, FILE * f);
struct label * label_from_identifier(struct parser_context * parser_context);
struct statement * parse_instruction(struct parser_context * context, struct OP *, FILE * f);
int parse_label(struct parser_context * context, struct segment_context * seg_context);
int parse_statement(struct parser_context * context, struct segment_context * seg_context, FILE * f);
int parse_segment(struct parser_context * context, struct segment_context * seg_context, FILE * f);

#endif // PARSER_H
