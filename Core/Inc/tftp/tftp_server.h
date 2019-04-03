/**
 ******************************************************************************
 * @file    tftp_server.c
 * @author  turchenkov@gmail.com
 * @date    04-March-2018
 * @date    27-March-2019
 * @brief   Simple single-connection TFTP server
 ******************************************************************************
 */
#ifndef	TFTP_SERVER_H
#define	TFTP_SERVER_H

/* exported functions */
void tftpd_init(ip_pair_t *ip_params);
void tftpd_run(void);

#endif
/* ####################################  EOF #################################################### */
