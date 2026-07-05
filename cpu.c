#include <stdio.h>

#include "cpu.h"

static inline uint8_t fetch8(CPU* const c){
	return c->mem[c->pc++]; 
}

static inline uint16_t fetch16(CPU* const c){
	uint8_t lo = fetch8(c);
	uint8_t hi = fetch8(c);
	return (hi << 8) | lo;
}

void cpu_step(CPU* const c){
	uint8_t opcode = fetch8(c);

	printf("opcode: %04X, pc: %04X\n",opcode,c->pc);	

	if(opcode == 0xcb){
		printf("this is cb prefix\n");	
		uint8_t cb_opcode = fetch8(c);
	}

	switch(opcode){
		case 0x00: break;	
		default:
			   printf("unimplmented opcode: %04X\n",opcode);
			   c->is_running = 0;
			   break;
	}
} 


