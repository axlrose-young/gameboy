#ifndef CPU_H
#define CPU_H

#include <stdint.h>

typedef struct {
	bool is_running, IME;

	bool zf, nf, hf, cf;			// flags
	
	uint16_t sp, pc;			// stack ptr, program ctr

	uint8_t a, b, c, d, e, h, l, f;		// registers 
						
						
	char mem[0x10000];
}CPU;

void cpu_step(CPU* const c);

#endif
