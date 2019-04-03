/** @file tiny-fs.h
 *  @brief Tiny filesystem
 *
 *  @author turchenkov@gmail.com
 *  @bug
 *  @date 26-Jan-2019
 *  @date 08-Mar-2019
 */

#ifndef TINY_FS_H
#define TINY_FS_H

#ifdef __cplusplus
extern "C"
{
#endif

#ifdef STM32F303xC

#include "stm32f303xc.h"
#include "stm32f3xx_hal.h"
#include "stm32f3xx_hal_gpio.h"
#include "stm32f3xx_hal_rcc.h"
#define NDEBUG_STATIC static inline

#elif STM32F103xB

#include "stm32f103xb.h"
#include "stm32f1xx_hal.h"
#include "stm32f1xx_hal_gpio.h"
#include "stm32f1xx_hal_rcc.h"
#define NDEBUG_STATIC static inline

#else

#include "debug.h"
#include "port.h"
#include "mock.h"

#endif


#define MAX_FILENAME_LEN 7U

#define DIR_ENTRIES 64U /* elements */

#define FS_DTA_SIZE 64U /* data transfer area size for filesystem driver */

#define FS_CLUSTER_SIZE 16U /* 16 bytes per cluster */

#define FS_VERSION 0x00000001U

#define FS_CONCURRENT_FILES 01U /* number of files processed at the momnet */

/*****************************************************************************
 *
 * MEDIA MAP
 *
 * OFFSET	LENGTH			CONTENT
 *
 * 0000		12 bytes		FAT_Header_t
 * 0012		24 * DIR_ENTRIES	DIR_Entry_t * DIR_ENTRIES
 * 1548		X			Cluster_Table = "FATFAT" special file
 * 1548 + X	STORAGE AREA
 */


typedef char TCHAR;

typedef size_t FSIZE_t;

/* This type MUST be 8 bit */
typedef unsigned char	BYTE;

typedef uint32_t UINT;

/* error codes */

/* File function return code (FRESULT) */
/* (c) ChaN */

typedef enum {
	FR_OK = 0, /* (0) Succeeded */
	FR_DISK_ERR, /* (1) A hard error occurred in the low level disk I/O layer */
	FR_INT_ERR,      /* (2) Assertion failed */
	FR_NOT_READY,    /* (3) The physical drive cannot work */
	FR_NO_FILE,      /* (4) Could not find the file */
	FR_NO_PATH,      /* (5) Could not find the path */
	FR_INVALID_NAME, /* (6) The path name format is invalid */
	FR_DENIED, /* (7) Access denied due to prohibited access or directory full */
	FR_EXIST,	   /* (8) Access denied due to prohibited access */
	FR_INVALID_OBJECT,  /* (9) The file/directory object is invalid */
	FR_WRITE_PROTECTED, /* (10) The physical drive is write protected */
	FR_INVALID_DRIVE,   /* (11) The logical drive number is invalid */
	FR_NOT_ENABLED,     /* (12) The volume has no work area */
	FR_NO_FILESYSTEM,   /* (13) There is no valid FAT volume */
	FR_MKFS_ABORTED,    /* (14) The f_mkfs() aborted due to any problem */
	FR_TIMEOUT, /* (15) Could not get a grant to access the volume within defined period */
	FR_LOCKED, /* (16) The operation is rejected according to the file sharing policy */
	FR_NOT_ENOUGH_CORE, /* (17) LFN working buffer could not be allocated */
	FR_TOO_MANY_OPEN_FILES, /* (18) Number of open files > FF_FS_LOCK */
	FR_INVALID_PARAMETER    /* (19) Given parameter is invalid */
} FRESULT;

typedef enum {
	MEDIA_RW,	/* media can be read and written */
	MEDIA_RO	/* read-only media */
} Meida_Mode_t;

/* Media decriptor */
typedef struct {
	ErrorStatus (*readFunc)(uint8_t *, uint32_t,
				size_t); /* read function for the media */
	ErrorStatus (*writeFunc)(uint8_t *, uint32_t,
				 size_t); /* write function for the media */
	size_t MediaSize;

	Meida_Mode_t mode;
} Media_Desc_t;

typedef Media_Desc_t *Media_Desc_p;

/**
  * @brief  retVal32_t is universal 32-bit return value
  */
typedef union {
	size_t size_tRV;
	uint32_t u32_tRV;
	uintptr_t uintptr_tRV;
	void *ptr_tRV;
} retVal32_t;

/**
  * @brief  urv_t is universal 32-bit return value plus error code
  */
typedef struct __attribute__((packed)) {
	retVal32_t val;
	uint8_t errCode;
} urv_t;

typedef urv_t *urv_p;

/**
  * @brief  fMode_t - file opening modes
  */
typedef enum fMode { FModeRead = 1, FModeWrite } fMode_t;

/**
  * @brief  fState_t - file status on the disk
  */
typedef enum fState {
	FStateNoFile = 1, /* deleted file = free space */
	FStateClosed,
	FStateOpenedR,
	FStateOpenedW,
	FStateFAT /* only one and the first file is the FAT storage */
} fState_t;

/* FAT header */
typedef struct {
	uint32_t FS_version;
	uint32_t DIR_CRC32;
	size_t FAT_ClusterTableSize;
	uint32_t dummy;		/* not used */
} FAT_Header_t;

/* FAT entry */
typedef struct __attribute__((packed)) {
	char FileName[MAX_FILENAME_LEN + 1U];
	fState_t FileStatus;		// 2 bytes
	uint16_t FileSize;
	size_t FileAddress;
	uint32_t FileCRC32;
} DIR_Entry_t;

typedef DIR_Entry_t *DIR_Entry_p;

typedef uint32_t mutex_t;

/**
  * @brief  fMode_t - file handle
  */
typedef struct {
	Media_Desc_p media;	/* descriptor of the media */
	DIR_Entry_t fileDir;
	size_t filePtr;
	mutex_t * pmutex;
} fHandle_t;

typedef fHandle_t FIL;

typedef fHandle_t *fHandle_p;

/* FAT cluster table */
typedef struct __attribute__((packed)) {
	uint8_t ctElement; /* 8 bits = 8 clusters */
} FAT_ClusterTable_t;

typedef FAT_ClusterTable_t *FAT_ClusterTable_p;

/* Begin of table */
typedef struct __attribute__((packed)) {
	FAT_Header_t h;
	DIR_Entry_t entry0;
	DIR_Entry_t entries[DIR_ENTRIES - 1U];
	//	FAT_ClusterTable_t clusterTable;

} FAT_Begin_t;

typedef FAT_Begin_t *FAT_Begin_p;

#define RESERVED_AREA                                                          \
	(sizeof(FAT_Header_t) + (DIR_ENTRIES * sizeof(DIR_Entry_t)))


/* these functions ain't static only in DEBUG mode */

#ifdef DEBUG
#if !defined(STM32F303xC) && !defined(STM32F103xB)

uint32_t findEntry(const Media_Desc_t * const media, const DIR_Entry_p entry);

uint8_t getBitMask(uint8_t n);

uint32_t allocateClusters(uint8_t *clusterTable,
				       size_t clusterTableSize,
				       size_t requestedSize);

size_t getNumClusters(const Media_Desc_t * const media);

size_t getClusterFileSize(size_t clusterTableSize);

size_t findMaxFreeBlock(const Media_Desc_t * media);

size_t getClusterTableSize(const Media_Desc_t * const media);

#endif
#endif

void InitFS(void);

ErrorStatus Format(const Media_Desc_t * const media);

FRESULT NewFile(fHandle_p file, const char *name, size_t size, fMode_t mode);

FRESULT f_open(FIL* fp, const TCHAR* path, BYTE mode);				/* Open or create a file */

FRESULT CloseFile(fHandle_p file);

FRESULT f_close (FIL* fp);							/* Close an open file object */

FRESULT DeleteFile(const Media_Desc_t *media, const char *name);

FRESULT f_unlink (const TCHAR* path);						/* Delete an existing file or directory */

FRESULT f_rewind(FIL *fp);

FRESULT f_lseek(FIL *fp, FSIZE_t ofs);

FSIZE_t f_tell (FIL* fp);

FRESULT f_write (FIL* fp, void * const buff, UINT btw, UINT* bw);

FRESULT f_read(FIL *fp, void *buff, UINT btr, UINT *br);

FRESULT f_checkFS(const Media_Desc_t *media);

const char *FRESULT_String(FRESULT res);

#ifdef __cplusplus
}
#endif

#endif
