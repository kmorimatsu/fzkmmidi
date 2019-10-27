/*
	This file was modified for KM-Z80 implementation,
	from the file provided by Microchip.
	USB_ApplicationEventHandler() is provided by Microchip.
	The other functions are written by Katsumi.
*/

/********************************************************************
 FileName:     main.c
 Dependencies: See INCLUDES section
 Processor:		PIC18, PIC24, and PIC32 USB Microcontrollers
 Hardware:		This demo is natively intended to be used on Microchip USB demo
 				boards supported by the MCHPFSUSB stack.  See release notes for
 				support matrix.  This demo can be modified for use on other hardware
 				platforms.
 Complier:  	Microchip C18 (for PIC18), C30 (for PIC24), C32 (for PIC32)
 Company:		Microchip Technology, Inc.

 Software License Agreement:

 The software supplied herewith by Microchip Technology Incorporated
 (the “CompanyE for its PIC® Microcontroller is intended and
 supplied to you, the Company’s customer, for use solely and
 exclusively on Microchip PIC Microcontroller products. The
 software is owned by the Company and/or its supplier, and is
 protected under applicable copyright laws. All rights are reserved.
 Any use in violation of the foregoing restrictions may subject the
 user to criminal sanctions under applicable laws, as well as to
 civil liability for the breach of the terms and conditions of this
 license.

 THIS SOFTWARE IS PROVIDED IN AN “AS ISECONDITION. NO WARRANTIES,
 WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING, BUT NOT LIMITED
 TO, IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 PARTICULAR PURPOSE APPLY TO THIS SOFTWARE. THE COMPANY SHALL NOT,
 IN ANY CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL OR
 CONSEQUENTIAL DAMAGES, FOR ANY REASON WHATSOEVER.

********************************************************************
 File Description:

 Change History:
  Rev   Description
  1.0   Initial release
  2.1   Updated for simplicity and to use common
                     coding style
********************************************************************/

#include "main.h"
#include "usbhost/FSIO.h"

volatile BOOL deviceAttached;

/*
	This module needs heap area, >= 448 bytes. Using 512 bytes would be a good idea.
*/

void init_usb(void){
	int i;
	// Wait for PLL lock to stabilize
	while(!(OSCCON&0x0020));
    deviceAttached = FALSE;
    //Initialize the stack
    USBInitialize(0);
}

/****************************************************************************
  Function:
    BOOL USB_ApplicationEventHandler( BYTE address, USB_EVENT event,
                void *data, DWORD size )

  Summary:
    This is the application event handler.  It is called when the stack has
    an event that needs to be handled by the application layer rather than
    by the client driver.

  Description:
    This is the application event handler.  It is called when the stack has
    an event that needs to be handled by the application layer rather than
    by the client driver.  If the application is able to handle the event, it
    returns TRUE.  Otherwise, it returns FALSE.

  Precondition:
    None

  Parameters:
    BYTE address    - Address of device where event occurred
    USB_EVENT event - Identifies the event that occured
    void *data      - Pointer to event-specific data
    DWORD size      - Size of the event-specific data

  Return Values:
    TRUE    - The event was handled
    FALSE   - The event was not handled

  Remarks:
    The application may also implement an event handling routine if it
    requires knowledge of events.  To do so, it must implement a routine that
    matches this function signature and define the USB_HOST_APP_EVENT_HANDLER
    macro as the name of that function.
  ***************************************************************************/

BOOL USB_ApplicationEventHandler( BYTE address, USB_EVENT event, void *data, DWORD size )
{
    switch( event )
    {
        case EVENT_VBUS_REQUEST_POWER:
            // The data pointer points to a byte that represents the amount of power
            // requested in mA, divided by two.  If the device wants too much power,
            // we reject it.
            return TRUE;

        case EVENT_VBUS_RELEASE_POWER:
            // Turn off Vbus power.
            // The PIC24F with the Explorer 16 cannot turn off Vbus through software.

            //This means that the device was removed
            deviceAttached = FALSE;
            return TRUE;
            break;

        case EVENT_HUB_ATTACH:
            return TRUE;
            break;

        case EVENT_UNSUPPORTED_DEVICE:
            return TRUE;
            break;

        case EVENT_CANNOT_ENUMERATE:
            //UART2PrintString( "\r\n***** USB Error - cannot enumerate device *****\r\n" );
            return TRUE;
            break;

        case EVENT_CLIENT_INIT_ERROR:
            //UART2PrintString( "\r\n***** USB Error - client driver initialization error *****\r\n" );
            return TRUE;
            break;

        case EVENT_OUT_OF_MEMORY:
            //UART2PrintString( "\r\n***** USB Error - out of heap memory *****\r\n" );
            return TRUE;
            break;

        case EVENT_UNSPECIFIED_ERROR:   // This should never be generated.
            //UART2PrintString( "\r\n***** USB Error - unspecified *****\r\n" );
            return TRUE;
            break;

        default:
            break;
    }

    return FALSE;
}

char connect_usb(void){
	// Detect USB memory
	drawcount=0;
	while(1){
		USBTasks();
		if (USBHostMSDSCSIMediaDetect()) {
			if (FSInit()) break;
			// USB memory format error
			return 0;
		} else if (120<drawcount) {
			// Time out
			return 0;
		}
	}
	deviceAttached=TRUE;
	return 1;
}

/*
; This is fake CP/M system file.
; To obtain valid disk image file containing CP/M (ver 2.2), go to:
; http://www.recfor.net/projects/cpm/
; 
; This file can be assembled by SDCC:
; sdasz80 -o %1
; sdcc *.rel -mz80 --code-loc 0xbc00 --data-loc 0xdc00 --no-std-crt0

CONOUT       =0xd20c

.area _CODE

startAddress:
	di
	ld sp,#startAddress
	ld hl,#infoString

	ld c,#0x0a
loop:
	call CONOUT
	ld c,(hl)
	inc hl
	xor a
	cp c
	jr nz,loop

	halt
	jr startAddress

infoString:
	.db 0x0d
	.db 0x0a
	.db 0x0d
	.db 0x0a
	.ascii "To start CP/M 2.2, a valid disk image containing CPM.SYS is reuired."
	.db 0x0d
	.db 0x0a
	.ascii "To obtain the disk image, go to:"
	.db 0x0d
	.db 0x0a
	.ascii "http://www.recfor.net/projects/cpm/"
	.db 0x0d
	.db 0x0a
	.db 0
*/
const unsigned char fakesys[]={
0xF3,0x31,0x00,0xBC,0x21,0x15,0xBC,0x0E,0x0A,0xCD,0x0C,0xD2,0x4E,0x23,0xAF,0xB9,
0x20,0xF7,0x76,0x18,0xEB,0x0D,0x0A,0x0D,0x0A,0x54,0x6F,0x20,0x73,0x74,0x61,0x72,
0x74,0x20,0x43,0x50,0x2F,0x4D,0x20,0x32,0x2E,0x32,0x2C,0x20,0x61,0x20,0x76,0x61,
0x6C,0x69,0x64,0x20,0x64,0x69,0x73,0x6B,0x20,0x69,0x6D,0x61,0x67,0x65,0x20,0x63,
0x6F,0x6E,0x74,0x61,0x69,0x6E,0x69,0x6E,0x67,0x20,0x43,0x50,0x4D,0x2E,0x53,0x59,
0x53,0x20,0x69,0x73,0x20,0x72,0x65,0x75,0x69,0x72,0x65,0x64,0x2E,0x0D,0x0A,0x54,
0x6F,0x20,0x6F,0x62,0x74,0x61,0x69,0x6E,0x20,0x74,0x68,0x65,0x20,0x64,0x69,0x73,
0x6B,0x20,0x69,0x6D,0x61,0x67,0x65,0x2C,0x20,0x67,0x6F,0x20,0x74,0x6F,0x3A,0x0D,
0x0A,0x68,0x74,0x74,0x70,0x3A,0x2F,0x2F,0x77,0x77,0x77,0x2E,0x72,0x65,0x63,0x66,
0x6F,0x72,0x2E,0x6E,0x65,0x74,0x2F,0x70,0x72,0x6F,0x6A,0x65,0x63,0x74,0x73,0x2F,
0x63,0x70,0x6D,0x2F,0x0D,0x0A,0x00,
};

void cpm_read(unsigned short address){
}

void cpm_write(unsigned short address){
}

unsigned char nextIs(unsigned char* str,int point){
	unsigned char i;
	// Skip blank
	for(i=0;0x20==RAM[point+i];i++);
	// Check
	while(str[0]){
		if (str[0]!=RAM[point+i]) return 0;
		str++;
		i++;
	}
	// All done
	return i;
}

void init_file(void){
	int i,point,end;
	unsigned char c;
	FSFILE* fhandle;
	// Read ini file to RAM area
	fhandle=FSfopen("fzkmmidi.ini","r");
	if (!fhandle) return;
	end=0xbc00+FSfread(&RAM[0xbc00],1,0x2400,fhandle);
	FSfclose(fhandle);
	// All Upper cases. 0x0d to 0x0a. Tab to space
	for(point=0xbc00;point<end;point++){
		c=RAM[point];
		if ('a'<=c && c<='z') RAM[point]=c-0x20;
		else if (0x0d==c) RAM[point]=0x0a;
		else if (0x09==c) RAM[point]=0x20;
	}
	// Explore
	point=0xbc00;
	while(point<end){
		// Check if comment
		if ('#'!=RAM[point]) {
			// Check CAPSLOCK
			if (c=nextIs("CAPSLOCK ",point)) {
				point+=c;
				if (c=nextIs("ON",point)) {
					point+=c;
					g_caps=1;
				} else if (c=nextIs("OFF",point)) {
					point+=c;
					g_caps=0;
				}
			// Check DISKFILE
			} else if (c=nextIs("DISKFILE ",point)) {
				point+=c;
				// Skip blank
				while(point<end && 0x20==RAM[point++]);
				// Set the file name
				for(i=0;i<12;i++) {
					if ((g_ide_filename[i]=RAM[point++])<0x21) break;
				}
				g_ide_filename[i]=0;
			// Check SWAPFILE
			} else if (c=nextIs("SWAPFILE ",point)) {
				point+=c;
				// Skip blank
				while(point<end && 0x20==RAM[point++]);
				// Set the file name
				for(i=0;i<12;i++) {
					if ((g_swap_filename[i]=RAM[point++])<0x21) break;
				}
				g_ide_filename[i]=0;
			}
		}
		// Go to next line
		while(point<end && 0x0a!=RAM[point++]);
		// Skip blank line
		while(point<end && 0x0a==RAM[point]) point++;
	}
}