// myCRT.h
// implementation of simple VT-100 style
// cursor positioning, colors and text attributes
// for usb vcp output
// 23/03/2015

/* Define to prevent recursive inclusion -------------------------------------*/

#ifndef MYCRT_H
#define MYCRT_H

/* Includes ------------------------------------------------------------------*/

#include <stdint.h>

/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/

// COLORS
#define		cBLACK		(uint8_t)0x00      /* Black */
#define		cRED		(uint8_t)0x01      /* Red */
#define		cGREEN		(uint8_t)0x02      /* Green */
#define		cYELLOW		(uint8_t)0x03      /* Yellow */
#define		cBLUE		(uint8_t)0x04      /* Blue */
#define		cMAGENTA	(uint8_t)0x05      /* Magenta */
#define		cCYAN		(uint8_t)0x06      /* Cyan */
#define		cWHITE		(uint8_t)0x07      /* White */
#define		cBOLDBLACK	(uint8_t)0x10      /* Bold Black */
#define		cBOLDRED	(uint8_t)0x11     /* Bold Red */
#define		cBOLDGREEN	(uint8_t)0x12      /* Bold Green */
#define		cBOLDYELLOW	(uint8_t)0x13      /* Bold Yellow */
#define		cBOLDBLUE	(uint8_t)0x14      /* Bold Blue */
#define		cBOLDMAGENTA	(uint8_t)0x15      /* Bold Magenta */
#define		cBOLDCYAN	(uint8_t)0x16      /* Bold Cyan */
#define		cBOLDWHITE	(uint8_t)0x17      /* Bold White */


/* Exported macro ------------------------------------------------------------*/
/* Exported functions ------------------------------------------------------- */

void CRT_gotoXY(uint8_t Xpos, uint8_t Ypos);
void CRT_textColor(uint8_t newColor);
void CRT_textBackground(uint8_t newColor);
uint8_t CRT_whereX(void);
uint8_t CRT_whereY(void);

/* Clear the screen */
static inline void CRT_clearScreen(void)
{
//	ESC [ 2 J
	xputs("\033[2J");
}

/**
 * @brief CRT_resetToDefaults
 */
static inline void CRT_resetToDefaults(void)
{
	xputs("\033[39;49m");
}

static inline void CRT_resetAllAttr(void)
{
	xputs("\033[0m");
}

// move cursor to UL corner
static inline void CRT_cursorHome(void)
{
	xputs("\033[0;0f");
}

// clear all from current position to end of the linr
static inline void CRT_clrEOL()
{
	xputs("\033[K");
}

// bells :)
static inline void CRT_bell()
{
	xputc((char)0x07);
}

#endif /* MYCRT_H */

/************************ (C) COPYRIGHT tvv *****END OF FILE****/
