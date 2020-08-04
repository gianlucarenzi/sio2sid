/*
 * fonts.c
 *
 *  Created on: Jul 9, 2020
 *      Author: debian
 */
#include "fonts.h"

#define RAMTOP (0x006A)

static end_of_memory(void)
{
	unsigned char top = *((unsigned char *)RAMTOP);
	int byte;
	for (;;)
	{
		byte = top << 8;
		unsigned char test = 0xFF - *((unsigned char *) byte);
		*((unsigned char *) byte) = test;
		if (*((unsigned char *)byte) == test)
		{
			++top;
			*((unsigned char *) byte) = 0xFF - test;
		}
		else
			break;
	}
	printf("MEMORY ENDS AT %d\n", byte);
}

int setup_fonts(void)
{
	unsigned char * chargen;
	unsigned char * charbase;
	unsigned char * chbase = (unsigned char *) CHBASEOS;

	charbase = (unsigned char *) CHBASE;
#ifndef __ATARIXL__
	// New character base
	chargen = (unsigned char *) CHRAMBASE;
	memcpy(chargen, charbase, 1024);
#else
	// Character base in ROM/RAM
	chargen = charbase;
#endif
	// Commodore 64 Chargen ROM */
	// chars from 0 to 31 ' !"#...
	addr = (unsigned char *) (chargen + (0 * 8));
	for (reg = 0; reg < (32 * 8); reg++)
		*(addr + reg) = c64_font[reg + (32 * 8)];

	// chars from 32 to 63 '@ABC...[\]^_
	addr = (unsigned char *) (chargen + (32 * 8));
	for (reg = 0; reg < (32 * 8); reg++)
		*(addr + reg) = c64_font[reg];

	// chars from 64 to 95...
	addr = (unsigned char *) (chargen + (64 * 8));
	for (reg = 0; reg < (32 * 8); reg++)
		*(addr + reg) = c64_font[reg + (64 * 8)];

#ifndef __ATARIXL__
	*(chbase) = (CHRAMBASE & 0xff00) >> 8;
#else
	*(chbase) = (CHBASE & 0xff00) >> 8;
#endif

}


