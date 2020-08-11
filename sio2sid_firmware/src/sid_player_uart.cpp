/*

<!> Programmer = USBasp for arduino Nano ATmega328

 Brian Tucker 1-5-2015
 sid-arduino-lib:
 https://code.google.com/p/sid-arduino-lib/
 based SID player example program sets
 the 25 SID/6581 registers at 50Hz (delay(19)=~50Hz) 
 The register data is created using the excellent app
 "SIDDumper" from the SIDCog project:
 SIDCog:
 http://forums.parallax.com/showthread.php/118285-SIDcog-The-sound-of-the-Commodore-64-!-%28Now-in-the-OBEX%29
 SIDDumper:
 http://gadgetgangster.com/scripts/displayasset.php?id=361
 For best results use an IRQ at 50 or 60Hz and set the registers programatically.
 For info on how to make music with the SID chip and related registers
 I recomend the e-magazine "Commodore Hacking":
 http://www.ffd2.com/fridge/chacking/
 Specifically the Rob Hubbard play subroutine as that is the code
 all serious C64 SID musicians must understand.
 
 convert the dump to number with : http://www.expertsetup.com/~brian/c64/JConverter.jar / http://softcollection.sytes.net/javaprog
 
*/

// Connect PIN 9 from your arduino Uno to audio output (+ ground to the other pin of the audio output)!
#include <Arduino.h>
#include <avr/pgmspace.h>  //used to store data in the flash rom
#include "Alternative_Fuel_dmp.h"       //RAW SID register data file in flash
//#include "Castlevania_64_Mixes_dmp.h"       //RAW SID register data file in flash
//#include "Zelda_1_Dungeon_dmp.h"       //RAW SID register data file in flash
//#include "Open_Groove_dmp.h"
//#include "Vibralux_dmp.h"
//#include "Visitors_dmp.h"
//#include "marche_hiver_dmp.h"
//#include "/home/forgeot/ownCloud/arduino/sketches/_musique/sidTest/Commando.dmp.h"
//#include "/home/path/ownCloud/arduino/sketches/_musique/sidTest/Commando.dmp.h"
#include <SID.h>           //
#include <SoftwareSerial.h>

#define LED 13
#define LED_OFF     digitalWrite(LED, HIGH);
#define LED_ON      digitalWrite(LED, LOW);

static int serial = 1;
static SID mySid;                 //
SoftwareSerial debugUART(2, 3);

enum {ID, COMMAND, AUX1, AUX2, CHECKSUM, ACK, NAK, PROCESS, WAIT} cmdState;

#define DELAY_T2            50
#define DELAY_T4            850
#define DELAY_T5            250
#define READ_CMD_TIMEOUT    12
#define CMD_TIMEOUT         5000

#define SID_DEBUG
#undef  SID_DEBUG

#define SID_DEVICE   's'
#define SID_ACK      'A'
#define SID_NACK     'N'
#define SID_COMPLETE 'C'
#define SID_ERROR    'E'
#define SID_COMMAND_REGISTER  0x01
#define SID_COMMAND_PLAY      0x02

static unsigned long cmdTimer = 0;

union {
    struct {
        unsigned char devic;
        unsigned char comnd;
        unsigned char aux1;
        unsigned char aux2;
        unsigned char cksum;
    };
    unsigned char cmdFrameData[5];
} cmdFrame;

typedef enum {
    NOOP,
    REG,
    PLAY
} t_status;

#define SID_REGISTERS 25
typedef struct {
	unsigned char registers[SID_REGISTERS];
	unsigned char cksum;
} t_sid_dataframe;

static void sio_process(void);
static void sio_complete(void);
static void sio_ack(void);
static void sio_nack(void);
static void sio_get_id(void);
static void sio_get_command(void);
static void sio_get_aux1(void);
static void sio_get_aux2(void);

static uint8_t csum(uint8_t *d, int len)
{
	int c = 0;
	for (int i = 0; i < len; i++)
		c = ((c + d[i]) >> 8) + (u8)(c + d[i]);
	return c;
}

// Get ID
static void sio_get_id(void)
{
    cmdFrame.devic = Serial.read();
    if (cmdFrame.devic == SID_DEVICE)
        cmdState = COMMAND;
    else
    {
        cmdState = WAIT;
        cmdTimer = 0;
    }
#ifdef SID_DEBUG
    debugUART.print("CMD DEVC: ");
    debugUART.println(cmdFrame.devic, HEX);
#endif
}

// Get COMMAND
static void sio_get_command(void)
{
    cmdFrame.comnd = Serial.read();
    cmdState = AUX1;
#ifdef SID_DEBUG
    debugUART.print("CMD CMND: ");
    debugUART.println(cmdFrame.comnd, HEX);
#endif
}

// Get AUX1
static void sio_get_aux1(void)
{
    cmdFrame.aux1 = Serial.read();
    cmdState = AUX2;
#ifdef SID_DEBUG
    debugUART.print("CMD AUX1: ");
    debugUART.println(cmdFrame.aux1, HEX);
#endif
}

// Get AUX2
static void sio_get_aux2(void)
{
    cmdFrame.aux2 = Serial.read();
    cmdState = CHECKSUM;
#ifdef SID_DEBUG
    debugUART.print("CMD AUX2: ");
    debugUART.println(cmdFrame.aux2, HEX);
#endif
}


// SID Read Frame
static void sid_read_frame(void)
{
    Serial.flush();
    cmdTimer = millis();

    // Do busy loop here!
    for (;;)
    {
        // We need to read 25 + 1 bytes within a vblank 50Hz
        #define PAL_HZ       (1000/50)
        #define NTSC_HZ      (1000/60)
        #define VBLANK_MS    PAL_HZ
        if (Serial.available() > SID_REGISTERS)
        {
            t_sid_dataframe siddataframe;
            unsigned char cksum;
            Serial.readBytes((uint8_t *) &siddataframe.registers[0], SID_REGISTERS);
            cksum = Serial.read();
            if (cksum != csum((uint8_t *) &siddataframe.registers[0], SID_REGISTERS))
            {
                debugUART.println("BAD DATAFRAME CHECKSUM");
                delayMicroseconds(DELAY_T4);
                Serial.write(SID_NACK);
                Serial.flush();
                cmdState = WAIT;
                cmdTimer = 0;
            }
            else
            {
                for (int sidRegister = 0; sidRegister <= (SID_REGISTERS - 1); sidRegister++)
                    mySid.set_register(sidRegister, siddataframe.registers[sidRegister]);
                delayMicroseconds(DELAY_T4);
                Serial.write(SID_ACK);
                sio_complete();
            }
            break;
        }
        else
        {
            // Wait a little bit more...
        }
    }
}

// Process command
static void sio_process(void)
{
    switch (cmdFrame.comnd)
    {
        case SID_COMMAND_REGISTER:
#ifdef SID_DEBUG
            debugUART.println("ASK SID REGISTER WRITE");
#endif
            sid_read_frame();
            cmdState = ID;
            cmdTimer = millis();
            break;
        case SID_COMMAND_PLAY:
#ifdef SID_DEBUG
            debugUART.println("ASK SID TO PLAY INTERNAL TUNE");
#endif
            break;
        default:
            cmdState = WAIT;
            cmdTimer = 0;
            break;
    }
}

// Send an acknowledgement
static void sio_complete(void)
{
    delayMicroseconds(DELAY_T5);
    Serial.write(SID_COMPLETE);
    Serial.flush();
}

// Send an acknowledgement
static void sio_ack(void)
{
    delayMicroseconds(DELAY_T2);
    Serial.write(SID_ACK);
    Serial.flush();
    sio_process();
}

// Send an non-acknowledgement
static void sio_nack(void)
{
    delayMicroseconds(DELAY_T2);
    Serial.write(SID_NACK);
    Serial.flush();
    cmdState = WAIT;
    cmdTimer = 0;
}


// Get CheckSum and compare
static void sio_get_checksum(void)
{
    unsigned char ck;
    cmdFrame.cksum = Serial.read();
    ck = csum((uint8_t *) &cmdFrame.cmdFrameData, 4);
#ifdef SID_DEBUG
    debugUART.print("CMD CHKSUM: ");
    debugUART.println(cmdFrame.cksum, HEX);
#endif
    if (ck == cmdFrame.cksum)
    {
#ifdef SID_DEBUG
        debugUART.println("--> ACK");
#endif
        sio_ack();
    }
    else
    {
#ifdef SID_DEBUG
        debugUART.println("--> NACK");
#endif
        // As page 14 of the SIOSPEC pdf documents:
        // COMMAND FRAME ACKNOWLEDGE
        // The peripheral being addressed would normally respond to
        // a command frame by sending an ACK byte ($41) to the computer;
        // if there is a checksum problem with the command frame, the
        // peripheral should not respond.
        // sio_nack();
        Serial.flush();
        cmdState = WAIT;
        cmdTimer = 0;
    }
}

static void sio_incoming(void)
{
    switch(cmdState)
    {
        case ID:
            sio_get_id();
            break;
        case COMMAND:
            sio_get_command();
            break;
        case AUX1:
            sio_get_aux1();
            break;
        case AUX2:
            sio_get_aux2();
            break;
        case CHECKSUM:
            sio_get_checksum();
            break;
        case ACK:
            sio_ack();
            break;
        case NAK:
            sio_nack();
            break;
        case PROCESS:
            sio_process();
            break;
        case WAIT:
            Serial.flush(); // Toss it for now
            cmdTimer = 0;
            break;
    }
}


void setup(void)
{
    // Initialize SID emulator
    mySid.begin();
    // Initialize DEBUG Port
    debugUART.begin(115200);
    // Initialize SIO UART Port
    Serial.begin(19200);
   
    debugUART.println("#################");
    debugUART.println("# SIO2SID ATARI #");
    debugUART.println("#################");
    cmdState = ID;
    cmdTimer = millis();
}



void loop(void)
{
    if (Serial.available() > 0)
    {
        sio_incoming();
    }
 
    if (millis() - cmdTimer > CMD_TIMEOUT && cmdState != WAIT)
    {
#ifdef SID_DEBUG
        debugUART.print("SIO CMD TIMEOUT: ");
        debugUART.println(cmdState);
#endif
        cmdState = WAIT;
        cmdTimer = 0;
        Serial.flush();
    }
}
