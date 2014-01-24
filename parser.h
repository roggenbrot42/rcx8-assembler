#ifndef PARSER_H
#define PARSER_H

#include "stack.h"

struct FILE;

enum gamma {
	LIN,MNE,OP,ADDR,RNO,COMM,DOT,SEG,NOP
};
enum mnemno {
	NOP, JCN, FIM, ZER, FIN, FTC, JUN, JMS,
	INC, INO, ISZ, STO, ADD, XOR, SUB, STI, LD
	XCH, BBL, LDM, WMP, RDR, KBI, ASC, CLB, CLC
	IAC, CMC, CMA, RAL, RAR, TCC, DAC, STC, DAA,
	HLT
};

struct token{
	char content[15];
	int chars;
	int type;
}

struct symbol{
	char name[15];
	int addr;
}

struct list_entry{
	void * cont;
	struct list_entry *next;
	struct list_entry *prev;
}

struct pda {
	stack_t * st;
	int q;
	int fin;
};

int pda_init(struct pda *, int q0, int Z);

void parseInput(FILE* fin);
#endif // PARSER_H
