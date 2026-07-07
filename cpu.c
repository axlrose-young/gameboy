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

static inline uint16_t get_bc(CPU* const c){
	return (c->b << 8) | c->c;
}

static inline void set_zf(uint8_t val, CPU* const c){
	c->zf = (val == 0) ? 1: 0; 	
}

static inline uint8_t format_flags(CPU* const c){
	uint8_t f = 0;
	f |= (c->zf << 3);
	f |= (c->nf << 2);
	f |= (c->hf << 1);
	f |= c->cf;	
	return f;
}

static inline void unformat_flags(uint8_t f, CPU* const c){
	c->zf = f >> 3;
	c->nf = f >> 2;
	c->hf = f >> 1;
	c->cf = f & 0x01;	
}

static inline void push_stack(uint8_t high, uint8_t low, CPU* const c){
	c->sp--;
	mem_write(c->sp, high, c);
	c->sp--;
	mem_write(c->sp, low, c);
}

static inline uint16_t pop_stack(CPU* const c){
	uint8_t low = c->mem[c->sp];
	c->sp++;
	uint8_t high = c->mem[c->sp];
	c->sp++;
	return high << 8 | low;
}

static inline void bit_or(uint8_t val, CPU* const c){
	c->a |= val;

	set_zf(c->a, c);
	c->nf = c->hf = c->cf = 0;
}

static inline void bit_cmp(uint8_t val, CPU* const c){
	uint8_t result = c->a - val;
	c->hf = ((c->a&0x0f) < (val&0x0f));
	c->cf = (c->a < val);
	set_zf(result,c);
	c->nf = 1;
}

static inline void bit_and(uint8_t val, CPU* const c){
	c->a &= val;

	set_zf(c->a, c);
	c->nf = c->cf = 0;
	c->hf = 1;
}

static inline uint8_t inc(uint8_t val, CPU* const c){
	c->hf = ((val&0x0f) + (0x01) > 0x0f);
	val++;
	set_zf(val, c);
	c->nf = 0;
	return val;
}

static inline uint8_t dec(uint8_t val, CPU* const c){
	c->hf = ((val&0x0f) < (0x01));
	val--;
	set_zf(val,c);
	c->nf = 1;
	return val;
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

		case 0x20:	// jmp cc relative
			   int8_t offset = (int8_t)fetch8(c);
			   if(!c->zf){
			  	c->pc += offset; 
			   }
			   break;

		case 0x28:{
				   int8_t offset = (int8_t)fetch8(c); 
				   if(c->zf){
				   	c->pc += offset;	
				   }
				   break;
			  }

		case 0x18:{	// jmp relative 
				   int8_t offset = (int8_t)fetch8(c);
				   c->pc += offset;
				   break;
			  }

		case 0xcd:	// call u16
			uint16_t addr = fetch16(c);
			push_stack(c->pc >> 8, c->pc&0xff, c);
			c->pc = addr;
			break;

		case 0xc4:{
				uint16_t addr = fetch16(c);
				if(!c->nf){
					push_stack(c->pc >> 8, c->pc&0xff, c);		
					c->pc = addr;
				}
				break;
			  }

		case 0xc9: 	// ret
			c->pc = pop_stack(c);
			break;

		// load
		case 0x47: c->b = c->a; break;
		case 0x78: c->a = c->b; break;
		case 0x7d: c->a = c->l; break;
		case 0x7c: c->a = c->h; break;
		case 0x21: c->l = fetch8(c); c->h = fetch8(c); break; 
		case 0x11: c->e = fetch8(c); c->d = fetch8(c); break;
		case 0x01: c->c = fetch8(c); c->b = fetch8(c); break;
		case 0x31: c->sp = fetch16(c); break;

		case 0x0e: c->c = fetch8(c); break;
		case 0x3e: c->a = fetch8(c); break;
		case 0x06: c->b = fetch8(c); break;

		case 0xfa: c->a = c->mem[fetch16(c)]; break;
		case 0xf0: c->a = c->mem[(fetch8(c) + 0xff00)]; break;

		case 0x2a: 
			   uint16_t hl = get_hl(c);
			   c->a = c->mem[hl];
			   hl++;
			   c->h = hl >> 8;
			   c->l = hl & 0xff;
			   break;

		// write operations
		case 0x12: mem_write(get_de(c), c->a, c); break;
		case 0xea: mem_write(fetch16(c), c->a, c); break; 
		case 0x77: mem_write(get_hl(c), c->a, c); break;

		case 0xe0: mem_write(0xff00 + fetch8(c), c->a, c); break;

		// push
		case 0xe5: push_stack(c->h, c->l, c); break;
		case 0xc5: push_stack(c->b, c->c, c); break;
		case 0xf5: push_stack(c->a, format_flags(c), c); break;

		// pop
		case 0xe1:{ 
				uint16_t hl = pop_stack(c); 
				c->h = hl >> 8; c->l = hl & 0xff; 
				break;
			  }
		case 0xc1:{
			 	uint16_t bc = pop_stack(c);
			        c->b = bc >> 8; c->c = bc & 0xff;
				break;	
			  }
		case 0xf1:{
			 	uint16_t af = pop_stack(c);
			        c->a = af >> 8;
				unformat_flags(af & 0xff, c);
				break;	
			  }

		// alu
		case 0xb1: bit_or(c->c, c); break;				// or a,c 

		case 0xfe: bit_cmp(fetch8(c), c); break;			// cmp a,u8

		case 0xe6: bit_and(fetch8(c), c); break;			// amd a,u8
			  
		case 0x1c: c->e = inc(c->e, c); break;				// inc e
		case 0x14: c->d = inc(c->d, c); break;				// inc d
		case 0x2c: c->l = inc(c->l, c); break;				// inc l 
		case 0x24: c->h = inc(c->h, c); break; 				// inc h

		case 0x0d: c->c = dec(c->c, c); break;				// dec c
		case 0x05: c->b = dec(c->b, c); break; 				// dec b 

		case 0x23:{ 	// inc r16 (does not modify flags)
				uint16_t hl = get_hl(c) + 1;
				c->h = hl >> 8; c->l = hl & 0xff;
				break;	
			  }
		case 0x03:{
				uint16_t bc = get_bc(c) + 1;		
				c->b = bc >> 8; c->c = bc & 0xff;
				break;
			  }

		// interupts
		case 0xf3: c->IME = 0; break;

		default:
			   printf("unimplmented opcode: %04X\n",opcode);
			   c->is_running = 0;
			   break;
	}
} 


