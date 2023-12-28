/* tuxctl-ioctl.c
 *
 * Driver (skeleton) for the mp2 tuxcontrollers for ECE391 at UIUC.
 *
 * Mark Murphy 2006
 * Andrew Ofisher 2007
 * Steve Lumetta 12-13 Sep 2009
 * Puskar Naha 2013
 */

#include <asm/current.h>
#include <asm/uaccess.h>

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/file.h>
#include <linux/miscdevice.h>
#include <linux/kdev_t.h>
#include <linux/tty.h>
#include <linux/spinlock.h>

#include "tuxctl-ld.h"
#include "tuxctl-ioctl.h"
#include "mtcp.h"

static int ACK_Message = 1;
/* store the status of buttons, active low, right | left | down | up | c | b | a | start */
static unsigned char buttons = 0xFF;
/* store the status of four LEDs, active high, each stores A | E | F | dp | G | C | B | D */
static unsigned char LEDs[LED_NUM] = {0x00, 0x00, 0x00, 0x00};

/* need to use irqrestore and irqsave to prevent interruption from keyboard */
static spinlock_t lock;
static unsigned long flags;

#define debug(str, ...) \
	printk(KERN_DEBUG "%s: " str, __FUNCTION__, ## __VA_ARGS__)


/* set the ACK_Message to 1 to indicate the MTC successfully completes a command */
static void tuxctl_handle_ACK(void);

/* update the button's status when a button is pressed or released and the Button Interrupt-on-change mode is enabled */
static void tuxctl_handle_EVT(unsigned char Byte1, unsigned char Byte2);

/* reinitialized the Tux and store the LED status after a power-up, a RESET button press or an MTCP_RESET_DEV command */
static void tuxctl_handle_RESET(struct tty_struct* tty);

/* initialize any variables associated with the driver, set all LED to black and returns 0 */
static int tuxctl_ioctl_INIT(struct tty_struct* tty);

/* sets the bits of the low byte corresponding to the currently pressed buttons and returns -EINVAL if pointer not valid, 0 otherwise */
static int tuxctl_ioctl_BUTTONS(int* INT_PTR);

/* show the hexadecimal value of low 2 byte of input value with specific rules and returns 0 */
static int tuxctl_ioctl_SET_LED(struct tty_struct* tty, int DisplayMsg);


/************************ Protocol Implementation *************************/

/* tuxctl_handle_packet()
 * IMPORTANT : Read the header for tuxctl_ldisc_data_callback() in 
 * tuxctl-ld.c. It calls this function, so all warnings there apply 
 * here as well.
 * This function calls from an interruption context, thus no use of semaphor or sleep
 */
void tuxctl_handle_packet (struct tty_struct* tty, unsigned char* packet)
{
    unsigned a, b, c;
 

	/* a: opcode; b: data bits; c: data bits */
    a = packet[0]; /* Avoid printk() sign extending the 8-bit */
    b = packet[1]; /* values when printing them. */
    c = packet[2];

    /*printk("packet : %x %x %x\n", a, b, c); */
	switch (a){
		case MTCP_ACK:
		return tuxctl_handle_ACK();
		case MTCP_BIOC_EVENT:
		return tuxctl_handle_EVT(b, c);
		case MTCP_RESET:
		return tuxctl_handle_RESET(tty);
		default:
		return;
	}
}


/*
 * tuxctl_handle_ACK
 *   DESCRIPTION: set the ACK_Message to 1 to indicate the MTC successfully completes a command
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: none
 */
void
tuxctl_handle_ACK(void){
	ACK_Message = 1;
}


/*
 * tuxctl_handle_EVT
 *   DESCRIPTION: update the button's status when a button is pressed or released and the Button Interrupt-on-change mode is enabled
 *   INPUTS: unsigned char Byte1: Byte1 of packet that stores data bits
 * 			 unsigned char Byte2: Byte2 of packet that stores data bits
 *   OUTPUTS: none
 *   RETURN VALUE: none
 */
void
tuxctl_handle_EVT(unsigned char Byte1, unsigned char Byte2){
	/* buttons: right | left | down | up | c | b | a | start */
	/* Byte1: | 1 X X X | C | B | A | START | */
	/* Byte2: | 1 X X X | right | down | left | up | */
	unsigned char Byte1_Mask = 0x0F;
	unsigned char Right_Mask = 0x08;
	unsigned char Down_Mask = 0x04;
	unsigned char Left_Mask = 0x02;
	unsigned char Up_Mask = 0x01;
	spin_lock_irqsave(&lock, flags);			// need the lock to write shared sources
	buttons = Byte1 & Byte1_Mask;
	buttons |= (Byte2 & Right_Mask) << 4;
	buttons |= (Byte2 & Down_Mask) << 3;
	buttons |= (Byte2 & Left_Mask) << 5;
	buttons |= (Byte2 & Up_Mask) << 4;
	spin_unlock_irqrestore(&lock, flags);
}


/*
 * tuxctl_handle_RESET
 *   DESCRIPTION: reinitialized the Tux and store the LED status after a power-up, a RESET button press or an MTCP_RESET_DEV command
 *   INPUTS: struct tty_struct* tty - serial port driver that stores the cmd for TUX
 *   OUTPUTS: none
 *   RETURN VALUE: none
 */
void
tuxctl_handle_RESET(struct tty_struct* tty){
	int i;
	unsigned char cmd_reset[2 + LED_NUM] = {MTCP_LED_SET, 0x0F, 0, 0, 0, 0};			// 2 as two basic cmd
	/* store LED */
	unsigned char LEDs_copy[4];
	for(i = 0; i < LED_NUM; i++){
		LEDs_copy[i] = LEDs[i];
	}
	/* initialize the Tux first */
	tuxctl_ioctl_INIT(tty);
	/* then set LED again */
	for(i = 0; i < LED_NUM; i++){
		LEDs[i] = LEDs_copy[i];
		cmd_reset[2+i] = LEDs_copy[i];
	}
	if(ACK_Message == 1){
		/* call tuxctl_ldisc_put to write command to Tux */
		if(tuxctl_ldisc_put(tty, cmd_reset, 2 + LED_NUM) > 0){
			//printk("\nLine Discipline's Internal Buffer is Full\n");
		}
		ACK_Message = 0;
	}
}


/******** IMPORTANT NOTE: READ THIS BEFORE IMPLEMENTING THE IOCTLS ************
 *                                                                            *
 * The ioctls should not spend any time waiting for responses to the commands *
 * they send to the controller. The data is sent over the serial line at      *
 * 9600 BAUD. At this rate, a byte takes approximately 1 millisecond to       *
 * transmit; this means that there will be about 9 milliseconds between       *
 * the time you request that the low-level serial driver send the             *
 * 6-byte SET_LEDS packet and the time the 3-byte ACK packet finishes         *
 * arriving. This is far too long a time for a system call to take. The       *
 * ioctls should return immediately with success if their parameters are      *
 * valid.                                                                     *
 *                                                                            *
 ******************************************************************************/
int 
tuxctl_ioctl (struct tty_struct* tty, struct file* file, 
	      unsigned cmd, unsigned long arg)
{
    switch (cmd) {
		case TUX_INIT:
		return tuxctl_ioctl_INIT(tty);
		case TUX_BUTTONS:
		return tuxctl_ioctl_BUTTONS((int*)(arg));
		case TUX_SET_LED:
		return tuxctl_ioctl_SET_LED(tty, (int)(arg));
		case TUX_LED_ACK:
		return 0;
		case TUX_LED_REQUEST:
		return 0;
		case TUX_READ_LED:
		return 0;
		default:
	    return -EINVAL;
    }
}


/*
 * tuxctl_ioctl_INIT
 *   DESCRIPTION: initialize any variables associated with the driver, set all LED to black and returns 0
 *   INPUTS: struct tty_struct* tty - serial port driver that stores the cmd for TUX
 *   OUTPUTS: none
 *   RETURN VALUE: none
 */
int
tuxctl_ioctl_INIT(struct tty_struct* tty){
	/* to initialize the Tux, execute cmd MTCP_BIOC_ON, MTCP_LED_USR */
	/* MTCP_LED_SET followed by 0x0F to indicate all LED should show now */
	unsigned char cmd[8] = {MTCP_BIOC_ON, MTCP_LED_USR, MTCP_LED_SET, 0x0F, 0, 0, 0, 0};
	/* set ACK_Message to 0x00 */
	ACK_Message = 1;
	/* initialize the button, LED, lock */
	buttons = 0xFF;
	LEDs[0] = LEDs[1] = LEDs[2] = LEDs[3] = 0;
	lock = SPIN_LOCK_UNLOCKED;
	if(tuxctl_ldisc_put(tty, cmd, 8) > 0){
		//printk("\nLine Discipline's Internal Buffer is Full\n");
	}
	return 0;
}


/*
 * tuxctl_ioctl_BUTTONS
 *   DESCRIPTION: sets the bits of the low byte corresponding to the currently pressed buttons
 *   INPUTS: int* - a pointer pointing to a 32-bit integer whose low byte should be set the same as buttons
 *   OUTPUTS: none
 *   RETURN VALUE: return -EINVAL if pointer not valid; return 0 otherwise
 */
int
tuxctl_ioctl_BUTTONS(int* INT_PTR){
	/* check pointer at first */
	if(INT_PTR == NULL) return -EINVAL;
	/* then set the button */
	spin_lock_irqsave(&lock, flags);
	*INT_PTR = (unsigned long)(buttons);
	spin_unlock_irqrestore(&lock, flags);
	return 0;
}

/*
 * tuxctl_ioctl_SET_LED
 *   DESCRIPTION: show the hexadecimal value of low 2 byte of input value with specific rules and returns 0
 *   INPUTS: struct tty_struct* tty - serial port driver that stores the cmd for TUX
 * 			 int DisplayMsg - one integer that specificy the display rule: 
 * 							  [15:0] hexadecimal value to display
 * 								mapping:  _A
 *										F| |B
 *		  								  -G
 *										E| |C
 *		                                  -D .dp
 *					__7___6___5___4____3___2___1___0___
 *					| A | E | F | dp | G | C | B | D |
 * 					----+---+---+---+----+---+---+---+
 * 							  [19:16] specifies which LED should be turned on
 * 							  [27:24] specify whether corresponding decimal points should be turned on
 *   OUTPUTS: none
 *   RETURN VALUE: return 0
 */
int
tuxctl_ioctl_SET_LED(struct tty_struct* tty, int DisplayMsg){
	int i;
	/* compute the bits need to set to 1 to display */
	int DisplayBytes = DisplayMsg & 0x0000FFFF;
	/* set the Bitmask of which LED's to set */
	unsigned char LEDMask = (unsigned char) ((DisplayMsg & 0x000F0000) >> 16);		// 16 to move to first byte
	/* set the Dot Point value */
	unsigned char DPMask = (unsigned char) ((DisplayMsg & 0x0F000000) >> 24);		// 24 to move to first byte
	unsigned char cmd[2 + LED_NUM] = {MTCP_LED_SET, LEDMask, 0, 0, 0, 0};			// 2 as two basic cmd
	int DisplayLEDNum = 0;
	for(i = 0; i < LED_NUM; i++){
		unsigned char CurHexValue = (unsigned char) (DisplayBytes & 0x0000000F);
		DisplayBytes = DisplayBytes >> 4;				// prepare to get next Byte
		if(((0x1 << i) & LEDMask) != 0){
			/* only draw LED that is not masked */
			switch(CurHexValue){
				case 0x0:
				LEDs[i] = 0xE7;
				break;
				case 0x1:
				LEDs[i] = 0x06;
				break;
				case 0x2:
				LEDs[i] = 0xCB;
				break;
				case 0x3:
				LEDs[i] = 0x8F;
				break;
				case 0x4:
				LEDs[i] = 0x2E;
				break;
				case 0x5:
				LEDs[i] = 0xAD;
				break;
				case 0x6:
				LEDs[i] = 0xED;
				break;
				case 0x7:
				LEDs[i] = 0x86;
				break;
				case 0x8:
				LEDs[i] = 0xEF;
				break;
				case 0x9:
				LEDs[i] = 0xAF;
				break;
				case 0xA:
				LEDs[i] = 0xEE;
				break;
				case 0xB:
				LEDs[i] = 0x6D;
				break;
				case 0xC:
				LEDs[i] = 0xE1;
				break;
				case 0xD:
				LEDs[i] = 0x4F;
				break;
				case 0xE:
				LEDs[i] = 0xE9;
				break;
				case 0xF:
				LEDs[i] = 0xE8;
				break;
			}
			/* the corresponding bit is 1, set the DP to 1 for that LED */
			if((0x01 << i) & DPMask) LEDs[i] |= 0x10;								// 0x10 as dp at 5th bit
		} else {
			/* if this LED has been masked, set to 0x00 to show nothing */
			LEDs[i] = 0x00;
		}
	}

	/* send the cmd to Tux */
	for(i = 0; i < LED_NUM; i++){
		/* send corresponding LED to cmd data */
		if(((0x1 << i) & LEDMask) != 0){
			cmd[2 + DisplayLEDNum] = LEDs[i];
			DisplayLEDNum++;
		}
	}

	/* check if allowed to set LED */
	if(ACK_Message == 1){
		tuxctl_ldisc_put(tty, cmd, 2 + DisplayLEDNum);
		ACK_Message = 0;
	}
	return 0;
}
