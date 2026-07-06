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

static inline void mem_write(uint16_t addr, uint8_t val, CPU* const c){
	if(addr == 0xff01){
		fputc(val, stdout);
		fflush(stdout);	
	}	
	c->mem[addr] = val;
}

static inline uint16_t get_hl(CPU* const c){
	return (c->h << 8) | c->l;
}

static inline uint16_t get_de(CPU* const c){
	return (c->d << 8) | c->e;
}

static inline void set_zf(uint8_t val, CPU* const c){
	c->zf = (val == 0) ? 1: 0; 	
}

void cpu_step(CPU* const c){
	uint8_t opcode = fetch8(c);

	printf("opcode: %04X, pc: %04X\n",opcode,c->pc);	

	if(opcode == 0xcb){
		printf("this is cb prefix\n");	
		uint8_t cb_opcode = fetch8(c);
		c->is_running = 0;
	}

	switch(opcode){
		case 0x00: break;	
		
		// jmp
		case 0xc3: c->pc = fetch16(c); break;	

		case 0x20:
			   int8_t offset = (int8_t)fetch8(c);
			   if(!c->zf){
			  	c->pc += offset; 
			   }
			   break;

		// load
		case 0x47: c->b = c->a; break;
		case 0x78: c->a = c->b; break;
		case 0x21: c->l = fetch8(c); c->h = fetch8(c); break; 
		case 0x11: c->e = fetch8(c); c->d = fetch8(c); break;
		case 0x01: c->c = fetch8(c); c->b = fetch8(c); break;
		case 0x31: c->sp = fetch16(c); break;

		case 0x0e: c->c = fetch8(c); break;
		case 0x3e: c->a = fetch8(c); break;

		case 0x2a: 
			   uint16_t hl = get_hl(c);
			   c->a = c->mem[hl];
			   hl++;
			   c->h = hl >> 8;
			   c->l = hl & 0xff;
			   break;

		case 0x12: mem_write(get_de(c), c->a, c); break;
		case 0xea: mem_write(fetch16(c), c->a, c); break; 

		case 0xe0: mem_write(0xff00 + fetch8(c), c->a, c); break;

		// alu
		case 0x1c: 	// inc e
			   c->hf = ((c->e&0x0f) + (0x01) > 0x0f);
			   c->e++;
			   set_zf(c->e,c);
			   c->nf = 0;
			   break;

		case 0x14:	// inc d
			c->hf = ((c->d&0x0f) + (0x01) > 0x0f);
			c->d++;
			set_zf(c->d,c);
			c->nf = 0;
			break;	

		case 0x0d:	// dec c
			c->hf = ((c->c&0x0f) < (0x01));
			c->c--;
			set_zf(c->c,c);
			c->nf = 1;
			break;

		// interupts
		case 0xf3: c->IME = 0; break;

		default:
			   printf("unimplmented opcode: %04X\n",opcode);
			   c->is_running = 0;
			   break;
	}
} 


