const SWSButton = document.getElementById("SWSButton");
const StallButton = document.getElementById("StallButton");
const StartButton = document.getElementById("StartButton");
const usbConnectButton = document.getElementById("usbConnectButton");
const DownloadButton = document.getElementById("DownloadButton");
const EraseButton = document.getElementById("EraseButton");
const ActivateButton = document.getElementById("ActivateButton");
const RunButton = document.getElementById("RunButton");
const PauseButton = document.getElementById("PauseButton");
const StepButton = document.getElementById("StepButton");
const PCButton = document.getElementById("PCButton");
const ResetButton = document.getElementById("ResetButton");
const ClearButton = document.getElementById("ClearButton");
const usbRWAddr = document.getElementById("usbRWAddr");
const usbRWLength = document.getElementById("usbRWLength");
const usbRWData = document.getElementById("usbRWData");
const FwDownloadButton1 = document.getElementById("FwDownloadButton1");
const FwDownloadButton2 = document.getElementById("FwDownloadButton2");
const MultiDownloadButton = document.getElementById("MultiDownloadButton");

const input1 = document.getElementById("input1");
const input2 = document.getElementById("input2");
const input3 = document.getElementById("input3");
const input4 = document.getElementById("input4");
const input5 = document.getElementById("input5");
const input6 = document.getElementById("input6");
const input7 = document.getElementById("input7");
const input8 = document.getElementById("input8");

SWSButton.onclick = async () => {
 	console.log("SWSButton");
	if(0 == BurningEVKBusy) {
		BurningEVKBusy = 1;
		var sws_config = new Array();
		sws_config[0] = parseInt("0x"+input1.value); sws_config[1] = parseInt("0x"+input2.value);
		sws_config[2] = parseInt("0x"+input3.value); sws_config[3] = parseInt("0x"+input4.value);
		await usb_evk_sws_mcu(ChipType,sws_config);
		BurningEVKBusy = 0;
	}
	else {
		LogConsole("BurningEVK is Busy");
		return ERROR_Typdef.EBUSY;
	}
	return 0;
}

StallButton.onclick = async () => {
	console.log("StallButton");
	if(0 == BurningEVKBusy) {
		BurningEVKBusy = 1;
		SingleStepFlag = 1;
 		await usb_evk_stall_mcu(TL_ModeTypdef.EVK,ChipType,parseInt("0x"+input5.value),parseInt("0x"+input6.value));
		 BurningEVKBusy = 0;
	}
	else {
		LogConsole("BurningEVK is Busy");
		return ERROR_Typdef.EBUSY;
	}
	return 0;
}

StartButton.onclick = async () => {
	console.log("StartButton");
	if(0 == BurningEVKBusy) {
		BurningEVKBusy = 1;
		await usb_evk_start_mcu(TL_ModeTypdef.EVK,ChipType,parseInt("0x"+input7.value),parseInt("0x"+input8.value));
		BurningEVKBusy = 0;
	}
	else {
		LogConsole("BurningEVK is Busy");
	}
	return 0;
}	

EraseButton.onclick = async () => {
	if(0 == BurningEVKBusy) {
		BurningEVKBusy = 1;
		var msecond1;
		var msecond2;
		msecond1 = new Date();
		console.log("EraseButton");
		//LogConsole("EraseSizeInput.value/4,:"+EraseSizeInput.value/4);
		await SectorEraseFlash(TL_ModeTypdef.EVK,ChipType,hex2int(EraseAddrInput.value),EraseSizeInput.value/4,flash_erase_callback);
		msecond2 = new Date();
		LogConsole("Total Time:  "+ parseInt(msecond2-msecond1) +"  ms");
		BurningEVKBusy = 0; 
	}
	else {
		LogConsole("BurningEVK is Busy");
		return ERROR_Typdef.EBUSY;
	}
	return 0;
}

function flash_erase_callback(page_adr) {
	LogConsole("Flash Sector (4K) Erase at address: 0x"+dec2hex(page_adr,6));
}

ActivateButton.onclick = async () => {
	if(0 == BurningEVKBusy) {
		BurningEVKBusy = 1;
	 	console.log("ActivateButton");
	 	await usb_evk_activate_mcu(ChipType);
		BurningEVKBusy = 0; 
	}
	else {
		LogConsole("BurningEVK is Busy");
		return ERROR_Typdef.EBUSY;
	}
	return 0;
}

RunButton.onclick = async () => {
	if(0 == BurningEVKBusy) {
		BurningEVKBusy = 1;
	    console.log("RunButton");
		await usb_evk_run_mcu(TL_ModeTypdef.EVK,ChipType);
		BurningEVKBusy = 0; 
	}
	else {
		LogConsole("BurningEVK is Busy");
		return ERROR_Typdef.EBUSY;
	}
	return 0;
}

PauseButton.onclick = async () => {
	console.log("PauseButton");
	if(0 == BurningEVKBusy) {
		BurningEVKBusy = 1;
		SingleStepFlag = 1;
	 	await usb_evk_pause_mcu(TL_ModeTypdef.EVK,ChipType);
		BurningEVKBusy = 0; 
	}
	else {
		LogConsole("BurningEVK is Busy");
		return ERROR_Typdef.EBUSY;
	}
	return 0;
}

StepButton.onclick = async () => {
	console.log("StepButton");
	if(0 == BurningEVKBusy) {
		BurningEVKBusy = 1;
		 if(0 == SingleStepFlag) {
			while(0 == SingleStepFlag) {
				await usb_evk_step_mcu(TL_ModeTypdef.EVK,ChipType);
				await sleep(200);
			}
		}
		else {
			await usb_evk_step_mcu(TL_ModeTypdef.EVK,ChipType);
		}
		BurningEVKBusy = 0; 
	}
	else {
		LogConsole("BurningEVK is Busy");
		return ERROR_Typdef.EBUSY;
	}
	return 0;
}

PCButton.onclick = async () => {
	 console.log("PCButton");
	 if(0 == BurningEVKBusy) {
		BurningEVKBusy = 1;
		if(0 == SingleStepFlag) {
			while(0 == SingleStepFlag) {
				await usb_evk_getpc_mcu(TL_ModeTypdef.EVK,ChipType);
				await sleep(200);
			}
		}
		else {
			await usb_evk_getpc_mcu(TL_ModeTypdef.EVK,ChipType);
		}
		BurningEVKBusy = 0; 
	}
	else {
		LogConsole("BurningEVK is Busy");
		return ERROR_Typdef.EBUSY;
	}
	return 0;
}
 
ResetButton.onclick = async () => {
	console.log("ResetButton");
	if(0 == BurningEVKBusy) {
		BurningEVKBusy = 1;
	    await usb_evk_reset_mcu(TL_ModeTypdef.EVK,ChipType,LoadOBJChoose);
		BurningEVKBusy = 0; 
	}
	else {
		LogConsole("BurningEVK is Busy");
		return ERROR_Typdef.EBUSY;
	}
	return 0;
}
 
ClearButton.onclick = async () => {
	 console.log("ClearButton");
 	 var ele1 = document.getElementById("Console1");
	 var ele2 = document.getElementById("Console2");
	 var ele3 = document.getElementById("Console3");
	 var ele4 = document.getElementById("Console4");
	 var ele5 = document.getElementById("Console5");
	 var ele6 = document.getElementById("Console6");
	 ele1.value = ""; 
	 ele2.value = "";
	 ele3.value = "";
	 ele4.value = "";  
	 ele5.value = "";  
	 ele6.value = "";  
};

usbConnectButton.onclick = async () => {
	//if(USBConnectedFlag == -1) {
		if(0 == BurningEVKBusy) {
			BurningEVKBusy = 1;
			await usb_connect();
			BurningEVKBusy = 0;
		}
		else {
			LogConsole("BurningEVK is Busy");
			return ERROR_Typdef.EBUSY;
		}
	/*}
	else {
		LogConsole("USB is Linked");
		layer.msg ("USB is Linked");
	}*/
	return 0;
}
 
DownloadButton.onclick = async () => {
	await LogConsole("DownloadButton","backstage");
	if(0 == BurningEVKBusy) {
		BurningEVKBusy = 1;
		if(TL_ObjTypdef.FLASH == LoadOBJChoose) {
			await BinDownload(g_file_data,g_file_length,hex2int(DownloadAddrInput.value));
		}
		else if(TL_ObjTypdef.CORE == LoadOBJChoose) {
			await BinDownload(g_file_data,g_file_length,hex2int(SramDownloadAddrInput.value));
		}
 		BurningEVKBusy = 0;
	}
	else {
		LogConsole("BurningEVK is Busy");
		return ERROR_Typdef.EBUSY;
	}
	return 0;
}

FwDownloadButton1.onclick = async () => {
	if(0 == BurningEVKBusy) {
		BurningEVKBusy = 1;
 		await DownloadBin(CHIP_8266,"./bin/fw/V3.5.bin","BurningEVKFirmware");
 	}
	else {
		LogConsole("BurningEVK is Busy");
		return ERROR_Typdef.EBUSY;
	}
	return 0;
}

FwDownloadButton2.onclick = async () => {
	if(0 == BurningEVKBusy) {
		BurningEVKBusy = 1;
 		await DownloadBin(CHIP_8266,"./bin/fw/V3.6.bin","BurningEVKFirmware");
 	}
	else {
		LogConsole("BurningEVK is Busy");
		return ERROR_Typdef.EBUSY;
	}
	return 0;
}

MultiDownloadButton.onclick = async () => {
	await LogConsole("MultiDownloadButton","backstage");
	if(USBConnectedFlag != 1) {
		await LogConsole("USB device not connected!");
		return ERROR_Typdef.EPERM;
	}
	if(0 == BurningEVKBusy) {
		BurningEVKBusy = 1;
		var ret = -1;
		var show = "";
 		if(true == MultiCheck1.checked) {
			if(MultiBinFileLen1 > 0) {
 				ret = await BinDownload(MultiBinFileData1,MultiBinFileLen1,hex2int(DownloadAddrInput1.value),"1");
				if(0 == ret) {
					await LogConsole("Address1 Download Success");
					await sleep(20);
					show += "\r\n FileName:  "+  MultiBinFileName1 +"  adr1:  0x"+DownloadAddrInput1.value+"  size:  " + MultiBinFileLen1 +"B\r\n";
				}
				else {
					await LogConsole("Address1 Download Filed!");
					await LogConsole(" Download  terminated!");
					return ERROR_Typdef.EAGAIN;
				}
			} 
			else {
				await LogConsole("Skip, File not selected");
			}
		}
    	if(false == MultiCheck1.checked && true == MultiCheck6.checked && 0 == NetWorkBusy) {
			NetWorkBusy = 1;
		    await DownloadBin(ChipType,ServerFilePath[0],ServerFileAttribute[0],DownloadAddrInput1.value);
			await sleep(20);
 			show += "\r\n FileName:  "+  ServerFileName[0] +"  adr1:  0x"+DownloadAddrInput1.value+"  size:  " + ServerFileLength[0] +"B\r\n";
 		}
		while(NetWorkBusy) {
			await sleep(2);
 		}
		if(true == MultiCheck2.checked) {
			if(MultiBinFileLen2 > 0) {
				ret = 	await BinDownload(MultiBinFileData2,MultiBinFileLen2,hex2int(DownloadAddrInput2.value),"1");
				if(0 == ret) {
					await LogConsole("Address2 Download Success");
					await sleep(20);
					show += "\r\n FileName:  "+  MultiBinFileName2 +"  adr2:  0x"+DownloadAddrInput2.value+"  size:  " + MultiBinFileLen2 +"B\r\n";
				}
				else {
					await LogConsole("Address2 Download Filed!");
					await LogConsole(" Download  terminated!");
					return ERROR_Typdef.EAGAIN;
				}
			} 
			else {
				await LogConsole("Skip, File not selected");
			}
		}

		if(false == MultiCheck2.checked && true == MultiCheck7.checked && 0 == NetWorkBusy) {
		   NetWorkBusy = 1;
		   await DownloadBin(ChipType,ServerFilePath[1],ServerFileAttribute[1],DownloadAddrInput2.value);
		   await sleep(20);
 		   show += "\r\n FileName:  "+  ServerFileName[1] +"  adr2:  0x"+DownloadAddrInput2.value+"  size:  " + ServerFileLength[1] +"B\r\n";
 		}
		while(NetWorkBusy) {
			await sleep(2);
 		}
		if(true == MultiCheck3.checked) {
			if(MultiBinFileLen3 > 0) {
				ret = await BinDownload(MultiBinFileData3,MultiBinFileLen3,hex2int(DownloadAddrInput3.value),"1");
				if(0 == ret) {
					await LogConsole("Address3 Download Success");
					await sleep(20);
					show += "\r\n FileName:  "+  MultiBinFileName3 +"  adr3:  0x"+DownloadAddrInput3.value+"  size:  " + MultiBinFileLen3 +"B\r\n";
				}
				else {
					await LogConsole("Address3 Download Filed!");
					await LogConsole(" Download  terminated!");
					return ERROR_Typdef.EAGAIN;
				}
			} 
			else {
				await LogConsole("Skip, File not selected");
			}
		}

		if(false == MultiCheck3.checked && true == MultiCheck8.checked && 0 == NetWorkBusy) {
		   NetWorkBusy = 1;
		   await DownloadBin(ChipType,ServerFilePath[2],ServerFileAttribute[2],DownloadAddrInput3.value);
		   await sleep(20);
 		   show += "\r\n FileName:  "+  ServerFileName[2] +"  adr3:  0x"+DownloadAddrInput3.value+"  size:  " + ServerFileLength[2] +"B\r\n";
	   }
	    while(NetWorkBusy) {
			await sleep(2);
 		}
		if(true == MultiCheck4.checked) {
			if(MultiBinFileLen4 > 0) {
				ret = await BinDownload(MultiBinFileData4,MultiBinFileLen4,hex2int(DownloadAddrInput4.value),"1");
				if(0 == ret) {
					await LogConsole("Address4 Download Success");
					await sleep(20);
					show += "\r\n FileName:  "+  MultiBinFileName4 +"  adr4:  0x"+DownloadAddrInput4.value+"  size:  " + MultiBinFileLen4 +"B\r\n";
				}
				else {
					await LogConsole("Address4 Download Filed!");
					await LogConsole(" Download  terminated!");
					return ERROR_Typdef.EAGAIN;
				}
			} 
			else {
				await LogConsole("Skip, File not selected");
			}
		}
 
		if(false == MultiCheck4.checked && true == MultiCheck9.checked && 0 == NetWorkBusy) {
		   NetWorkBusy = 1;
		   await DownloadBin(ChipType,ServerFilePath[3],ServerFileAttribute[3],DownloadAddrInput4.value);
		   await sleep(20);
 		   show += "\r\n FileName:  "+  ServerFileName[3] +"  adr4:  0x"+DownloadAddrInput4.value+"  size:  " + ServerFileLength[3] +"B\r\n";
	   }
	   while(NetWorkBusy) {
			await sleep(2);
 		}
		if(true == MultiCheck5.checked) {
			if(MultiBinFileLen5 > 0) {
				ret = await BinDownload(MultiBinFileData5,MultiBinFileLen5,hex2int(DownloadAddrInput5.value),"1");
				if(0 == ret) {
					await LogConsole("Address5 Download Success");
					await sleep(20);
					show += "\r\n FileName:  "+  MultiBinFileName5 +"  adr5:  0x"+DownloadAddrInput5.value+"  size:  " + MultiBinFileLen5 +"B\r\n";
				}
				else {
					await LogConsole("Address5 Download Filed!");
					await LogConsole(" Download  terminated!");
					return ERROR_Typdef.EAGAIN;
				}
			} 
			else {
				await LogConsole("Skip, File not selected");
			}
		}

		if(false == MultiCheck5.checked && true == MultiCheck10.checked && 0 == NetWorkBusy) {
		   NetWorkBusy = 1;
		   await DownloadBin(ChipType,ServerFilePath[4],ServerFileAttribute[4],DownloadAddrInput5.value);
		   await sleep(20);
 		   show += "\r\n FileName:  "+  ServerFileName[4] +"  adr5:  0x"+DownloadAddrInput5.value+"  size:  " + ServerFileLength[4] +"B\r\n";
	   }
	    while(NetWorkBusy) {
			await sleep(2);
 		}
		if(show != "") {
			await LogConsole(show + "Download Success");
			if(1 == AutoResetFlag) {
				await LogConsole("Automatic reset after download");
				await usb_evk_reset_mcu(TL_ModeTypdef.EVK,ChipType,LoadOBJChoose);
			}
 		}
		else {
			await LogConsole("no file selected!");
		}
 		BurningEVKBusy = 0;
	}
	else {
		LogConsole("BurningEVK is Busy");
		return ERROR_Typdef.EBUSY;
	}
	return 0;
}

