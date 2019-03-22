/** @file file_io.c
 *  @brief file IO helpers
 *
 *  @author turchenkov@gmail.com
 *  @bug
 *  @date 10-Mar-2019
 */

#include "string.h"

#include "tiny-fs.h"


/**
 * @brief ReadBytes reads bytes from file
 * @param media pointer to media descriptor
 * @param name filename
 * @param fpos start flie position to read from
 * @param btr how many bytes must be read
 * @param br how many bytes are actually read
 * @param buf pointer to the buffer
 * @return FRESULT
 */
FRESULT ReadBytes(const Media_Desc_t * const media,
		   const char * const name,
		   size_t fpos,
		   size_t btr,
		   size_t * const br,
		   uint8_t * buf)
{
	FRESULT retVal = FR_INVALID_PARAMETER;
	FRESULT res;
	if ((media == NULL) || (name == NULL) || (br == NULL) || (buf == NULL)) {
		return retVal;
	}
	fHandle_t file;
	file.media = (Media_Desc_t*)media;
	char cut_name[MAX_FILENAME_LEN + 1];
	strncpy(cut_name, name, MAX_FILENAME_LEN);
	cut_name[MAX_FILENAME_LEN] = '\0';

	retVal = f_open(&file, cut_name, (BYTE)FModeRead);
	if (retVal != FR_OK) {
		goto fExit;
	}
	retVal = f_lseek(&file, (FSIZE_t)fpos);
	if (retVal != FR_OK) {
		goto fExit;
	}
	retVal = f_read(&file, buf, (UINT)btr, (UINT*)br);

	res = f_close(&file);

fExit:
	retVal = (retVal != FR_OK) ? retVal : res;
	return retVal;
}

/**
 * @brief WriteBytes writes bytes from buffer
 * @param media pointer to media descriptor
 * @param name filename
 * @param fpos start flie position to write to
 * @param btr how many bytes must be written
 * @param br how many bytes are actually written
 * @param buf pointer to the buffer
 * @return FRESULT
 */
FRESULT WriteBytes(const Media_Desc_t *const media,
		    const char * name,
		    size_t fpos,
		    size_t btw,
		    size_t * const bw,
		    const uint8_t * buf)
{
	FRESULT retVal = FR_INVALID_PARAMETER;
	FRESULT res;
	if ((media == NULL) || (name == NULL) || (bw == NULL) || (buf == NULL)) {
		return retVal;
	}
	fHandle_t file;
	file.media = (Media_Desc_t*)media;
	char cut_name[MAX_FILENAME_LEN + 1];
	strncpy(cut_name, name, MAX_FILENAME_LEN);
	cut_name[MAX_FILENAME_LEN] = '\0';

//	retVal = f_open(&file, cut_name, (BYTE)FModeWrite);

	retVal = NewFile(&file, cut_name, btw, FModeWrite);
	if (retVal != FR_OK) {
		goto fExit;
	}
	retVal = f_lseek(&file, (FSIZE_t)fpos);
	if (retVal != FR_OK) {
		goto fExit;
	}
	retVal = f_write(&file, buf, (UINT)btw, (UINT*)bw);
fExit:
	res = f_close(&file);
	retVal = (retVal != FR_OK) ? retVal : res;
	return retVal;
}

