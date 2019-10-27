/*
   This file is provided under the LGPL license ver 2.1.
   Written by Katsumi.
   https://github.com/kmorimatsu/
*/
#include "./main.h"
#include "usbhost/FSIO.h"

/*
 * Public functions:
 *
 *   ide.init(data);
 *     data: an array containing disk image data
 *   ide.ide_read(r);
 *     r: CF register to read
 *   ide.ide_write(r,v);
 *     r: CF register to write
 *     v: 8 bit value
 */

// RDY & DSC
#define ST_NORMAL 0x50
// RDY & DSC & DRQ
#define ST_RW 0x58
// 128 Mbytes disk
#define IDE_DISK_LEN 134218752

char g_ide_filename[13]="FUZIXIMG.IDE";

static unsigned char g_ide_registers[8];
static unsigned char g_ide_drivenum;     // Either 0 or 1
static unsigned char g_ide_data[512];    // 512 bytes read/write buffer

static unsigned short g_ide_datapoint;
static FSFILE* g_ide_fhandle;

void ide_init(void){
	g_ide_registers[0]=0x00; // read/write (dummy)
	g_ide_registers[1]=0x00; // error
	g_ide_registers[2]=0x01; // count
	g_ide_registers[3]=0x00; // lba1
	g_ide_registers[4]=0x00; // lba2
	g_ide_registers[5]=0x00; // lba3
	g_ide_registers[6]=0x00; // lba4
	g_ide_registers[7]=0x50; // status (RDY & DSC)
	g_ide_drivenum=0;
	g_ide_datapoint=0;
	// Open file
	g_ide_fhandle=FSfopen(g_ide_filename,"r+");
	if (!g_ide_fhandle) {
		printhex8(28*80,FSerror());
		printstr(28*80+3,"File open error");
		printstr(29*80,g_ide_filename);
		while(1) asm volatile("wait");
	}
}

void ide_lseek(void){
	unsigned int point;
	point =g_ide_registers[6]<<24;
	point|=g_ide_registers[5]<<16;
	point|=g_ide_registers[4]<<8;
	point|=g_ide_registers[3];
	FSfseek(g_ide_fhandle,512*(2+point),SEEK_SET);
}

void ide_lseek_identify(void){
	FSfseek(g_ide_fhandle,512,SEEK_SET);
}
void ide_read512(void){
	FSfread(&g_ide_data[0],1,512,g_ide_fhandle);
	// Blink LED
	LATBINV=1<<1;
}
void ide_write512(void){
	FSfwrite(&g_ide_data[0],1,512,g_ide_fhandle);
	// Blink LED
	LATBINV=1<<1;
}

unsigned char ide_read(unsigned char reg){
	unsigned char b;
	if (g_ide_drivenum!=0) {
		// Only drive 0 is the valid drive
		// Go back to drive 0 when accessing empty one
		g_ide_drivenum=0;
		// Return zero
		return 0;
	}
	// Emulate
	switch(reg){
		case 0: // data
			if (g_ide_registers[7]==ST_RW && g_ide_datapoint<512) {
				b=g_ide_data[g_ide_datapoint++];
				if (512<=g_ide_datapoint) {
					g_ide_datapoint=0;
					g_ide_registers[7]=ST_NORMAL;
				}
				return b;
			} else {
				printstr(29*80,"Bad IDE read request");
				while(1) asm volatile("wait");
			}
		case 1: // error
		case 2: // count
		case 3: // lba1
		case 4: // lba2
		case 5: // lba3
		case 6: // lba4
		case 7: // status
			return g_ide_registers[reg];
		default:
			printhex8(29*80,reg);
			printstr(29*80+2,"h: Wrong register #");
			while(1) asm volatile("wait");
	}
	return 0;
}

void ide_write(unsigned char reg, unsigned char val){
	if (g_ide_drivenum!=0) return;
	// Emulate
	switch(reg){
		case 0: // data
			if (g_ide_registers[7]==ST_RW && g_ide_datapoint<512) {
				g_ide_data[g_ide_datapoint++]=val;
				if (512<=g_ide_datapoint) {
					g_ide_datapoint=0;
					g_ide_registers[7]=ST_NORMAL;
					ide_write512();
				}
				return;
			} else {
				printstr(29*80,"Bad IDE write request");
				while(1) asm volatile("wait");
			}
		case 1: // feature
			return;
		case 2: // count
		case 3: // lba1
		case 4: // lba2
		case 5: // lba3
			g_ide_registers[reg]=val;
			return;
		case 6: // lba4
			g_ide_drivenum=(val&16) ? 1:0;
			g_ide_registers[reg]=val&15;
			return;
		case 7: // command
			switch(val){
				case 0x20: case 0x21: // Read sector
					ide_lseek();
					ide_read512();
					g_ide_registers[7]=ST_RW;
					g_ide_datapoint=0;
					return;
				case 0x30: case 0x31: // Write sector
					ide_lseek();
					g_ide_registers[7]=ST_RW;
					g_ide_datapoint=0;
					return;
				case 0xef: // Set feature (8 bit mode)
					return;
				case 0xec: // Read identify
					ide_lseek_identify();
					ide_read512();
					g_ide_registers[7]=ST_RW;
					g_ide_datapoint=0;
					return;
				default:
					printhex8(29*80,val);
					printstr(29*80+2,"h: Wrong command #");
					while(1) asm volatile("wait");
			}
		default:
			printhex8(29*80,reg);
			printstr(29*80+2,"h: Wrong register #");
			while(1) asm volatile("wait");
	}
}

/*
//*/