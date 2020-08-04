/*
 * -------------------------------------------
 * SIO Emulation Code for Arduino SID Emulator
 * -------------------------------------------
 *
 * written by: Gianluca Renzi
 * mailto: <icjtqr@gmail.com>
 *
 * I hope it will cover all issues for serial programming
 * 
 * Good luck! 
 *
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <linux/serial.h>
#include <sys/types.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <inttypes.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <termios.h>
#include "serial.h"
#include "debug.h"
#include "ec_types.h"

// C64 SID Dump
#include "siddump.h"

static int debuglevel = DBG_ERROR;

// Human date/time macros
#define USEC(a)       (a)
#define MSEC(a)       (USEC(a * 1000L))
#define SEC(a)        (MSEC(a * 1000L))
#define MIN(a)        (SEC(a * 60))
#define HOUR(a)       (MIN(a * 60))
#define DAY(a)        (HOUR(a * 24))

extern const char *fwBuild;

#define BUFFER_SIZE           (4096)
#define TIMEOUT_MAIN_MS       (15000)
#define TIMER_TICK            (0)
#define SID_DEVICE            's'
#define SID_ACK               'A'
#define SID_NACK              'N'
#define SID_COMPLETE          'C'
#define SID_ERROR             'E'
#define SID_COMMAND_REGISTER  0x01
#define SID_COMMAND_PLAY      0x02
#define SID_REGISTERS_SIZE    (25)

typedef struct {
	unsigned char ddevic;
	unsigned char cmd;
	unsigned char aux1;
	unsigned char aux2;
	unsigned char cksum;
} t_sio_command;

typedef struct {
	unsigned char registers[SID_REGISTERS_SIZE];
	unsigned char cksum;
} t_sid_dataframe;

static void print_hex_ascii_line(const unsigned char *payload, int len, int offset)
{
	int i;
	int gap;
	const unsigned char *ch;

	// offset
	printR("%05x   ", offset);

	// hex
	ch = payload;
	for (i = 0; i < len; i++)
	{
		printR("%02x ", *ch);
		ch++;
		// print extra space after 8th byte for visual aid
		if (i == 7)
			printR(" ");
	}
	// print space to handle line less than 8 bytes
	if (len < 8)
	{
		printR(" ");
	}

	// fill hex gap with spaces if not full line
	if (len < 16)
	{
		gap = 16 - len;
		for (i = 0; i < gap; i++)
		{
			printR("   ");
		}
	}
	printR("   ");

	// ascii (if printable)
	ch = payload;
	for(i = 0; i < len; i++)
	{
		if (isprint(*ch))
		{
			printR("%c", *ch);
		}
		else
		{
			printR(".");
		}
		ch++;
	}
	printR("\n");
}

// print packet payload data (avoid printing binary data)
void print_payload(const char *func, const unsigned char *payload, int len, int dbglvl)
{
	int len_rem = len;
	int line_width = 16;           // number of bytes per line
	int line_len;
	int offset = 0;                // zero-based offset counter
	const unsigned char *ch = payload;

	// Stampiamo il payload se siamo >= DBG_VERBOSE oppure
	// in errore.
	if ((dbglvl >= DBG_VERBOSE) || (dbglvl == DBG_ERROR))
	{
		printR("Enter %p LEN: %d from %s\n", payload, len, func);

		if (len <= 0)
		{
			printR("No LEN. Exit\n");
			return;
		}

		// data fits on one line
		if (len <= line_width)
		{
			print_hex_ascii_line(ch, len, offset);
			printR("Small Line. Exiting\n");
			return;
		}

		// data spans multiple lines
		for (;;)
		{
			// compute current line length
			line_len = line_width % len_rem;
			// print line
			print_hex_ascii_line(ch, line_len, offset);
			// compute total remaining
			len_rem = len_rem - line_len;
			// shift pointer to remaining bytes to print
			ch = ch + line_len;
			// add offset
			offset = offset + line_width;
			// check if we have line width chars or less
			if (len_rem <= line_width)
			{
				// print last line and get out
				print_hex_ascii_line(ch, len_rem, offset);
				break;
			}
		}
		printR("Exit\n");
	}
}


static void banner(void)
{
	fprintf(stdout, "\n\n");
	fprintf(stdout, ANSI_BLUE "ATARI SIO Emulation" ANSI_RESET "\n");
	fprintf(stdout, ANSI_YELLOW );
	fprintf(stdout, "FWVER: %s", fwBuild);
	fprintf(stdout, ANSI_RESET "\n");
	fprintf(stdout, "\n\n");
}

static void signal_handle(int sig)
{
	char signame[8];

	switch( sig )
	{
		case SIGSEGV:
			sprintf(signame, "SIGSEGV");
			DBG_E("signal %s - %d caught\n", signame, sig);
			break;
		case SIGINT:
			sprintf(signame, "SIGINT");
			DBG_E("signal %s - %d caught\n", signame, sig);
			break;
		case SIGTERM:
			sprintf(signame, "SIGTERM");
			DBG_E("signal %s - %d caught\n", signame, sig);
			break;
		case SIGUSR1:
			return;
			break; // NEVERREACHED
		case SIGUSR2:
			return;
			break; // NEVERREACHED
		default:
			sprintf(signame, "UNKNOWN");
			DBG_E("signal %s - %d caught\n", signame, sig);
			break;
	}
	exit(sig);
}

static void version(const char * filename, const char *ver)
{
	char version[256];

	if (filename == NULL || ver == NULL)
		return;

	memset(version, 0, 256);

	sprintf(version, "mkdir -p /tmp/%s && echo %s > /tmp/%s/version",
			filename, ver, filename);
	if (system(version) != 0)
	{
		DBG_E("Error: %s gives error %d\n", version, errno);
	}
}


static uint8_t csum(uint8_t *d, int len)
{
	int i;
	int sum = 0;
	DBG_N("Enter: SUM %d\n", sum);

	for (i = 0; i < len; i++)
	{
		sum += (uint8_t)*(d+i);
		if (sum > 255)
		{
			sum -= 255;
		}
	}
	i = (uint8_t) sum;
	DBG_N("Exit: SUM %d\n", i);
	return i;
}


static int sio_send_command_frame(int sioport, t_sio_command *siocmdframe)
{
	int rval = -1;

	if (siocmdframe == NULL)
		return rval;

	for (;;)
	{
		// Send SIO Command
		rval = serial_send_raw(sioport, (const unsigned char *) siocmdframe, sizeof(t_sio_command));
		if (rval <= 0)
		{
			if (errno != EAGAIN && errno != EINTR)
			{
				DBG_E("Error on SENDING COMMAND FRAME\n");
				break;
			}
			// Se siamo stati interrotti o la periferica non e' disponibile
			// ci riproviamo
		}
		else
		{
			if (rval != sizeof(t_sio_command))
			{
				DBG_E("BAD WRITE - ERROR\n");
				rval = -1;
				break;
			}
			else
			{
				DBG_V("\t\t*** COMMAND HEADER SENT ***\n");
				rval = 0;
				break;
			}
		}
	}

	if (rval < 0)
	{
		// Unrecoverable error!
		DBG_E("UNRECOVERABLE ERROR\n");

	}
	else
	{
		DBG_V("SIO COMMAND FRAME SENT\n");
		if (debuglevel >= DBG_NOISY)
			print_payload(__FUNCTION__, (const unsigned char*) &siocmdframe, sizeof(t_sio_command), 0); 
	}

	DBG_N("Exit with: %d\n", rval);
	return rval;
}


static int sio_command_frame(int sioport)
{
	t_sio_command siocmdframe;

	// Prepare SIO COMMAND FRAME for SID
	siocmdframe.ddevic = SID_DEVICE;
	siocmdframe.cmd    = SID_COMMAND_REGISTER;
	siocmdframe.aux1   = 0;
	siocmdframe.aux2   = 0;
	siocmdframe.cksum  = csum((uint8_t *) &siocmdframe, 4);

	return sio_send_command_frame(sioport, &siocmdframe);
}

static int sio_send_data_frame(int sioport, const unsigned char *buff)
{
	t_sid_dataframe siddataframe;
	int rval;

	// Now fillup data bytes
	for (int i = 0; i < SID_REGISTERS_SIZE; i++)
		siddataframe.registers[ i ] = *(buff + i );

	// Add checksum
	siddataframe.cksum = csum((uint8_t *) &siddataframe.registers[0], SID_REGISTERS_SIZE);

	// Send dataframe
	for (;;)
	{
		rval = serial_send_raw(sioport, (const unsigned char *) &siddataframe, sizeof(t_sid_dataframe));
		if (rval <= 0)
		{
			if (errno != EAGAIN && errno != EINTR)
			{
				DBG_E("Error on SENDING DATA FRAME\n");
				break;
			}
			// Se siamo stati interrotti o la periferica non e' disponibile
			// ci riproviamo
		}
		else
		{
			if (rval != sizeof(t_sid_dataframe))
			{
				DBG_E("BAD WRITE (2) - ERROR\n");
				rval = -1;
				break;
			}
			else
			{
				DBG_V("\t\t*** SID DATAFRAME SENT ***\n");
				rval = 0;
				break;
			}
		}
	}

	if (rval >= 0)
	{
		DBG_N("SID DATA FRAME SENT\n");
		if (debuglevel >= DBG_NOISY)
			print_payload("SIO SENT:", (const unsigned char*) &siddataframe, sizeof(t_sid_dataframe), 0);
		rval = 0;
	}

	DBG_N("Exit with: %d\n", rval);
	return rval;
}


static int sio_ack(int sioport)
{
	int rval;
	unsigned char rcvx[1];

	rcvx[0] = 0;

	// Now wait 'A', 'N'
	for (;;)
	{
		rval = serial_read_raw(sioport, rcvx, 1);
		if (rval < 0)
		{
			if (errno != EAGAIN && errno != EINTR)
			{
				DBG_E("Error on AWAITING FRAME RESPONSE\n");
				break;
			}
			// per qualsiasi altro errore possiamo riprovarci!
		}
		else
		if (rval == 0)
		{
			DBG_N("Wait a little bit more...\n");
		}
		else
		{
			if (rval != 1)
			{
				DBG_E("WEIRD RECEIVING!\n");
				rval = -1;
				break;
			}
			else
			{
				// RECEIVING GOOD
				rval = 1; // 1 means ack
				break;
			}
		}
	}

	if (rval > 0)
	{
		switch (rcvx[0])
		{
			case 'A':
				DBG_N("DEVICE ACKNOWLEDGED\n");
				rval = 1;
				break;
			case 'N':
				DBG_E("DEVICE NOT ACKNOWLEDGED. GARBAGE\n");
				rval = 0;
				break;
			default:
				DBG_E("UNKNOWN DATA 0x%02x\n", rcvx[0]);
				rval = -1;
				break;
		}
	}

	DBG_N("Exit with: %d\n", rval);
	return rval;
}

static int sio_complete(int sioport)
{
	int rval;
	unsigned char rcvx[1];

	rcvx[0] = 0;

	// Now wait 'C', 'E'
	for (;;)
	{
		rval = serial_read_raw(sioport, rcvx, 1);
		if (rval < 0)
		{
			if (errno != EAGAIN && errno != EINTR)
			{
				DBG_E("Error on AWAITING COMPLETEE/ERROR\n");
				break;
			}
			// per qualsiasi altro errore possiamo riprovarci!
		}
		else
		if (rval == 0)
		{
			DBG_N("Wait a little bit more...\n");
		}
		else
		{
			if (rval != 1)
			{
				DBG_E("WEIRD RECEIVING!\n");
				rval = -1;
				break;
			}
			else
			{
				// RECEIVING GOOD
				rval = 1; // 1 means 'C' or 'E'
				break;
			}
		}
	}

	if (rval > 0)
	{
		switch (rcvx[0])
		{
			case 'C':
				DBG_N("DEVICE COMMAND COMPLETED\n");
				rval = 1;
				break;
			case 'E':
				DBG_E("DEVICE COMMAND ERROR\n");
				rval = 0;
				break;
			default:
				DBG_E("UNKNOWN DATA 0x%02x\n", rcvx[0]);
				rval = -1;
				break;
		}
	}

	DBG_N("Exit with: %d\n", rval);
	return rval;
}

int main(int argc, char *argv[])
{
	int serfd = -1;                // serial port file descriptor handle
	int baudrate;
	unsigned char sidbuffer[SID_REGISTERS_SIZE];
	long timeout = TIMEOUT_MAIN_MS;
	int rval = 0;
	char device[1024];
	int sidPointer;

	version(argv[0], fwBuild);
	banner();

	// Adesso posso istanziare l'handle dei segnali che utilizza il mutex
	signal(SIGSEGV, signal_handle);
	signal(SIGINT, signal_handle);
	signal(SIGTERM, signal_handle);
	signal(SIGUSR1, signal_handle);
	signal(SIGUSR2, signal_handle);

	// Arguments check
	if (argc > 1)
		sprintf(device, "%s", argv[1]);
	else
		sprintf(device, "/dev/ttyUSB0");

	if (argc > 2)
		baudrate = strtoul(argv[2], NULL, 10);
	else
		baudrate = 19200;

	DBG_I("Using %s as device @ BaudRate: %d...\n", device, baudrate);

	serfd = serial_device_init(device, baudrate, 0, 0);
	if (serfd < 0)
	{
		DBG_E("Unable to initialize port 1 for device %s\n", device);
		return -1;
	}

	DBG_N("START SIO EMULATOR\n");
	serial_flush_rx(serfd);
	serial_flush_tx(serfd);
	srand(0xdeadbeef);
	usleep(2500 * 1000L);

	for (;;)
	{
		// Read SID file in chunks
		for (sidPointer = 0; sidPointer <= sidLength; sidPointer++)
		{
			// Send SIO COMMAND FRAME
			rval = sio_command_frame(serfd);
			if (rval >= 0)
			{
				// Awaiting ACK/NAK
				rval = sio_ack(serfd);
				if (rval >= 0)
				{
					// As reference OS Manual pag.139 t3 = 1000usec - 1800usec
					//usleep(1500);

					// Send DATA FRAME
					memcpy(sidbuffer, &sidData[sidPointer], SID_REGISTERS_SIZE);
					rval = sio_send_data_frame(serfd, sidbuffer);
					if (rval >= 0)
					{
						// Wait ACK
						rval = sio_ack(serfd);
						if (rval >= 0)
						{
							// Awaiting for COMPLETE
							rval = sio_complete(serfd);
							if (rval >= 0)
							{
								sidPointer += (SID_REGISTERS_SIZE - 1);
							}
							else
							{
								DBG_E("Error on sending dataframes\n");
								break; // Error on sending dataframes
							}
						}
						else
						{
							DBG_E("Error on awating ack after dataframe\n");
							// Error on awating ack after dataframe
							break;
						}
					}
					else
					{
						DBG_E("Error sending dataframe\n");
						// Error sending dataframe
						break;
					}
				}
				else
				{
					// Error on awating ack after command frame
					DBG_E("Error on awating ack after command frame\n");
					break;
				}
			}
			else
			{
				// Error on sending command frame!
				DBG_E("Error on sending command frame\n");
				break;
			}
//			usleep(3000);
		}
	}

	close(serfd);
	return 0;
}

