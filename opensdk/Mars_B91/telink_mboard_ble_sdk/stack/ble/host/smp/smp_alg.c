/********************************************************************************************************
 * @file	smp_alg.c
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
#include "stack/ble/host/ble_host.h"
#include "stack/ble/controller/ble_controller.h"

#include "smp_alg.h"




static void blt_smp_alg_aes_xor_128(unsigned char *a, unsigned char *b, unsigned char *out)
{
	for (int i=0;i<16; i++){
		out[i] = a[i] ^ b[i];
	}
}

/**
 * @brief   	This function is used to generate the confirm values
 * @param[out]  c1: the confirm value, 	little--endian.
 * @param[in]   key: aes key, 			little--endian.
 * @param[in]   r: the plaintext, 		little--endian.
 * @param[in]   pres: packet buffer2, 	little--endian.
 * @param[in]   preq: packet buffer2, 	little--endian.
 * @param[in]   iat: initiate address type
 * @param[in]   ia: initiate address, 	little--endian.
 * @param[in]   rat: response address type
 * @param[in]   ra: response address, 	little--endian.
 * @return  	none.
 * @Note		Input data requires strict Word alignment
 */
void blt_smp_alg_c1(unsigned char *c1, unsigned char key[16], unsigned char r[16], unsigned char pres[7], unsigned char preq[7], unsigned char iat, unsigned char ia[6], unsigned char rat, unsigned char ra[6])
{
	u8 p1[16] = {0};
	u8 p2[16] = {0};

	//p1
	p1[0] = iat;
	p1[1] = rat;
	smemcpy(&p1[2], preq, 7);
	smemcpy(&p1[9], pres, 7);
	blt_smp_alg_aes_xor_128(r, p1, p1);

	//p2
	smemcpy(&p2[0], ra, 6);//responder address
	smemcpy(&p2[6], ia, 6);//initiate address

	aes_ll_encryption (key, p1, c1);

	blt_smp_alg_aes_xor_128(c1, p2, p2);

	aes_ll_encryption (key, p2, c1);
}


/**
 * @brief   	This function is used to generate the STK during the LE legacy pairing process.
 * @param[out]  *STK - the result of encrypt, little--endian.
 * @param[in]   *key - aes key, little--endian.
 * @param[in]   *r1 - the plaintext1, little--endian.
 * @param[in]   *r2 - the plaintext2, little--endian.
 * @return  	none.
 * @Note		Input data requires strict Word alignment
 */
void blt_smp_alg_s1(unsigned char *stk, unsigned char key[16], unsigned char r1[16], unsigned char r2[16])
{
	memcpy (stk, r1, 8);
	memcpy (stk+8, r2, 8);
	aes_ll_encryption (key, stk, stk);
}


#if (1)   //Support Secure Connection

static void blt_smp_alg_aes_leftshift_onebit(unsigned char *input,unsigned char *output)
{
	unsigned char overflow = 0;

	for (int i=15; i>=0; i-- ) {
		unsigned char in = input[i];
		output[i] = in << 1;
		output[i] |= overflow;
		overflow = (in & 0x80)?1:0;
	}
}


static void blt_smp_alg_aes_padding ( unsigned char *lastb, unsigned char *pad, int length )
{
	/* original last block */
	for (int j=0; j<16; j++ ) {
		if ( j < length ) {
			pad[j] = lastb[j];
		}
		else if ( j == length ) {
			pad[j] = 0x80;
		}
		else {
			pad[j] = 0x00;
		}
	}
}


/////////////////////////////////////////////////////////////////////////////////////
//		AES_CMAC
/////////////////////////////////////////////////////////////////////////////////////

/**
 * @brief		The function is used to calculate the AES-CMAC of the input message
 * @param[in]   key: 	is the 128-bit key, big--endian.
 * @param[in]   *msg: 	is the input message to be authenticated, big--endian.
 * @param[in]   msg_len:is the input message's length
 * @param[out]  mac: 	is the 128-bit message authentication code (MAC), big--endian.
 * @return	none.
 */
static void blt_smp_alg_aes_cmac ( unsigned char *key, unsigned char *input, int length, unsigned char *mac )
{
	unsigned char const_Zero_Rb[20] = {
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x87
	};

	unsigned char       K[16], L[16], X[16];
	int         n, i, flag;

	n = (length+15) / 16;       /* n is number of rounds */

	if ( n == 0 ) {
		n = 1;
		flag = 0;
	}
	else {
		if ( (length%16) == 0 ) { /* last block is a complete block */
			flag = 1;
		}
		else { /* last block is not complete block */
			flag = 0;
		}
	}

	//////////////// Generate K1 or K2 => K ///////////////
	aes_encrypt(key,(unsigned char *)const_Zero_Rb,L);

	if ( (L[0] & 0x80) == 0 ) { /* If MSB(L) = 0, then K1 = L << 1 */
		blt_smp_alg_aes_leftshift_onebit(L,K);
	}
	else {    /* Else K1 = ( L << 1 ) (+) Rb */
		blt_smp_alg_aes_leftshift_onebit(L,K);
		blt_smp_alg_aes_xor_128(K,(unsigned char *)(const_Zero_Rb + 4),K);
	}

	if (!flag)
	{
		if ( (K[0] & 0x80) == 0 ) {
			blt_smp_alg_aes_leftshift_onebit(K,K);
		} else {
			blt_smp_alg_aes_leftshift_onebit(K,L);
			blt_smp_alg_aes_xor_128(L,(unsigned char *)(const_Zero_Rb + 4),K);
		}
	}
	///////////////////////////////////////////////////////

	if ( flag ) { /* last block is complete block */
		blt_smp_alg_aes_xor_128(&input[16*(n-1)],K,K);
	}
	else {
		blt_smp_alg_aes_padding(&input[16*(n-1)],L,length%16);
		blt_smp_alg_aes_xor_128(L,K,K);
	}

	for ( i=0; i<16; i++ ) {
		X[i] = 0;
	}

	for ( i=0; i<n-1; i++ ) {
		blt_smp_alg_aes_xor_128(X,&input[16*i],X); /* Y := Mi (+) X  */
		aes_encrypt(key,X,X);     		  /* X := AES-128(KEY, Y); */
	}

	blt_smp_alg_aes_xor_128(X,K,X);
	aes_encrypt(key,X,X);
	memcpy (mac, X, 16);
}


/**
 * @brief		This function is used to compute confirm value by function f4
 * 				---  Ca: f4(U, V, X, Z) = AES-CMACX (U || V || Z)  ---
 * @param[out]  r: the output of the confirm:128-bits, big--endian.
 * @param[in]   u: 	is the 256-bits, 	big--endian.
 * @param[in]   v: 	is the 256-bits,	big--endian.
 * @param[in]   x:  is the 128-bits, 	big--endian.
 * @param[in]   z:  is the 8-bits
 * @return	none.
 */
void blt_smp_alg_f4 (unsigned char *r, unsigned char u[32], unsigned char v[32], unsigned char x[16], unsigned char z)
{
	unsigned char d[65];
	memcpy (d, u, 32);
	memcpy (d + 32, v, 32);
	d[64] = z;
	blt_smp_alg_aes_cmac (x, d, 65, r);
}


/**
 * @brief	This function is used to generate the numeric comparison values during authentication
 * 			stage 1 of the LE Secure Connections pairing process by function g2
 * @param[in]   u: 	is the 256-bits, 	big--endian.
 * @param[in]   v: 	is the 256-bits, 	big--endian.
 * @param[in]   x:  is the 128-bits, 	big--endian.
 * @param[in]   y:  is the 128-bits, 	big--endian.
 * @return	pincode value: 32-bits.
 */
unsigned int blt_smp_alg_g2 (unsigned char u[32], unsigned char v[32], unsigned char x[16], unsigned char y[16])
{
	unsigned char d[80], z[32];
	memcpy (d, u, 32);
	memcpy (d + 32, v, 32);
	memcpy (d + 64, y, 16);

	blt_smp_alg_aes_cmac (x, d, 80, z);

	return z[12] | (z[13] << 8) | (z[14] << 16) | (z[15] << 24);;
}


/**
 * @brief	This function is used to generate derived keying material in order to create the LTK
 * 			and keys for the commitment function f6 by function f5
 * 			--- f5(W, N1, N2, KeyID, A1, A2) = HMAC-SHA-256W (N1 || N2|| KeyID || A1|| A2) ---
 * @param[out]  mac: the output of the MAC value:128-bits, big--endian.
 * @param[out]  ltk: the output of the LTK value:128-bits, big--endian.
 * @param[in]   w: 	is the 256-bits, 	big--endian.
 * @param[in]   n1:	is the 128-bits, 	big--endian.
 * @param[in]   n2:	is the 128-bits, 	big--endian.
 * @param[in]   a1:	is the 56-bits, 	big--endian.
 * @param[in]   a2:	is the 56-bits, 	big--endian.
 * @return	none.
 */
void blt_smp_alg_f5 (unsigned char *mac, unsigned char *ltk, unsigned char w[32], unsigned char n1[16], unsigned char n2[16],
				  unsigned char a1[7], unsigned char a2[7])
{
	unsigned char d[56], t[16];
	const unsigned char s[16] =	{0x6C,0x88, 0x83,0x91, 0xAA,0xF5, 0xA5,0x38, 0x60,0x37, 0x0B,0xDB, 0x5A,0x60, 0x83,0xBE};
	//const unsigned charkeyID[4] = {0x65, 0x6c, 0x74, 0x62};		//0x62746c65;

	blt_smp_alg_aes_cmac ((unsigned char *)s, w, 32, t);

	d[0] = 0;
	d[1] = 0x62;
	d[2] = 0x74;
	d[3] = 0x6c;
	d[4] = 0x65;

	memcpy (d + 5, n1, 16);
	memcpy (d + 21, n2, 16);
	memcpy (d + 37, a1, 7);
	memcpy (d + 44, a2, 7);

	d[51] = 1;
	d[52] = 0;

	blt_smp_alg_aes_cmac (t, d, 53, mac);

	d[0] = 1;
	blt_smp_alg_aes_cmac (t, d, 53, ltk);
}


/**
 * @brief	This function is used to generate check values during authentication stage 2 of the
 * 			LE Secure Connections pairing process by function f6
 * 			--- f6(W, N1, N2, R, IOcap, A1, A2) = AES-CMACw (N1 || N2 || R || IOcap || A1 || A2) ---
 * @param[out]  *e: the output of Ea or Eb:128-bits, big--endian.
 * @param[in]   w: 	is the 256-bits, 	big--endian.
 * @param[in]   n1:	is the 128-bits, 	big--endian.
 * @param[in]   n2:	is the 128-bits, 	big--endian.
 * @param[in]   a1:	is the 56-bits, 	big--endian.
 * @param[in]   a2:	is the 56-bits, 	big--endian.
 * @return	none.
 */
void blt_smp_alg_f6 (unsigned char *e, unsigned char w[16], unsigned char n1[16], unsigned char n2[16],
				  unsigned char r[16], unsigned char iocap[3], unsigned char a1[7], unsigned char a2[7])
{
	unsigned char d[68];

	memcpy (d + 0, n1, 16);
	memcpy (d + 16, n2, 16);
	memcpy (d + 32, r, 16);
	memcpy (d + 48, iocap, 3);
	memcpy (d + 51, a1, 7);
	memcpy (d + 58, a2, 7);

	blt_smp_alg_aes_cmac (w, d, 65, e);
}


/**
 * @brief	This function is used to convert keys of a given size from one key type to another
 * 			key type with equivalent strength
 * 			--- h6(W, keyID) = AES-CMACW(keyID ---
 * @param[out]  r: the output of h6:128-bits, 	big--endian.
 * @param[in]   w: 	is the 128-bits, 			big--endian.
 * @param[in]   keyid:	is the 32-bits, 		big--endian.
 * @return	none.
 */
void blt_smp_alg_h6 (unsigned char *r, unsigned char w[16], unsigned char keyid[4])
{
	blt_smp_alg_aes_cmac (w, keyid, 4, r);
}


/**
 * @brief	This function is used to convert keys of a given size from one key type to another
 * 			key type with equivalent strength
 * 			--- h7(SALT, W) = AES-CMACsalt(W) ---
 * @param[out]  r: the output of h7:128-bits, 	big--endian.
 * @param[in]   salt: is the 128-bits, 			big--endian.
 * @param[in]   w:	is the 128-bits, 			big--endian.
 * @return	none.
 */
void blt_smp_alg_h7 (unsigned char *r, unsigned char salt[16], unsigned char w[16])
{
	blt_smp_alg_aes_cmac (salt, w, 16, r);
}

#endif


#if (LL_FEATURE_ENABLE_LL_PRIVACY)

void blt_crypto_alg_prand(u8 prand[3])
{
#if 0
	u16 sum = 0;

	do{
		sum = 0;
		generateRandomNum(3, prand);
		sum = prand[0]+ prand[1] + prand[2];
		/*
		 * All bits of the random part of prand shall not be equal to 0
		 * All bits of the random part of prand shall not be equal to 1
		 */
		if(sum != 0 && sum != 0x02FD){
			/* Upper two bits must be 01 */
			prand[2] = (prand[2] & 0x3F) | 0x40;
			break;
		}
	}while(1);
#else
	u32 random = rand();
	u8* prandom = (u8*)&random;
	prand[0] = prandom[0];
	prand[1] = prandom[1];
	prand[2] = (prandom[2] & 0x3F) | 0x40;
#endif
}


/*
 * @brief 	Resolvable Private Address Generation and Resolution
 * @param[in]	irk - The IRKs are stored in little endian format
 * @param[in]	r   - The r are stored in little endian format
 * @param[out]	out - The out are stored in little endian format
 * @return      none
 * */
#if RPA_OPTIMIZE_RAM

#else
_attribute_ram_code_
#endif
void  blt_crypto_alg_ah(const u8 irk[16], u8 r[3], u8 out[3])
{
#if (MCU_CORE_TYPE == MCU_CORE_825x || MCU_CORE_TYPE == MCU_CORE_827x)
	u32 aes_temp;
	u8* pAesRslt = NULL;

	//need irq disable protect (to prevent reentry)
	u32 irq = irq_disable();

	/*Set AES as encryption mode*/
	reg_aes_ctrl = 0x00;

    //set AES key (stored in big endian format)
	REG_ADDR32(aeskey_reg_start) = *((volatile u32 *) irk);
	REG_ADDR32(aeskey_reg_start + 4) = *((volatile u32 *)(irk + 4));
	REG_ADDR32(aeskey_reg_start + 8) = *((volatile u32 *)(irk + 8));
	REG_ADDR32(aeskey_reg_start + 12) = *((volatile u32 *)(irk + 12));

	//Set AES data, after write four times, start to encrypt automatically (stored in big endian format)
	reg_aes_data = 0x00;
	reg_aes_data = 0x00;
	reg_aes_data = 0x00;
	reg_aes_data = (r[0] << 24) + (r[1] << 16) + (r[2] << 8);  //prand 3 byte

	//start encrypt, wait for aes ready
	while(!(reg_aes_ctrl & BIT(2)));

	//Read AES Result, need only the highest 3 bytes (stored in big endian format)
	aes_temp = reg_aes_data;
	aes_temp = reg_aes_data;
	aes_temp = reg_aes_data;
	aes_temp = reg_aes_data; //hash 3 byte

	irq_restore(irq);

	/* The output of the random address function ah is: ah(h, r) = e(k, r') mod 2^24
	 * The output of the security function e is then truncated to 24 bits by taking the least significant 24
	 * bits of the output of e as the result of ah.
	 */
	pAesRslt = (u8*)&aes_temp;
	out[0] = pAesRslt[3];
	out[1] = pAesRslt[2];
	out[2] = pAesRslt[1];
#elif (MCU_CORE_TYPE == MCU_CORE_9518)
	//need irq disable protect (to prevent reentry)
	u32 irq = irq_disable();

	u32 *irk_key = (u32*)irk;

	int i;

	for (i=0; i<4; i++) {
		reg_aes_key(i) = irk_key[i];
	}

	aes_data_buff[0] = ((r[0] << 0) | (r[1] << 8) | (r[2] << 16) );  //prand 3 byte
	aes_data_buff[1] = aes_data_buff[2] = aes_data_buff[3] = 0;
	reg_aes_ptr = (u32)aes_data_buff;

    reg_aes_mode = FLD_AES_START|AES_ENCRYPT_MODE;   //encrypt

    while(reg_aes_mode & FLD_AES_START);

	irq_restore(irq);

	/* The output of the random address function ah is: ah(h, r) = e(k, r') mod 2^24
	 * The output of the security function e is then truncated to 24 bits by taking the least significant 24
	 * bits of the output of e as the result of ah.
	 */
	u32 aesRslt = aes_data_buff[4];
	out[0] = U32_BYTE0(aesRslt);
	out[1] = U32_BYTE1(aesRslt);
	out[2] = U32_BYTE2(aesRslt);
#endif
}
#endif   ////#if (LL_FEATURE_ENABLE_LL_PRIVACY)

#if (LL_FEATURE_ENABLE_CONNECTIONLESS_ISO)
/**
 * @brief	This function is used to generate the Group Session Key (GSK) for encrypting or
 * 			decrypting payloads of an encrypted BIS.
 * 			--- h8(K, S, keyID) = AES-CMACik(keyID) ---
 * @param[out]  r: the output of h8:128-bits, 	big--endian.
 * @param[in]   k: is the 128-bits, 			big--endian.
 * @param[in]   s: is the 128-bits, 			big--endian.
 * @param[in]   keyid: is the 32-bits, 			big--endian.
 * @return	none.
 */
void blt_smp_alg_h8 (unsigned char *r, unsigned char k[16], unsigned char s[16], unsigned char keyId[4])
{
	unsigned char ik[16];
	blt_smp_alg_aes_cmac (s, k, 16, ik);
	blt_smp_alg_aes_cmac (ik, keyId, 4, r);
	//printf("IK:");array_printf(ik, 16);
}

#endif


#if 0

int test_crypto_func ()
{
	unsigned char u[32] = {
		0x20,0xb0,0x03,0xd2,0xf2,0x97,0xbe,0x2c, 0x5e,0x2c,0x83,0xa7,0xe9,0xf9,0xa5,0xb9,
		0xef,0xf4,0x91,0x11,0xac,0xf4,0xfd,0xdb, 0xcc,0x03,0x01,0x48,0x0e,0x35,0x9d,0xe6};
	unsigned char v[32] = {
		0x55,0x18,0x8b,0x3d,0x32,0xf6,0xbb,0x9a, 0x90,0x0a,0xfc,0xfb,0xee,0xd4,0xe7,0x2a,
		0x59,0xcb,0x9a,0xc2,0xf1,0x9d,0x7c,0xfb, 0x6b,0x4f,0xdd,0x49,0xf4,0x7f,0xc5,0xfd};
	unsigned char x[16] = {0xd5,0xcb,0x84,0x54,0xd1,0x77,0x73,0x3e,0xff,0xff,0xb2,0xec,0x71,0x2b,0xae,0xab};
	unsigned char y[16] = {0xa6,0xe8,0xe7,0xcc,0x25,0xa7,0x5f,0x6e,0x21,0x65,0x83,0xf7,0xff,0x3d,0xc4,0xcf};

	unsigned char f_z00[16] = {0xf2,0xc9,0x16,0xf1 ,0x07,0xa9,0xbd,0x1c ,0xf1,0xed,0xa1,0xbe ,0xa9,0x74,0x87,0x2d};
	//unsigned char f_z00[16] = {0xD3,0x01,0xCE,0x92,0xCC,0x7B,0x9E,0x3F,0x51,0xD2,0x92,0x4B,0x8B,0x33,0xFA,0xCA};
	//unsigned char f_z80[16] = {0x7E,0x43,0x11,0x12,0xC1,0x0D,0xE8,0xA3,0x98,0x4C,0x8A,0xC8,0x14,0x9F,0xF6,0xEC};

	unsigned char mac[16], ltk[16];

	//-------
	unsigned char w[32] = {
		0xec,0x02,0x34,0xa3,0x57,0xc8,0xad,0x05,0x34,0x10,0x10,0xa6,0x0a,0x39,0x7d,0x9b,
		0x99,0x79,0x6b,0x13,0xb4,0xf8,0x66,0xf1,0x86,0x8d,0x34,0xf3,0x73,0xbf,0xa6,0x98};
	unsigned char n1[16] = {0xd5,0xcb,0x84,0x54,0xd1,0x77,0x73,0x3e,0xff,0xff,0xb2,0xec,0x71,0x2b,0xae,0xab};
	unsigned char n2[16] = {0xa6,0xe8,0xe7,0xcc,0x25,0xa7,0x5f,0x6e,0x21,0x65,0x83,0xf7,0xff,0x3d,0xc4,0xcf};
	unsigned char keyID = 0x62746c65;
	unsigned char a1[7] = {0x00,0x56,0x12,0x37,0x37,0xbf,0xce};
	unsigned char a2[7] = {0x00,0xa7,0x13,0x70,0x2d,0xcf,0xc1};
	unsigned char f5_ltk[16] = {0x69,0x86,0x79,0x11 ,0x69,0xd7,0xcd,0x23 ,0x98,0x05,0x22,0xb5 ,0x94,0x75,0x0a,0x38};
	//unsigned char f5[16] = {0x47,0x30,0x0b,0xb9,0x5c,0x74,0x04,0x12,0x94,0x50,0x67,0x4b,0x17,0x41,0x10,0x4d};
	unsigned char f5_mac[16] = {0x29,0x65,0xf1,0x76 ,0xa1,0x08,0x4a,0x02 ,0xfd,0x3f,0x6a,0x20 ,0xce,0x63,0x6e,0x20};

	unsigned char r[16] = {0x12,0xa3,0x34,0x3b ,0xb4,0x53,0xbb,0x54 ,0x08,0xda,0x42,0xd2 ,0x0c,0x2d,0x0f,0xc8};
	unsigned char iocap[3] = {0x01, 0x01, 0x02};
	unsigned char f6[16] = {0xe3,0xc4,0x73,0x98 ,0x9c,0xd0,0xe8,0xc5 ,0xd2,0x6c,0x0b,0x09 ,0xda,0x95,0x8f,0x61};

	unsigned char g2[16] = {0x15,0x36,0xd1,0x8d ,0xe3,0xd2,0x0d,0xf9 ,0x9b,0x70,0x44,0xc1 ,0x2f,0x9e,0xd5,0xba};
	unsigned int g2int;

	//---- h6
	unsigned char h6_key[16] = {0xec,0x02,0x34,0xa3 ,0x57,0xc8,0xad,0x05 ,0x34,0x10,0x10,0xa6 ,0x0a,0x39,0x7d,0x9b};
	unsigned char h6_keyid[4] = {0x6c,0x65,0x62,0x72};
	unsigned char h6[16] = {0x2d,0x9a,0xe1,0x02 ,0xe7,0x6d,0xc9,0x1c ,0xe8,0xd3,0xa9,0xe2 ,0x80,0xb1,0x63,0x99};
	int ret;

	blt_smp_alg_f4 (mac, u, v, x, 0);
	ret = memcmp (mac, f_z00, 16);
	if (ret)
		return ret;

	blt_smp_alg_f5 (mac, ltk, w, n1, n2, a1, a2);
	ret = memcmp (mac, f5_mac, 16);
	if (ret)
		return ret;
	ret = memcmp (ltk, f5_ltk, 16);
	if (ret)
		return ret;

	blt_smp_alg_f6 (ltk, mac, n1, n2, r, iocap, a1, a2);
	ret = memcmp (ltk, f6, 16);
	if (ret)
		return ret;

	g2int = blt_smp_alg_g2 (u, v, x, y);
	ret = memcmp (g2 + 12, &g2int, 4);
	if (ret)
		return ret;

	blt_smp_alg_h6 (ltk, h6_key, h6_keyid);
	ret = memcmp (ltk, h6, 16);
	if (ret)
		return ret;

#if (LL_FEATURE_ENABLE_CONNECTIONLESS_ISO)
	unsigned char K[16] = {0xec,0x02,0x34,0xa3,0x57,0xc8,0xad,0x05,0x34,0x10,0x10,0xa6,0x0a,0x39,0x7d,0x9b};
	unsigned char S[16] = {0x15,0x36,0xd1,0x8d,0xe3,0xd2,0x0d,0xf9,0x9b,0x70,0x44,0xc1,0x2f,0x9e,0xd5,0xba};
	unsigned char keyID[4] = {0xcc,0x03,0x01,0x48};
	unsigned char IK[16] = {0xfe,0x77,0xab,0x4e,0xfa,0x98,0x29,0x91,0xc1,0x48,0x6a,0x3b,0x28,0x1f,0xd4,0xbc};
	unsigned char h8[16] = {0xe5,0xe5,0xbe,0xba,0xae,0x72,0x28,0xe7,0x22,0xa3,0x89,0x04,0xed,0x35,0x0f,0x6d};

	/*
	 * Core5.2 Page3185  Group Session Key Derivation Function h8
	 */
	unsigned char tmp_h8[16];
	blt_smp_alg_h8 (tmp_h8, K, S, keyID);
	printf("h8:");array_printf(tmp_h8, 16);

	ret = memcmp(tmp_h8, h8, 16);
	if(ret){
		printf("test h8 func failed\n");
		return ret;
	}

	/* Core5.2 Page3186 Derivation of Group Session Key
	 * IGLTK = h7(???BIG1??ڧ, Broadcast_Code)
	 * GLTK = h6(IGLTK, ???BIG2??ڧ)
	 * GSK = h8 (GLTK, GSKD, ???BIG3??ڧ)
	 */

	/*
	 * Version 5.2 | Vol 6, Part C Page3092 GROUP SESSION KEY DERIVATION FOR BIG
	 */
	unsigned char Broadcast_Code[16] = {0x00,0x00,0x00,0x00,0x65,0x73,0x75,0x6f,0x48,0x20,0x65,0x6e,0x72,0xb8,0xc3,0x42};
	unsigned char GSKD[16] =  {0x55,0x18,0x8b,0x3d,0x32,0xf6,0xbb,0x9a,0x90,0x0a,0xfc,0xfb,0xee,0xd4,0xe7,0x2a};
	unsigned char BIG1[16] =  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x42,0x49,0x47,0x31};
	unsigned char BIG2[4] =   {0x42,0x49,0x47,0x32};
	unsigned char BIG3[4] =   {0x42,0x49,0x47,0x33};
	unsigned char IGLTK[16] = {0x4c,0x0d,0xd7,0x4c,0x2b,0x19,0xaa,0x95,0xd8,0x98,0x23,0x85,0x5f,0x10,0x01,0xb8};
	unsigned char GLTK[16] =  {0xc4,0xcd,0x4b,0x83,0x49,0xb5,0xa1,0x8a,0x02,0xde,0x66,0x20,0x90,0x17,0xae,0xd3};
	unsigned char GSK[16] =   {0xbe,0x2a,0x16,0xfc,0x7a,0xc4,0x64,0xe7,0x52,0x30,0x1b,0xcc,0xc8,0x18,0x81,0x2c};

	unsigned char tmp_igltk[16], tmp_gltk[16], tmp_gsk[16];

	blt_smp_alg_h7 (tmp_igltk, BIG1, Broadcast_Code);

	ret = memcmp(tmp_igltk, IGLTK, 16);
	if(ret){
		printf("Check IGLTK failed\n");
		return ret;
	}

	blt_smp_alg_h6 (tmp_gltk, tmp_igltk, BIG2);
	ret = memcmp(tmp_gltk, GLTK, 16);
	if(ret){
		printf("Check GLTK failed\n");
		return ret;
	}

	blt_smp_alg_h8 (tmp_gsk, tmp_gltk, GSKD, BIG3);
	ret = memcmp(tmp_gsk, GSK, 16);
	if(ret){
		printf("test GSK failed\n");
		return ret;
	}

#endif
}
#endif



