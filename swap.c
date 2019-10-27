/*
   This file is provided under the LGPL license ver 2.1.
   Written by Katsumi.
   https://github.com/kmorimatsu/
*/

#include "usbhost/FSIO.h"
#include "z80.h"
#include "./main.h"

static unsigned int g_priorities[16];
static unsigned char* g_pointers[16];
static char g_swap_bank[3];
static FSFILE* g_swap_fhandle;
char g_swap_filename[13]="FZKMSWAP";

void init_swap(void){
	int i;
	// Reset parameters
	for(i=0;i<16;i++){
		g_priorities[i]=0;
		g_pointers[i]=RAM+i*0x1000;
	}
	// Banks 14 and 15 are in swap disk
	g_pointers[14]=0;
	g_pointers[15]=0;
	g_swap_bank[0]=14;
	g_swap_bank[1]=15;
	g_swap_bank[2]=-1;
	// Open file
	g_swap_fhandle=FSfopen(g_swap_filename,"w+");
	if (!g_swap_fhandle) {
		printhex8(28*80,FSerror());
		printstr(28*80+3,"File open error");
		printstr(29*80,g_swap_filename);
		while(1) asm volatile("wait");
	}
	// Create swap file (dummy)
	FSfwrite(RAM,1,4096*3,g_swap_fhandle);
}

void swapData(int bank){
	int i;
	int swap;
	int fnum;
	unsigned int min=0xffffffff;
	unsigned char* ramp;
	// Determine whick bank to be stored in disk
	for(i=0;i<16;i++){
		g_priorities[i]>>=1;
		if (bank==i) continue;
		if (!g_pointers[i]) continue;
		if (g_priorities[i]<min) {
			swap=i;
			min=g_priorities[i];
		}
	}
	// Detemine file number to use to store
	for(fnum=0;fnum<2;fnum++){
		if (g_swap_bank[fnum]<0) break;
	}
	// Store RAM to disk
	g_swap_bank[fnum]=swap;
	ramp=g_pointers[swap];
	g_pointers[swap]=0;
	FSfseek(g_swap_fhandle,4096*fnum,SEEK_SET);
	for(i=0;i<8;i++){
		FSfwrite(&ramp[i*512],1,512,g_swap_fhandle);
	}
	// Detemine file number to use to pick up
	for(fnum=0;fnum<2;fnum++){
		if (g_swap_bank[fnum]==bank) break;
	}
	// Recall RAM from disk
	g_swap_bank[fnum]=-1;
	g_pointers[bank]=ramp;
	FSfseek(g_swap_fhandle,4096*fnum,SEEK_SET);
	for(i=0;i<8;i++){
		FSfread(&ramp[i*512],1,512,g_swap_fhandle);
	}
}

UINT8 readRAM(UINT16 addr){
	int bank=addr>>12;
	g_priorities[bank]++;
	if (!g_pointers[bank]) swapData(bank);
	return ((char*)(g_pointers[bank]))[addr&0x0fff];
}

void writeRAM(UINT16 addr, UINT8 data){
	int bank=addr>>12;
	g_priorities[bank]++;
	if (!g_pointers[bank]) swapData(bank);
	((char*)(g_pointers[bank]))[addr&0x0fff]=data;
}
