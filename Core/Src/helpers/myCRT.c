// myCRT.c
// implementation of simple VT-100 style
// cursor positioning, colors and text attributes
// for usb vcp output
// 23/03/2015


#include "myCRT.h"
#include "xprintf.h"
#include "hex_gen.h"

// move cursor to position Xpos, Ypos
// upper left corner is 0,0


void CRT_gotoXY(uint8_t Xpos, uint8_t Ypos) // origin at 0,0
{
// Esc[Line;ColumnH	Move cursor to screen location v,h	CUP
	char		seq[] = "\033[000;000f";

	uint8_to_asciiz(Ypos, &seq[2]);
	uint8_to_asciiz(Xpos, &seq[6]);
	xputs(seq);
}

// set text foreground color
void CRT_textColor(uint8_t newColor)
{
	xputs("\033[0m\033[");  // start of ESC-seq
	if (newColor >= (uint8_t)0x10)
	{
		xputs("1m\033["); // second part of ESC-seq
	}; // of if
	switch (newColor)
	{
		case cBLACK:
		case cBOLDBLACK: /* thrird part*/
			xputs("30m");
			break;
		case cRED:
		case cBOLDRED:
			xputs("31m");
			break;
		case cGREEN:
		case cBOLDGREEN:
			xputs("32m");
			break;
		case cYELLOW:
		case cBOLDYELLOW:
			xputs("33m");
			break;
		case cBLUE:
		case cBOLDBLUE:
			xputs("34m");
			break;
		case cMAGENTA:
		case cBOLDMAGENTA:
			xputs("35m");
			break;
		case cCYAN:
		case cBOLDCYAN:
			xputs("36m");
			break;
		case cWHITE:
		case cBOLDWHITE:
			xputs("37m");
			break;
		default:
			xputs("37m");
			break;
	}
}

// set text background color
void CRT_textBackground(uint8_t newColor)
{
	xputs("\033[");  // start of ESC-seq
	switch (newColor) {
		case cBLACK:
		case cBOLDBLACK: /* thrird part*/
			xputs("40m");
			break;
		case cRED:
		case cBOLDRED:
			xputs("41m");
			break;
		case cGREEN:
		case cBOLDGREEN:
			xputs("42m");
			break;
		case cYELLOW:
		case cBOLDYELLOW:
			xputs("43m");
			break;
		case cBLUE:
		case cBOLDBLUE:
			xputs("44m");
			break;
		case cMAGENTA:
		case cBOLDMAGENTA:
			xputs("45m");
			break;
		case cCYAN:
		case cBOLDCYAN:
			xputs("46m");
			break;
		case cWHITE:
		case cBOLDWHITE:
			xputs("47m");
			break;
		default:
			xputs("40m");
			break;

	}
}

// return current X coordinate of the cursor
uint8_t CRT_whereX()
{
	return 0;
}

// return current Y coordinate of the cursor
uint8_t CRT_whereY()
{
	return 0;
}

/* ################################# EOF ##########################################################*/

