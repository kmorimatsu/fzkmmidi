/*
   This file is provided under the MachiKania license,
   but not the LGPL license.
   Written by Katsumi.
   https://github.com/kmorimatsu/
*/

#include "./main.h"
#include "cgrom16.h"

/*
	Use following pins for connecting ILI9341
		RB13: SDO1 (MOSI)
		RB5 : SDI1 (MISO)
		RB14: SCK1
		RB0 : DC
		RB4 : CS
*/

volatile unsigned short drawcount;
unsigned short g_vline,g_keypoint;
unsigned char _VRAM[2400];
unsigned char* VRAM=(unsigned char*)&_VRAM[0];
//unsigned char g_font[256*8];
unsigned short g_keybuff[32];
unsigned char g_keymatrix[16];
volatile unsigned char g_keymatrix2[10];
unsigned char g_video_disabled;
unsigned char g_beep;
volatile unsigned char g_vblank;

#define lcd_send_command() do {LATBbits.LATB0=0;} while(0)
#define lcd_send_data() do {LATBbits.LATB0=1;} while(0)
#define lcd_cs_enable() do {asm volatile("nop \n nop"); LATBbits.LATB4=0;} while(0)
#define lcd_cs_disable() do {asm volatile("nop \n nop"); LATBbits.LATB4=1;} while(0)
#define lcd_if_enabled() (LATBbits.LATB4 ? 0:1)
#define lcd_if_spi_busy() (SPI1STATbits.SPIBUSY)
#define lcd_if_spi_transmit() (SPI1STATbits.SPITUR)
#define lcd_send_byte(x) do {SPI1BUF=(x);} while(0)
#define lcd_if_spi_rx_empty() (SPI1STATbits.SPITBE)
#define lcd_if_spi_rx_full() (SPI1STATbits.SPITBF)
#define lcd_receive_byte() (SPI1BUF)
#define lcd_if_buff_full() (SPI1STATbits.SPITBF)
#define lcd_spi_clk48() do {SPI1BRG=0;} while(0)
#define lcd_spi_clk10() do {SPI1BRG=4;} while(0)
#define lcd_spi_clk6()  do {SPI1BRG=7;} while(0)
#define dma_busy() (DCH3CONbits.CHEN)

void blink_led(int num){
	volatile int i,j;
	TRISBbits.TRISB1=0;
	LATBbits.LATB1=1;
	while(1){
		for(j=0;j<num;j++){
			LATBbits.LATB1=1;
			for(i=0;i<500000;i++);
			LATBbits.LATB1=0;
			for(i=0;i<500000;i++);
		}
		for(i=0;i<500000;i++);
	}
}

void LCD_command(unsigned char command, unsigned char* data, int databytes){
	int i;
	// Wait while DMA is busy
	while(dma_busy());
	// Wait until SPI1 will be ready
	while( lcd_if_spi_busy() );
	// Check CS
	if (lcd_if_enabled()) {
		// Disable, first
		lcd_cs_disable();
	}
	// Enable LCD
	lcd_cs_enable();
	// Send command
	lcd_send_command();
	lcd_send_byte(command);
	// Return if no data exists
	if (!data) return;
	if (!databytes) return;
	// Wait while SPI1 is busy
	while( lcd_if_spi_busy() );
	// Send data next
	lcd_send_data();
	if (databytes<=16) {
		// Use FIFO buffer
		for(i=0;i<databytes;i++) lcd_send_byte(data[i]);
	} else {
		lcd_send_byte(data[0]);
		// Use DMA for sending data
		DCH3SSA=((unsigned int)&data[1])&0x1fffffff;
		DCH3SSIZ=databytes-1;
		DCH3CONbits.CHEN=1;
	}
}

void video_init(void){
	int i,x,y;
	unsigned char data[4];

	// Initialize output for LCD control
	lcd_cs_disable();
	lcd_send_command();

	// Output pins
	TRISBbits.TRISB0=0;  // DC
	TRISBbits.TRISB4=0;  // CS
	TRISBbits.TRISB13=0; // SDO1
	TRISBbits.TRISB14=0; // SCK1
	// Input pin
	TRISBbits.TRISB5=1;  // SDI1

	// REFO module setting for 96 MHz.
	// Note that when RODIV=0, ROTRIM seems ignored and
	// that REFO output frequency seems the same as input one.
	REFOCON=0;
	REFOCONbits.ROSEL=7; // System PLL output: 96 MHz
	REFOCONbits.RODIV=0;
	REFOTRIMbits.ROTRIM=256;
	REFOCONbits.ON=1; // 96/2/(0+256/512) = 96 (MHz)

	// SPI1 module settings follow
	SPI1CON=0;
	SPI1CONbits.MCLKSEL=1; // Use REFO as clock source (96 MHz)
	SPI1CONbits.ENHBUF=1;  // Enhanced Buffer Enable
	SPI1CONbits.MODE32=0;  // 8 bit mode
	SPI1CONbits.STXISEL=1; // SPI_TBE_EVENT is set when the buffer is completely empty
	SPI1CONbits.MSTEN=1;   // Master mode
	SPI1CONbits.CKP=0;     // Idle state for clock is a low level; active state is a high level
	SPI1CONbits.CKE=1;     // Serial output data changes on transition from active clock state to Idle clock state
	SPI1CON2=0x0300;       // All extended setting are off
	//SPI1BRG=0;             // SPI clock=REFCLK/(2x(SPI2BRG+1))=48 Mhz.
	SPI1BRG=4;             // SPI clock=REFCLK/(2x(SPI2BRG+1))=9.6 Mhz.
	//SPI1BRG=7;             // SPI clock=REFCLK/(2x(SPI2BRG+1))=6 Mhz.
	// Output SDO1 to RB13
	RPB13R=0x03;
	// Input SDI1 from RB5
	SDI1R=0x01;

	// DMA3 setting used for sending SPI data of 128 bytes
	DMACONSET=0x8000;
	DCH3CON=0x00000003;  // CHBUSY=0, CHCHNS=0, CHEN=0, CHAED=0, CHCHN=0, CHAEN=0, CHEDET=0, CHPRI=b11
	DCH3ECON=0x2610;     // CHAIRQ=0, CHSIRQ=24, CFORCE=0, CABRT=0, PATEN=0, SIRQEN=1, AIRQEN=0
	                     // CHSIRQ=38: SPI1TX interrupt
	DCH3SSA=((unsigned int)&cgrom16[0])&0x1fffffff;
	DCH3DSA=0x1F805820;  //SPI1BUF
	DCH3SSIZ=128;
	DCH3DSIZ=1;
	DCH3CSIZ=1;
	DCH3INTCLR=0x00FF00FF;

	// Timer2 setting for 60.11 Hz
	TMR2=0;
	PR2=3048-1;
	T2CON=0x0000;
	// Interrupt settings
	IPC2bits.T2IP=7;
	IPC2bits.T2IS=0;
	IFS0bits.T2IF=0;
	IEC0bits.T2IE=1;

	// Initialize beep
	g_beep=0;
	// Output to RB15
	TRISBbits.TRISB15=0;

	// SPI ON
	SPI1CONbits.ON=1;

	// Initializing sequence
	lcd_spi_clk10();
	LCD_command(0xCB,"\x39\x2C\x00\x34\x02",5); // Power control A
	LCD_command(0xCF,"\x00\xC1\x30",3); // Power control B
	LCD_command(0xE8,"\x85\x00\x78",3); // Driver timing control A
	LCD_command(0xEA,"\x00\x00",2); // Driver timing control B
	LCD_command(0xED,"\x64\x03\x12\x81",4); // Power on sequence control
	LCD_command(0xF7,"\x20",1); // Pump ratio control
	LCD_command(0xC0,"\x23",1); // Power control 1: 4.60 V
	LCD_command(0xC1,"\x10",1); // Power control 2
	LCD_command(0xC5,"\x3E\x28",1); // VCOM control: 4.250V, -1.500 V
	LCD_command(0xC7,"\x86",1); // VCOM control 2

/*
	Memory access control values:
	    0x48 0x0C 0x88 0xC8
	MY   0    0    1    1    Row Address Order
	MX   1    0    0    1    Column Address Order
	MV   0    0    0    0    Row/Column Exchange
	ML   0    0    0    0    Vertical Refresh Order
	BGR  1    1    1    1    0: RGB, 1: BGR
	MH   0    1    0    0    Horizontal Refresh Order
*/
	LCD_command(0x36,"\xe8",1); // Memory access control: 320x240

	LCD_command(0x37,"\x00\x00",2); // Vertical Scrolling Start Address
	LCD_command(0x3A,"\x55",1); // Pixel Format Set: 16 bits
	LCD_command(0xB1,"\x00\x18",2); // Frame Rate Control: fosc, 119 Hz
	LCD_command(0xB6,"\x0A\x82\x27\x00",4); // Display Function Control: Interval scan, AGND, 320 lines
	LCD_command(0x26,"\x01",1); //Gamma set; Gamma curve 1
	LCD_command(0xE0,"\x0F\x31\x2B\x0C\x0E\x08\x4E\xF1\x37\x07\x10\x03\x0E\x09\x00",15); // Positive Gamma Correction
	LCD_command(0xE1,"\x00\x0E\x14\x03\x11\x07\x31\xC1\x48\x08\x0F\x0C\x31\x36\x0F",15); // Negative Gamma Correction
	LCD_command(0x11,0,0); // Sleep Out
	wait_msec(120);
	LCD_command(0x29,0,0); // Display ON
	wait_msec(20);

	// Use 48 MHz clock
	lcd_spi_clk48();

	// Clear screen
	clearscreen();

	// Keyboard input settings follow
	// Output ports for RA5, RB7, RB8, and RB9
	TRISBSET=0x0380;
	TRISASET=0x10;
	LATBCLR=0x0380;
	LATACLR=0x10;
	// Input ports from RA0, RA1, RB2 and RB3
	TRISASET=0x03;
	TRISBSET=0x000c;
	// A/D converter settings
	AD1CON1=0x00E0;
	AD1CON2=0x0000;
	AD1CON3=0x1fff; // SAMC=31, TAD=10.8 usec (slowest mode)
	// CH0NA=0, CH0SA=0 (AN0, first)
	AD1CHS=0;

	// Timer2 and A/D converter ON
	T2CONbits.ON=1;
	AD1CON1bits.ON=1;
	g_video_disabled=0;
}

#define read_ad_for_keymatrix(a) do {\
		x=ADC1BUF0;\
		g_keybuff[g_keypoint+a]=x;\
		y=g_keybuff[g_keypoint+16-a];\
		if (-20<(x-y) && (x-y)<20) {\
			x=(x+y)>>1;\
			g_keymatrix[g_keypoint]=(x>0x397)?0:((x>0x318)?16:((x>0x2d5)?8:((x>0x250)?4:((x>0x100)?2:1))));\
		}\
	} while(0) 	

void __ISR(_TIMER_2_VECTOR,IPL7SOFT) T2Handler(void){
	int i,x,y;
	static int s_line=0;
	static short s_x=39;
	static short s_y=25;
	static char s_video_disabled=0;
	static char s_beep;
	unsigned char data[4];
	// Clear flag
	IFS0bits.T2IF=0;
	// Beep if needed
	if (g_beep) {
		s_beep++;
		if (18<s_beep) {
			LATBINV=1<<15;
			s_beep=0;
		}
	}
	// Main starts here
	s_line++;
	if (s_line<11) {
		// s_line: 1-10
		g_vblank=0;
	} else if (s_line<12) {
		// s_line: 11
		g_vblank=1;
	} else if (s_line<262) {
		// s_line: 12-261
		// Keyboard detection routines follow
		x=s_line & 0x0f;
		if (0x00==x) {
			if (s_line<(96)) {
				switch(s_line){
					case (16):
						// Read from RA0
						AD1CHSbits.CH0SA=0;
						AD1CON1CLR=1; // AD1CON1bits.DONE=0;
						AD1CON1SET=2; // AD1CON1bits.SAMP=1;
						break;
					case (32):
						// Read value
						read_ad_for_keymatrix(0);
						g_keypoint++;
						// Read from RA1
						AD1CHSbits.CH0SA=1;
						AD1CON1CLR=1; // AD1CON1bits.DONE=0;
						AD1CON1SET=2; // AD1CON1bits.SAMP=1;
						break;
					case (48):
						// Read value
						read_ad_for_keymatrix(0);
						g_keypoint++;
						// Read from RB2
						AD1CHSbits.CH0SA=4;
						AD1CON1CLR=1; // AD1CON1bits.DONE=0;
						AD1CON1SET=2; // AD1CON1bits.SAMP=1;
						break;
					case (64):
						// Read value
						read_ad_for_keymatrix(0);
						g_keypoint++;
						// Read from RB3
						AD1CHSbits.CH0SA=5;
						AD1CON1CLR=1; // AD1CON1bits.DONE=0;
						AD1CON1SET=2; // AD1CON1bits.SAMP=1;
						break;
					case (80):
						// Read value
						read_ad_for_keymatrix(0);
						g_keypoint-=3;
						// Read from RA0
						AD1CHSbits.CH0SA=0;
						AD1CON1CLR=1; // AD1CON1bits.DONE=0;
						AD1CON1SET=2; // AD1CON1bits.SAMP=1;
						break;
					default:
						break;
				}
			} else {
				switch(s_line){
					case (96):
						// Read value
						read_ad_for_keymatrix(16);
						g_keypoint++;
						// Read from RA1
						AD1CHSbits.CH0SA=1;
						AD1CON1CLR=1; // AD1CON1bits.DONE=0;
						AD1CON1SET=2; // AD1CON1bits.SAMP=1;
						break;
					case (112):
						// Read value
						read_ad_for_keymatrix(16);
						g_keypoint++;
						// Read from RB2
						AD1CHSbits.CH0SA=4;
						AD1CON1CLR=1; // AD1CON1bits.DONE=0;
						AD1CON1SET=2; // AD1CON1bits.SAMP=1;
						break;
					case (128):
						// Read value
						read_ad_for_keymatrix(16);
						g_keypoint++;
						// Read from RB3
						AD1CHSbits.CH0SA=5;
						AD1CON1CLR=1; // AD1CON1bits.DONE=0;
						AD1CON1SET=2; // AD1CON1bits.SAMP=1;
						break;
					case (144):
						// Read value
						read_ad_for_keymatrix(16);
						g_keypoint++;
						g_keypoint&=15;
						// Set next aquisition
						switch(g_keypoint>>2){
							case 0:
								TRISBSET=0x380; // RB7-RB9: off
								TRISACLR=0x10;  // RA4: on
								break;
							case 1:
								TRISASET=0x10;  // RA4: off
								TRISBCLR=0x80;  // RB7: on
								break;
							case 2:
								TRISBSET=0x380; // RB7-RB9: off
								TRISBCLR=0x100; // RB8: on
								break;
							default:
								TRISBSET=0x380; // RB7-RB9: off
								TRISBCLR=0x200; // RB9: on
								break;
						}
						break;
					default:
						break;
				}
			}	
		} else if (0x01==x) {
			if (s_line<(97)) {
				switch(s_line){
					case (17):
						x =(g_keymatrix[ 0]&0x01);
						x|=(g_keymatrix[ 2]&0x01)<<1;
						x|=(g_keymatrix[ 4]&0x01)<<2;
						x|=(g_keymatrix[ 6]&0x01)<<3;
						x|=(g_keymatrix[ 8]&0x01)<<4;
						x|=(g_keymatrix[10]&0x01)<<5;
						x|=(g_keymatrix[12]&0x01)<<6;
						x|=(g_keymatrix[14]&0x01)<<7;
						g_keymatrix2[0]=x;
						break;
					case (33):
						x =(g_keymatrix[ 1]&0x01);
						x|=(g_keymatrix[ 3]&0x01)<<1;
						x|=(g_keymatrix[ 5]&0x01)<<2;
						x|=(g_keymatrix[ 7]&0x01)<<3;
						x|=(g_keymatrix[ 9]&0x01)<<4;
						x|=(g_keymatrix[11]&0x01)<<5;
						x|=(g_keymatrix[13]&0x01)<<6;
						x|=(g_keymatrix[15]&0x01)<<7;
						g_keymatrix2[1]=x;
						break;
					case (49):
						x =(g_keymatrix[ 0]&0x02)>>1;
						x|=(g_keymatrix[ 2]&0x02);
						x|=(g_keymatrix[ 4]&0x02)<<1;
						x|=(g_keymatrix[ 6]&0x02)<<2;
						x|=(g_keymatrix[ 8]&0x02)<<3;
						x|=(g_keymatrix[10]&0x02)<<4;
						x|=(g_keymatrix[12]&0x02)<<5;
						x|=(g_keymatrix[14]&0x02)<<6;
						g_keymatrix2[2]=x;
						break;
					case (65):
						x =(g_keymatrix[ 1]&0x02)>>1;
						x|=(g_keymatrix[ 3]&0x02);
						x|=(g_keymatrix[ 5]&0x02)<<1;
						x|=(g_keymatrix[ 7]&0x02)<<2;
						x|=(g_keymatrix[ 9]&0x02)<<3;
						x|=(g_keymatrix[11]&0x02)<<4;
						x|=(g_keymatrix[13]&0x02)<<5;
						x|=(g_keymatrix[15]&0x02)<<6;
						g_keymatrix2[3]=x;
						break;
					case (81):
						x =(g_keymatrix[ 0]&0x04)>>2;
						x|=(g_keymatrix[ 2]&0x04)>>1;
						x|=(g_keymatrix[ 4]&0x04);
						x|=(g_keymatrix[ 6]&0x04)<<1;
						x|=(g_keymatrix[ 8]&0x04)<<2;
						x|=(g_keymatrix[10]&0x04)<<3;
						x|=(g_keymatrix[12]&0x04)<<4;
						x|=(g_keymatrix[14]&0x04)<<5;
						g_keymatrix2[4]=x;
						break;
					default:
						break;
				}
			} else {
				switch(s_line){
					case (97):
						x =(g_keymatrix[ 1]&0x04)>>2;
						x|=(g_keymatrix[ 3]&0x04)>>1;
						x|=(g_keymatrix[ 5]&0x04);
						x|=(g_keymatrix[ 7]&0x04)<<1;
						x|=(g_keymatrix[ 9]&0x04)<<2;
						x|=(g_keymatrix[11]&0x04)<<3;
						x|=(g_keymatrix[13]&0x04)<<4;
						x|=(g_keymatrix[15]&0x04)<<5;
						g_keymatrix2[5]=x;
						break;
					case (113):
						x =(g_keymatrix[ 0]&0x08)>>3;
						x|=(g_keymatrix[ 2]&0x08)>>2;
						x|=(g_keymatrix[ 4]&0x08)>>1;
						x|=(g_keymatrix[ 6]&0x08);
						x|=(g_keymatrix[ 8]&0x08)<<1;
						x|=(g_keymatrix[10]&0x08)<<2;
						x|=(g_keymatrix[12]&0x08)<<3;
						x|=(g_keymatrix[14]&0x08)<<4;
						g_keymatrix2[6]=x;
						break;
					case (129):
						x =(g_keymatrix[ 1]&0x08)>>3;
						x|=(g_keymatrix[ 3]&0x08)>>2;
						x|=(g_keymatrix[ 5]&0x08)>>1;
						x|=(g_keymatrix[ 7]&0x08);
						x|=(g_keymatrix[ 9]&0x08)<<1;
						x|=(g_keymatrix[11]&0x08)<<2;
						x|=(g_keymatrix[13]&0x08)<<3;
						x|=(g_keymatrix[15]&0x08)<<4;
						g_keymatrix2[7]=x;
						break;
					case (145):
						x =(g_keymatrix[ 0]&0x10)>>4;
						x|=(g_keymatrix[ 2]&0x10)>>3;
						x|=(g_keymatrix[ 4]&0x10)>>2;
						x|=(g_keymatrix[ 6]&0x10)>>1;
						x|=(g_keymatrix[ 8]&0x10);
						x|=(g_keymatrix[10]&0x10)<<1;
						x|=(g_keymatrix[12]&0x10)<<2;
						x|=(g_keymatrix[14]&0x10)<<3;
						g_keymatrix2[8]=x;
						break;
					case (161):
						x =(g_keymatrix[ 1]&0x10)>>4;
						x|=(g_keymatrix[ 3]&0x10)>>3;
						x|=(g_keymatrix[ 5]&0x10)>>2;
						x|=(g_keymatrix[ 7]&0x10)>>1;
						x|=(g_keymatrix[ 9]&0x10);
						x|=(g_keymatrix[11]&0x10)<<1;
						x|=(g_keymatrix[13]&0x10)<<2;
						x|=(g_keymatrix[15]&0x10)<<3;
						g_keymatrix2[9]=x;
						break;
					default:
						break;
				}
			}	
		}
	} else {
		// s_line: 262
		s_line=0;
		drawcount++;
		// Raise CS0 interrupt every 60.1 Hz
		IFS0bits.CS0IF=1;
	}
}

unsigned char char2ascii(unsigned char code){return code;}
unsigned char ascii2char(unsigned char ascii){return ascii;}

void clearscreen(void){
	int i;
	// Clear VRAM
	for(i=0;i<2400;i++) VRAM[i]=0x20;
	// Clear screen
	LCD_command(0x2a,"\x00\x00\x01\x3f",4); // Width: 0 - 319
	LCD_command(0x2b,"\x00\x00\x00\xef",4); // Height: 0 - 239
	LCD_command(0x2c,0,0);
	// Wait while SPI1 is busy
	while( lcd_if_spi_busy() );
	// Send data next
	lcd_send_data();
	for(i=0;i<320*240*2;i++){
		while( lcd_if_buff_full() );
		lcd_send_byte(0x00);
	}
}

void printchar(int x,unsigned char ascii){
	unsigned char data[4];
	static short s_x=-1;
	static short s_y=-1;
	short y=0;
	// Update cache
	VRAM[x]=ascii;
	// Calculate x and y
	while(79<x) {
		y++;
		x-=80;
	}
	// Set x and y position to write
	if (y!=s_y) {
		s_y=y;
		data[0]=data[2]=0;
		data[1]=(y<<3);
		data[3]=data[1]+7;
		// Page Address Set
		LCD_command(0x2b,(unsigned char*)&data[0],4);
	}
	if (x!=s_x) {
		s_x=x;
		data[0]=data[2]=x>>(8-2);
		data[1]=(x<<2)&0xff;
		data[3]=data[1]+3;
		// Column Address Set
		LCD_command(0x2a,(unsigned char*)&data[0],4);
	}
	// Memory Write
	LCD_command(0x2c,(unsigned char*)&cgrom16[(ascii-0x20)<<5],64);
}

void printstr(int cursor,char* str){
	int i;
	unsigned char c;
	for(i=0;c=str[i];i++) printchar(cursor+i,c);
}
void printhex4(int cursor, unsigned char val){
	if (val<10) printchar(cursor,val+0x30);
	else printchar(cursor,val+0x37);
}
void printhex8(int cursor, unsigned char val){
	printhex4(cursor,val>>4);
	printhex4(cursor+1,val&15);
}
void printhex16(int cursor, unsigned short val){
	printhex8(cursor,val>>8);
	printhex8(cursor+2,val&255);
}
void printhex32(int cursor, unsigned int val){
	printhex16(cursor,val>>16);
	printhex16(cursor+4,val&65535);
}

void vertical_scroll(void){
	int cursor,x,y;
	for(y=0;y<29;y++){
		for(x=0;x<80;x++){
			cursor=y*80+x;
			printchar(cursor,VRAM[cursor+80]);
		}
	}
	for(x=0;x<80;x++){
		cursor=y*80+x;
		printchar(cursor,0x20);
	}
}
