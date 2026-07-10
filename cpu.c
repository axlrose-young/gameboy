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

static inline void bit_xor(uint8_t val, CPU* const c){
	c->a ^= val;
	set_zf(c->a,c);
	c->nf = c->hf = c->cf = 0;
}

static inline uint8_t inc(uint8_t val, CPU* const c){
	c->hf = ((val&0x0f) + (0x01) > 0x0f);
	val++;
	set_zf(val, c);
	c->nf = 0;
	return val;
}

static inline void inc_rp(uint8_t* hi, uint8_t* lo){	
	uint16_t val = *hi << 8 | *lo;
	val++;
	*hi = val >> 8;
	*lo = val & 0xff;	
}

static inline uint8_t dec(uint8_t val, CPU* const c){
	c->hf = ((val&0x0f) < (0x01));
	val--;
	set_zf(val,c);
	c->nf = 1;
	return val;
}

static inline void dec_rp(uint8_t* hi, uint8_t* lo){
	uint16_t val = *hi << 8 | *lo;
	val--;
	*hi = val >> 8;
	*lo = val & 0xff;	
}

static inline void add(uint8_t val, CPU* const c){
	c->hf = ((c->a&0x0f) + (val&0x0f) > 0x0f);
	uint16_t result = (uint16_t)c->a + val;	
	c->cf = (result > 0xff);

	c->a += val;
	set_zf(c->a,c);
	c->nf = 0;
}

static inline void add16(uint16_t val, CPU* const c){
	uint16_t result = get_hl(c) + val;

	c->cf = (result < val);	// result wraps around 
	c->hf = ((result&0xff) + (val&0xff) > 0xff);
	c->nf = 0;

	c->h = result >> 8;
	c->l = result & 0xff;
}

static inline void adc(uint8_t val, CPU* const c){
	uint16_t result = c->a + c->cf + val;

	c->hf = ((c->a&0x0f) + c->cf + (val&0x0f) > 0x0f);  
	c->cf = (result > 0xff);
	set_zf(result&0xff, c);
	c->nf = 0;

	c->a = result & 0xff;
}

static inline void sub(uint8_t val, CPU* const c){
	uint8_t result = c->a - val;

	c->hf = ((c->a&0x0f) < (val&0x0f));
	c->cf = (c->a < val);
	set_zf(result,c);
	c->nf = 0;

	c->a = result;
}

// this is cb prefixed helpers
void cb_step(uint8_t cb_opcode, CPU* const c);

static inline void cb_rr(uint8_t* val, CPU* const c){
	uint8_t cy_in = (uint8_t)c->cf;

	c->cf = *val & 0x01;
	*val >>= 1;
	*val |= (cy_in << 7);

	c->zf = (*val == 0);
	c->nf = c->hf = 0;
}

void cpu_step(CPU* const c){
	uint8_t opcode = fetch8(c);

//	printf("opcode: %04X, pc: %04X\n",opcode,c->pc);	

	if(opcode == 0xcb){
		uint8_t cb_opcode = fetch8(c);
		cb_step(cb_opcode,c);
		return;
	}

	switch(opcode){
		case 0x00: break;	
		
		// jmp
		case 0xc3: c->pc = fetch16(c); break;	
		case 0xe9: c->pc = get_hl(c); break;					// jmp hl

		case 0x20:{
				int8_t offset = (int8_t)fetch8(c);
				if(!c->zf) c->pc += offset; 
				break; 
			  }

		case 0x28:{
			 	int8_t offset = (int8_t)fetch8(c); 
				if(c->zf) c->pc += offset;	
				break;
			  }

		case 0x30:{
			 	int8_t offset = (int8_t)fetch8(c);
			        if(!c->cf) c->pc += offset;
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

		case 0xc9: c->pc = pop_stack(c); break;					// ret
		case 0xd0: c->pc = (!c->cf) ? pop_stack(c) : c->pc; break;		// ret nc
		case 0xc8: c->pc = (c->zf) ? pop_stack(c) : c->pc; break;		// ret z


		// load
		case 0x78: c->a = c->b; break; 
		case 0x79: c->a = c->c; break;						// ld a,c
		case 0x7a: c->a = c->d; break;						// ld a,d
		case 0x7b: c->a = c->e; break;						// ld a,e
		case 0x7d: c->a = c->l; break;
		case 0x7c: c->a = c->h; break;
		case 0x47: c->b = c->a; break;
		case 0x4f: c->c = c->l; break;						// ld c,a
		case 0x57: c->d = c->a; break;						// ld d,a
		case 0x5f: c->e = c->a; break;						// ld e,a
		case 0x5d: c->e = c->l; break;						// ld e,l
		case 0x6f: c->l = c->a; break;						// ld l,a
		case 0x21: c->l = fetch8(c); c->h = fetch8(c); break; 
		case 0x11: c->e = fetch8(c); c->d = fetch8(c); break;
		case 0x01: c->c = fetch8(c); c->b = fetch8(c); break;
		case 0x31: c->sp = fetch16(c); break;

		case 0x0e: c->c = fetch8(c); break;
		case 0x3e: c->a = fetch8(c); break;
		case 0x06: c->b = fetch8(c); break;
		case 0x26: c->h = fetch8(c); break;					// ld h,u8
		case 0x2e: c->l = fetch8(c); break;					// ld l,u8

		case 0xfa: c->a = c->mem[fetch16(c)]; break;
		case 0xf0: c->a = c->mem[(fetch8(c) + 0xff00)]; break;

		case 0x1a: c->a = c->mem[get_de(c)]; break;
		case 0x56: c->e = c->mem[get_hl(c)]; break;				// ld d,hl
		case 0x5e: c->e = c->mem[get_hl(c)]; break;				// ld e,hl
		case 0x6e: c->l = c->mem[get_hl(c)]; break;				// ld l,hl

		case 0x2a: 
			   uint16_t hl = get_hl(c);
			   c->a = c->mem[hl];
			   hl++;
			   c->h = hl >> 8;
			   c->l = hl & 0xff;
			   break;

		// write operations
		case 0x12: mem_write(get_de(c), c->a, c); break;
		case 0x71: mem_write(get_hl(c), c->c, c); break;		// ld hl,c
		case 0x72: mem_write(get_hl(c), c->d, c); break;		// ld hl,d
		case 0x73: mem_write(get_hl(c), c->e, c); break;		// ld hl,e
		case 0x77: mem_write(get_hl(c), c->a, c); break;
		case 0xea: mem_write(fetch16(c), c->a, c); break; 

		case 0x22: mem_write(get_hl(c), c->a, c); 			// ld hl++,a
			   inc_rp(&c->h, &c->l); 
			   break; 

		case 0x32: mem_write(get_hl(c), c->a, c);			// ld hl--,a
			   dec_rp(&c->h, &c->l); 
			   break;

		case 0xe0: mem_write(0xff00 + fetch8(c), c->a, c); break;
		case 0x35: mem_write(get_hl(c), dec(c->mem[get_hl(c)],c), c); break;  	// dec hl 

		// push
		case 0xe5: push_stack(c->h, c->l, c); break;
		case 0xd5: push_stack(c->d, c->e, c); break;			// push de
		case 0xc5: push_stack(c->b, c->c, c); break;
		case 0xf5: push_stack(c->a, format_flags(c), c); break;

		// pop
		case 0xe1:{ 
				uint16_t hl = pop_stack(c); 
				c->h = hl >> 8; c->l = hl & 0xff; 
				break;
			  }
		case 0xd1:{
			 	uint16_t de = pop_stack(c);
			        c->d = de >> 8; c->e = de & 0xff;
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
		case 0xb7: bit_or(c->a, c); break;				// or a,a
		case 0xb6: bit_or(c->mem[get_hl(c)], c); break;			// or a,hl
	
		case 0xa9: bit_xor(c->c, c); break; 				// xor a,c
		case 0xaf: bit_xor(c->a, c); break;				// xor a,a
		case 0xee: bit_xor(fetch8(c),c); break;				// xor a,u8

		case 0xfe: bit_cmp(fetch8(c), c); break;			// cmp a,u8

		case 0xe6: bit_and(fetch8(c), c); break;			// and a,u8
			  
		case 0x1c: c->e = inc(c->e, c); break;				// inc e
		case 0x14: c->d = inc(c->d, c); break;				// inc d
		case 0x2c: c->l = inc(c->l, c); break;				// inc l 
		case 0x24: c->h = inc(c->h, c); break; 				// inc h

		case 0x0d: c->c = dec(c->c, c); break;				// dec c
		case 0x05: c->b = dec(c->b, c); break; 				// dec b 
		case 0x1d: c->e = dec(c->e, c); break; 				// dec e
		case 0x25: c->h = dec(c->h, c); break;				// dec h
		case 0x2d: c->l = dec(c->l, c); break;				// dec l
		case 0x3d: c->a = dec(c->a, c); break;				// dec a

		case 0x23: inc_rp(&c->h, &c->l); break;				// inc hl
		case 0x13: inc_rp(&c->d, &c->e); break;				// inc de
		case 0x03: inc_rp(&c->b, &c->c); break;				// inc bc

		case 0xc6: add(fetch8(c), c); break;				// add a,u8

		case 0xce: adc(fetch8(c), c); break;  				// adc a,u8

		case 0x29: add16(get_hl(c), c); break;				// add hl,hl

		case 0xd6: sub(fetch8(c), c); break;				// sub u8

		// interupts
		case 0xf3: c->IME = 0; break;

		// rotates
		case 0x1f:							// rra
			   uint8_t cy_in = (uint8_t)c->cf;
			   c->cf = c->a & 0x01;
			   c->a >>= 1;
			   c->a |= (cy_in << 7); 
			   c->nf = c->hf = c->zf = 0;
			   break;

		default:
			   printf("unimplmented opcode: %04X\n",opcode);
			   c->is_running = 0;
			   break;
	}
} 

void cb_step(uint8_t cb_opcode, CPU* const c){
	switch(cb_opcode){
		case 0x19: cb_rr(&c->c, c); break;				// rr c
		case 0x1a: cb_rr(&c->d, c); break;				// rr d
		case 0x1b: cb_rr(&c->e, c); break;				// rr e

		default:
			   printf("unimplemented cb opcode: %04X\n",cb_opcode);
			   c->is_running = 0;
			   break;
	}
}


