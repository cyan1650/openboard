/********************************************************************************************************
 * @file	ota.c
 *
 * @brief	This is the source file for BLE SDK
 *
 * @author	BLE GROUP
 * @date	06,2020
 *
 * @par		Copyright (c) 2020, Telink Semiconductor (Shanghai) Co., Ltd.
 *			All rights reserved.
 *
 *          The information contained herein is confidential property of Telink
 *          Semiconductor (Shanghai) Co., Ltd. and is available under the terms
 *          of Commercial License Agreement between Telink Semiconductor (Shanghai)
 *          Co., Ltd. and the licensee or the terms described here-in. This heading
 *          MUST NOT be removed from this file.
 *
 *          Licensee shall not delete, modify or alter (or permit any third party to delete, modify, or
 *          alter) any information contained herein in whole or in part except as expressly authorized
 *          by Telink semiconductor (shanghai) Co., Ltd. Otherwise, licensee shall be solely responsible
 *          for any claim to the extent arising out of or relating to such deletion(s), modification(s)
 *          or alteration(s).
 *
 *          Licensees are granted free, non-transferable use of the information in this
 *          file under Mutual Non-Disclosure Agreement. NO WARRENTY of ANY KIND is provided.
 *
 *******************************************************************************************************/
#include "tl_common.h"
#include "drivers.h"

#include "ota.h"
#include "ota_stack.h"
#include "ota_server.h"



_attribute_ble_data_retention_  unsigned short 		crc16_poly[2] = {0, 0xa001}; //0x8005 <==> 0xa001

_attribute_no_inline_  //for big OTA PDU, CRC calculate should be quick
unsigned short crc16 (unsigned char *pD, int len)
{
    unsigned short crc = 0xffff;
    //unsigned char ds;
    int i,j;

    for(j=len; j>0; j--)
    {
        unsigned char ds = *pD++;
        for(i=0; i<8; i++)
        {
            crc = (crc >> 1) ^ crc16_poly[(crc ^ ds ) & 1];
            ds = ds >> 1;
        }
    }

     return crc;
}


#if 0
unsigned long crc32_cal(unsigned long crc, unsigned char* input, unsigned long* table, int len)
{
    unsigned char* pch = input;
    for(int i=0; i<len; i++)
    {
        crc = (crc>>8) ^ table[(crc^*pch) & 0xff];
        pch++;
    }

    return crc;
}
#endif


_attribute_no_inline_
unsigned long crc32_half_cal(unsigned long crc, unsigned char* input, unsigned long* table, int len)
{
    unsigned char* pch = input;
    for(int i=0; i<len; i++)
    {
        crc = (crc>>4) ^ table[(crc^*pch) & 0x0f];
        pch++;
    }

    return crc;
}


_attribute_no_inline_
unsigned long crc32_cal(unsigned long crc, unsigned char* input, unsigned long* table, int len)
{
    unsigned char* pch = input;
    for(int i=0; i<len; i++)
    {
        crc = (crc>>8) ^ table[(crc^*pch) & 0xff];
        pch++;
    }

    return crc;
}
