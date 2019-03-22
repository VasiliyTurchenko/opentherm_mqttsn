/** @file ip_helpers.c
 *  @brief ip configuration helpers
 *  @author turchenkov@gmail.com
 *  @bug
 *  @date 13-Mar-2019
 */

#include <stdbool.h>
#include <string.h>

#include "config_files.h"
#include "tiny-fs.h"
#include "ip_helpers.h"
#include "file_io.h"
#include "ascii_helpers.h"

/**
 * @brief ReadIPConfigFile reads IP configuration from the file
 * @param media
 * @param fname
 * @param ip_pair
 * @return FR_OK or error
 */
FRESULT ReadIPConfigFile(const Media_Desc_t * const media,
			 const char * const fname,
			 const struct IP * const template,
			 ip_pair_t * const ip_pair)
{
	FRESULT retVal = FR_INVALID_PARAMETER;

	if ((media == NULL) || (fname == NULL) || (ip_pair == NULL) || (template == NULL)) {
		goto fExit;
	}

	struct IP buf;
	uint32_t ip;
	uint16_t port;

	size_t br = 0U;

	retVal = ReadBytes(media,
			fname,
			0U,
			sizeof(struct IP),
			&br,
			(uint8_t*)&buf);

	if (retVal != FR_OK) {
		goto fExit;
	}
	if (br != sizeof(struct IP) ) {
		retVal = FR_INVALID_OBJECT;
		goto fExit;
	}


	if (memcmp(buf.ip_n, template->ip_n, 4U) != 0) {
		retVal = FR_INVALID_OBJECT;
		goto fExit;
	}
	if (isDec(&buf.ip_v, IP_LEN) == false) {
		retVal = FR_INVALID_OBJECT;
		goto fExit;
	}
	{
		uint8_t ip0;
		uint8_t ip1;
		uint8_t ip2;
		uint8_t ip3;

		ip0 = adec2byte(&buf.ip_v, 3U);
		ip1 = adec2byte(&buf.ip_v[3], 3U);
		ip2 = adec2byte(&buf.ip_v[6], 3U);
		ip3 = adec2byte(&buf.ip_v[9], 3U);
		ip = MAKE_IP(ip0, ip1, ip2, ip3);
	}

	if (isDec(&buf.ip_p, PORT_LEN) == false) {
		retVal = FR_INVALID_OBJECT;
		goto fExit;
	}

	port = adec2uint16(&buf.ip_p, PORT_LEN);

	ip_pair->ip = ip;
	ip_pair->port = port;
	retVal = FR_OK;

fExit:
	return retVal;
}


/**
 * @brief SaveIPConfigFile
 * @param media
 * @param fname
 * @param template
 * @return
 */
FRESULT SaveIPConfigFile(const Media_Desc_t * const media,
			 const char * const fname,
			 const struct IP * const template)
{
	FRESULT retVal = FR_INVALID_PARAMETER;

	if ((media == NULL) || (fname == NULL) || (template == NULL)) {
		goto fExit;
	}
	size_t bw = 0U;

	retVal = WriteBytes(media,
			fname,
			0U,
			sizeof(struct IP),
			&bw,
			(const uint8_t*)template);

	if (bw != sizeof(struct IP) ) {
		retVal = FR_INVALID_OBJECT;
	}

fExit:
	return retVal;
}
