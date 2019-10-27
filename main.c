/*
   This file is provided under the LGPL license ver 2.1.
   Written by Katsumi.
   https://github.com/kmorimatsu/
*/

#include "./main.h"
#include "z80.h"
#include "peripheral.h"

// Note: 4 MHz crystal is used. CPU clock is 48 MHz.
#pragma config FUSBIDIO  = OFF          // Not using USBID (pin #14, RB5)
#pragma config FVBUSONIO = OFF          // Not using VBUSON (pin #25, RB14)
#pragma config DEBUG = OFF

#pragma config UPLLEN   = ON            // USB PLL Enabled
#pragma config FPLLMUL  = MUL_24        // PLL Multiplier
#pragma config UPLLIDIV = DIV_1         // USB PLL Input Divider
#pragma config FPLLIDIV = DIV_1         // PLL Input Divider
#pragma config FPLLODIV = DIV_2         // PLL Output Divider
#pragma config FPBDIV   = DIV_1         // Peripheral Clock divisor
#pragma config FWDTEN   = OFF           // Watchdog Timer
#pragma config WDTPS    = PS1           // Watchdog Timer Postscale
//#pragma config FCKSM    = CSDCMD        // Clock Switching & Fail Safe Clock Monitor
#pragma config OSCIOFNC = OFF           // CLKO Enable
#pragma config POSCMOD  = HS            // Primary Oscillator
#pragma config IESO     = OFF           // Internal/External Switch-over
#pragma config FSOSCEN  = OFF           // Secondary Oscillator Enable (KLO was off)
#pragma config FNOSC    = PRIPLL        // Oscillator Selection
#pragma config CP       = OFF           // Code Protect
#pragma config BWP      = OFF           // Boot Flash Write Protect
#pragma config PWP      = OFF           // Program Flash Write Protect
#pragma config ICESEL   = ICS_PGx1      // ICE/ICD Comm Channel Select
#pragma config JTAGEN   = OFF           // JTAG disabled

#define INTEnableSystemMultiVectoredInt() do{ INTCONSET=0x1000; asm volatile ("ei"); }while(0)

// Boot area assembly. Jump to 0x9d000000 here.
volatile const unsigned int const __attribute__((address(0xBFC00000))) boot[]={
	0x3c089d00, // lui $t0,0x9d00
	0x01000008, // jr $to
	0x00000000, // nop
};

void _general_exception_handler(void){
	printstr(0,"Exception at:");
	printhex32(80,_CP0_GET_EPC());
	while(1) asm volatile("nop");
}

int g_temp=0;

int coretimer(void){ 
	return _CP0_GET_COUNT();
}

void wait_msec(int msec){
	int endtime=coretimer()+msec*24000;
	while(0<(endtime-(int)coretimer()));
}

void led_green(){
	LATBbits.LATB1=1;
	TRISBbits.TRISB1=0;
}

void led_red(){
	LATBbits.LATB1=0;
	TRISBbits.TRISB1=0;
}

void main(void){
	int i,j;

	// Enable interrupt
	INTEnableSystemMultiVectoredInt();

	// Initialize video and keyboard
	video_init();

	// Initialize USB host
	init_usb();

	// Start bootloader if a button is pushed
	while(drawcount<5) asm volatile("wait");
	for(i=0;i<10;i++){
		if (g_keymatrix2[i]) file_select();
	}

	// Initilize peripheral
	peripheral_init();

	// Connect to usb
	if (!connect_usb()) {
		printstr(0,"USB memory not found");
		while(1) asm volatile("wait");
	}

	// Read ini file
	init_file();

	// Initialize IDE
	ide_init();

	// Initilize swap
	init_swap();

	// Initialize Z80
	resetZ80();
	
	// Main loop
	while(1){
		// Timer is not used for Fuzix emulator.
		// Z80 CPU will work at the highest speed, that is ~2.5 MHz
		execZ80infinite();
	}
}
