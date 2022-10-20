/********************************************************************************************************
 * @file	hci.c
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
#include "stack/ble/controller/ble_controller.h"



_attribute_data_retention_ u16 lmp_subversion=0;
///////////////////////////////////////////////////////////////////////////////////////////////////////
//	controller hci event call back function
///////////////////////////////////////////////////////////////////////////////////////////////////////
_attribute_data_retention_	hci_event_handler_t		blc_hci_event_handler = 0;

#if (MCU_CORE_TYPE == MCU_CORE_9518)
	hci_fifo_t				bltHci_rxfifo;
	hci_fifo_t			    bltHci_txfifo;
	hci_fifo_t				bltHci_rxAclfifo; /* H2C */


	u8* hci_fifo_wptr (hci_fifo_t *f)
	{
		if (((f->wptr - f->rptr) & 255) < f->num)
		{
			return f->p + (f->wptr & (f->num-1)) * f->size;
		}
		return 0;
	}


	void hci_fifo_pop (hci_fifo_t *f)
	{
		f->rptr++;
	}

	u8 * hci_fifo_get (hci_fifo_t *f)
	{
		if (f->rptr != f->wptr)
		{
			u8 *p = f->p + (f->rptr & (f->num-1)) * f->size;
			return p;
		}
		return 0;
	}
	void hci_fifo_next (hci_fifo_t *f)
	{
		f->wptr++;
	}
	/**
	 * @brief      for user to initialize HCI TX FIFO.
	 * @param[in]  pRxbuf - TX FIFO buffer address.
	 * @param[in]  fifo_size - RX FIFO size
	 * @param[in]  fifo_number - RX FIFO number, can only be 4, 8, 16 or 32
	 * @return     status, 0x00:  succeed
	 * 					   other: failed
	 */
	ble_sts_t blc_ll_initHciTxFifo(u8 *pTxbuf, int fifo_size, int fifo_number)
	{
		/* number must be 2^n */
		if( IS_POWER_OF_2(fifo_number) ){
			bltHci_txfifo.num = fifo_number;
			bltHci_txfifo.mask = fifo_number - 1;
		}
		else{
			return LL_ERR_INVALID_PARAMETER;
		}

		/* size must be 4*n */
		if( (fifo_size & 3) == 0){
			bltHci_txfifo.size = fifo_size;
		}
		else{
			return LL_ERR_INVALID_PARAMETER;
		}

		bltHci_txfifo.wptr = bltHci_txfifo.rptr = 0;
		bltHci_txfifo.p = pTxbuf;

		return BLE_SUCCESS;
	}


	/**
	 * @brief      for user to initialize HCI RX FIFO.
	 * @param[in]  pRxbuf - RX FIFO buffer address.
	 * @param[in]  fifo_size - RX FIFO size
	 * @param[in]  fifo_number - RX FIFO number, can only be 4, 8, 16 or 32
	 * @return     status, 0x00:  succeed
	 * 					   other: failed
	 */
	ble_sts_t blc_ll_initHciRxFifo(u8 *pRxbuf, int fifo_size, int fifo_number)
	{
		/* number must be 2^n */
		if( IS_POWER_OF_2(fifo_number) ){
			bltHci_rxfifo.num = fifo_number;
			bltHci_rxfifo.mask = fifo_number - 1;
		}
		else{
			return LL_ERR_INVALID_PARAMETER;
		}

		/* size must be 16*n */
		if( (fifo_size & 15) == 0){
			bltHci_rxfifo.size = fifo_size;
		}
		else{
			return LL_ERR_INVALID_PARAMETER;
		}

		bltHci_rxfifo.wptr = bltHci_rxfifo.rptr = 0;
		bltHci_rxfifo.p = pRxbuf;

		return BLE_SUCCESS;
	}

#endif

void blc_hci_registerControllerEventHandler (hci_event_handler_t  handler)
{
	blc_hci_event_handler = handler;
}


int blc_hci_send_event (u32 h, u8 *para, int n)
{
	if (blc_hci_event_handler)
	{
		return blc_hci_event_handler (h, para, n);
	}
	return 1;
}


_attribute_data_retention_	u32		hci_eventMask = HCI_EVT_MASK_DEFAULT;

_attribute_data_retention_	u32		hci_le_eventMask = HCI_LE_EVT_MASK_DEFAULT;  //11 event in core_4.2
_attribute_data_retention_	u32		hci_le_eventMask_2 = HCI_LE_EVT_MASK_DEFAULT;


ble_sts_t 	blc_hci_setEventMask_cmd(u32 evtMask)
{
	hci_eventMask = evtMask;
	return BLE_SUCCESS;
}

ble_sts_t 	blc_hci_le_setEventMask_cmd(u32 evtMask)
{
	hci_le_eventMask = evtMask;
	return BLE_SUCCESS;
}

ble_sts_t 	blc_hci_le_setEventMask_2_cmd(u32 evtMask_2)
{
	hci_le_eventMask_2 = evtMask_2;
	return BLE_SUCCESS;
}



/////////////////////////////////
//	send ACL packet to HCI
/////////////////////////////////
int blc_hci_sendACLData2Host (u16 handle, u8 *p)
{
	//start    pkt:  llid 2 -> 0x02
	//continue pkt:  llid 1 -> 0x01

#if (LL_FEATURE_SUPPORT_LE_DATA_LENGTH_EXTENSION)
	int len = p[1]; //rf_len

	int report_len = min(len, bltData.connEffectiveMaxRxOctets);
	p[1] = report_len;
	blc_hci_send_data (HCI_FLAG_ACL_BT_STD | handle, p, report_len);

	for (int i=report_len; i<len; i+=bltData.connEffectiveMaxRxOctets)
	{
		report_len = (len - i) > bltData.connEffectiveMaxRxOctets ? bltData.connEffectiveMaxRxOctets : (len - i);
		p[0] = L2CAP_CONTINUING_PKT; //llid, fregment pkt
		p[1] = report_len;
		memcpy (p + 2, p + 2 + i, report_len);
		blc_hci_send_data (HCI_FLAG_ACL_BT_STD | handle, p, report_len);
	}

#else
	//para[0]&3: llid    para[1]+5: rf_len+5,total data len     para[2] data begin
	blc_hci_send_data (HCI_FLAG_ACL_BT_STD | handle, p, p[1]);
#endif
	return 0;
}


//////////////////////////////////////////////////////////////////////////////////
//	HCI
//		1. from l2cap function: from/to link layer (packet_handeler/push fifo)
//		2. from host: (46 commands)
//				01: command; 02: ACL; 03: synchronous data; 04: event
//               ---- command ---
//				 01 cmd_code_16 length_8 parameter
//               01 06 04 03 01 00 13: disconnect (handle 01 00; reason: 0x13)
//				 01 1d 04 02 01 00   : read remote version info
//				 01 01 0c   ... set event mask
//				 01 03 0c   ... reset hci
//				 01 14 0c   ... read local name
//						03(0c) --> xx(10) ->	20 18 1c 1e 24 13 1a
//						09 02 05 01 03
//				 01 01 10   ... read local version info
//				 01 02 10   ... read local supported command
//				 01 03 10   ... read local supported feature
//				 01 05 10   ... read buffer size
//               01 09 10   ... read BD address
//               01 01 20	... group 08 (0x20)
//					01 set event mask
//					02 read buffer size
//					03 read local supported feature
//					04 set random address
//					05 set advertising parameter
//					06 set advertising channel Tx power
//					07 set advertising data
//					08 set scan response data
//					09 set advertise enable
//					0a set scan parameters
//					0b set scan enable
//					0c create connection
//					0d create connection cancel
//					0e read white list size
//					0f clear white list
//					10 add device to white list
//					11 remove device from white list
//					12 connection update
//					13 set host channel classification
//					14 read channel map
//					15 read remote used feature
//					16 encrypt
//					17 random
//					18 start encryption
//					19 long term key request reply
//					1a long term key request negative
//					1b read supported states
//					1c receiver test
//					1d transmitter test
//					1e test end
//					1f remote connection parameter request reply		(4.1)
//					20 remote connection parameter request negative reply
//					21 set data length									(4.2)
//					22 read suggested default data length
//					23 write suggested default data length
//					24 read local P-256 public key
//					25 generate DHKey
//					26 add device to resolving list
//					27 remove device from resolving list
//					28 clear resolving list
//					29 read resolving list size
//					2a read peer resolvable address
//					2b read local resolvable address
//					2c set address resolution enable
//					2d set resolvable private address timeout
//					2e read maximum data length
//  controller to host event
//               ---- event ---
//				 04 3e 01 statu8 connHandle16 role8...
//				 04 3e 02 ... advertising report
//				 04 3e 03 ...  connection update compete
//				 04 3e 04 ...  read remote used feature complete
//				 04 3e 05 ...  long term key request event
//				 04 3e 06 ...  remote connection parameter request (4.1)
//				 04 3e 07 ...  data length change	(4.2)
//				 04 3e 08 ...  read local P-256 public key complete
//				 04 3e 09 ...  generate DHKey complete
//				 04 3e 0a ...  enhanced connection complete
//				 04 3e 0b ...  direct advertising report
//               04 03 ...    connection complete
//               04 05 ...    disconnection complete
//               04 08 ...    encryption change
//				 04 0c 08 00 01 00 06 11 02 08 00: read remote version complete command
//               04 0e ...    command complete
//               04 0f ...    command status
//				 04 13 ...    number of complete packet
// host/contoller ACL data format
//				 ---- ACL ---
//				 02 handle_16 length_16 l2cap_payload:
//						handle_16( handl_12, packet_boundary_flag_2, broadcast_flag_2)
//						pbf_2: 00 host-to-controller; 2 controller-to-host
///////////////////////////////////////////////////////////////////////////////////
//	vendor command (SPP module)
//  ----  vendor event --------
//	ff 03 01 07  00	command complete
//	ff 03 30 07  04 (stack state change event: 0(reset) 1(standby) 2(prepare advertising)
//                                             3(advertising) 4(connected) 5(terminated)
//                                             6(error) 7(encrypted) 8(bonded)
//  ff n+2 31 07 d1 .. dn	data received
//  ff 03 32 07 00 data sent  0(ok) 1(fail) 2(data too long) 3(wrong data length) 4(no connection)
//                            5(data transmission busy)
//  ff 03 0c 07 n get available buffer number
//////////////////////////////////////////

_attribute_data_retention_	blc_hci_rx_handler_t		blc_hci_rx_handler = 0;
_attribute_data_retention_	blc_hci_tx_handler_t		blc_hci_tx_handler = 0;
blc_hci_app_handler_t		hci_app_smp_cert = 0;

#define 	BTUSB_TYPE		"Telink Controller"
const u8 				blc_controller_name[] = BTUSB_TYPE;

int blc_hci_send_data (u32 h, u8 *para, int n);

void blc_register_hci_handler (void *prx, void *ptx)
{
	blc_hci_rx_handler = prx;
	blc_hci_tx_handler = ptx;
}

void blc_register_hci_app_handler (void* smp_cert_func)
{
	hci_app_smp_cert = smp_cert_func;
}

void blc_set_customer_lmp_subversion(u16 subversion)
{
	lmp_subversion = subversion;
}
/////////////////////////////////
int blc_hci_handler (u8 *p, int n)
{
	u8  status = BLE_SUCCESS;
//	int  cmdLen;
	u8 *cmdPara;

	u32	header = 0;
	u8	para[72] = {0};
#if (BLT_CONN_MASTER_EN)
	u32 *para32 = (u32*)para;
#endif
	//		BLE module command, see in module folder spp
	////////////    ACL_data/L2CAP pay load: p[1] handle; p[2]: flag; p[4:3] length //
	if (p[0] == HCI_TYPE_ACL_DATA)		//(p[2] & 0x30) == 1: continuous data; 0: first data
	{
	#if (BQB_5P0_TEST_ENABLE) // for compatibility of master and slave role
		static u16 h;
		h = p[1] + (p[2] & 15) * 256; //handle
		p[4] = p[3];
		p[3] = (p[2] & 0x30) == 0x10 ? 1 : 2;
		while( !ll_push_tx_fifo_handler (h, p + 3));
	#else
		//Handle  PB_Flag  BC_Flag  *pData
		blc_hci_receiveHostACLData( p[1] + (p[2]&15) * 256, (p[2]&0x30)>>4, (p[2]&0xc0)>>6, p+3);  //TODO: slave/master share API "blc_hci_receiveHostACLData"
	#endif

		para[0] = 1;
		para[1] = p[1];
		para[2] = p[2] & 15;
		para[3] = 1;
		para[4] = 0;
		blc_hci_send_data (HCI_FLAG_EVENT_BT_STD | HCI_EVT_NUM_OF_COMPLETE_PACKETS, para, 5);

		return 1;
	}

	////////////   host command ////////////////////////////////
	else if (p[0] == HCI_TYPE_CMD)
	{
//		cmdLen = p[3];
		cmdPara = p + 4;   //cmdPara is 4 byte aligned
		u8 eventCode;
		u8 resultLen = 0;
		u8 opcode = p[1];

		//link control cmd
		if(p[2] == HCI_CMD_LINK_CTRL_OPCODE_OGF){
			if(opcode == HCI_CMD_INQUIRY)
			{
				status = HCI_ERR_UNKNOWN_HCI_CMD;
				eventCode = HCI_EVT_CMD_STATUS;
				resultLen = 4;
				hci_cmdStatus_evt(1, opcode, HCI_CMD_LINK_CTRL_OPCODE_OGF, status, para);
			}
			else if(opcode == HCI_CMD_DISCONNECT)
			{
			#if(BQB_5P0_TEST_ENABLE)
				if( IS_LL_CONNECTION_VALID(cmdPara[1]<<8 | cmdPara[0]) )
				{
					ble_sts_t  blt_disconnect(u16 connHandle, u8 reason);
					status = (u8)blt_disconnect(cmdPara[1]<<8 | cmdPara[0], cmdPara[2]);
					eventCode = HCI_EVT_CMD_STATUS;
					resultLen = 4;
					hci_cmdStatus_evt(1, opcode, HCI_CMD_LINK_CTRL_OPCODE_OGF, status, para);
				}
				else
				{
					status = (u8)blm_ll_disconnect (cmdPara[1]<<8 | cmdPara[0], cmdPara[2]);
					eventCode = HCI_EVT_CMD_STATUS;
					resultLen = 4;
					*para32 = HCI_EVT_CMDSTATUS(1, opcode, HCI_CMD_LINK_CTRL_OPCODE_OGF, status);
					hci_cmdStatus_evt(1, opcode, HCI_CMD_LINK_CTRL_OPCODE_OGF, status, para);
				}
			#else
					ble_sts_t  blt_disconnect(u16 connHandle, u8 reason);
					status = (u8)blt_disconnect(cmdPara[1]<<8 | cmdPara[0], cmdPara[2]);
					eventCode = HCI_EVT_CMD_STATUS;
					resultLen = 4;
					hci_cmdStatus_evt(1, opcode, HCI_CMD_LINK_CTRL_OPCODE_OGF, status, para);
			#endif
			}
			else if(opcode == HCI_CMD_READ_REMOTE_NAME_REQ)
			{
//				// send event remote name request complete event
//				hci_remoteNateReqComplete_evt (p + 4);
				// send command status first, and send remote name request complete event.
				status = HCI_ERR_UNSUPPORTED_REMOTE_FEATURE;//BLE_SUCCESS;
				eventCode = HCI_EVT_CMD_STATUS;
				resultLen = 4;
				hci_cmdStatus_evt(1, opcode, HCI_CMD_LINK_CTRL_OPCODE_OGF, status, para);
			}
			//	1d read remote version
			else if(opcode == HCI_CMD_READ_REMOTE_VER_INFO)
			{
				//TODO: add code here, refer to "bls_ll_readRemoteVersion" by zhiTao,
				//and final use slave/master shared API "blc_ll_readRemoteVersion"
			}
		}
		else if(p[2] == HCI_CMD_CBC_OPCODE_OGF){

			switch(opcode) {
				//	01 set event mask, classic
				case HCI_CMD_SET_EVENT_MASK:
				{
					blc_hci_setEventMask_cmd(cmdPara[0] | cmdPara[1]<<8 | cmdPara[2]<<16 | cmdPara[3]<<24);
					eventCode = HCI_EVT_CMD_COMPLETE;
					resultLen = hci_cmdComplete_evt(1, opcode, HCI_CMD_CBC_OPCODE_OGF, 1, &status, para);
				}
				break;

				//	03 reset HCI
				case HCI_CMD_RESET:
				{
					status = (u8)blc_hci_reset();
					#if (MCU_CORE_TYPE == MCU_CORE_9518)
						blt_InitPhyTestDriver(RF_MODE_BLE_1M);
					#elif (MCU_CORE_TYPE == MCU_CORE_825x || MCU_CORE_TYPE == MCU_CORE_827x)
						phy_test_driver_init(RF_MODE_BLE_1M);
					#endif
					eventCode = HCI_EVT_CMD_COMPLETE;
					resultLen = hci_cmdComplete_evt(1, opcode, HCI_CMD_CBC_OPCODE_OGF, 1, &status, para);
				}
				break;

				case HCI_CMD_DELETE_STORED_LINK_KEY :
				{

				}
				break;



				case HCI_CMD_SET_EVENT_FILTER:
				case HCI_CMD_WRITE_PIN_TYPE:
				case HCI_CMD_CREATE_NEW_UINT_KEY:
				case HCI_CMD_WRITE_CONNECTION_ACCEPT_TIMEOUT:
				case HCI_CMD_WRITE_PAGE_TIMEOUT:
				case HCI_CMD_WRITE_SCAN_ENABLE:
				case HCI_CMD_WRITE_PAGE_SCAN_ACTIVITY:
				case HCI_CMD_WRITE_INQUIRY_SCAN_ACTIVITY:
				case HCI_CMD_WRITE_AUTHENTICATION_ENABLE:
				case HCI_CMD_WRITE_CLASS_OF_DEVICE :
				case HCI_CMD_WRITE_VOICE_SETTING :
				case HCI_CMD_WRITE_NUM_BROADCAST_RETRANSMISSIONS:
				case HCI_CMD_WRITE_HOLD_MODE_ACTIVITY:
				case HCI_CMD_SYNCHRONOUS_FLOW_CONTROL_ENABLE:
				case HCI_CMD_SET_CONTROLLER_TO_HOST_FLOW_CTRL:
				case HCI_CMD_HOST_BUF_SIZE :
				case HCI_CMD_WRITE_CURRENT_IAC_LAP:
				case HCI_CMD_WRITE_INQUIRY_SCAN_TYPE:
				case HCI_CMD_WRITE_INQUIRY_MODE:
				case HCI_CMD_WRITE_PAGE_SCAN_TYPE:
				{
					status = BLE_SUCCESS;
					eventCode = HCI_EVT_CMD_COMPLETE;
					resultLen = hci_cmdComplete_evt(1, opcode, HCI_CMD_CBC_OPCODE_OGF, 1, &status, para);
				}
				break;


				default: break;
			}

		}
		//Informational Parameters
		else if(p[2] == HCI_CMD_IP_OPCODE_OGF){
			switch(opcode) {
				case HCI_CMD_READ_LOCAL_VER_INFO:
				{
					const u8 tbl[9] = {
							0x00,    		//status
							BLUETOOTH_VER_5_0,  			//HCI Version,Bluetooth Core Specification 5.0
							0xbb, 0x22, 	//HCI Revision
							BLUETOOTH_VER_5_0,			//LMP/PAL Version, Bluetooth Core Specification 5.0
							//NOTE: some special case vendor_id should be changed,e.g. some customer buy our IC but declared their own IC, we need change vendor_id for them
							VENDOR_ID_LO_B, VENDOR_ID_HI_B,    //Manufacturer_Name,
							lmp_subversion&0xff, (lmp_subversion>>8)&0xff,		//LMP/PAL_Subversion implementation dependent
									  };

					resultLen = hci_cmdComplete_evt(1, opcode, HCI_CMD_IP_OPCODE_OGF, 9, (u8 *)tbl, para);
					eventCode = HCI_EVT_CMD_COMPLETE;
				}
				break;

				case HCI_CMD_READ_LOCAL_SUPPORTED_CMDS:
				{
					//##$$
					const u8 tbl[37 + 28] = {0x00,   //status
						0x00, // 0
						0x00, // 1
						0x20, // 2	<5>: Read Remote Supported Features
						0x00, // 3
						0x00, // 4
						0xC0, // 5  <7>:Reset  <6>:set event mask
						0x00, // 6
						0x00, // 7

						0x00, // 8
						0x00, // 9
						0x80, //10  Host Number Of Completed Packets
						0x00, //11
						0x00, //12
						0x00, //13
						0x08, //14  Read Local Version Information
						0x02, //15  Read BD ADDR

						0x00, //16
						0x00, //17
						0x00, //18
						0x00, //19
						0x00, //20
						0x00, //21
						0x00, //22
						0x00, //23

						0x00, //24
						0xf7, //25  LE Set Event Mask/LE Read Buffer Size/LE Read Local Supported Features
							  //	LE Set Random Address/LE Set Advertising Parameters/LE Read Advertising Channel TX Power/LE Set Advertising Data
						0xff, //26  LE Set Scan Response Data/LE Set Advertise Enable/LE Set Scan Parameters/LE Set Scan Enable
							  //    LE Create Connection/LE Create Connection Cancel/LE Read White List Size/LE Clear White List
						0xff, //27  LE Add Device To White List/LE Remove Device From White List/LE Connection Update/LE Set Host Channel Classification
							  //    LE Read Channel Map/LE Read Remote Used Features/LE Encrypt/LE Rand
						0x7f, //28  LE Test End/LE Transmitter Test/LE Receiver Test
							  //    LE Start Encryption/LE Long Term Key Request Reply/LE Long Term Key Request Negative Reply/LE Read Supported States

						0x00, //29
						0x00, //30
						0x00, //31

						0x00, //32

						//		33
						// 0 Read Extended Page Timeout
						// 1 Write Extended Page Timeout
						// 2 Read Extended Inquiry Length
						// 3 Write Extended Inquiry Length
						// 4 LE Remote Connection Parameter Request Reply
						// 5 LE Remote Connection Parameter Request Negative Reply
						// 6 LE Set Data Length
						// 7 LE Read Suggested Default Data Length
						LL_FEATURE_ENABLE_LE_DATA_LENGTH_EXTENSION<<7 | LL_FEATURE_ENABLE_LE_DATA_LENGTH_EXTENSION<<6,

						//		34
						// 0 LE Write Suggested Default Data Length
						// 1 LE Read Local P-256 Public Key
						// 2 LE Generate DH Key
						// 3 LE Add Device To Resolving List
						// 4 LE Remove Device From Resolving List
						// 5 LE Clear Resolving List
						// 6 LE Read Resolving List Size
						// 7 LE Read Peer Resolvable Address
						0xFE | LL_FEATURE_ENABLE_LE_DATA_LENGTH_EXTENSION<<0,

						//	   35
						// 0 LE Read Local Resolvable Address
						// 1 LE Set Address Resolution Enable
						// 2 LE Set Resolvable Private Address Timeout
						// 3 LE Read Maximum Data Length
						// 4 LE Read PHY Command
						// 5 LE Set Default PHY Command
						// 6 LE Set PHY Command
						// 7 LE Enhanced Receiver Test Command
						0x03 | (LL_FEATURE_ENABLE_LE_2M_PHY | LL_FEATURE_ENABLE_LE_CODED_PHY) <<6 | \
							   (LL_FEATURE_ENABLE_LE_2M_PHY | LL_FEATURE_ENABLE_LE_CODED_PHY) <<5 | \
							   (LL_FEATURE_ENABLE_LE_2M_PHY | LL_FEATURE_ENABLE_LE_CODED_PHY) <<4 | \
							    LL_FEATURE_ENABLE_LE_DATA_LENGTH_EXTENSION 					  <<3,


					    //     36
						//	   0 LE Enhanced Transmitter Test Command
						//	   1 LE Set Advertising Set Random Address Command
						//	   2 LE Set Extended Advertising Parameters Command
						//	   3 LE Set Extended Advertising Data Command
						//	   4 LE Set Extended Scan Response Data Command
						//	   5 LE Set Extended Advertising Enable Command
						//	   6 LE Read Maximum Advertising Data Length Command
						//	   7 LE Read Number of Supported Advertising Sets Command
					    LL_FEATURE_ENABLE_LE_EXTENDED_ADVERTISING ? 0xFE: 0x00,


					    //     37
						// 0 LE Remove Advertising Set Command
						// 1 LE Clear Advertising Sets Command
					    // 2 LE Set Periodic Advertising Parameters Command
					    // 3 LE Set Periodic Advertising Data Command
					    // 4 LE Set Periodic Advertising Enable Command
					    // 5 LE Set Extended Scan Parameters Command
					    // 6 LE Set Extended Scan Enable Command
					    // 7 LE Extended Create Connection Command
					    LL_FEATURE_ENABLE_LE_EXTENDED_ADVERTISING<<1 | 	LL_FEATURE_ENABLE_LE_EXTENDED_ADVERTISING<<0,


					    //     38
					    // 0 LE Periodic Advertising Create Sync Command
					    // 1 LE Periodic Advertising Create Sync Cancel Command
					    // 2 LE Periodic Advertising Terminate Sync Command
					    // 3 LE Add Device To Periodic Advertiser List Command
					    // 4 LE Remove Device From Periodic Advertiser List Command
					    // 5 LE Clear Periodic Advertiser List Command
					    // 6 LE Read Periodic Advertiser List Size Command
					    // 7 LE Read Transmit Power Command
						0x00, //38

						//	 39
						// 0 LE Read RF Path Compensation Command
						// 1 LE Write RF Path Compensation Command
						// 2 LE Set Privacy Mode
						// 3 Reserved for Future Use
						// 4 Reserved for Future Use
						// 5 Reserved for Future Use
						// 6 Reserved for Future Use
						// 7 Reserved for Future Use
						0x00, //39

						0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
						0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
						0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
						};

					resultLen = hci_cmdComplete_evt(1, opcode, HCI_CMD_IP_OPCODE_OGF, 37 + 28, (u8 *)tbl, para);
					eventCode = HCI_EVT_CMD_COMPLETE;
				}
				break;

				case HCI_CMD_READ_LOCAL_SUPPORTED_FEATURES:
				{
					const u8 tbl[9] = {0x00, 0x00,0x00,0x00,0x00, 0x60,0x00,0x00,0x00};
					//const u8 tbl[9] = {0x00, 0xff,0xff,0x8f,0xfe, 0xdb,0xff,0x5b,0x87};
					resultLen = hci_cmdComplete_evt(1, opcode, HCI_CMD_IP_OPCODE_OGF, 9, (u8 *)tbl, para);
					eventCode = HCI_EVT_CMD_COMPLETE;
				}
				break;

				case HCI_CMD_READ_EXTENDED_LOCAL_SUPPORTED_FEATURES:
				{
					u8 page[11] = {0x00, 0x00, 0x01, 0x0b,0x00,0x00,0x00, 0x00,0x00,0x00,0x00};
					if(cmdPara[0] == 0)
						page[1] = 0;
					else if(cmdPara[0] == 1)
						page[1] = 1;
					resultLen = hci_cmdComplete_evt(1, opcode, HCI_CMD_IP_OPCODE_OGF, 11, page, para);
					eventCode = HCI_EVT_CMD_COMPLETE;
				}
				break;

				case HCI_CMD_READ_BUFFER_SIZE_COMMAND:		//not support in LE only
				{
					//const u8 tbl[7] = {0x1b,0x00, 0x40, 0x04,0x00, 0x01,0x00};
					const u8 tbl[8] = {0x00, 0x36,0x01, 0x40, 0x0a,0x00, 0x08,0x00};
					resultLen = hci_cmdComplete_evt(1, opcode, HCI_CMD_IP_OPCODE_OGF, 8, (u8 *)tbl, para);
					eventCode = HCI_EVT_CMD_COMPLETE;
				}
				break;

				case HCI_CMD_READ_BD_ADDR:
				{
					u8 returnPara[8];
					returnPara[0] = blc_ll_readBDAddr(&(returnPara[1]));
					resultLen = hci_cmdComplete_evt(1, opcode, HCI_CMD_IP_OPCODE_OGF, 7, returnPara, para);
					eventCode = HCI_EVT_CMD_COMPLETE;
				}
				break;

				default:
					break;
			}

		}
		// LE command
		else if (p[2] == HCI_CMD_LE_OPCODE_OGF)  //OGF = 0x08 = 0b001000,  <<2 = 0x20
		{

			switch(opcode) {
				//	01 set event mask
				case HCI_CMD_LE_SET_EVENT_MASK:
				{
					blc_hci_le_setEventMask_cmd(cmdPara[0] | cmdPara[1]<<8 | cmdPara[2]<<16 | cmdPara[3]<<24);
					blc_hci_le_setEventMask_2_cmd(cmdPara[4] | cmdPara[5]<<8 | cmdPara[6]<<16 | cmdPara[7]<<24);
					eventCode = HCI_EVT_CMD_COMPLETE;
					resultLen = hci_cmdComplete_evt(1, opcode, HCI_CMD_LE_OPCODE_OGF, 1, &status, para);
				}
				break;

				//	02 read buffer size
				case HCI_CMD_LE_READ_BUF_SIZE:    //ACL DATA
				{
					u8 returnPara[4];
					returnPara[0] = (u8)blc_hci_le_readBufferSize_cmd( (u8 *)(returnPara+1) );
					eventCode = HCI_EVT_CMD_COMPLETE;
					resultLen = hci_cmdComplete_evt(1, opcode, HCI_CMD_LE_OPCODE_OGF, 4, returnPara, para);
				}
				break;

				//	03 read local supported feature
				case HCI_CMD_LE_READ_LOCAL_SUPPORTED_FEATURES:
				{
					u8 returnPara[9];
					returnPara[0] = (u8)blc_hci_le_getLocalSupportedFeatures(&(returnPara[1]));
					eventCode = HCI_EVT_CMD_COMPLETE;
					resultLen = hci_cmdComplete_evt(1, opcode, HCI_CMD_LE_OPCODE_OGF, 9, returnPara, para);
				}
				break;

				//	05 set random address
				case HCI_CMD_LE_SET_RANDOM_ADDR:
				{
					status = (u8)blc_ll_setRandomAddr(cmdPara);
					eventCode = HCI_EVT_CMD_COMPLETE;
					resultLen = hci_cmdComplete_evt(1, opcode, HCI_CMD_LE_OPCODE_OGF, 1, &status, para);
				}
				break;

				//	06 set advertising parameter
				case HCI_CMD_LE_SET_ADVERTISE_PARAMETERS:
				{
					status = (u8)bls_hci_le_setAdvParam( (adv_para_t *)cmdPara );
					eventCode = HCI_EVT_CMD_COMPLETE;
					resultLen = hci_cmdComplete_evt(1, opcode, HCI_CMD_LE_OPCODE_OGF, 1, &status, para);
				}

				break;

				//	07 set advertising channel Tx power
				case HCI_CMD_LE_READ_ADVERTISING_CHANNEL_TX_POWER:
				{
					u8 returnPara[2];
					ble_sts_t status = BLE_SUCCESS;

					if(bltParam.adv_hci_cmd & ADV_EXTENDED_MASK){
						status = HCI_ERR_CMD_DISALLOWED;
					}
					else
					{
						bltParam.adv_hci_cmd |= ADV_LEGACY_MASK;
					}

					returnPara[0] = status;
					returnPara[1] = 0; //@@@ (u8)rf_get_tx_power_level();
					eventCode = HCI_EVT_CMD_COMPLETE;
					resultLen = hci_cmdComplete_evt(1, opcode, HCI_CMD_LE_OPCODE_OGF, 2, returnPara, para);
				}
				break;

				//	08 set advertising data
				case HCI_CMD_LE_SET_ADVERTISE_DATA:
				{
					status = (u8)bls_ll_setAdvData(cmdPara+1, cmdPara[0]);
					eventCode = HCI_EVT_CMD_COMPLETE;
					resultLen = hci_cmdComplete_evt(1, opcode, HCI_CMD_LE_OPCODE_OGF, 1, &status, para);
				}
				break;

				//	09 set scan response data
				case HCI_CMD_LE_SET_SCAN_RSP_DATA:
				{
					status = (u8)bls_ll_setScanRspData(cmdPara+1, cmdPara[0]);
					eventCode = HCI_EVT_CMD_COMPLETE;
					resultLen = hci_cmdComplete_evt(1, opcode, HCI_CMD_LE_OPCODE_OGF, 1, &status, para);
				}
				break;

				//	0a set advertise enable
				case HCI_CMD_LE_SET_ADVERTISE_ENABLE:
				{
					status = (u8)bls_ll_setAdvEnable(cmdPara[0]);
				#if (BQB_5P0_TEST_ENABLE)// for compatibility of master and slave role
					if(status==BLE_SUCCESS)
					{
						blc_ll_initSlaveRole_module();				//slave module: 	 mandatory for BLE slave,
						blc_l2cap_register_handler (blc_hci_sendACLData2Host);  	//l2cap initialization
					}
				#endif
					eventCode = HCI_EVT_CMD_COMPLETE;
					resultLen = hci_cmdComplete_evt(1, opcode, HCI_CMD_LE_OPCODE_OGF, 1, &status, para);
				}
				break;

				//	0b set scan parameters 			@@ MASTER
				case HCI_CMD_LE_SET_SCAN_PARAMETERS:
				{
					status = (u8)blc_ll_setScanParameter(cmdPara[0], cmdPara[1] | cmdPara[2]<<8, \
														 cmdPara[3] | cmdPara[4]<<8, cmdPara[5], cmdPara[6]);
					eventCode = HCI_EVT_CMD_COMPLETE;
					resultLen = hci_cmdComplete_evt(1, opcode, HCI_CMD_LE_OPCODE_OGF, 1, &status, para);
				}
				break;

				//	0c set scan enable				@@ MASTER
				case HCI_CMD_LE_SET_SCAN_ENABLE:
				{

					status = (u8)blc_ll_setScanEnable (cmdPara[0], cmdPara[1]);
				#if (BQB_5P0_TEST_ENABLE)
					if(status==BLE_SUCCESS)
					{
						u8 mac_public[6];
						flash_read_page(CFG_ADR_MAC, 6, mac_public);
						blc_ll_initScanning_module(mac_public); 	//scan module: 		 mandatory for BLE master,
					}
				#endif
					eventCode = HCI_EVT_CMD_COMPLETE;
					resultLen = hci_cmdComplete_evt(1, opcode, HCI_CMD_LE_OPCODE_OGF, 1, &status, para);
				}
				break;

#if (BLT_CONN_MASTER_EN)
				//	0d create connection			@@ MASTER
				case HCI_CMD_LE_CREATE_CONNECTION:
				{
					status = blc_ll_createConnection (
							cmdPara[0] + cmdPara[1]*256,		//scan interval
							cmdPara[2] + cmdPara[3]*256,		//scan window
							cmdPara[4],							//police
							cmdPara[5],							//address type
							cmdPara + 6, 						//mac
							cmdPara[12],						//own address type
							cmdPara[13] + cmdPara[14]*256,		//conn min
							cmdPara[15] + cmdPara[16]*256,		//conn max
							cmdPara[17] + cmdPara[18]*256,		//conn latency
							cmdPara[19] + cmdPara[20]*256,		//timeout
							cmdPara[21] + cmdPara[22]*256,		//ce min
							cmdPara[23] + cmdPara[24]*256		//ce max
							);
					#if (BQB_5P0_TEST_ENABLE)// for compatibility of master and slave role
						if(status==BLE_SUCCESS)
						{
							u8 mac_public[6];

							flash_read_page(CFG_ADR_MAC, 6, mac_public);
							blc_ll_initInitiating_module();
							blc_ll_initScanning_module(mac_public); 	//scan module: 		 mandatory for BLE master,
							blc_ll_initMasterRoleSingleConn_module ();
							blc_l2cap_register_handler (blc_hci_sendACLData2Host);  	//l2cap initialization
						}
					#endif

					eventCode = HCI_EVT_CMD_STATUS;
					resultLen = 4;
					hci_cmdStatus_evt(1, opcode, HCI_CMD_LE_OPCODE_OGF, status, para);

				}
				break;

				//	0e create connection cancel		@@ MASTER
				case HCI_CMD_LE_CREATE_CONNECTION_CANCEL:
				{
					status = blc_ll_createConnectionCancel ();
					eventCode = HCI_EVT_CMD_COMPLETE;
					resultLen = 4;
					hci_cmdStatus_evt(1, opcode, HCI_CMD_LE_OPCODE_OGF, status, para);
				}
				break;
#endif

				//	0f read white list size
				case HCI_CMD_LE_READ_WHITE_LIST_SIZE:
				{
					u8 returnPara[2];
					returnPara[0] = (u8)ll_whiteList_getSize(&returnPara[1]);
					eventCode = HCI_EVT_CMD_COMPLETE;
					resultLen = hci_cmdComplete_evt(1, opcode, HCI_CMD_LE_OPCODE_OGF, 2, returnPara, para);
				}
				break;

				//	10 clear white list
				case HCI_CMD_LE_CLEAR_WHITE_LIST:
				{
					status = (u8)ll_whiteList_reset();
					eventCode = HCI_EVT_CMD_COMPLETE;
					resultLen = hci_cmdComplete_evt(1, opcode, HCI_CMD_LE_OPCODE_OGF, 1, &status, para);
				}
				break;

				//	11 add device to white list
				case HCI_CMD_LE_ADD_DEVICE_TO_WHITE_LIST:
				{
					//u8 addrType = cmdPara[0];
					//u8 *addr = cmdPara + 1;
					status = (u8)ll_whiteList_add(cmdPara[0], cmdPara + 1);
					eventCode = HCI_EVT_CMD_COMPLETE;
					resultLen = hci_cmdComplete_evt(1, opcode, HCI_CMD_LE_OPCODE_OGF, 1, &status, para);
				}
				break;

				//	12 remove device from white list
				case HCI_CMD_LE_REMOVE_DEVICE_FROM_WL:
				{
//					status = (u8)ll_whiteList_delete(cmdPara[0], cmdPara + 1);
					ll_whiteList_delete(cmdPara[0], cmdPara + 1);
					status = BLE_SUCCESS;
					eventCode = HCI_EVT_CMD_COMPLETE;
					resultLen = hci_cmdComplete_evt(1, opcode, HCI_CMD_LE_OPCODE_OGF, 1, &status, para);
				}
				break;
#if (BLT_CONN_MASTER_EN)
				//	13 connection update
				//  TODO: add "blm_ll_updateConnection" here
				case HCI_CMD_LE_CONNECTION_UPDATE:
				{
					u16 connHandle = (cmdPara[0] | cmdPara[1] <<8);
					status = (u8) blm_ll_updateConnection (connHandle,
							cmdPara[2] + cmdPara[3]*256,		//conn min
							cmdPara[4] + cmdPara[5]*256,		//conn max
							cmdPara[6] + cmdPara[7]*256,		//conn latency
							cmdPara[8] + cmdPara[9]*256,		//timeout
							cmdPara[10] + cmdPara[11]*256,		//ce min
							cmdPara[12] + cmdPara[13]*256		//ce max
							);
					eventCode = HCI_EVT_CMD_STATUS;
					resultLen = 4;
					*para32 = HCI_EVT_CMDSTATUS(1, opcode, HCI_CMD_LE_OPCODE_OGF, status);
				}
				break;

				//	14 set host channel classification
				//  TODO: add "blm_ll_setHostChannel" here
				case HCI_CMD_LE_SET_HOST_CHANNEL_CLASSIFICATION:
				{
					u16 connHandle = (cmdPara[0] | cmdPara[1] <<8);

					status = (u8) blm_ll_setHostChannel (connHandle, cmdPara + 2);
					eventCode = HCI_EVT_CMD_COMPLETE;
					resultLen = 4;
					*para32 = HCI_EVT_CMD_COMPLETE_STATUS(1, opcode, HCI_CMD_LE_OPCODE_OGF, status);
				}
				break;
#endif

				//	15 read channel map
				case HCI_CMD_LE_READ_CHANNEL_MAP:
				{
					//u16 connHandle = cmdPara[0] | cmdPara[1]<<8;
					u8 returnPara[8];
					returnPara[1] = cmdPara[0];
					returnPara[2] = cmdPara[1];
					returnPara[0] = (u8)bls_hci_le_readChannelMap(cmdPara[0] | cmdPara[1]<<8, returnPara + 3);
					eventCode = HCI_EVT_CMD_COMPLETE;
					resultLen = hci_cmdComplete_evt(1, opcode, HCI_CMD_LE_OPCODE_OGF, 8, returnPara, para);
				}
				break;

				//	16 read remote used feature
				case HCI_CMD_LE_READ_REMOTE_USED_FEATURES:
				{
					u16 connHandle = (cmdPara[0] | cmdPara[1] <<8);

				#if(BQB_5P0_TEST_ENABLE)
					if( IS_LL_CONNECTION_VALID(connHandle) )
					{
						status = (u8)bls_hci_le_getRemoteSupportedFeatures(connHandle);
					}
					else
					{
						status = (u8) blm_ll_readRemoteFeature (connHandle);
					}
				#else
					status = (u8)bls_hci_le_getRemoteSupportedFeatures(connHandle);
				#endif
					eventCode = HCI_EVT_CMD_STATUS;
					resultLen = 4;
					hci_cmdStatus_evt(1, opcode, HCI_CMD_LE_OPCODE_OGF, status, para);

				}
				break;


				//	17 encrypt
				case HCI_CMD_LE_ENCRYPT:
				{
					///// return param: param_total_len : 17 byte
					/////				param0: status   		 len : 1 byte
					///// 				param1: encrypteTextData len : 16 byte
					///// event code : HCI_EVT_CMD_COMPLETE
					u8* hciCmdParam = (u8*)cmdPara;

					u8 *key = (u8*)hciCmdParam;
					u8 *plaintextData = (u8*)(hciCmdParam + 16);

					u8 returnPara[17] = {0};

					//pointer encrypteTextData must be 4 byte aligned, or there will be ERR
					u8 encrypteTextData[16];
					extern int blc_ll_encrypted_data(u8*key, u8*plaintextData, u8* encrypteTextData);
					status = blc_ll_encrypted_data(key, plaintextData, (u8*)encrypteTextData);

					returnPara[0] = status;
					memcpy(returnPara+1, encrypteTextData, 16);

					eventCode = HCI_EVT_CMD_COMPLETE;
					resultLen = hci_cmdComplete_evt(1, opcode, HCI_CMD_LE_OPCODE_OGF, 17, returnPara, para);
				}
				break;

				//	18 random
				case HCI_CMD_LE_RANDOM:
				{
					///// return param: param_total_len : 9 byte
					/////				param0: status   		 len : 1 byte
					///// 				param1: random_number	 len : 8 byte
					///// event code : HCI_EVT_CMD_COMPLETE
					u8 returnPara[9] = {0};
					u8* randomNumeber = returnPara + 1;

					status = blc_ll_getRandomNumber(randomNumeber);
					returnPara[0] = status;

					eventCode = HCI_EVT_CMD_COMPLETE;
					resultLen = hci_cmdComplete_evt(1, opcode, HCI_CMD_LE_OPCODE_OGF, 9, returnPara, para);
				}
					break;

				//	19 start encryption @@ master
				case HCI_CMD_LE_START_ENCRYPTION:
				{
				#if (BQB_5P0_TEST_ENABLE)
					u16 connhandle = p[4] + p[5]*256;
					u16 ediv_num;
					ediv_num =  p[14] + p[15]*256;
					///// return param: param_total_len : 0
					status = blm_ll_startEncryption (connhandle, ediv_num, p+6 , p + 16 );
					eventCode = HCI_EVT_CMD_STATUS;
					resultLen = 4;
					*para32 = HCI_EVT_CMDSTATUS (1, opcode, HCI_CMD_LE_OPCODE_OGF, status);
				#else

//					if(master_encryption_state == 1){
					if(1){  // encrypting
						eventCode = HCI_EVT_ENCRYPTION_KEY_REFRESH;
					}else{
						eventCode = HCI_EVT_CHANGE_LINK_KEY_COMPLETE;
					}
					// event1: key refresh complete event event code = 0x30
					// event2: encryption change event   event code = 0x09
				#endif
				}
					break;

				//	1a long term key request reply
				case HCI_CMD_LE_LONG_TERM_KEY_REQUESTED_REPLY:
				{
					///// return param: param_total_len : 3 byte
					/////				param0: status   		  len : 1 byte
					///// 				param1: connection handle len : 2 byte
					///// event code : HCI_EVT_CMD_COMPLETE
					u16 connectHandle = cmdPara[0] | cmdPara[1]<<8;
					u8* specifiesLtk = (cmdPara + 2);

					u8 returnPara[3] = {0};

//					status = blt_setLtkVsConnHandle(connectHandle, specifiesLtk);
					status = blc_hci_ltkRequestReply(connectHandle, specifiesLtk);
					returnPara[0] = status;
					returnPara[1] = connectHandle;
					returnPara[2] = connectHandle >> 8;

					eventCode = HCI_EVT_CMD_COMPLETE;
					resultLen = hci_cmdComplete_evt(1, opcode, HCI_CMD_LE_OPCODE_OGF, 3, returnPara, para);
				}
					break;

				//	1b long term key request negative
				case HCI_CMD_LE_LONG_TERM_KEY_REQUESTED_NEGATIVE_REPLY:
				{
					///// return param: param_total_len : 3 byte
					/////				param0: status   		  len : 1 byte
					///// 				param1: connection handle len : 2 byte
					///// event code : HCI_EVT_CMD_COMPLETE
					u16 connectHandle = cmdPara[0] | cmdPara[1]<<8;

					u8 returnPara[3] = {0};

//					status = blt_getLtkVsConnHandleFail(connectHandle);
					status = blc_hci_ltkRequestNegativeReply(connectHandle);
					returnPara[0] = status;
					returnPara[1] = connectHandle;
					returnPara[2] = connectHandle >> 8;

					eventCode = HCI_EVT_CMD_COMPLETE;
					resultLen = hci_cmdComplete_evt(1, opcode, HCI_CMD_LE_OPCODE_OGF, 3, returnPara, para);

				}
					break;
				//	1c read supported states
				case HCI_CMD_LE_READ_SUPPORTED_STATES:
				{
					const u8 le_states[12] = {0, 0x8f, 0x00, 0x30, 0x20, 0x00, 0x01, 0x00, 0x00};//slave  TODO: merge slave/master state together
					resultLen = hci_cmdComplete_evt(1, opcode, HCI_CMD_LE_OPCODE_OGF, 9, (u8*)le_states, para);
					eventCode = HCI_EVT_CMD_COMPLETE;
				}
					break;
				//	1d receiver test
				case HCI_CMD_LE_RECEIVER_TEST:
				{
				#if (MCU_CORE_TYPE == MCU_CORE_9518)
					status = blt_phyTest_setReceiverTest_V1(cmdPara[0]);
				#elif (MCU_CORE_TYPE == MCU_CORE_825x || MCU_CORE_TYPE == MCU_CORE_827x)
					status = blc_phy_setReceiverTest(cmdPara[0]);
				#endif
					eventCode = HCI_EVT_CMD_COMPLETE;
					resultLen = hci_cmdComplete_evt(1, opcode, HCI_CMD_LE_OPCODE_OGF, 1, &status, para);
				}
					break;
				//	1e transmitter test
				case HCI_CMD_LE_TRANSMITTER_TEST:
				{
				#if (MCU_CORE_TYPE == MCU_CORE_9518)
					status = blt_phyTest_hci_setTransmitterTest_V1(cmdPara[0], cmdPara[1], cmdPara[2]);
				#elif (MCU_CORE_TYPE == MCU_CORE_825x || MCU_CORE_TYPE == MCU_CORE_827x)
					status = blc_phy_setTransmitterTest(cmdPara[0], cmdPara[1], cmdPara[2]);
				#endif
					eventCode = HCI_EVT_CMD_COMPLETE;
					resultLen = hci_cmdComplete_evt(1, opcode, HCI_CMD_LE_OPCODE_OGF, 1, &status, para);
				}

					break;
				//	1f test end
				case HCI_CMD_LE_TEST_END:
				{
					u8 returnPara[3] = {0};
				#if (MCU_CORE_TYPE == MCU_CORE_9518)
					returnPara[0] = blt_phyTest_setTestEnd(returnPara+1);
				#elif (MCU_CORE_TYPE == MCU_CORE_825x || MCU_CORE_TYPE == MCU_CORE_827x)
					returnPara[0] = blc_phy_setPhyTestEnd(returnPara+1);
				#endif
					eventCode = HCI_EVT_CMD_COMPLETE;
					resultLen = hci_cmdComplete_evt(1, opcode, HCI_CMD_LE_OPCODE_OGF, 3, returnPara, para);
				}
					break;

				//	20 remote connection parameter request reply		(4.1)
				case HCI_CMD_LE_REMOTE_CONNECTION_PARAM_REQ_REPLY:
					break;
				//	21 remote connection parameter request negative reply
				case HCI_CMD_LE_REMOTE_CONNECTION_PARAM_REQ_NEGATIVE_REPLY:
					break;
				//	21 set data length									(4.2)
				case HCI_CMD_LE_SET_DATA_LENGTH:
				{
					u8 returnPara[12] = {0};
					returnPara[0] = blc_hci_setTxDataLength(cmdPara[0] + cmdPara[1]*256, cmdPara[2] + cmdPara[3]*256, cmdPara[4] + cmdPara[5]*256);
					returnPara[1] = cmdPara[0];
					returnPara[2] = cmdPara[1];
					eventCode = HCI_EVT_CMD_COMPLETE;
					resultLen = hci_cmdComplete_evt(1, opcode, HCI_CMD_LE_OPCODE_OGF, 3, returnPara, para);
				}
					break;
				//	23 read suggested default data length
				case HCI_CMD_LE_READ_SUGGESTED_DEFAULT_DATA_LENGTH:
				{
					u8 returnPara[8] = {0};
//					returnPara[0] = 0;
//					returnPara[1] = blt_txfifo.size - 13;
//					returnPara[2] = 0;
//					u16 t = LL_PACKET_OCTET_TIME (returnPara[1]);
//					returnPara[3] = t;
//					returnPara[4] = t >> 8;

					returnPara[0] = blc_hci_readSuggestedDefaultTxDataLength(returnPara + 1, returnPara + 3);

					eventCode = HCI_EVT_CMD_COMPLETE;
					resultLen = hci_cmdComplete_evt(1, opcode, HCI_CMD_LE_OPCODE_OGF, 5, returnPara, para);
				}
					break;
				//	24 write suggested default data length
				case HCI_CMD_LE_WRITE_SUGGESTED_DEFAULT_DATA_LENGTH:
				{
					status = blc_hci_writeSuggestedDefaultTxDataLength(cmdPara[0]+cmdPara[1]*256, cmdPara[2]+cmdPara[3]*256);

					eventCode = HCI_EVT_CMD_COMPLETE;
					resultLen = hci_cmdComplete_evt(1, opcode, HCI_CMD_LE_OPCODE_OGF, 1, &status, para);
				}
					break;

	#if (SECURE_CONNECTION_ENABLE)
				//	25 read local P-256 public key
				case HCI_CMD_LE_READ_LOCAL_P256_PUBLIC_KEY:
				{
					status = blc_ll_getP256publicKeyStart ();

					eventCode = HCI_EVT_CMD_STATUS;
					resultLen = 4;
					hci_cmdStatus_evt(1, opcode, HCI_CMD_LE_OPCODE_OGF, status, para);
				}
					break;

				//	26 generate DHKey

				case HCI_CMD_LE_GENERATE_DHKEY:
				{
					///// return param: structure (hci_le_generateDHKeyCompleteEvt_t)
					///// event code : HCI_EVT_LE_META
					u8* hciCmdParam = (u8*)cmdPara;
					u8* remoteP256Key = (u8*) hciCmdParam;

					status = blc_ll_generateDHkey (remoteP256Key);  //6K code

					eventCode = HCI_EVT_CMD_STATUS;
					resultLen = 4;
					hci_cmdStatus_evt(1, opcode, HCI_CMD_LE_OPCODE_OGF, status, para);
				}
					break;
	#endif

	#if (LL_FEATURE_ENABLE_LL_PRIVACY)
				//	27 add device to resolving list
				case HCI_CMD_LE_ADD_DEVICE_TO_RESOLVING_LIST:
				{
					///// return param: param_total_len : 1 byte
					/////				param0: status   		  len : 1 byte
					///// event code : HCI_EVT_CMD_COMPLETE
					u8* hciCmdParam = (u8*)cmdPara;

					u8  peerIdAddrType = *hciCmdParam;
					u8* peerIdAddr = hciCmdParam + 1;
					u8* peerIRK = hciCmdParam + 7;
					u8* localIRK = hciCmdParam + 23;

//					status = blc_ll_addDeviceToResolvingList(peerIdAddrType, peerIdAddr, peerIRK, localIRK);
					status = ll_resolvingList_add (peerIdAddrType, peerIdAddr, peerIRK, localIRK);

					eventCode = HCI_EVT_CMD_COMPLETE;
					resultLen = hci_cmdComplete_evt(1, opcode, HCI_CMD_LE_OPCODE_OGF, 1, &status, para);
				}
					break;

				//	28 remove device from resolving list
				case HCI_CMD_LE_REMOVE_DEVICE_FROM_RESOLVING_LIST:
				{
					///// return param: param_total_len : 1 byte
					/////				param0: status   		  len : 1 byte
					///// event code : HCI_EVT_CMD_COMPLETE
					u8* hciCmdParam = (u8*)cmdPara;
					u8 	peerIdAddrType = *hciCmdParam;
					u8* peerIdAddr = hciCmdParam + 1;

//					status = blt_removeDevAddr(peerIdAddrType, peerIdAddr );
					status = ll_resolvingList_delete(peerIdAddrType, peerIdAddr );
					status = BLE_SUCCESS;

					eventCode = HCI_EVT_CMD_COMPLETE;
					resultLen = hci_cmdComplete_evt(1, opcode, HCI_CMD_LE_OPCODE_OGF, 1, &status, para);
				}
					break;

				//	29 clear resolving list
				case HCI_CMD_LE_CLEAR_RESOLVING_LIST:
				{
					///// return param: param_total_len : 1 byte
					/////				param0: status   		  len : 1 byte
					///// event code : HCI_EVT_CMD_COMPLETE
					status = ll_resolvingList_reset ();

					eventCode = HCI_EVT_CMD_COMPLETE;
					resultLen = hci_cmdComplete_evt(1, opcode, HCI_CMD_LE_OPCODE_OGF, 1, &status, para);
				}
					break;

				//	2a read resolving list size
				case HCI_CMD_LE_READ_RESOLVING_LIST_SIZE:
				{
					///// return param: param_total_len : 2 byte
					/////				param0: status   		  len : 1 byte
					/////				param1: list size   	  len : 1 byte
					///// event code : HCI_EVT_CMD_COMPLETE
					u8 returnPara[2] = {0};
					returnPara[0] = ll_resolvingList_getSize (returnPara + 1);

					eventCode = HCI_EVT_CMD_COMPLETE;
					resultLen = hci_cmdComplete_evt(1, opcode, HCI_CMD_LE_OPCODE_OGF, 2, returnPara, para);
				}
					break;

				//	2b read peer resolvable address
				case HCI_CMD_LE_READ_PEER_RESOLVABLE_ADDRESS:
				{
					///// return param: param_total_len : 7 byte
					/////				param0: status   		     len : 1 byte
					/////				param1: peerResolvableAddr   len : 6 byte
					///// event code : HCI_EVT_CMD_COMPLETE
					u8* hciCmdParam = (u8*)cmdPara;
					u8	peerIdAddrType = *hciCmdParam;
					u8* peerIdAddr = hciCmdParam + 1;

					u8 returnPara[7] = {0};
					u8* peerResolvableAddr = (u8*) (returnPara + 1);

					status = ll_resolvingList_getPeerResolvableAddr(peerIdAddrType, peerIdAddr, peerResolvableAddr);
					returnPara[0] = status;

					eventCode = HCI_EVT_CMD_COMPLETE;
					resultLen = hci_cmdComplete_evt(1, opcode, HCI_CMD_LE_OPCODE_OGF, 7, returnPara, para);
				}
					break;

				//	2c read local resolvable address
				case HCI_CMD_LE_READ_LOCAL_RESOLVABLE_ADDRESS:
				{
					///// return param: param_total_len : 7 byte
					/////				param0: status   		     len : 1 byte
					/////				param1: localResolvableAddr   len : 6 byte
					///// event code : HCI_EVT_CMD_COMPLETE
					u8* hciCmdParam = (u8*)cmdPara;
					u8 	peerIdAddrType = *hciCmdParam;
					u8* peerIdAddr = hciCmdParam + 1;

					u8 returnPara[7] = {0};
					u8* localResolvableAddr = (u8*) (returnPara + 1);

					status = ll_resolvingList_getLocalResolvableAddr(peerIdAddrType, peerIdAddr, localResolvableAddr);
					returnPara[0] = status;

					eventCode = HCI_EVT_CMD_COMPLETE;
					resultLen = hci_cmdComplete_evt(1, opcode, HCI_CMD_LE_OPCODE_OGF, 7, returnPara, para);
				}
					break;

				//	2d set address resolution enable
				case HCI_CMD_LE_SET_ADDRESS_RESOLUTION_ENABLE:
				{
					///// return param: param_total_len : 1 byte
					/////				param0: status   		     len : 1 byte
					///// event code : HCI_EVT_CMD_COMPLETE
					u8* hciCmdParam = (u8*)cmdPara;
					u8 resolutionEn = hciCmdParam[0];

					status = ll_resolvingList_setAddrResolutionEnable(resolutionEn);

					eventCode = HCI_EVT_CMD_COMPLETE;
					resultLen = hci_cmdComplete_evt(1, opcode, HCI_CMD_LE_OPCODE_OGF, 1, &status, para);
				}
					break;

				//	2e set resolvable private address timeout
				case HCI_CMD_LE_SET_RESOLVABLE_PRIVATE_ADDRESS_TIMEOUT:
				{
					///// return param: param_total_len : 1 byte
					/////				param0: status   		     len : 1 byte
					///// event code : HCI_EVT_CMD_COMPLETE
					u8* hciCmdParam = (u8*)cmdPara;
					u16 timeout_s = hciCmdParam[0] + hciCmdParam[1] * 256 ;

					status = ll_resolvingList_setResolvablePrivateAddrTimer(timeout_s);

					eventCode = HCI_EVT_CMD_COMPLETE;
					resultLen = hci_cmdComplete_evt(1, opcode, HCI_CMD_LE_OPCODE_OGF, 1, &status, para);
				}
					break;
	#endif

				//	2f read maximum data length
				case HCI_CMD_LE_READ_MAX_DATA_LENGTH:
				{
					u8 returnPara[12] = {0};
					returnPara[0] = 0;
					returnPara[1] = blt_txfifo.size - 13;
					returnPara[2] = 0;
					u16 t = LL_PACKET_OCTET_TIME (returnPara[1]);
					returnPara[3] = t;
					returnPara[4] = t >> 8;

					returnPara[5] = blt_rxfifo.size - 37;
					returnPara[6] = 0;
					t = LL_PACKET_OCTET_TIME (returnPara[5]);
					returnPara[7] = t;
					returnPara[8] = t >> 8;
					eventCode = HCI_EVT_CMD_COMPLETE;
					resultLen = hci_cmdComplete_evt(1, opcode, HCI_CMD_LE_OPCODE_OGF, 9, returnPara, para);
				}
					break;



	#if (LL_FEATURE_ENABLE_LE_2M_PHY | LL_FEATURE_ENABLE_LE_CODED_PHY)
				case HCI_CMD_LE_READ_PHY:
				{
					u8 returnPara[6];
					u16 connHandle = (cmdPara[0] | cmdPara[1] <<8);
					blc_ll_readPhy(connHandle, (hci_le_readPhyCmd_retParam_t *)returnPara );  //note that status is included in "returnPara"

					eventCode = HCI_EVT_CMD_COMPLETE;
					resultLen = hci_cmdComplete_evt(1, opcode, HCI_CMD_LE_OPCODE_OGF, 5, returnPara, para);
				}
				break;

				case HCI_CMD_LE_SET_DEFAULT_PHY:
				{
					status = (u8) blc_ll_setDefaultPhy (cmdPara[0], cmdPara[1], cmdPara[2]);
					eventCode = HCI_EVT_CMD_COMPLETE;
					resultLen = hci_cmdComplete_evt(1, opcode, HCI_CMD_LE_OPCODE_OGF, 1, &status, para);
				}
				break;

				case HCI_CMD_LE_SET_PHY:
				{
					status = (u8) blc_hci_le_setPhy ( (hci_le_setPhyCmd_param_t*)cmdPara);
					eventCode = HCI_EVT_CMD_STATUS;
					resultLen = 4;
					hci_cmdStatus_evt(1, opcode, HCI_CMD_LE_OPCODE_OGF, status, para);
				}
				break;
	#endif


				case HCI_CMD_LE_ENHANCED_RECEIVER_TEST:
				{
				#if (MCU_CORE_TYPE == MCU_CORE_9518)
					status = blt_phyTest_hci_setReceiverTest_V2(cmdPara[0],cmdPara[1],cmdPara[2]);
				#elif (MCU_CORE_TYPE == MCU_CORE_825x || MCU_CORE_TYPE == MCU_CORE_827x)
					status = blc_phy_setEnhancedReceiverTest(cmdPara[0],cmdPara[1],cmdPara[2]);
				#endif
					eventCode = HCI_EVT_CMD_COMPLETE;
					resultLen = hci_cmdComplete_evt(1, opcode, HCI_CMD_LE_OPCODE_OGF, 1, &status, para);
				}
				break;


				case HCI_CMD_LE_ENHANCED_TRANSMITTER_TEST:
				{
				#if (MCU_CORE_TYPE == MCU_CORE_9518)
					status = blt_phyTest_hci_setTransmitterTest_V2(cmdPara[0],cmdPara[1],cmdPara[2],cmdPara[3]);
				#elif (MCU_CORE_TYPE == MCU_CORE_825x || MCU_CORE_TYPE == MCU_CORE_827x)
					status = blc_phy_setEnhancedTransmitterTest(cmdPara[0],cmdPara[1],cmdPara[2],cmdPara[3]);
				#endif
					eventCode = HCI_EVT_CMD_COMPLETE;
					resultLen = hci_cmdComplete_evt(1, opcode, HCI_CMD_LE_OPCODE_OGF, 1, &status, para);
				}
				break;


	#if (LL_FEATURE_ENABLE_LE_EXTENDED_ADVERTISING)
				case HCI_CMD_LE_SET_ADVERTISING_SET_RANDOM_ADDRESS:
				{
					status = (u8) blc_ll_setAdvRandomAddr(cmdPara[0], cmdPara+1);
					eventCode = HCI_EVT_CMD_COMPLETE;
					resultLen = hci_cmdComplete_evt(1, opcode, HCI_CMD_LE_OPCODE_OGF, 1, &status, para);
				}
				break;



				case HCI_CMD_LE_SET_EXTENDED_ADVERTISING_PARAMETERS:
				{
					u8 returnPara[4];
					returnPara[0] = (u8)blc_hci_le_setExtAdvParam( (hci_le_setExtAdvParam_cmdParam_t *)cmdPara, returnPara+1 );
					eventCode = HCI_EVT_CMD_COMPLETE;
					resultLen = hci_cmdComplete_evt(1, opcode, HCI_CMD_LE_OPCODE_OGF, 2, returnPara, para);
				}
				break;


				case HCI_CMD_LE_SET_EXTENDED_ADVERTISING_DATA:
				{
					status = (u8)blc_ll_setExtAdvData(cmdPara[0], cmdPara[1], cmdPara[2], cmdPara[3], cmdPara+4);
					eventCode = HCI_EVT_CMD_COMPLETE;
					resultLen = hci_cmdComplete_evt(1, opcode, HCI_CMD_LE_OPCODE_OGF, 1, &status, para);
				}
				break;

				case HCI_CMD_LE_SET_EXTENDED_SCAN_RESPONSE_DATA:
				{
					status = (u8)blc_ll_setExtScanRspData(cmdPara[0], cmdPara[1], cmdPara[2], cmdPara[3], cmdPara+4);
					eventCode = HCI_EVT_CMD_COMPLETE;
					resultLen = hci_cmdComplete_evt(1, opcode, HCI_CMD_LE_OPCODE_OGF, 1, &status, para);
				}
				break;

				case HCI_CMD_LE_SET_EXTENDED_ADVERTISING_ENABLE:
				{
					status = (u8)blc_hci_le_setExtAdvEnable(cmdPara[0], cmdPara[1], cmdPara+2);
					#if (BQB_5P0_TEST_ENABLE)
						if(status==BLE_SUCCESS)
						{
							blc_ll_initSlaveRole_module();				//slave module: 	 mandatory for BLE slave,
							blc_l2cap_register_handler (blc_hci_sendACLData2Host);  	//l2cap initialization
						}
					#endif
					eventCode = HCI_EVT_CMD_COMPLETE;
					resultLen = hci_cmdComplete_evt(1, opcode, HCI_CMD_LE_OPCODE_OGF, 1, &status, para);
				}
				break;


				case HCI_CMD_LE_READ_MAXIMUM_ADVERTISING_DATA_LENGTH:
				{
					u8 returnPara[4];

					//HCI/GEV/BV-02-C [Disallow Mixing Legacy and Extended Advertising Commands]
					ble_sts_t status = BLE_SUCCESS;
					if(bltParam.adv_hci_cmd & ADV_LEGACY_MASK){
						status =  HCI_ERR_CMD_DISALLOWED;
					}
					else
					{
						bltParam.adv_hci_cmd |= ADV_EXTENDED_MASK;
					}

					u16  max_len = blc_ll_readMaxAdvDataLength();
					returnPara[0] = status;
					returnPara[1] = U16_LO(max_len);
					returnPara[2] = U16_HI(max_len);
					eventCode = HCI_EVT_CMD_COMPLETE;
					resultLen = hci_cmdComplete_evt(1, opcode, HCI_CMD_LE_OPCODE_OGF, 3, returnPara, para);
				}
				break;

				case HCI_CMD_LE_READ_NUMBER_OF_SUPPORTED_ADVERTISING_SETS:
				{
					u8 returnPara[4];

					//HCI/GEV/BV-02-C [Disallow Mixing Legacy and Extended Advertising Commands]
					ble_sts_t status = BLE_SUCCESS;
					if(bltParam.adv_hci_cmd & ADV_LEGACY_MASK){
						status =  HCI_ERR_CMD_DISALLOWED;
					}
					else
					{
						bltParam.adv_hci_cmd |= ADV_EXTENDED_MASK;
					}

					returnPara[0] = status;
					returnPara[1] = blc_ll_readNumberOfSupportedAdvSets();
					eventCode = HCI_EVT_CMD_COMPLETE;
					resultLen = hci_cmdComplete_evt(1, opcode, HCI_CMD_LE_OPCODE_OGF, 2, returnPara, para);
				}
					break;



				case HCI_CMD_LE_REMOVE_ADVERTISING_SET:
				{
					status = blc_ll_removeAdvSet(cmdPara[0]);
					eventCode = HCI_EVT_CMD_COMPLETE;
					resultLen = hci_cmdComplete_evt(1, opcode, HCI_CMD_LE_OPCODE_OGF, 1, &status, para);
				}
				break;

				case HCI_CMD_LE_CLEAR_ADVERTISING_SETS:
				{
					status = blc_ll_clearAdvSets();
					eventCode = HCI_EVT_CMD_COMPLETE;
					resultLen = hci_cmdComplete_evt(1, opcode, HCI_CMD_LE_OPCODE_OGF, 1, &status, para);
				}
				break;
	#endif


	#if (LL_FEATURE_ENABLE_LE_PERIODIC_ADVERTISING)
				///////////////// Periodic Advertising ///////////////////////////////////////////
				case HCI_CMD_LE_SET_PERIODIC_ADVERTISING_PARAMETERS:
				{
					status = (u8)blc_ll_setPeriodicAdvParam(cmdPara[0], cmdPara[1] | cmdPara[2]<<8, cmdPara[3] | cmdPara[4]<<8, cmdPara[5] | cmdPara[6]<<8);
					eventCode = HCI_EVT_CMD_COMPLETE;
					resultLen = hci_cmdComplete_evt(1, opcode, HCI_CMD_LE_OPCODE_OGF, 1, &status, para);
				}
				break;

				case HCI_CMD_LE_SET_PERIODIC_ADVERTISING_DATA:
				{
					status = (u8)blc_ll_setPeriodicAdvData(cmdPara[0], cmdPara[1], cmdPara[2], cmdPara+3);
					eventCode = HCI_EVT_CMD_COMPLETE;
					resultLen = hci_cmdComplete_evt(1, opcode, HCI_CMD_LE_OPCODE_OGF, 1, &status, para);
				}
				break;

				case HCI_CMD_LE_SET_PERIODIC_ADVERTISING_ENABLE:
				{
					status = (u8)blc_ll_setPeriodicAdvEnable(cmdPara[0], cmdPara[1]);
					eventCode = HCI_EVT_CMD_COMPLETE;
					resultLen = hci_cmdComplete_evt(1, opcode, HCI_CMD_LE_OPCODE_OGF, 1, &status, para);
				}
				break;
				/////////////////////////////////////////////////////////////////////////////////////
	#endif

				///////////////// Extended Scanning ///////////////////////////////////////////
				//	41 set ext scan parameters
	#if (LL_FEATURE_ENABLE_LE_EXTENDED_SCAN)
				case HCI_CMD_LE_SET_EXTENDED_SCAN_PARAMETERS:
				{
					status = (u8)blc_hci_le_setExtScanParam(cmdPara[0], cmdPara[1], cmdPara[2], cmdPara+3);
					eventCode = HCI_EVT_CMD_COMPLETE;
					resultLen = hci_cmdComplete_evt(1, opcode, HCI_CMD_LE_OPCODE_OGF, 1, &status, para);
				}
				break;

				//	42 set ext scan enable
				case HCI_CMD_LE_SET_EXTENDED_SCAN_ENABLE:
				{
					status = (u8)blc_hci_le_setExtScanEnable(cmdPara[0], cmdPara[1], cmdPara[2]|cmdPara[3]<<8, cmdPara[4]|cmdPara[5]<<8);
					eventCode = HCI_EVT_CMD_COMPLETE;
					resultLen = hci_cmdComplete_evt(1, opcode, HCI_CMD_LE_OPCODE_OGF, 1, &status, para);
				}
				break;
	#endif
				/////////////////////////////////////////////////////////////////////////////////////



				case HCI_CMD_LE_EXTENDED_CREATE_CONNECTION:
				{

				}
				break;



#if (LL_FEATURE_ENABLE_ISO)
				case HCI_CMD_LE_READ_ISO_TX_SYNC:
				{
					u8 returnPara[12];
					u16 iso_connHandle = (cmdPara[0] | cmdPara[1] <<8);
					returnPara[0] = blc_hci_le_read_iso_tx_sync_cmd(iso_connHandle, &(returnPara[1]));
					eventCode = HCI_EVT_CMD_COMPLETE;
					resultLen = hci_cmdComplete_evt(1, opcode, HCI_CMD_LE_OPCODE_OGF, 12, returnPara, para);
				}
				break;

				case HCI_CMD_LE_SET_CIG_PARAMETERS:
				{
					u8 returnPara[4 + (BLT_CIS_NUM_MAX<<1)];  //3 + CIS_CounT_max*2
					hci_le_setCigParam_cmdParam_t* pCmdParm = (hci_le_setCigParam_cmdParam_t*) cmdPara;
					returnPara[0] = blc_hci_le_setCigParams(pCmdParm, returnPara + 1);
					eventCode = HCI_EVT_CMD_COMPLETE;
					resultLen = hci_cmdComplete_evt(1, opcode, HCI_CMD_LE_OPCODE_OGF, 3 + pCmdParm->cis_count*2, returnPara, para);
				}
				break;

				case HCI_CMD_LE_SET_CIG_PARAMETERS_TEST:
				{
					u8 returnPara[4 + (BLT_CIS_NUM_MAX<<1)];  //3 + CIS_CounT_max*2
					hci_le_setCigParamTest_cmdParam_t* pCmdParm = (hci_le_setCigParamTest_cmdParam_t*) cmdPara;
					returnPara[0] = blc_hci_le_setCigParamsTest(pCmdParm, returnPara + 1);
					eventCode = HCI_EVT_CMD_COMPLETE;
					resultLen = hci_cmdComplete_evt(1, opcode, HCI_CMD_LE_OPCODE_OGF, 3 + pCmdParm->cis_count*2, returnPara, para);
				}
				break;

				case HCI_CMD_LE_CREATE_CIS:
				{
					status = blc_hci_le_createCis((hci_le_CreateCisParams_t *)cmdPara);
					eventCode = HCI_EVT_CMD_STATUS;
					resultLen = 4;
					*para32 = HCI_EVT_CMDSTATUS(1, opcode, HCI_CMD_LE_OPCODE_OGF, status);
				}
				break;

				case HCI_CMD_LE_REMOVE_CIG:
				{
					u8 returnPara[2];
					returnPara[0] = blc_hci_le_removeCig(cmdPara[0], returnPara + 1);
					eventCode = HCI_EVT_CMD_COMPLETE;
					resultLen = hci_cmdComplete_evt(1, opcode, HCI_CMD_LE_OPCODE_OGF, 2, returnPara, para);
				}
				break;

				case HCI_CMD_LE_ACCEPT_CIG_REQUEST:
				{
					u16 cisHandle = (cmdPara[0] | cmdPara[1] <<8);
					status = blc_hci_le_acceptCisReq(cisHandle);
					eventCode = HCI_EVT_CMD_STATUS;
					resultLen = 4;
					*para32 = HCI_EVT_CMDSTATUS(1, opcode, HCI_CMD_LE_OPCODE_OGF, status);
				}
				break;

				case HCI_CMD_LE_REJECT_CIS_REQUEST:
				{
					u8 returnPara[3];
					returnPara[0] = blc_hci_le_rejectCisReq((hci_le_rejectCisReqParams_t*)cmdPara, returnPara + 1);
					eventCode = HCI_EVT_CMD_COMPLETE;
					resultLen = hci_cmdComplete_evt(1, opcode, HCI_CMD_LE_OPCODE_OGF, 3, returnPara, para);
				}
				break;

				case HCI_CMD_LE_CREATE_BIG:
				{
					status = blc_hci_le_createBigParams((hci_le_setCigParam_cmdParam_t*)cmdPara);
					eventCode = HCI_EVT_CMD_STATUS;
					resultLen = 4;
					*para32 = HCI_EVT_CMDSTATUS(1, opcode, HCI_CMD_LE_OPCODE_OGF, status);
				}
				break;

				case HCI_CMD_LE_CREATE_BIG_TEST:
				{
					status = blc_hci_le_createBigParamsTest((hci_le_createBigParamsTest_t*)cmdPara);
					eventCode = HCI_EVT_CMD_STATUS;
					resultLen = 4;
					*para32 = HCI_EVT_CMDSTATUS(1, opcode, HCI_CMD_LE_OPCODE_OGF, status);
				}
				break;

				case HCI_CMD_LE_TERMINATE_BIG:
				{
					status = blc_hci_le_terminateBig((hci_le_terminateBigParams_t*)cmdPara);
					eventCode = HCI_EVT_CMD_STATUS;
					resultLen = 4;
					*para32 = HCI_EVT_CMDSTATUS(1, opcode, HCI_CMD_LE_OPCODE_OGF, status);
				}
				break;

				case HCI_CMD_LE_BIG_CREATE_SYNC:
				{
					status = blc_hci_le_bigCreateSync((hci_le_bigCreateSyncParams_t*)cmdPara);
					eventCode = HCI_EVT_CMD_STATUS;
					resultLen = 4;
					*para32 = HCI_EVT_CMDSTATUS(1, opcode, HCI_CMD_LE_OPCODE_OGF, status);
				}
				break;

				case HCI_CMD_LE_BIG_TERMINATE_SYNC:
				{
					u8 returnPara[2];
					returnPara[0] = blc_hci_le_bigTerminateSync(cmdPara[0], returnPara+1);
					eventCode = HCI_EVT_CMD_COMPLETE;
					resultLen = hci_cmdComplete_evt(1, opcode, HCI_CMD_LE_OPCODE_OGF, 2, returnPara, para);
				}
				break;

				case HCI_CMD_LE_REQUEST_PEER_SCA:
				{

				}
				break;

				case HCI_CMD_LE_SETUP_ISO_DATA_PATH:
				{

				}
				break;

				case HCI_CMD_LE_REMOVE_ISO_DARA_PATH:
				{

				}
				break;
#endif
				case HCI_CMD_LE_PERIODIC_ADVERTISING_CREATE_SYNC:
				{

				}
				break;
				case HCI_CMD_LE_PERIODIC_ADVERTISING_CREATE_SYNC_CANCEL:
				{

				}
				break;

				case HCI_CMD_LE_PERIODIC_ADVERTISING_TERMINATE_SYNC:
				{

				}
				break;

				case HCI_CMD_LE_ADD_DEVICE_TO_PERIODIC_ADVERTISER_LIST:
				{

				}
				break;

				case HCI_CMD_LE_REMOVE_DEVICE_FROM_PERIODIC_ADVERTISER_LIST:
				{

				}
				break;

				case HCI_CMD_LE_CLEAR_PERIODIC_ADVERTISER_LIST:
				{

				}
				break;

				case HCI_CMD_LE_READ_PERIODIC_ADVERTISER_LIST_SIZE:
				{

				}
				break;

				case HCI_CMD_LE_READ_TRANSMIT_POWER:
				{

				}
				break;

				case HCI_CMD_LE_READ_RF_PATH_COMPENSATION:
				{

				}
				break;

				case HCI_CMD_LE_WRITE_RF_PATH_COMPENSATION:
				{

				}
				break;

	#if (LL_FEATURE_ENABLE_LL_PRIVACY)
				case HCI_CMD_LE_SET_PRIVACY_MODE:
				{
					u8* hciCmdParam = (u8*)cmdPara;
					u8  peerIdAddrType = *hciCmdParam;
					u8* peerIdAddr = hciCmdParam + 1;
					u8  privacyMode = *(hciCmdParam + 7);

					status = ll_resolvingList_setPrivcyMode(peerIdAddrType, peerIdAddr, privacyMode);
					eventCode = HCI_EVT_CMD_COMPLETE;
					resultLen = hci_cmdComplete_evt(1, opcode, HCI_CMD_LE_OPCODE_OGF, 1, &status, para);
				}
				break;
	#endif

				default :
					break;
			}//end of switch
		}
		else if (p[2] == HCI_CMD_TEST_OPCODE_OGF || p[2] == HCI_CMD_LINK_POLICY_OPCODE_OGF)		//vendor cmds group
		{
			status = HCI_ERR_UNKNOWN_HCI_CMD;
			eventCode = HCI_EVT_CMD_COMPLETE;
			resultLen = hci_cmdComplete_evt(1, opcode, p[2], 1, &status, para);
		}
		else if (p[2] == HCI_CMD_VENDOR_OPCODE_OGF)		//vendor cmds group
		{
			switch(opcode)
			{
			//	01 read register
				case HCI_TELINK_READ_REG:
				{
					u8 result;
					u8 type = cmdPara[0];		//0 for digital 1 for analog
					if(type==0)
					{
						u16 addr = (cmdPara[2]<<8) + cmdPara[1] + 0x800000;
						result = read_reg8(addr);
					}
					else
						result = analog_read_reg8(cmdPara[1]);
					eventCode = HCI_EVT_CMD_COMPLETE;
					resultLen = hci_cmdComplete_evt(1, opcode, HCI_CMD_VENDOR_OPCODE_OGF, 1, &result, para);
				}
				break;
				//	02 write register
				case HCI_TELINK_WRITE_REG:
				{
					u8 type = cmdPara[0];		//0 for digital 1 for analog
					u8 value = cmdPara[3];
					if(type==0)
					{
						u16 addr = (cmdPara[2]<<8) + cmdPara[1] + 0x800000;
						write_reg8(addr,value);
					}
					else
						analog_write_reg8(cmdPara[1],value);
					eventCode = HCI_EVT_CMD_COMPLETE;
					resultLen = hci_cmdComplete_evt(1, opcode, HCI_CMD_VENDOR_OPCODE_OGF, 1, &status, para);
				}
				break;
				//	03 set rf tx power
				case HCI_TELINK_SET_TX_PWR:
				{
					u8 power = cmdPara[0];
					rf_set_power_level_index(power);
					eventCode = HCI_EVT_CMD_COMPLETE;
					resultLen = hci_cmdComplete_evt(1, opcode, HCI_CMD_VENDOR_OPCODE_OGF, 1, &status, para);
				}
				break;

				case HCI_TELINK_SET_RXTX_DATA_LEN:
				{
				#if (MCU_CORE_TYPE == MCU_CORE_825x || MCU_CORE_TYPE == MCU_CORE_827x)
					extern void   blc_hci_telink_setIRXTxDataLength (u8 req_en, u16 maxRxOct, u16 maxTxOct);
					blc_hci_telink_setIRXTxDataLength(cmdLen, cmdPara[0] | cmdPara[1]<<8, cmdPara[2] | cmdPara[3]<<8);
					resultLen = 0;
				#endif
				}
				break;


				default :
					break;
			}
		}
		else
		{
			//  01 06 04 03 01 00 13: disconnect (handle 01 00; reason: 0x13)
			//	01 0d 04 02 01 00   : read remote version info
			//	01 01 0c   ... set event mask
			//	01 03 0c   ... reset hci
			//	01 01 10   ... read local version info
			//	01 03 10   ... read local supported feature
			//	01 05 10   ... read buffer size
			//  01 09 10   ... read BD address

			status = 0;				//OK
			u32  ret = HCI_EVT_CMD_COMPLETE_STATUS(1, opcode, p[2], status);
			blc_hci_send_data (HCI_FLAG_EVENT_BT_STD | HCI_EVT_CMD_COMPLETE, (u8 *)&ret, 4);
			return 0;
		}

		if(resultLen)
		{
			header = HCI_FLAG_EVENT_BT_STD | eventCode;
			blc_hci_send_data (header, para, resultLen);
		}
		else{   // IUT which supports LE only, does not respond to BR/EDR HCI commands.return an HCI Command
				// Complete Event or HCI Command Status Event with Status = Unknown HCI Command.
			#if 1
//				if(opcode != HCI_CMD_LE_OPCODE_OGF)
				{
					status = HCI_ERR_UNKNOWN_HCI_CMD;//HCI/GEV/BV-01-C
					eventCode = HCI_EVT_CMD_STATUS;
					resultLen = 4;
					hci_cmdStatus_evt(1, opcode, p[2], status, para);

					header = HCI_FLAG_EVENT_BT_STD | eventCode;
					blc_hci_send_data (header, para, resultLen);
				}
			#endif
		}
	}
	return 0;
}



///////////////////////////////////////////
// TX
///////////////////////////////////////////
int blc_hci_send_data (u32 h, u8 *para, int n)
{
#if (MCU_CORE_TYPE == MCU_CORE_9518)
	u8 *p = hci_fifo_wptr (&bltHci_txfifo);
	if (!p || n >= bltHci_txfifo.size)
#elif (MCU_CORE_TYPE == MCU_CORE_825x || MCU_CORE_TYPE == MCU_CORE_827x)
	u8 *p = my_fifo_wptr (&hci_tx_fifo);
	if (!p || n >= hci_tx_fifo.size)
#endif
	{
		return -1;
	}

	int nl = n + 4;
	if (h & HCI_FLAG_EVENT_TLK_MODULE)
	{
		*p++ = nl;
		*p++ = nl >> 8;
		*p++ = 0xff;
		*p++ = n + 2;
		*p++ = h;
		*p++ = h>>8;
		memcpy (p, para, n);
		p += n;
	}
	else if (h & HCI_FLAG_EVENT_BT_STD)
	{
		*p++ = n + 3;   // n+3 ?
		*p++ = 0x0;
		*p++ = HCI_TYPE_EVENT;
		*p++ = h;
		*p++ = n;
		memcpy (p, para, n);
		p += n;
	}
	else if (h & HCI_FLAG_EVENT_STACK)
	{
		*p++ = 0xff;
		*p++ = h;
		*p++ = h>>8;
		*p++ = n + 2;
		memcpy (p, para, n);
		p += n;
	}
	else if (h & HCI_FLAG_ACL_BT_STD)			//ACL data
	{
		n = para[1];
		*p++ = n + 5;
		*p++ = (n + 5) >> 8;
		*p++ = 0x02;
		*p++ = h;								//handle
		*p++ = (h>>8) | ((para[0]&3) == L2CAP_CONTINUING_PKT ? 0x10 : 0x20);  //start llid 2 ->0x20 ;  continue llid 1 ->0x10
		*p++ = n;							//length
		*p++ = n >> 8;
		memcpy (p, para + 2, n);
		p += n;
	}
	else if(h & HCI_FLAG_EVENT_PHYTEST_2_WIRE_UART)
	{
		*p++ = n;							//length
		*p++ = n >> 8;
		memcpy (p, para, n);
	}

#if (MCU_CORE_TYPE == MCU_CORE_9518)
	hci_fifo_next (&bltHci_txfifo);
#elif (MCU_CORE_TYPE == MCU_CORE_825x || MCU_CORE_TYPE == MCU_CORE_827x)
	my_fifo_next (&hci_tx_fifo);
#endif
	return 0;
}

#if (MCU_CORE_TYPE == MCU_CORE_9518)
	int blc_hci_tx_to_usb ()
	{
		u8 *p = hci_fifo_get (&bltHci_txfifo);
		if (p)
		{
			extern int usb_bulk_in_packet (u8 *p, int n);
			if (usb_bulk_in_packet (p + 2, p[0] + p[1] * 256) == 0)
			{
				hci_fifo_pop (&bltHci_txfifo);
			}
		}
		return 0;
	}
#elif (MCU_CORE_TYPE == MCU_CORE_825x || MCU_CORE_TYPE == MCU_CORE_827x)
	int blc_hci_tx_to_usb ()
	{
		u8 *p = my_fifo_get (&hci_tx_fifo);
		if (p)
		{
			extern int usb_bulk_in_packet (u8 *p, int n);
			if (usb_bulk_in_packet (p + 2, p[0] + p[1] * 256) == 0)
			{
				my_fifo_pop (&hci_tx_fifo);
			}
		}
		return 0;
	}
#endif

#if (MCU_CORE_TYPE == MCU_CORE_825x || MCU_CORE_TYPE == MCU_CORE_827x)
int blc_hci_tx_to_btusb ()
{
	u8 *p = my_fifo_get (&hci_tx_fifo);
	if (p)
	{
		extern int btusb_interrupt_in_packet (u8 *p, int n);
		extern int btusb_bulk_in_packet (u8 *p, int n);
		if (p[2] == 4)		//event
		{
			if (btusb_interrupt_in_packet (p + 3, p[0] + p[1] * 256 - 1) == 0)
			{
				my_fifo_pop (&hci_tx_fifo);
			}
		}
		else		//ACL data
		{
			if ((btusb_bulk_in_packet (p + 3, p[0] + p[1] * 256 - 1) == 0))
			{
				my_fifo_pop (&hci_tx_fifo);
			}
		}
	}
	return 0;
}
#endif

////////////////////////////
// RX
////////////////////////////
int blc_hci_rx_from_usb (void)
{
	u8 buff[72];
	static u32 hci_tx_ddd;

	extern int usb_bulk_out_get_data (u8 *p);
	int n = usb_bulk_out_get_data (buff);
	if (n)
	{
		hci_tx_ddd++;
		blc_hci_handler (buff, n);
	}
	return 0;
}

int blc_hci_rx_from_btusb (u8 *p, int n)
{
	blc_hci_handler (p, n);

	return 0;
}

#if (MCU_CORE_TYPE == MCU_CORE_825x || MCU_CORE_TYPE == MCU_CORE_827x)
int blc_acl_from_btusb ()
{
	u8 p[72];
	extern int usb_bulk_out_get_data (u8 *p);
	int n = usb_bulk_out_get_data (p);
	if (n)
	{
		u16 h = p[0] + (p[1] & 15) * 256;
		p[3] = p[2];
		p[2] = (p[1] & 0x30) == 0x10 ? 1 : 2;
		blm_push_fifo (h, p + 2);
		p[0] = 1;
		p[1] = h;
		p[2] = 0;
		p[3] = 1;
		p[4] = 0;
		blc_hci_send_data (HCI_FLAG_EVENT_BT_STD | HCI_EVT_NUM_OF_COMPLETE_PACKETS, p, 5);

	}
	return 0;
}
#endif
/////////////////////////////////////
//	checking incoming packet
//  send out pending data in buffer
////////////////////////////////////
int blc_hci_proc (void)
{
	///////// RX //////////////
	if (blc_hci_rx_handler)
	{
		blc_hci_rx_handler ();
	}

	///////// TX //////////////
	if (blc_hci_tx_handler)
	{
		blc_hci_tx_handler ();
	}
	return 0;
}
