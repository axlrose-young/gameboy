#include <stdio.h>

#include "cpu.h"

static void cpu_init(CPU* const c){
	c->pc = 0x100;
	c->is_running = 1;

	FILE* pfile = fopen("test-roms/cpu_instrs/individual/04-op-r,imm.gb", "rb");

	if(pfile == NULL){
		perror("error reading file");	
	}
	else{
		fseek(pfile,0,SEEK_END);
		size_t filesize = ftell(pfile);

		// go back to start of file 
		rewind(pfile);
		size_t count = fread(c->mem, 1, filesize, pfile);
		if(count != filesize){
			perror("filesizes do not match");	
		}
	}
	fclose(pfile);
}

int main(){
	CPU c = { 0 };
	cpu_init(&c);

	while(c.is_running){
		cpu_step(&c);	
	}

	return 0;
}
