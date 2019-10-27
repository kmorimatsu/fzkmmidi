/*
   This file is provided under the LGPL license ver 2.1.
   Written by Katsumi.
   https://github.com/kmorimatsu/
*/

#include <xc.h>
#include <sys/attribs.h>

extern int g_temp;

extern volatile unsigned short drawcount;
extern unsigned short g_keybuff[32];
extern unsigned char g_keymatrix[16];
extern volatile unsigned char g_keymatrix2[10];
extern unsigned char g_video_disabled;
extern unsigned char g_beep;
extern volatile unsigned char g_vblank;
extern unsigned char* RAM;
extern unsigned char* VRAM;
extern unsigned char g_caps;

int coretimer(void);
void led_green(void);
void led_red(void);

void video_init(void);
unsigned char char2ascii(unsigned char code);
unsigned char ascii2char(unsigned char ascii);
void clearscreen(void);
void printchar(int cursor,unsigned char ascii);
void printstr(int cursor,char* str);
void printhex4(int cursor, unsigned char val);
void printhex8(int cursor, unsigned char val);
void printhex16(int cursor, unsigned short val);
void printhex32(int cursor, unsigned int val);
void vertical_scroll(void);

void init_usb(void);
char connect_usb(void);
char try_usbmemory(unsigned short regPC);
void cpmdisks_init(void);
void init_file(void);

void file_select(void);

extern char g_ide_filename[13];
void ide_init(void);
unsigned char ide_read(unsigned char reg);
void ide_write(unsigned char reg, unsigned char val);

extern char g_swap_filename[13];
void init_swap(void);
