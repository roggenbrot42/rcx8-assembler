#ifndef PARSER_H
#define PARSER_H

#include<inttypes.h>

char * mnemonic_str[] = {
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

enum {
    arg_none,
    arg_reg,
    arg_dirct,
    arg_cnd, //unused
    arg_addr,
    arg_ptr,
    arg_port,
    num_argst
} arg_types;

struct OP{
    uint8_t opid;
    uint8_t op; //op binary
    uint8_t arg_types[2];
};

struct OP opcodes[MNEM_MAX] = {{0}};

char * def_str[] = {
	"DS", "DB", "DW"
};

enum def_id {
	DEF_STR, DEF_WORD, DEF_BYTE, DEF_MAX
};



#endif // PARSER_H
