#include "abi.h"

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
	opcodes[i++].arg_types[1] = arg_addr;

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