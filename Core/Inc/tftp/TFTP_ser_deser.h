/**
  ******************************************************************************
  * @file    TFTP_ser_deser.h
  * @author  turchenkov@gmail.com
  * @date    11-Mar-2018
  * @brief   Serialisation and deserialization functions for the tftp protocol header file 
  ******************************************************************************
  * @attention use at your own risk
  ******************************************************************************
  */ 

#ifndef __TFTP_SER_DESER_H
#define __TFTP_SER_DESER_H

#include "TFTP_data.h"

/* exported functions */

ErrorStatus TFTP_Deserialize_Packet(uint8_t * const buf, const size_t buflen, const tftp_context_p context);
size_t TFTP_Serialize_Packet(uint8_t * const buf, const size_t buflen, const tftp_context_p context);

#endif
/* ################################### E.O.F. ################################################### */
