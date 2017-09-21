/**************************************
*This file is function of Spectrometer
*
*
**************************************/
#include "box.h"
#include <windows.h>
#include <iostream>
#include <sstream>
#include <iomanip>
#include "FirstechNMR.h"
using namespace std;
extern SoftWareApp theApp;
void Spectrometer::SInitSample(int sampleSize,int realnoSamples,int noEchoes)
{

	unsigned int fileIndex, boardIndex,i;
	start_chNo = 0;
	//samplebuf already allcate but need more size,so samplesize only growth	
	if (!samplebuf || sampleSize > theApp.lastSampleBufSize)
	{
		if (samplebuf) delete[] samplebuf;
		samplebuf = new char[sampleSize];
		memset(samplebuf, 0, sampleSize);
		theApp.lastSampleBufSize = sampleSize;
	}
	//0 complex data
	samplebuf[0] = b_index;
	samplebuf[1] = 0;
	//theApp.samplebuf[2] = 1;
	ConfigReg(&(samplebuf[3]), noEchoes, realnoSamples);
	//clear all file
	dmaAllFile.str("");
	for (i = 0; i < 33; i++)
		tagConfiged[i] = false;
	for (i = 0; i < 33; i++)
		reiveData[i].clear();
	for (fileIndex = 0; fileIndex<RX_SOURCE_CHANNEL_NUM; fileIndex++)
	{
		for (boardIndex = 0; boardIndex < RX_SOURCE_BOARD_NUM; boardIndex++)
		{
			recInfo[fileIndex][boardIndex].sampleStart = false;
			recInfo[fileIndex][boardIndex].sampleCount = 0;
		}
	}
	receiveChs = 0;
	iTotal = 0;
	lineNum = 0;
	fifoIRecevieNum = 0;
	fifoQRecevieNum = 0;
	alreadyClosed = false;

	header[0] = b_index;
	
	ConfigQuiry(&header[2], theApp.lines);
	ConfigReg(&(header[6]), noEchoes, realnoSamples);
	singleChannelSize = noEchoes*realnoSamples * 6 * theApp.lines;

}
void Spectrometer::SCloseDmaFile()
{
	unsigned int fileIndex;

	if (dmaFile.is_open())
		dmaFile.close();
	//if (dmarawfile.is_open())
	//	dmarawfile.close();
#ifdef  REC_DATA
	if (recData.is_open())
	{
		recData.close();
	}
#endif
	for (fileIndex = 0; fileIndex<(RX_SOURCE_CHANNEL_NUM / 2 * RX_SOURCE_BOARD_NUM + 1); fileIndex++)
	{
		if (fileFifoWrite[fileIndex].is_open())
		{
			fileFifoWrite[fileIndex].close();
			theApp.PrintToLogNC("file Closed", NULL, 0);
		}
	}

}
Spectrometer::Spectrometer()
{
	channel_lw_receive = false;
	channel_hi_receive = false;
	receiveData = new char[BUF_STANDARD_SIZE];
	pch = new char[ONE_PACK_MAX_SIZE];
	dmaDataProcess = false;
	queryPreempOrChannel = false;
	querySingleReg = false;
	queryFPGAInfo = false;
	samplebuf = NULL;
	processEnable = false;
	//AUTO RESET ,INITIAL STATE IS NO SIGNAL 
	syncEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	readsyncEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	m_sample = new char[SINGLE_DATA_MAX];
	boxChecked = false;
}
Spectrometer::Spectrometer(Spectrometer&& other)
{
	receiveData = other.receiveData;
	channel_lw_receive = other.channel_lw_receive;
	channel_hi_receive = other.channel_hi_receive;
	dmaDataProcess = other.dmaDataProcess;
	queryPreempOrChannel = other.queryPreempOrChannel;
	querySingleReg = other.querySingleReg;
	queryFPGAInfo = other.queryFPGAInfo;
	samplebuf = other.samplebuf;
	processEnable = other.processEnable;
	//AUTO RESET ,INITIAL STATE IS NO SIGNAL 
	syncEvent = other.syncEvent;
	readsyncEvent = other.readsyncEvent;
	m_sample = other.m_sample;
	boxChecked = other.boxChecked;
	pch = other.pch;
	strncpy_s(armAppVer, 20, "unknown", 7);
	strncpy_s(armDriverVer, 20, "unknown", 7);
	
	other.samplebuf = nullptr;
	other.m_sample = nullptr;
	other.syncEvent = nullptr;
	other.readsyncEvent = nullptr;
	other.pch = nullptr;
}
Spectrometer::~Spectrometer()
{
	
	if (samplebuf)
	{
		delete[] samplebuf;
	}
	if (m_sample)
	{
		delete[] m_sample;
	}
	if (pch)
	{
		delete[] pch;
	}
	if (syncEvent)
	{
		CloseHandle(syncEvent);
	}
	if (readsyncEvent)
	{
		CloseHandle(readsyncEvent);
	}
	califile.close();

}
DWORD WINAPI  Spectrometer::ReadPortThread(LPVOID lpParameter)
{
	DWORD dwLength = 0;
	//int i=0;

	Spectrometer *sbox = (Spectrometer *)lpParameter;
	char* recvBuf = new char[REV_BUF];
	//Debug Thread
	//PrintStr("runing from thread");
	while (1)
	{
	
		if (sbox->m_hReadThread == NULL) break;
		//PrintStr("runing from thread1");
		//sprintf_s(str,"param sockclient:%d,recv:%d,buf:%d", sockClient, recvBuf, REV_BUF);
		//PrintStr(str);
		dwLength = recv(sbox->m_SockClient, recvBuf, REV_BUF, 0);
		//	sprintf_s(str,"recv length:%d", dwLength);
		//	PrintStr(str);
		if (dwLength > 0 && dwLength != -1)
		{
			if (VERBOSE_LEVEL2 == theApp.verboseLevel)
			{
				theApp.PrintToLog("接收到 " + sbox->boxname + " ARM数据：", recvBuf, dwLength);
			}
			//cout << "GOOD:" << sockClient << "recBuf"<<(int)recvBuf << endl;
			sbox->ProcRevMsg(recvBuf, dwLength);
		}

		else if (dwLength == -1)
		{
			sbox->connect_status = 1;
			FormatOutPut(PRINT_LEVEL_ERROR, "recv error ocurr %d\n", GetLastError());
			HandleAbnormal(sbox->b_index);
			//safe close
			if (sbox->m_SockClient != INVALID_SOCKET)
			{
				closesocket(sbox->m_SockClient);
				sbox->m_SockClient = INVALID_SOCKET;
			}
			sbox->m_SockClient = INVALID_SOCKET;
			CloseHandle(sbox->m_hReadThread);
			break;
		}
	}
	delete[] recvBuf;
	return 0;
}
void Spectrometer::ProcRevMsg(char *buf, unsigned int  length)
{
	unsigned int i, j;
	unsigned int packLength = 0;
	unsigned int totalLength = 0;
	int  fileIndex;
	ostringstream dmaFileName, debugInfo, dmaRawFileName, printtemp;
	unsigned int regCount, updataTotal, updataNow, dataCounts;
	int fifoData, chNo, boardNo, chInt, index, chIndex, currentDataNum = 0;
	boardType bt;
	
	int offset = 0;
	//char fifoDataStr[15];
	char dma_data_save[4], other_info;
	LPCTSTR writeAddr;
	int writeLen;
	string califileName;
	for (i = 0; i<length; i++)
	{
		m_Buf[(m_rBuf_Tail + i) % REV_BUF_LEN] = buf[i];
	}
	m_rBuf_Tail = (m_rBuf_Tail + length) % REV_BUF_LEN;

	if (m_rBuf_Tail < m_rBuf_Head)
		totalLength = m_rBuf_Tail - m_rBuf_Head + REV_BUF_LEN;
	else
		totalLength = m_rBuf_Tail - m_rBuf_Head;

	while (totalLength>6)
	{
		if ((m_Buf[m_rBuf_Head % REV_BUF_LEN] == '$')
			&& (m_Buf[(m_rBuf_Head + 1) % REV_BUF_LEN] == 'E')
			&& (m_Buf[(m_rBuf_Head + 2) % REV_BUF_LEN] == 'R')
			&& (m_Buf[(m_rBuf_Head + 3) % REV_BUF_LEN] == 'S'))
		{
			packLength = (BYTE)m_Buf[(m_rBuf_Head + 5) % REV_BUF_LEN];
			packLength <<= 8;
			packLength += (BYTE)m_Buf[(m_rBuf_Head + 6) % REV_BUF_LEN];

			if (packLength <= totalLength)
			{


				//char *pch = (char*)malloc(packLength);
				for (i = 0; i<packLength; i++)
					pch[i] = m_Buf[(m_rBuf_Head + i) % REV_BUF_LEN];
				if (VERBOSE_LEVEL2 == theApp.verboseLevel)
				{
					theApp.PrintToLog("设置响应信息：", pch, packLength);
				}

				char checkSum = theApp.CheckSum(pch, packLength - 1);
				if (checkSum == pch[packLength - 1])
				{
					if (pch[4] == AD9520_CFG)
					{
						SetEvent(syncEvent);
						if (pch[10] == 0x00)
						{
							single_reg_value = 0x1;
							theApp.PrintToLog(boxname+" AD9520 config failed", NULL, 0);
						}
						else
						{
							single_reg_value = 0x0;
							theApp.PrintToLog(boxname + " AD9520 config success", NULL, 0);
						}
					}
					else if (pch[4] == DUMP_ARM_FILE)
					{
							//write the data to file
						califileName = "calis_" + GetCurrentTimeFormat1()+".cali";
						califile.open(califileName, ios::app | ios::out);
						if (!califile.is_open())
						{
							PrintStr(PRINT_LEVEL_ERROR, "open calis failed\n");
						}
						else
						{
							califile.write(&pch[10], packLength-11);
							califile.close();
						}
					}
						

				}
				else
				{
					theApp.PrintToLog("设置响应校验和错误！", NULL, 0);
				}

				m_rBuf_Head = (m_rBuf_Head + packLength) % REV_BUF_LEN;
				//		free(pch);

			}
			else
				return;
		}
		else if ((m_Buf[m_rBuf_Head % REV_BUF_LEN] == '$')
			&& (m_Buf[(m_rBuf_Head + 1) % REV_BUF_LEN] == 'U')
			&& (m_Buf[(m_rBuf_Head + 2) % REV_BUF_LEN] == 'P')
			&& (m_Buf[(m_rBuf_Head + 3) % REV_BUF_LEN] == 'D'))
		{
			packLength = (BYTE)m_Buf[(m_rBuf_Head + 5) % REV_BUF_LEN];
			packLength <<= 8;
			packLength += (BYTE)m_Buf[(m_rBuf_Head + 6) % REV_BUF_LEN];

			if (packLength <= totalLength)
			{


				//char *pch = (char*)malloc(packLength);
				for (i = 0; i<packLength; i++)
					pch[i] = m_Buf[(m_rBuf_Head + i) % REV_BUF_LEN];
				char checkSum = theApp.CheckSum(pch, packLength - 1);
				if (checkSum == pch[packLength - 1])
				{
					if (pch[4] == ERASING_STATUS)
					{
						updataStatus = pch[10];
						if (updataStatus == 1)
							//transfer code 
							updataStatus += 100;
					}
					else if (pch[4] == BRUNING_STATUS)
					{
						updataTotal = (unsigned int)((unsigned char)pch[10]) << 24
							| (unsigned int)((unsigned char)pch[11]) << 16
							| (unsigned int)((unsigned char)pch[12]) << 8
							| (unsigned int)((unsigned char)pch[13]) << 0;
						updataNow = (unsigned int)((unsigned char)pch[14]) << 24
							| (unsigned int)((unsigned char)pch[15]) << 16
							| (unsigned int)((unsigned char)pch[16]) << 8
							| (unsigned int)((unsigned char)pch[17]) << 0;
						updataStatus = (float)updataNow / (float)updataTotal * 100;
						if (!updataStatus)
							updataStatus = 1;
					}
					else
					{
						theApp.PrintToLog("UPD 二级消息出错！", NULL, 0);
					}
				}
				else
				{
					theApp.PrintToLog("设置响应校验和错误！", NULL, 0);
				}

				m_rBuf_Head = (m_rBuf_Head + packLength) % REV_BUF_LEN;
				//		free(pch);

			}
			else
				return;
		}
		else if ((m_Buf[m_rBuf_Head % REV_BUF_LEN] == '$')
			&& (m_Buf[(m_rBuf_Head + 1) % REV_BUF_LEN] == 'A')
			&& (m_Buf[(m_rBuf_Head + 2) % REV_BUF_LEN] == 'V')
			&& (m_Buf[(m_rBuf_Head + 3) % REV_BUF_LEN] == 'R'))
		{
			packLength = (BYTE)m_Buf[(m_rBuf_Head + 5) % REV_BUF_LEN];
			packLength <<= 8;
			packLength += (BYTE)m_Buf[(m_rBuf_Head + 6) % REV_BUF_LEN];

			if (packLength <= totalLength)
			{


				//char *pch = (char*)malloc(packLength);
				for (i = 0; i<packLength; i++)
					pch[i] = m_Buf[(m_rBuf_Head + i) % REV_BUF_LEN];
				/*
				if (VERBOSE_LEVEL2 == theApp.verboseLevel)
				{

				PrintToLog("设置响应信息：", pch, packLength);
				}
				*/
				char checkSum = theApp.CheckSum(pch, packLength - 1);
				if (checkSum == pch[packLength - 1])
				{
					strncpy_s(armAppVer, 20, &pch[10], 4);
					strncpy_s(armDriverVer, 20, &pch[14], 4);
					SetEvent(readsyncEvent);
				}
				else
				{
					theApp.PrintToLog("设置响应校验和错误！", NULL, 0);
				}

				m_rBuf_Head = (m_rBuf_Head + packLength) % REV_BUF_LEN;
				//		free(pch);

			}
			else
				return;
		}
		else if ((m_Buf[m_rBuf_Head % REV_BUF_LEN] == '$')
			&& (m_Buf[(m_rBuf_Head + 1) % REV_BUF_LEN] == 'F')
			&& (m_Buf[(m_rBuf_Head + 2) % REV_BUF_LEN] == 'S')
			&& (m_Buf[(m_rBuf_Head + 3) % REV_BUF_LEN] == 'R'))
		{
			packLength = (BYTE)m_Buf[(m_rBuf_Head + 5) % REV_BUF_LEN];
			packLength <<= 8;
			packLength += (BYTE)m_Buf[(m_rBuf_Head + 6) % REV_BUF_LEN];
			if (packLength <= totalLength)
			{


				for (i = 0; i<packLength; i++)
					pch[i] = m_Buf[(m_rBuf_Head + i) % REV_BUF_LEN];

				char checkSum = theApp.CheckSum(pch, packLength - 1);
				if (checkSum == pch[packLength - 1])
				{
					if (queryFPGAInfo && (buf[10] == queryLowAddr))
					{
						offset = 0;
						strncpy_s(&receiveData[offset], BUF_STANDARD_SIZE, &pch[11], packLength - 12);
						SetEvent(readsyncEvent);
					}
					else
					{
						theApp.PrintToLog("获取的不是查询的slot！", NULL, 0);
					}
				}
				else
				{
					theApp.PrintToLog("设置响应校验和错误！", NULL, 0);
				}

				m_rBuf_Head = (m_rBuf_Head + packLength) % REV_BUF_LEN;


			}
			else
				return;
		}
		else if ((m_Buf[m_rBuf_Head % REV_BUF_LEN] == '$')
			&& (m_Buf[(m_rBuf_Head + 1) % REV_BUF_LEN] == 'E')
			&& (m_Buf[(m_rBuf_Head + 2) % REV_BUF_LEN] == 'R')
			&& (m_Buf[(m_rBuf_Head + 3) % REV_BUF_LEN] == 'R'))

		{
			//读取寄存器信息
			packLength = (BYTE)m_Buf[(m_rBuf_Head + 5) % REV_BUF_LEN];
			packLength <<= 8;
			packLength += (BYTE)m_Buf[(m_rBuf_Head + 6) % REV_BUF_LEN];

			if (packLength <= totalLength)
			{
				for (i = 0; i<packLength; i++)
					pch[i] = m_Buf[(m_rBuf_Head + i) % REV_BUF_LEN];
				if (VERBOSE_LEVEL2 == theApp.verboseLevel)
				{

					theApp.PrintToLog("读取结果信息：", pch, packLength);
				}


				//	char checkSum =CheckSum(pch,packLength-1);
				// don;'t check 
				if (1)
				{
					reg_info_t reginfo;
					//direct is 0x01
					if (pch[4] == 0x01)
					{
						regCount = (packLength - 11) / REG_INFO_WITH;

						for (i = 0; i<regCount; i++)
						{
							reginfo.regaddr = (unsigned int)((unsigned char)pch[10 + i*REG_INFO_WITH + 0]) << 24
								| (unsigned int)((unsigned char)pch[10 + i*REG_INFO_WITH + 1]) << 16
								| (unsigned int)((unsigned char)pch[10 + i*REG_INFO_WITH + 2]) << 8
								| (unsigned int)((unsigned char)pch[10 + i*REG_INFO_WITH + 3]) << 0;

							reginfo.regval = (unsigned int)((unsigned char)pch[10 + i*REG_INFO_WITH + 4]) << 24
								| (unsigned int)((unsigned char)pch[10 + i*REG_INFO_WITH + 5]) << 16
								| (unsigned int)((unsigned char)pch[10 + i*REG_INFO_WITH + 6]) << 8
								| (unsigned int)((unsigned char)pch[10 + i*REG_INFO_WITH + 7]) << 0;


							// 
							/*
							if(theApp.verboseLevel)
							{
							sprintf_s(charstr,100,"地址：%x,数值：%x\n",reginfo.regaddr,reginfo.regval);
							PrintToLog("读取寄存器结果：",charstr,strlen(charstr));

							}
							*/
							if (pch[9] == BAT_READ)
							{

							}
							//
							if (queryPreempOrChannel)
							{
								if (reginfo.regaddr == queryLowAddr)
								{
									channel_lw_receive = true;
									channel_value_low = reginfo.regval;
								}
								if (reginfo.regaddr == queryHighAddr)
								{
									channel_hi_receive = true;
									channel_value_high = reginfo.regval;
								}
								if (channel_lw_receive&&channel_hi_receive)
									SetEvent(syncEvent);
							}
							if (querySingleReg)
							{

								if (reginfo.regaddr == queryLowAddr)
								{
									single_reg_value = reginfo.regval & 0xFFFF;
									SetEvent(syncEvent);
								}
							}

							if (reginfo.regaddr == MAIN_CTRL_WAITE_STATE_REG)
							{

								m_WaitState = reginfo.regval;
							}
							else
							{
								//strtemp.Format(_T("%x"),reginfo.regval);
								//m_RegPage1Dlg.m_Ctrl_Reg_Val.SetWindowText(strtemp);
							}
							// 

						}

					}
				
					else if (pch[4] == DIRECT_BAT_READ)
					{
						offset = 0;
						regCount = (packLength - 11) / REG_INFO_WITH;

						for (i = 0; i < regCount; i++)
						{
							reginfo.regaddr = (unsigned int)((unsigned char)pch[10 + i*REG_INFO_WITH + 0]) << 24
								| (unsigned int)((unsigned char)pch[10 + i*REG_INFO_WITH + 1]) << 16
								| (unsigned int)((unsigned char)pch[10 + i*REG_INFO_WITH + 2]) << 8
								| (unsigned int)((unsigned char)pch[10 + i*REG_INFO_WITH + 3]) << 0;

							reginfo.regval = (unsigned int)((unsigned char)pch[10 + i*REG_INFO_WITH + 4]) << 24
								| (unsigned int)((unsigned char)pch[10 + i*REG_INFO_WITH + 5]) << 16
								| (unsigned int)((unsigned char)pch[10 + i*REG_INFO_WITH + 6]) << 8
								| (unsigned int)((unsigned char)pch[10 + i*REG_INFO_WITH + 7]) << 0;
							if (queryFPGAInfo)
							{
								receiveData[offset] = pch[10 + i*REG_INFO_WITH + 6];
								receiveData[offset + 1] = pch[10 + i*REG_INFO_WITH + 7];
								offset += 2;
								if (i == regCount - 1)
									SetEvent(readsyncEvent);
								//check reg addr
								/*
								if (fpgaRegChecked)
								{
								//if char==0 stop cpoy
								//strncpy_s(&receiveData[offset], BUF_STANDARD_SIZE, &pch[10 + i*REG_INFO_WITH + 6], 2);

								offset += 2;
								if (i == regCount - 1)
								SetEvent(readsyncEvent);
								}
								else if (reginfo.regaddr == queryLowAddr)
								{
								fpgaRegChecked = true;
								}
								*/
							}
						}
					}
					else if (pch[4] == DATABUS_READ_WRITE_READ)
					{
						//总线方式读的处理
						regCount = (packLength - 19) / REG_INFO_WITH;

						reginfo.regaddr = (unsigned int)((unsigned char)pch[10]) << 24
							| (unsigned int)((unsigned char)pch[11]) << 16
							| (unsigned int)((unsigned char)pch[12]) << 8
							| (unsigned int)((unsigned char)pch[13]) << 0;
						if (reginfo.regaddr == TX1_DATABUS_ADDR_REG_ADDR || reginfo.regaddr == TX2_DATABUS_ADDR_REG_ADDR || queryFPGAInfo)
						{
							if (reginfo.regaddr == TX1_DATABUS_ADDR_REG_ADDR)
								bt = TX1;
							else bt = TX2;
							for (i = 0; i < regCount; i++)
							{
								reginfo.regaddr = (unsigned int)((unsigned char)pch[18 + i*REG_INFO_WITH + 0]) << 24
									| (unsigned int)((unsigned char)pch[18 + i*REG_INFO_WITH + 1]) << 16
									| (unsigned int)((unsigned char)pch[18 + i*REG_INFO_WITH + 2]) << 8
									| (unsigned int)((unsigned char)pch[18 + i*REG_INFO_WITH + 3]) << 0;
								reginfo.regval = (unsigned int)((unsigned char)pch[18 + i*REG_INFO_WITH + 4]) << 24
									| (unsigned int)((unsigned char)pch[18 + i*REG_INFO_WITH + 5]) << 16
									| (unsigned int)((unsigned char)pch[18 + i*REG_INFO_WITH + 6]) << 8
									| (unsigned int)((unsigned char)pch[18 + i*REG_INFO_WITH + 7]) << 0;
								if (((reginfo.regaddr & 0xFFF) == 0x5) && (reginfo.regval & 0x3800))
									theApp.reConfigDAC(reginfo.regaddr, bt);

							}

						}
						else if (reginfo.regaddr == rx_cmd_addr)
						{
							rc_dc_value =
								(int)((char)pch[24]) << 8
								| (unsigned int)((unsigned char)pch[25]) << 0;
							SetEvent(readsyncEvent);
							/*
							//process
							switch(pch[16]&0xF0)
							{

							case ADC1_SEL:
							//ShowADC_RegConfigInfo(m_pMainFrame->box1Rx1Adc1Dlg);

							break;
							case ADC2_SEL:
							//ShowADC2_RegConfigInfo(m_pMainFrame->box1Rx1Adc2Dlg);
							break;
							case ADC3_SEL:
							//ShowADC3_RegConfigInfo(m_pMainFrame->box1Rx1Adc3Dlg);
							break;
							case ADC4_SEL:
							//	ShowADC4_RegConfigInfo(m_pMainFrame->box1Rx1Adc4Dlg);
							break;
							case ADC5_SEL:
							//ShowADC_RegConfigInfo(m_pMainFrame->box1Rx1Adc5Dlg);

							break;
							case ADC6_SEL:
							//ShowADC_RegConfigInfo(m_pMainFrame->box1Rx1Adc6Dlg);
							break;
							case ADC7_SEL:
							//ShowADC_RegConfigInfo(m_pMainFrame->box1Rx1Adc7Dlg);
							break;
							case ADC8_SEL:
							//ShowADC_RegConfigInfo(m_pMainFrame->box1Rx1Adc8Dlg);
							break;
							}

							*/
						}

					}
					else if (pch[4] == DMA_READ)// DMA数据读取并保存
					{
						dmaDataProcess = true;
						if (!processEnable)
						{
							theApp.PrintToLogNC("process is disable", NULL, 0);							
							goto end;
						}
						/*
						if (!dmaFile.is_open())
						{
						PrintStr("dma File is not open");
						goto end;
						}
						*/
						if ((0 == theApp.sampleType) && samplebuf == NULL)
						{
							PrintStr(PRINT_LEVEL_DEBUG, "sample is NULL");
							theApp.PrintToLogNC("sample is NULL", NULL, 0);
							goto end;
						}
						dataCounts = (packLength - 11); // 根据通信协议，获取消息包中的数据个数，每个数据是16位
						if (dataCounts % 4)
						{
							PrintStr(PRINT_LEVEL_DEBUG, "DMA中的数据不是4的倍数");
							theApp.PrintToLogNC("DMA中的数据不是4的倍数", NULL, 0);
							goto end;
						}
#ifdef ANALYSIS_DMA_DATA
						//	char* dma_temp_buf = new char[dataCounts];
						if (!dmaFile.is_open())
						{
							dmaRawFileName << "dma_raw" << runtime << ".raw";
							dmaFile.open(dmaRawFileName.str(), ios::out | ios::binary);
						}
#endif
						int low_index = 10, high_index = 10 + (dataCounts >> 1);
						//dmarawfile.write(&pch[10], dataCounts);	
						for (i = 0; i < dataCounts; low_index += 2, high_index += 2, i += 4)
						{
#ifdef ANALYSIS_DMA_DATA
							//analysis data
							dma_temp_buf[i] = pch[high_index + 1];
							dma_temp_buf[i + 1] = pch[high_index];
							dma_temp_buf[i + 2] = pch[low_index + 1];
							dma_temp_buf[i + 3] = pch[low_index];
#endif

							fifoData =
								(int)((char)pch[high_index]) << 16
								| (unsigned int)((unsigned char)pch[low_index + 1]) << 8
								| (unsigned int)((unsigned char)pch[low_index]) << 0;

							other_info = pch[high_index + 1];
							chNo = (other_info >> 2) & 0x0000003F;
							if (chNo > 32 || chNo == 0)
							{
								PrintStr(PRINT_LEVEL_DEBUG, "invalid recv data chNo must be in [1,32]");
								//CloseDmaFile();
								continue;
							}
							boardNo = (other_info)& 0x3;
							chInt = chNo / 2 + chNo % 2;
							if (chInt > 8) //debug
							{
								chIndex = boardNo * 8 + (chInt - 8);
								if (theApp.raw_data_format)
									fifoData = fifoData & 0x80FFFFFF;
								goto DEBUG_RAW;
							}
							else
							{
								chIndex = boardNo * 8 + chInt;
								if (theApp.raw_data_format)
									fifoData = fifoData & 0xFFFFFF;

							}
							if (theApp.sampleType)
							{
								if (chNo != sampleChannel || sampleBoard != boardNo)
								{
									theApp.PrintToLog("warning adc sample chNo or boardNo not match", NULL, 0);
									continue;
								}

								m_sample[sampleIndex++] = (fifoData >> 16) & 0xFF;
								m_sample[sampleIndex++] = (fifoData >> 8) & 0xFF;
								sampleTotal++;
								if (sampleIndex == SINGLE_DATA_MAX || sampleTotal == theApp.noSamples)
								{
									//show
									currentDataNum = (sampleIndex - 12) / 2;
									m_sample[0] = b_index;
									m_sample[1] = 1;
									m_sample[2] = 1;
									m_sample[3] = 0;
									m_sample[4] = 0;
									m_sample[5] = 0;
									m_sample[6] = 1;
									m_sample[7] = (currentDataNum >> 24) & 0xFF;
									m_sample[8] = (currentDataNum >> 16) & 0xFF;
									m_sample[9] = (currentDataNum >> 8) & 0xFF;
									m_sample[10] = currentDataNum & 0xFF;
									m_sample[11] = chIndex;
									Display(&m_sample, sampleIndex);

									//save file
									if (!adcfile.is_open())
									{
										dmaFileName << theApp.outputFileName << "\\m_board" << boardNo << "_ch" << chInt << theApp.runtime << ".raw";
										adcfile.open(dmaFileName.str(), ios::out | ios::binary);
										//clear data
										dmaFileName.str("");
										//theApp.PrintToLogNC("open dac file",NULL,0);
										adcfile.write(theApp.fileSaveTag, 8);
										adcfile.seekp(256, ios::cur);
									}
									//printtemp.str("");
									//printtemp << "write count:" << (sampleIndex - 12);
									//PrintStr(PRINT_LEVEL_ERROR, printtemp.str().c_str());
									adcfile.write(&m_sample[12], sampleIndex - 12);

									if (sampleIndex == SINGLE_DATA_MAX)
									{
										sampleIndex = 12;
									}
									if (sampleTotal == theApp.noSamples)
									{
										sampleTotal = 0;
										ReceiveEnvironmentClear(b_index);
										adcfile.close();
										//theApp.PrintToLogNC("close adc file", NULL, 0);
										processEnable = false;
									}
								}

							}
							else
							{
								recInfo[chNo - 1][boardNo].sampleCount++;
								if (recInfo[chNo - 1][boardNo].sampleCount == 1)
								{
									recInfo[chNo - 1][boardNo].sampleStart = true;
								}

								//set prediscard per sample start
								if (recInfo[chNo - 1][boardNo].sampleStart)
								{
									recInfo[chNo - 1][boardNo].preNOD = theApp.preDiscard;
									recInfo[chNo - 1][boardNo].sampleStart = false;
								}
								if (recInfo[chNo - 1][boardNo].preNOD)
								{
									recInfo[chNo - 1][boardNo].preNOD--;
									continue;
								}
								if (recInfo[chNo - 1][boardNo].sampleCount > theApp.noSamples - theApp.lastDiscard)
								{
									if (recInfo[chNo - 1][boardNo].sampleCount == theApp.noSamples)
									{
										recInfo[chNo - 1][boardNo].sampleCount = 0;
									}
									continue;
								}
								if (recInfo[chNo - 1][boardNo].sampleCount == theApp.noSamples)
								{
									recInfo[chNo - 1][boardNo].sampleCount = 0;
								}


								/*
								qsampleCount++;
								if (qsampleCount == 1)
								{
								qsampleStart = true;
								}

								if (qsampleStart)
								{
								qpreNOD = preDiscard;
								qsampleStart = false;
								}
								if (qpreNOD)
								{
								qpreNOD--;
								continue;
								}
								if (qsampleCount > noSamples - lastDiscard)
								{
								if (qsampleCount == noSamples)
								{
								qsampleCount = 0;
								}
								continue;
								}
								if (qsampleCount == noSamples)
								{
								qsampleCount = 0;
								}
								}
								*/

								//fifoData = fifoData & 0x00ffffff;
								//calc ch


								if (!tagConfiged[chIndex])
								{
									receiveChs++;
									//set i and q index
									if (chNo % 2) //I first
									{

										recInfo[chNo - 1][boardNo].index = 11 + (receiveChs - 1)*theApp.singleSampleNum * 6 + receiveChs;
										recInfo[chNo][boardNo].index = 14 + (receiveChs - 1)*theApp.singleSampleNum * 6 + receiveChs;
										recInfo[chNo - 1][boardNo].backupIndex = recInfo[chNo - 1][boardNo].index;
										recInfo[chNo][boardNo].backupIndex = recInfo[chNo][boardNo].index;
									}
									else        //Q first
									{
										recInfo[chNo - 1][boardNo].index = 14 + (receiveChs - 1)*theApp.singleSampleNum * 6 + receiveChs;
										recInfo[chNo - 2][boardNo].index = 11 + (receiveChs - 1)*theApp.singleSampleNum * 6 + receiveChs;
										recInfo[chNo - 1][boardNo].backupIndex = recInfo[chNo - 1][boardNo].index;
										recInfo[chNo - 2][boardNo].backupIndex = recInfo[chNo - 2][boardNo].index;
									}
									samplebuf[10 + (receiveChs - 1)*theApp.singleSampleNum * 6 + receiveChs] = chIndex;//boardNo * 8 + chInt;
									tagConfiged[chIndex] = true;
								}
								/**/
								if (chNo % 2) //Idata
								{
									fifoIRecevieNum++;
									//recInfo[chNo - 1][boardNo].index += ConfigSampleData(&samplebuf[recInfo[chNo - 1][boardNo].index], fifoData);
								}
								else         //Qdata
								{
									fifoQRecevieNum++;

								}
								index = recInfo[chNo - 1][boardNo].index;
								if (index > recInfo[chNo - 1][boardNo].backupIndex + (theApp.singleSampleNum - 1) * 6)
								{
									PrintStr(PRINT_LEVEL_ERROR, "dma packet receive not correct");
									processEnable = false;
									goto end;
								}
								recInfo[chNo - 1][boardNo].index += ConfigSampleData(&samplebuf[index], fifoData);
								//judge if buffer overflow

								//if (fifoData >= 0x800000)
								//fifoData = fifoData - 0x1000000;
								//strLen = itoaline(fifoData, fifoDataStr);

								//ch = chInt + 0x30;
								//boardNoChar = boardNo + 0x30;

								// 							
							DEBUG_RAW:
								if (!fileFifoWrite[chIndex].is_open())
								{
									if (theApp.outputFilePrefix.length())
									{

										dmaFileName << theApp.outputFileName << "\\" << theApp.outputFilePrefix << "000_";
										dmaFileName << setfill('0') << setw(3) << start_chNo << ".raw";

										start_chNo++;
									}
									else
										dmaFileName << theApp.outputFileName << "\\" << theApp.outputFilePrefix<<boxname<< "_board" << boardNo << "_ch" << chInt << theApp.runtime << ".raw";

									fileFifoWrite[chIndex].open(dmaFileName.str(), ios::out | ios::binary);
									dmaAllFile << dmaFileName.str() << ":";
									//clear data
									dmaFileName.str("");
									AppendParam(chIndex);
									fileFifoWrite[chIndex].write("_V2", 3);
									//seek to 64K 
									fileFifoWrite[chIndex].seekp(65536, ios::beg);
									fileFifoWrite[chIndex].write(theApp.fileSaveTag, 8);
									fileFifoWrite[chIndex].seekp(256, ios::cur);
								}


								if (theApp.saveMode == DONT_SAVE)
								{
									//ConfigQuiry(dma_data_save, fifoData);
									//fileFifoWrite[chIndex].write(dma_data_save, 4);
								}
								else
								{
									reiveData[chIndex].push_back(fifoData);
								}
								//fileFifoWrite[chInt][boardNo].write(fifoDataStr, strLen);
								/**/
								//if (fifoIRecevieNum == sampleNum*receiveChs&&fifoQRecevieNum == sampleNum*receiveChs&&!setupMode)
								//	CloseDmaFile();
								if (fifoIRecevieNum == theApp.singleSampleNum*receiveChs&&fifoQRecevieNum == theApp.singleSampleNum*receiveChs)
								{
#ifdef  REC_DATA
									//for test samplebuf
									if (recData.is_open())
									{
										recData.write(samplebuf, singleSampleNum * 6 * receiveChs + 11 + receiveChs);
										//recData.close();
									}
#endif
									//end
									/*
									for (j = 0; j < receiveChs; j++)
									{

									chIndex = samplebuf[singleSampleNum * 6 * j + 11 + j];
									fileFifoWrite[chIndex].write(&samplebuf[singleSampleNum * 6 * j + 12 + j], singleSampleNum * 6);
									}
									*/

									//fileFifoWrite[chIndex].flush();
									iTotal += theApp.singleSampleNum;
									samplebuf[2] = receiveChs;
									
									header[1] = receiveChs;
									theApp.onelinesize = singleChannelSize*receiveChs + 11 + receiveChs;
									//Display(&samplebuf, theApp.singleSampleNum * 6 * receiveChs + 11 + receiveChs);
									if (theApp.linesset)
									{
										if (theApp.firstdata)
										{
										 writeAddr = theApp.GetRTBuff() + theApp.triggerNum*theApp.onelinesize;
										 writeLen = 11 * sizeof(char);
										 if (!theApp.CheckRTAddr(writeAddr, writeLen))
										 {
											 theApp.status = 2;
											 theApp.linesset = false;
											 break;
										 }
										 CopyMemory((PVOID)writeAddr, samplebuf,11*sizeof(char));
											for (i = 0; i < receiveChs; i++)
											{
												ADDR[i] = writeAddr + 11 + i*singleChannelSize + i;
												writeLen = (theApp.singleSampleNum * 6 + 1) * sizeof(char);
												if (!theApp.CheckRTAddr(ADDR[i], writeLen))
												{
													theApp.status = 2;
													theApp.linesset = false;
													break;
												}
												CopyMemory((PVOID)ADDR[i], &samplebuf[11+i*theApp.singleSampleNum * 6+i], writeLen);
											}
											theApp.firstdata = false;
										}
										else
										{
											for (i = 0; i < receiveChs; i++)
											{
												ADDR[i] += theApp.singleSampleNum * 6 + 1;
												writeLen = (theApp.singleSampleNum * 6 ) * sizeof(char);
												if (!theApp.CheckRTAddr(ADDR[i], writeLen))
												{
													theApp.status = 2;
													theApp.linesset = false;
													break;
												}
												CopyMemory((PVOID)ADDR[i], &samplebuf[12 + i*theApp.singleSampleNum * 6 + i], writeLen);
											}
										}
										lineNum++;
										if (lineNum == theApp.lines)
										{
											//trigger once
											if (!theApp.tiggermode) theApp.linesset = false;
											Display((char **)&(writeAddr), theApp.singleSampleNum * 6 * receiveChs + 11 + receiveChs);
											theApp.triggerNum += 1;
											theApp.firstdata = true;
											theApp.status = 1;
											lineNum = 0;
										}
									}
									else 
										Display(&samplebuf, theApp.singleSampleNum * 6 * receiveChs + 11 + receiveChs);
									//recovery the index
									for (j = 1; j < (RX_SOURCE_CHANNEL_NUM / 2 * RX_SOURCE_BOARD_NUM + 1); j++)
									{

										if (tagConfiged[j])
										{
											boardNo = (j - 1) / 8;
											chNo = ((j - 1) % 8) * 2;
											recInfo[chNo][boardNo].index = recInfo[chNo][boardNo].backupIndex;
											recInfo[chNo + 1][boardNo].index = recInfo[chNo + 1][boardNo].backupIndex;
										}
									}
									fifoIRecevieNum = 0;
									fifoQRecevieNum = 0;
									if (iTotal == theApp.sampleNum)
									{
										//AppendParam();
										//first append end
										for (fileIndex = 0; fileIndex < (RX_SOURCE_CHANNEL_NUM / 2 * RX_SOURCE_BOARD_NUM + 1); fileIndex++)
										{
											if (fileFifoWrite[fileIndex].is_open())
											{
												if (theApp.saveMode == RECEIVED_SAVE)
												{

													for (j = 0; j < reiveData[fileIndex].size(); j++)
													{
														ConfigQuiry(dma_data_save, reiveData[fileIndex].at(j));
														fileFifoWrite[fileIndex].write(dma_data_save, 4);
													}
												}
												fileFifoWrite[fileIndex] << "END";
											}
										}
										SCloseDmaFile();
										alreadyClosed = true;
										ReceiveEnvironmentClear(b_index);
										//callback
										PrintF(dmaAllFile.str().c_str());
										dmaAllFile.str("");
										processEnable = false;
										theApp.lock = false;
										iTotal = 0;
										goto ExitNoraml;

									}
								}
							}
						}
#ifdef ANALYSIS_DMA_DATA
						dmaFile.write(dma_temp_buf, dataCounts);
#endif
					end:
						//Aha
						/*
						if (!processEnable&&!alreadyClosed)
						{
							SCloseDmaFile();
							alreadyClosed = true;
						}
						*/
					ExitNoraml:
						dmaDataProcess = false;

					}
				}
				else
				{
					if (VERBOSE_LEVEL2 == theApp.verboseLevel)
					{
						theApp.PrintToLog("读取响应校验和错误！", NULL, 0);
					}
				}
				m_rBuf_Head = (m_rBuf_Head + packLength) % REV_BUF_LEN;
				// 				//free(pch);
				// 				
			}
			else
				return;
		}
		////////////////////////////////////////////////////////////////
		else
		{
			m_rBuf_Head = (m_rBuf_Head + 1) % REV_BUF_LEN;
		}
		if (m_rBuf_Tail < m_rBuf_Head)
			totalLength = m_rBuf_Tail - m_rBuf_Head + REV_BUF_LEN;
		else
			totalLength = m_rBuf_Tail - m_rBuf_Head;

		// _cprintf("before for %d\n",GetTickCount());
		// for(i=0;i<2000000000;i++);
		// _cprintf("after for %d\n",GetTickCount());
		// totalLength = 0;
	}
}
void Spectrometer::AppendParam(int fileIndex)
{

	string filename, varname, strtemp;
	char str[40];
	map<std::string, paramInfo>::iterator iter;
	paramInfo pi;
	varBoxInfo vb;
	if (fileFifoWrite[fileIndex].is_open())
	{

		fileFifoWrite[fileIndex] << ":PAR\t" << theApp.parFileName << endl;
		fileFifoWrite[fileIndex] << ":SRC\t" << theApp.srcfilename << endl;
		for (iter = theApp.paramMap.begin(); iter != theApp.paramMap.end(); iter++)
		{
			varname = iter->first;
			pi = iter->second;
			if (pi.pt == TYPE_DOUBLE)
			{
				strtemp = "double";
				sprintf_s(str, "%f ", pi.value.doublelit);
			}
			else if (pi.pt == TYPE_INT)
			{
				strtemp = "int";
				sprintf_s(str, "%d ", pi.value.intlit);
			}
			fileFifoWrite[fileIndex] << ":" << strtemp << "\t" << varname << "," << str;
			fileFifoWrite[fileIndex] << endl;
		}
		fileFifoWrite[fileIndex] << ":float" << "\t" << "SysFreq" << "," << theApp.SYSFREQ << endl;
		//fileFifoWrite[fileIndex] << ":int" << "\t" << "preDiscard" << "," << preDiscard << endl;
		//fileFifoWrite[fileIndex] << ":int" << "\t" << "lastDiscard" << "," << lastDiscard<<endl;
		fileFifoWrite[fileIndex] << "PARAMEND";
	}
}