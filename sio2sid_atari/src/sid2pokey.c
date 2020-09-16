#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h>
#include <atari.h>
#include <cc65.h>
#include <6502.h>
#include <stdlib.h>
#include <peekpoke.h>
#include <dbg.h>
#include "c64font.h"
#include "siddump.h"

#define CRAZY_COLOR		*((unsigned char *) 0xD01A) = rand();

#define SIOREAD  0x40
#define SIOWRITE 0x80

typedef struct {
	unsigned char ddevic;
	unsigned char dunit;
	unsigned char dcomnd;
	unsigned char dstats;
	unsigned char * dbuf;
	unsigned char dtimlo;
	unsigned int dbyt;
	unsigned char daux1;
	unsigned char daux2;
} t_dcb;

#define SID_DEVICE   's'
#define SID_ACK      'A'
#define SID_NACK     'N'
#define SID_COMPLETE 'C'
#define SID_ERROR    'E'
#define SID_COMMAND_REGISTER  0x01
#define SID_COMMAND_PLAY      0x02
#define SID_REGISTERS_SIZE 25

extern void siov();

#define CHBASE		0xE000
#define CHRAMBASE	0x8000
#define CHBASEOS	0x02F4

/* Screen dimensions */
static unsigned char XSize, YSize;

static void MakeTeeLine (unsigned char Y)
/* Make a divider line */
{
    cputcxy (0, Y, CH_LTEE);
    chline (XSize - 2);
    cputc (CH_RTEE);
}


static void MakeNiceScreen (void)
/* Make a nice screen */
{
    typedef struct {
        unsigned char   Y;
        char*           Msg;
    } TextDesc;
    static TextDesc Text [] = {                       //
        {   2, "           ATARI SID Player           "},
        {   4, "       (C) 2020 Gianluca Renzi        "},
        {   5, "       (RetroBit Lab TechnoGuy)       "},
        {   9, "     Using SID Emulation using SIO    "},
        {  11, "  OS calls and protocols at maximum   "},
        {  12, "     speed available. It will be      "},
		{  13, "    ported to SPI65 interface asap    "},
        {  15, "          (icjtqr@gmail.com)          "},
        {  22, "        Press any key to quit...      "},
    };

    register const TextDesc* T;
    unsigned char I;
    unsigned char X;

    textcolor (COLOR_WHITE);
    bordercolor (COLOR_LIGHTBLUE);
    bgcolor (COLOR_BLUE);
    clrscr ();
    cursor (0);

    /* Top line */
    cputcxy (0, 0, CH_ULCORNER);
    chline (XSize - 2);
    cputc (CH_URCORNER);

    /* Left line */
    cvlinexy (0, 1, 22);

    /* Bottom line */
    cputc (CH_LLCORNER);
    chline (XSize - 2);
    cputc (CH_LRCORNER);

    /* Right line */
    cvlinexy (XSize - 1, 1, 22);

    /* Several divider lines */
    MakeTeeLine (7);
    MakeTeeLine (21);

    /* Write something into the frame */
    for (I = 0, T = Text; I < sizeof (Text) / sizeof (Text [0]); ++I) {
        X = (XSize - strlen (T->Msg)) / 2;
        cputsxy (X, T->Y, T->Msg);
        ++T;
    }
}

static void DoC64Font(void)
{
	unsigned char * chargen;
	unsigned char * charbase;
	unsigned char * chbase = (unsigned char *) CHBASEOS;
	unsigned char * addr;
	int reg;

	charbase = (unsigned char *) CHBASE;

	// New character base
	chargen = (unsigned char *) CHRAMBASE;
	memcpy(chargen, charbase, 1024);

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
//	addr = (unsigned char *) (chargen + (64 * 8));
//	for (reg = 0; reg < (32 * 8); reg++)
//		*(addr + reg) = c64_font[reg + (64 * 8)];

	*(chbase) = (CHRAMBASE & 0xff00) >> 8;
}

static void setup(void)
{
	unsigned char sdmctl;
	OS.coldst = 1;		// Reset coldstart flag to force a coldstart on RESET.
	sdmctl = OS.sdmctl;
	OS.sdmctl = 0x00;	// Turn off DMA

	DoC64Font();

	OS.sdmctl = sdmctl;	// Turn on DMA like before
	srand(0xdeadbeef);
}
//                       ---------.---------.---------.---------.
const char error_138[] ="       SID EMULATOR NOT RESPONDING   ";
const char error_139[] ="           SID EMULATOR NAK          ";
const char error[]     ="              SIO ERROR              ";

/**
 * Show error
 */

void err_sio(void)
{
	switch (OS.dcb.dstats)
	{
		case 138:
			cputsxy(4, 19,"    SID EMULATOR NOT RESPONDING   ");
			break;
		case 139:
			cputsxy(4, 19,"        SID EMULATOR NAK          ");
			break;
		default:
			cputsxy(4, 19,"           SIO ERROR              ");
			break;
	}
}

static unsigned char sio_call(t_dcb *sio)
{
	OS.dcb.ddevic = sio->ddevic;
	OS.dcb.dunit  = sio->dunit;
	OS.dcb.dcomnd = sio->dcomnd;
	OS.dcb.dstats = sio->dstats;
	OS.dcb.dbuf   = sio->dbuf;
	OS.dcb.dtimlo = sio->dtimlo;
	OS.dcb.dbyt   = sio->dbyt;
	OS.dcb.daux1  = sio->daux1;
	OS.dcb.daux2  = sio->daux2;
	siov();

	if (OS.dcb.dstats != 0x01)
	{
		err_sio();
//		exit(OS.dcb.dstats);
	}
	return OS.dcb.dstats;
}

static void udelay(int l)
{
	int c;
	for (c = 0; c < l; c++)
		__asm__("nop");
}

static void mdelay(int l)
{
	int c;
	for (c = 0; c < l; c++)
		udelay(100);
}

int main(int argc, char * argv[])
{
	t_dcb dcb;

	(void) argc;
	(void) argv;

	setup();

	/* Get the screen dimensions */
	screensize (&XSize, &YSize);

	MakeNiceScreen();

    dcb.ddevic = SID_DEVICE;
    dcb.dunit  = 1;  // 1
    dcb.dcomnd = SID_COMMAND_REGISTER;
    dcb.dstats = SIOWRITE;
    dcb.dbuf   = NULL; // must be filled in the loop
    dcb.daux1  = 0;
    dcb.daux2  = 0;
    dcb.dbyt   = SID_REGISTERS_SIZE;
    dcb.dtimlo = 0x7f; // max delay timeout?

    for (;;)
    {
    	int sidPointer;
    	for (sidPointer = 0; sidPointer <= sidLength; sidPointer++)
    	{
    		dcb.dbuf = (unsigned char *) &sidData[ sidPointer ];
    		sio_call(&dcb);
    		sidPointer += (SID_REGISTERS_SIZE - 1);
    	}
	}
    return 0;
}
