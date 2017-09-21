/**************************************
*This file is used for offer the core function 
*for Firstech NMR software.Include network connect
*data process...
*
*FUCTION            AUTHER            DATA
*first created      yuan,jian         2015.11.30
*Support ddr 16*32bit		
*if gradient/tx bram 
*is not module of 16*32bit
*padd with 0 and write 0x48 to 1              YJ          2016.4.14
*gradient_clr_reg set to 0x3f then 0 add clrSeq          YJ          2016.4.18
*seq start write 0 then write 1               YJ          2016.4.19
*
****************************************/
#define DLL_IMPLENT
#include <string>

#include <time.h>
#include <string.h>

#include <iostream>
#include <iomanip>
//#include <Windows.h>
#include <Winsock2.h>
#include <assert.h>
#include <process.h>
#include <math.h>
#include <sstream>
#include <io.h>
#include "server.h"
#include "server/connectors/tcpsocketserver.h"
#include "FirstechNMR.h"
#include "mridll.h"
#include "ErrorNo.h"
#include "matrix.h"
#define round(r) (((r)>0.0)?floor((r)+0.5):ceil((r)-0.5))
#pragma comment(lib,"Ws2_32.lib")
#define SIO_KEEPALIVE_VALS  _WSAIOW(IOC_VENDOR, 4)
#define BUF_SIZE (1024*1024) 
#define RT_BUF_SIZE (16*1024*1024) 
TCHAR szName[] = TEXT("Local\\BuffMappingObject");
TCHAR rtName[] = TEXT("Local\\RTBuffMappingObject");

//major ,minor ,patch
//1.1.4 add function SetFilter
//1.1.5 change the filename of the rawdata 
//      and the heart-beat to detect the server lost
//1.1.6 update the procv_ thread 
//      add the RegisterAbnormal() interface
//1.1.7 add init SetRxAtt in ConfigFile func
//2.0.0 the new ege for support multibox extend application
//      change the many functions to adapt the multi box
//      split the Spectrometer class in box.cpp
//2.0.1  fix can't update the fpga code to extend box
//2.0.2  fix SetRFWaves and ParseMapping function don't return error when the file is not exist
//2.0.3  change MAX_MATRIX_NUM from 256 to 4K
//		 set the 9520 setup  file to init  
//		 extend to unlimited box previous  limited to 5 boxs
//       gradient wave change lut base addr to 26 bit
//       solve the Run- Abort Dead-Lock problem caused by CloseDmaFile...
//       solve pause is not working problem
//       change box receive thread temp pch buffer location
//3.0.0  change the ARCH of the function call use json-RPC 2.0 version
//	     and par contains all the params seq needs
//		 make run more safe ,one can run only when the Last run is Abort or exit normally
//		 add the err log and optimise the output 
//		 change save mode 0 to don't save mode for fast recon
//       change receive data format @2017.6.17 
#define DLL_VER  "3.0.0"
#define DLL_VER_MAJOR 3
#define DLL_VER_MINOR 0
#define DLL_VER_PATH  0
#define MAX_LINE_CHAR 300
using namespace std; 
using namespace jsonrpc;
SoftWareApp theApp;
//This is used for print str to User Interface
//the default charset is GBK
static void (*printStr)(int level,const char *str)=NULL;
static void(*printFiles)(const char *str) = NULL;
static void(*displayData)(char **str,int size) = NULL;


string m_strLogInfo ="";
//reg_config_info_t m_DacRegConfigInfo[DAC_MAX_REG];

int  control_regs[]={GRADIENT_CONTROL_REG,TX1_CONTROL_REG,\
TX2_CONTROL_REG,RX1_CONTROL_REG,RX2_CONTROL_REG,RX3_CONTROL_REG,RX4_CONTROL_REG,MAIN_CTRL_CONTROL_REG}; 
int  seq_clr_regs[] = { MAIN_CTRL_SEQ_CLR,GRADIENT_SEQ_CLR_REG,TX1_SEQ_CLR_REG,TX2_SEQ_CLR_REG,\
RX1_SEQ_CLR_REG, RX2_SEQ_CLR_REG, RX3_SEQ_CLR_REG, RX4_SEQ_CLR_REG };
int  ctrl_clr_regs[] = { TX_B1_CTRL_REG ,TX_B2_CTRL_REG,RX_B1_CTRL_REG,RX_B2_CTRL_REG ,RX_B3_CTRL_REG,\
RX_B4_CTRL_REG, SYS_CTRL_REG };
int led_regs[] = { SYS_READY_LED ,LINK_STATUS_LED};
//错误编号
int errorno;
gradientParamRAM gpr[GRADIENT_MAX_PARAM];
int paramCount;
static int error = 0;
const char  * WINAPI GetDLLVersion()
{
	return DLL_VER;
}
const wchar_t  * WINAPI GetDLLVersionW()
{
	return 0;
//	return TEXT(DLL_VER);
}

std::vector<std::string> split(std::string str, std::string pattern)
{
	std::string::size_type pos;
	std::vector<string> result;
	str += pattern;
	unsigned int i;
	for (i = 0; i < str.size(); i++)
	{
		if ((pos = str.find(pattern, i)))
		{
			result.push_back(str.substr(i, pos - i));
			i = pos + pattern.size() - 1;
		}
	}
	return result;
}
int GetLastErr()
{
	return errorno;
}
void  FormatOutPut(int level, const char *format, ...)
{
	char buff[1024];
	va_list list;
	va_start(list, format);
	vsnprintf(buff, 1024, format, list);
	PrintStr(level, buff);
	va_end(list);
}
string LevelToString(int level)
{
	switch (level)
	{
	case 0:
		return "debug";
	case 1:
		return "info";
	case 2:
		return "warn";
	case 3:
		return "error";
	default:
		return "Impossible";
	}
}
void  PrintStr(int level,char *str)
{
	theApp.message = str;
	if (printStr)
	{
		printStr(level, theApp.message.c_str());
	}
	else
	{
		//fprintf(stderr, "%s\n", str);
		theApp.errfile << "[" << GetCurrentTimeFormat() << "] ";
		theApp.errfile << "[" << LevelToString(level) << "] ";
		theApp.errfile << " " << str << endl;
	}
}
int SingleSample(int box, int boardno, int channel,int samplePoints)
{


	int point_l,adc_off_en,rx_start,regCount=0,iWrittenBytes;
	if (theApp.boxs[box].dmaDataProcess)
	{
		PrintStr(PRINT_LEVEL_ERROR, "sample data is processing,wait");
		//while (theApp.dmaDataProcess)
		//	Sleep(1000);
		return ERROR_IN_PROCESS;
		
	}
	if (theApp.boxs[box].sampleTotal)
	{
		PrintStr(PRINT_LEVEL_ERROR, "last sample not end");
		return ERROR_IN_PROCESS;
	}
	ReceiveEnvironmentSet(box);
	//wait data clr  100ms
	Sleep(100);
	switch (boardno)
	{
	case RX1:
		point_l = (RX1_CH1_RX_POINT_L + channel * 2) * 2;
		adc_off_en = RX1_ADC_OFF_EN * 2;
		theApp.rx_bypass = RX1_RECEIVE_BYPASS * 2;
		rx_start = RX1_RECEIVE_START * 2;
		break;
	case RX2:
		point_l = (RX2_CH1_RX_POINT_L + channel * 2) * 2;
		adc_off_en = RX2_ADC_OFF_EN * 2;
		theApp.rx_bypass = RX2_RECEIVE_BYPASS * 2;
		rx_start = RX2_RECEIVE_START * 2;
		break;
	case RX3:
		point_l = (RX3_CH1_RX_POINT_L + channel * 2) * 2;
		adc_off_en = RX3_ADC_OFF_EN * 2;
		theApp.rx_bypass = RX3_RECEIVE_BYPASS * 2;
		rx_start = RX3_RECEIVE_START * 2;
		break;
	case RX4:
		point_l = (RX4_CH1_RX_POINT_L + channel * 2) * 2;
		adc_off_en = RX4_ADC_OFF_EN * 2;
		theApp.rx_bypass = RX4_RECEIVE_BYPASS * 2;
		rx_start = RX4_RECEIVE_START * 2;
		break;
	default:
		PrintStr(PRINT_LEVEL_ERROR, "set rx sample not correct,board no incorrect");
		return ERROR_IN_PROCESS;
	}
	
	char* buf = new char[BUF_STANDARD_SIZE];
	memcpy(buf, "$ETS", 4);
	buf[4] = DATABUS_READ_WRITE_READ;
	buf[7] = DUMMY_CMD;
	buf[8] = DUMMY_CMD;
	buf[9] = DUMMY_CMD;
	regCount = 10;
	regCount+=ConfigReg(&buf[10], point_l, samplePoints&0xFFFF);
	regCount += ConfigReg(&buf[regCount], point_l+2, samplePoints >>16);
	regCount += ConfigReg(&buf[regCount], adc_off_en, 0xFF );
	regCount += ConfigReg(&buf[regCount], theApp.rx_bypass, 0xFF);
	regCount += ConfigReg(&buf[regCount], rx_start, 1<< channel);
	regCount++;
	buf[5] = HIBYTE(regCount);
	buf[6] = LOBYTE(regCount);

	buf[regCount - 1] = theApp.CheckSum(buf, regCount - 1);
	iWrittenBytes = theApp.Send(box, buf, regCount);
	if (iWrittenBytes == SOCKET_ERROR)
	{
		delete[] buf;
		return ERROR_IN_PROCESS;
		
	}
	theApp.sampleType = 1;
	theApp.boxs[box].sampleChannel = channel*2+1;
	theApp.boxs[box].sampleBoard = boardno - RX1;
	theApp.fileSaveTag[3] = 1;
	theApp.fileSaveTag[4] = samplePoints >> 24;
	theApp.fileSaveTag[5] = samplePoints >> 16;
	theApp.fileSaveTag[6] = samplePoints >> 8;
	theApp.fileSaveTag[7] = samplePoints;
	theApp.noSamples = samplePoints;
	theApp.boxs[box].sampleTotal = 0;
	theApp.boxs[box].sampleIndex = 12;
	theApp.boxs[box].processEnable = true;
	SetTime();
	theApp.PrintToLog("发送单次采集命令：", buf, iWrittenBytes);
	delete[] buf;
	
	return RIGHT_IN_PROCESS;
	
}
int SetRxDCOffset(int box, int boardno)
{
	int i,point_l, adc_off_en, regCount = 0, iWrittenBytes, rx_bypass,rx_dc_cali;
	
	
	for (i = 0; i < 8; i++)
	{
		if (SetRxATT(box, boardno, i, 31, 0, 0, 0, 0x720))
		{
			FormatOutPut(PRINT_LEVEL_ERROR,"发送第 %d 个通道断开出错", i + 1);
		}
	}
	switch (boardno)
	{
	case RX1:
		point_l = RX1_CH1_RX_POINT_L * 2;
		adc_off_en = RX1_ADC_OFF_EN * 2;
		rx_bypass = RX1_RECEIVE_BYPASS * 2;
		rx_dc_cali = RX1_DC_CALI * 2;
		break;
	case RX2:
		point_l = RX2_CH1_RX_POINT_L * 2;
		adc_off_en = RX2_ADC_OFF_EN * 2;
		rx_bypass = RX2_RECEIVE_BYPASS * 2;
		rx_dc_cali = RX2_DC_CALI * 2;
		break;
	case RX3:
		point_l = RX3_CH1_RX_POINT_L * 2;
		adc_off_en = RX3_ADC_OFF_EN * 2;
		rx_bypass = RX3_RECEIVE_BYPASS * 2;
		rx_dc_cali = RX3_DC_CALI * 2;
		break;
	case RX4:
		point_l = RX4_CH1_RX_POINT_L * 2;
		adc_off_en = RX4_ADC_OFF_EN * 2;
		rx_bypass = RX4_RECEIVE_BYPASS * 2;
		rx_dc_cali = RX4_DC_CALI * 2;
		break;
	default:
		PrintStr(PRINT_LEVEL_ERROR, "set rx cali not correct,board no incorrect");
		return ERROR_IN_PROCESS;
	}

	char* buf = new char[BUF_STANDARD_SIZE];
	memcpy(buf, "$ETS", 4);
	buf[4] = DATABUS_READ_WRITE_READ;
	buf[7] = DUMMY_CMD;
	buf[8] = DUMMY_CMD;
	buf[9] = DUMMY_CMD;
	regCount = 10;
	for (i = 0; i < 8; i++, point_l+=4)
	{
		regCount += ConfigReg(&buf[regCount], point_l, 0);
		regCount += ConfigReg(&buf[regCount], point_l + 2, 0);
	}
	regCount += ConfigReg(&buf[regCount], rx_bypass, 0xFF);
	regCount += ConfigReg(&buf[regCount], adc_off_en, 0xFF);
	regCount += ConfigReg(&buf[regCount], rx_dc_cali, 0xFF);
	regCount++;
	buf[5] = HIBYTE(regCount);
	buf[6] = LOBYTE(regCount);

	buf[regCount - 1] = theApp.CheckSum(buf, regCount - 1);
	iWrittenBytes = theApp.Send(box, buf, regCount);
	if (iWrittenBytes == SOCKET_ERROR)
	{
		delete[] buf;
		return ERROR_IN_PROCESS;

	}

	theApp.PrintToLog(BoxToString(box)+" 发送单次接收直流校准命令：", buf, iWrittenBytes);
	delete[] buf;
	return RIGHT_IN_PROCESS;

}
int GetRxDCOffset(int box,int boardno,int channel)
{
	int rx_dc_reg, regCount, rx_cmd_addr, iWrittenBytes;
	char boardNo;
	DWORD waitReturn;
	switch (boardno)
	{
	case RX1:
		rx_dc_reg = RX1_C1_ADC_DC_ADJ;
		boardNo = RX1_BOARD;
		rx_cmd_addr = RX1_B1_WR_CMD*2;
		break;
	case RX2:
		rx_dc_reg = RX2_C1_ADC_DC_ADJ;
		boardNo = RX2_BOARD;
		rx_cmd_addr = RX2_B1_WR_CMD*2;
		break;
	case RX3:
		rx_dc_reg = RX3_C1_ADC_DC_ADJ;
		boardNo = RX3_BOARD;
		rx_cmd_addr = RX3_B1_WR_CMD*2;
		break;
	case RX4:
		rx_dc_reg = RX4_C1_ADC_DC_ADJ;
		boardNo = RX4_BOARD;
		rx_cmd_addr = RX4_B1_WR_CMD*2;
		break;
	default:
		PrintStr(PRINT_LEVEL_ERROR, "get rx cali not correct,board no incorrect");
		return ERROR_IN_PROCESS;
	}
	if (channel < 0 || channel>7)
	{
		PrintStr(PRINT_LEVEL_ERROR, "get rx cali not correct,channel no incorrect");
		return ERROR_IN_PROCESS;
	}
	theApp.boxs[box].rx_cmd_addr = rx_cmd_addr;
	theApp.boxs[box].rx_dc_reg = (rx_dc_reg + channel) * 2;
	char* buf = new char[BUF_STANDARD_SIZE];
	memcpy(buf, "$ETR", 4);
	buf[4] = DATABUS_READ_WRITE_READ;


	buf[7] = boardNo;
	buf[8] = DUMMY_CMD;
	buf[9] = DUMMY_CMD;
	regCount = 10;
	regCount += ConfigReg(&buf[regCount], rx_cmd_addr, 0x8000| rx_dc_reg);
	regCount++;
	buf[5] = HIBYTE(regCount);
	buf[6] = LOBYTE(regCount);

	buf[regCount - 1] = theApp.CheckSum(buf, regCount - 1);
	iWrittenBytes = theApp.Send(box, buf, regCount);
	if (iWrittenBytes == SOCKET_ERROR)
	{
		delete[] buf;
		return ERROR_IN_PROCESS;
	}

	theApp.PrintToLog("发送获取接收直流校准命令：", buf, iWrittenBytes);
	delete[] buf;
	waitReturn = WaitForSingleObject(theApp.boxs[box].readsyncEvent, 2000);
	if (waitReturn == WAIT_OBJECT_0)
	{
		//return dc value
		return theApp.boxs[box].rc_dc_value;
	}
	else if (waitReturn == WAIT_TIMEOUT)
	{
		errorno = EREADTIMEOUT;
		PrintStr(PRINT_LEVEL_DEBUG, "read time out");
		return REG_IMPOSSIBLE;
	}
	else if (waitReturn == WAIT_FAILED)
	{
		errorno = EREADFAILED;
		PrintStr(PRINT_LEVEL_DEBUG, "read failed");
		return REG_IMPOSSIBLE;
	}
	return REG_IMPOSSIBLE;
}
/****************************
*
*2016/6/22 add downloading the file to FPGA compatiable with old style
****************************/
int DownFile(int box, char *filename)
{
	ifstream file(filename, ios::in|ios::binary);
	if (file)
	{
		int length, packNum,totalPacks, LastPack, singlePad, i, iWrittenBytes;
		/// [2byte pack index][2byte total]
		char *buf = new char[65535];
		singlePad = 65535 - 11 - 4;
		file.seekg(0, file.end);
		length = file.tellg();
		file.seekg(0, file.beg);
		packNum = length / singlePad;
		LastPack = length % singlePad;
		//total pack
		totalPacks = LastPack ? packNum + 1 : packNum;
		//first down file name
		for (i = 1; i <= packNum; i++)
		{
			memcpy(buf, "$WTF", 4);
			buf[4] = DUMMY_CMD;
			buf[5] = 0xFF;
			buf[6] = 0xFF;
			buf[7] = DUMMY_CMD;
			buf[8] = DUMMY_CMD;
			buf[9] = DUMMY_CMD;
			ConfigQuiry(&buf[10], (i << 16) | totalPacks);

			file.read(&buf[14], singlePad);
			if (!file)
			{
				theApp.PrintToLogNC("error while read file", NULL, 0);
				file.close();
				delete[] buf;
				return  ERROR_IN_PROCESS;
			}
			buf[PACK_MAX_SIZE - 1] = theApp.CheckSum(buf, PACK_MAX_SIZE - 1);
			iWrittenBytes = theApp.Send(box, buf, PACK_MAX_SIZE);
			theApp.PrintToLog("发送更新文件指令：", buf, iWrittenBytes);
		}
		if (LastPack)
		{
			memcpy(buf, "$WTF", 4);
			buf[4] = DUMMY_CMD;
			buf[5] = HIBYTE(LastPack + 15);
			buf[6] = LOBYTE(LastPack + 15);
			buf[7] = DUMMY_CMD;
			buf[8] = DUMMY_CMD;
			buf[9] = DUMMY_CMD;
			ConfigQuiry(&buf[10], (totalPacks << 16) | totalPacks);
			file.read(&buf[14], LastPack);
			if (!file)
			{
				theApp.PrintToLogNC("error while read file", NULL, 0);
				file.close();
				delete[] buf;
				return ERROR_IN_PROCESS;
			}
			buf[LastPack + 14] = theApp.CheckSum(buf, LastPack + 14);

			iWrittenBytes = theApp.Send(box, buf, LastPack + 15);
			theApp.PrintToLog("发送更新文件指令：", buf, iWrittenBytes);
		}

		file.close();
		delete[] buf;
		return  RIGHT_IN_PROCESS;
	}
	else
	{
		theApp.PrintToLogNC("down file open failed", NULL, 0);
		return ERROR_IN_PROCESS;
	}
	
}
int UpdateFPGA(int bx, int slot, char *filename)
{
	if (slot < 0 || slot>5)
	{
		PrintStr(PRINT_LEVEL_ERROR, "slot must in [0,5]");
		return ERROR_IN_PROCESS;
	}
	int iWrittenBytes,regCount = 0;
	char *p,*back;
	char *pos;
	char* buf = new char[BUF_STANDARD_SIZE];
	pos = strrchr(filename,'\\');
	if (pos)
		back = pos + 1;
	else back = filename;
	//stupid code
	memcpy(buf, "$WTF", 4);
	buf[4] = 0xA;
	buf[7] = DUMMY_CMD;
	buf[8] = DUMMY_CMD;
	buf[9] = DUMMY_CMD;
	regCount = 10;
	p = back;
	while (*p)
		buf[regCount++] = *p++;
	//buf[regCount++] = '\\';
	regCount++;
	buf[5] = HIBYTE(regCount);
	buf[6] = LOBYTE(regCount);

	buf[regCount - 1] = theApp.CheckSum(buf, regCount - 1);
	iWrittenBytes = theApp.Send(bx, buf, regCount);
	if (iWrittenBytes == SOCKET_ERROR)
	{
		delete[] buf;
		return ERROR_IN_PROCESS;

	}
	theApp.PrintToLog("发送更新文件名命令：", buf, iWrittenBytes);
	if (DownFile(bx, filename))
	{
		PrintStr(PRINT_LEVEL_ERROR, "slot must in [0,5]");
		return ERROR_IN_PROCESS;
	}
	memcpy(buf, "$UPD", 4);
	buf[4] = FPGA_UPDATE;
	buf[7] = DUMMY_CMD;
	buf[8] = DUMMY_CMD;
	buf[9] = DUMMY_CMD;
	buf[10] = slot & 0xFF;
	
	regCount = 11;
	p = back;
	while (*p)
		buf[regCount++] = *p++;
	buf[regCount++] = '\\';
	regCount++;
	buf[5] = HIBYTE(regCount);
	buf[6] = LOBYTE(regCount);

	buf[regCount - 1] = theApp.CheckSum(buf, regCount - 1);
	iWrittenBytes = theApp.Send(bx, buf, regCount);
	if (iWrittenBytes == SOCKET_ERROR)
	{
		delete[] buf;
		return ERROR_IN_PROCESS;

	}
	theApp.PrintToLog("发送更新FPGA代码命令：", buf, iWrittenBytes);
	theApp.boxs[bx].updataStatus = -1;
	delete[] buf;
	return RIGHT_IN_PROCESS;
}
int  GetUpdataSatus(int box)
{
	return theApp.boxs[box].updataStatus;
}

void RegisterPrintStr(void (*callback)(int level,const char *))
{
  printStr=callback;
}
void  RegisterImageP(void(*callback)(const char *))
{
	printFiles = callback;

}
void RegisterDataRecv(void(*callback)(char **,int ))
{
	displayData = callback;
}
void(*handleAbnormal)(int boxType, char *jasonstr)=NULL;

void  RegisterAbnormal(void(*callback)(int, char *))
{
	handleAbnormal = callback;
}
void Display(char **buf,int size)
{
	if (displayData)
	{
		//PrintStr("Before data display");
		displayData(buf, size);
		//PrintStr("end data display");
	}
}
void HandleAbnormal(int boxindex)
{
	if (handleAbnormal)
		handleAbnormal(boxindex,NULL);

}
void PrintF(const char *files)
{
	if (printFiles)
		printFiles(files);

}
int  ConfigSampleData(char *buf,int data)
{
	buf[0] = data >> 16;
	buf[1] = data >> 8;
	buf[2] = data & 0xFF;
	return 6;
}
int Parse9520Stp(int bt,const char *filename)
{

	string readline, substr, substr1;
	string::size_type pos;
	ifstream file(filename, ios::in);
	reg_info_t regValue;
	DWORD waitReturn;
	//4 *N  regs
	vector<reg_info_t> ad9520_reg;
	unsigned int i = 0, regCount, iWrittenBytes;
	vector<string> result;
	// 0: initial 1 find addr  
	int state=0;
	if (!file.is_open())
	{
		PrintStr(PRINT_LEVEL_DEBUG, "文件打开失败");
		return ERROR_IN_PROCESS;
	}
	const char *p;
	while (getline(file, readline))
	{
		if (!readline.compare(0, 1, "#") || readline.empty())
			continue;
		switch (state)
		{
		case 0:
			if ((pos = readline.find("Addr")) != string::npos)
				state = 1;
			break;
		case 1:
			result=split(readline, ",");
			if (!readline.compare(0, 2, "\"\""))
			{
				state = 2;
				break;
			}
			p = result[0].c_str();
			p++;
			regValue.regaddr = strtol(p, NULL, 16);
			p = result[2].c_str();
			p++;
			regValue.regval = strtol(p, NULL, 16);
			ad9520_reg.push_back(regValue);
			break;
		default :
			break;
		}
		if (state == 2)
			break;
		
	}
	file.close();
	//config the regs
	char* buf = new char[BUF_STANDARD_SIZE];
	memcpy(buf, "$ETS", 4);
	buf[4] = AD9520_CFG;
	buf[7] = DUMMY_CMD;
	buf[8] = DUMMY_CMD;
	buf[9] = DUMMY_CMD;
	//write X
	regCount = 10;
	for (i = 0; i < ad9520_reg.size(); i++)
	{
		regValue = ad9520_reg.at(i);
		regCount += ConfigReg(&buf[regCount], regValue.regaddr, regValue.regval);
	}
	regCount++;
	buf[5] = HIBYTE(regCount);
	buf[6] = LOBYTE(regCount);

	buf[regCount - 1] = theApp.CheckSum(buf, regCount - 1);
	iWrittenBytes = theApp.Send(bt, buf, regCount);
	theApp.PrintToLog(BoxToString(bt)+" 发送设置9520配置指令：", buf, iWrittenBytes);
	delete[] buf;

	waitReturn = WaitForSingleObject(theApp.boxs[bt].syncEvent, 2000);
	if (waitReturn == WAIT_OBJECT_0)
	{
		return theApp.boxs[bt].single_reg_value;
	}
	else if (waitReturn == WAIT_TIMEOUT)
	{
		PrintStr(PRINT_LEVEL_DEBUG, "read time out");
		return ERROR_IN_PROCESS;
	}
	else if (waitReturn == WAIT_FAILED)
	{
		PrintStr(PRINT_LEVEL_DEBUG, "read failed");
		return ERROR_IN_PROCESS;

	}
	return RIGHT_IN_PROCESS;
}
int ParseGradInf(const char *filename)
{
	string readline,substr,substr1;
	string::size_type pos;
	ifstream file(filename,ios::in);
	reg_info_t regValue;
	//4 *N  regs
	vector<vector<reg_info_t> > gradRegs(4);
	
	//reg_config_info_t X[2]={0};
	//reg_config_info_t Y[2]={0};
	//reg_config_info_t Z[2]={0};
	//reg_config_info_t B0[2]={0};
	//reg_config_info_t *B0 = new reg_config_info_t[2];
	//reg_config_info_t *regtemp=NULL;
	unsigned int i=0,regCount,iWrittenBytes;
	bool cmdFlag=false;
	if(!file.is_open())
	{
		PrintStr(PRINT_LEVEL_DEBUG,"文件打开失败");
		return ERROR_IN_PROCESS;
	}
	while(getline(file,readline))
	{
		if(!readline.compare(0,1,"#")||readline.empty())
			continue;
		if((pos=readline.find(':'))==string::npos)
		{
		  pos=readline.find(',');
		  substr=readline.substr(0,pos);
		  substr1=readline.substr(pos+1,string::npos);
		  if (!i)
		  {
			  PrintStr(PRINT_LEVEL_DEBUG,"error in grad dac cfg file,need set channel first");
			  return ERROR_IN_PROCESS;
		  }
		  regValue.regaddr= strtol(substr.c_str(), NULL, 16);
		  regValue.regval= strtol(substr1.c_str(), NULL, 16);
		  gradRegs[i - 1].push_back(regValue);
		  if(gradRegs[i - 1].size()>4)
		  {
		  PrintStr(PRINT_LEVEL_DEBUG,"grad regs is over 4 ");
		  return ERROR_IN_PROCESS;
		  
		  }	

		  continue;
		} 
		substr=readline.substr(0,pos);
		if(!substr.compare("X"))
		{
		i=1;
		}else if(!substr.compare("Y"))
		{
		i=2;
		} 
		else  if(!substr.compare("Z"))
		{
		
		i=3;
		} else if(!substr.compare("B0"))
		{
		 i=4;
		} else 
		{
			//PrintStr("in grad file ");
			//PrintStr(filename);
			PrintStr(PRINT_LEVEL_DEBUG,"in grad file tag error tag is X,Y,Z,B0");
			return ERROR_IN_PROCESS;
					 
		}

	}
	
    file.close();
	//config the regs
	char* buf=new char[BUF_STANDARD_SIZE];
	memcpy(buf,"$ETS",4);
	buf[4] = DATABUS_READ_WRITE_READ;
	buf[7] = DUMMY_CMD;
	buf[8] = DUMMY_CMD;
	buf[9] = DUMMY_CMD;
	ConfigReg(&buf[10],GRADIENT_DAC_CFG,0xFF0);
	//write X
	regCount=18;
	for(i=0;i< gradRegs[0].size();i++)
	{
		regValue = gradRegs[0].at(i);
	   if(!cmdFlag)
	   {
         regCount+=ConfigReg(&buf[regCount],GRADIENT_X_SPI_CMD,0);
	     cmdFlag=true;
	   }
	   //first write the data reg then write the addr reg
		regCount+=ConfigReg(&buf[regCount],GRADIENT_X_SPI_DATA, regValue.regval);
		regCount+=ConfigReg(&buf[regCount],GRADIENT_X_SPI_ADDR, regValue.regaddr);
	} 

    //write Y
	cmdFlag=false;
	for(i=0; i<gradRegs[1].size();i++)
	{
		regValue = gradRegs[1].at(i);
		if(!cmdFlag)
		{
			regCount+=ConfigReg(&buf[regCount],GRADIENT_Y_SPI_CMD,0);
			cmdFlag=true;
		}
		//first write the data reg then write the addr reg
		regCount+=ConfigReg(&buf[regCount],GRADIENT_Y_SPI_DATA, regValue.regval);
		regCount+=ConfigReg(&buf[regCount],GRADIENT_Y_SPI_ADDR, regValue.regaddr);
	} 
	//write Z ...
	cmdFlag = false;
	for(i=0;i<gradRegs[2].size();i++)
	{
		regValue = gradRegs[2].at(i);
		if(!cmdFlag)
		{
			regCount+=ConfigReg(&buf[regCount],GRADIENT_Z_SPI_CMD,0);
			cmdFlag=true;
		}
		//first write the data reg then write the addr reg
		regCount+=ConfigReg(&buf[regCount],GRADIENT_Z_SPI_DATA, regValue.regval);
		regCount+=ConfigReg(&buf[regCount],GRADIENT_Z_SPI_ADDR, regValue.regaddr);
	} 
   //write to B0
	cmdFlag = false;
	for(i=0;i<gradRegs[3].size();i++)
	{
		regValue = gradRegs[3].at(i);
		//cout << "in B0" << i << endl;
		if(!cmdFlag)
		{
			regCount+=ConfigReg(&buf[regCount],GRADIENT_B0_SPI_CMD,0);
			cmdFlag=true;
		}
		//first write the data reg then write the addr reg
		regCount+=ConfigReg(&buf[regCount],GRADIENT_B0_SPI_DATA, regValue.regval);
		regCount+=ConfigReg(&buf[regCount],GRADIENT_B0_SPI_ADDR, regValue.regaddr);
	}

	regCount++;
	buf[5] = HIBYTE(regCount);
	buf[6] = LOBYTE(regCount);

	buf[regCount -1] = theApp.CheckSum(buf,regCount -1);
	iWrittenBytes = theApp.Send(M,buf,regCount);
	theApp.PrintToLog("发送设置梯度硬件参数指令：",buf,iWrittenBytes);
	delete[] buf;

	return RIGHT_IN_PROCESS;

}
int parseTxCali(const char *filename,vector<float> &a)
{
	string readline;
	ifstream file(filename, ios::in);
	if (!file.is_open())
	{
		FormatOutPut(PRINT_LEVEL_DEBUG,"%s open failed" ,filename);
		return ERROR_IN_PARSE;
	}
	
	while (getline(file, readline))
	{
		//comment
		if (!readline.compare(0, 1, "#") || readline.empty())
			continue;
		if (a.size() > 8)
		{
			PrintStr(PRINT_LEVEL_DEBUG,"发射校准文件中寄存器超过实际应有的个数");
			return ERROR_IN_PARSE;
		}
		a.push_back(atof(readline.c_str()));
	}
	file.close();
	return RIGHT_IN_PROCESS;
}
int parseRxCali(const char *filename, vector<rxCHCali> &a)
{
	string readline, substr;
	string::size_type pos,newpos;
	rxCHCali rc;
	ifstream file(filename, ios::in);
	if (!file.is_open())
	{
		FormatOutPut(PRINT_LEVEL_DEBUG, "%s open failed", filename);
		return ERROR_IN_PARSE;
	}
	while (getline(file, readline))
	{
		//comment
		if (!readline.compare(0, 1, "#") || readline.empty())
			continue;
		if (a.size() > 8)
		{
			PrintStr(PRINT_LEVEL_DEBUG,"接收校准文件中寄存器超过实际应有的个数");
			return ERROR_IN_PARSE;
		}
		pos = readline.find(',');
		substr = readline.substr(0, pos);
		rc.att = atof(substr.c_str());
		pos++;

		newpos= readline.find(',', pos);
		substr = readline.substr(pos, newpos-pos);
		rc.amp1= atof(substr.c_str());
		pos = ++newpos;

		newpos = readline.find(',', pos);
		substr = readline.substr(pos, newpos - pos);
		rc.amp2 = atof(substr.c_str());
		pos = ++newpos;

		newpos = readline.find(',', pos);
		substr = readline.substr(pos, newpos - pos);
		rc.amp3 = atof(substr.c_str());
		pos = ++newpos;

		newpos = readline.find(',', pos);
		substr = readline.substr(pos, newpos - pos);
		rc.swi = atoi(substr.c_str());
		pos = ++newpos;

		substr = readline.substr(pos, string::npos);
		rc.dc = atoi(substr.c_str());


		a.push_back(rc);
	}
	file.close();
	return RIGHT_IN_PROCESS;

}
int parseAmpFile(const char *filename, int &amp)
{
	string readline, addr, value;
	int regCount;
	ifstream file(filename, ios::in);
	if (!file.is_open())
	{

		FormatOutPut(PRINT_LEVEL_DEBUG, "%s open failed", filename);

		return ERROR_IN_PARSE;
	}
	regCount = 0;
	while (getline(file, readline))
	{
		//comment
		if (!readline.compare(0, 1, "#") || readline.empty())
			continue;
		amp= strtol(readline.c_str(), NULL, 16);
		regCount++;
	}
	file.close();
	return regCount;
}
int parseDacFile(const char *filename, reg_info_t *dacRegs,int maxRegs)
{
	string readline, addr, value;
	string::size_type pos;
	int regCount;
	ifstream file(filename, ios::in);
	if (!file.is_open())
	{
		
		FormatOutPut(PRINT_LEVEL_DEBUG, "%s open failed", filename);

		return ERROR_IN_PARSE;
	}
	regCount = 0;
	while (getline(file, readline))
	{
		//comment
		if (!readline.compare(0, 1, "#") || readline.empty())
			continue;
		if (regCount > maxRegs)
		{
			PrintStr(PRINT_LEVEL_DEBUG,"文件中寄存器超过实际应有的个数");
			file.close();
			return ERROR_IN_PARSE;
		}
		pos = readline.find(',');
		addr = readline.substr(0, pos);
		value = readline.substr(pos+1, string::npos);
		dacRegs[regCount].regaddr = strtol(addr.c_str(),NULL,16);
		dacRegs[regCount].regval = strtol(value.c_str(),NULL,16);
		regCount++;
	}
	file.close();
	return regCount;
}
void ConfigTxRegs(int bx,boardType bt,reg_info_t regs[4][47],vector<float> cali,int amp)
{
	
	int i,j,regIndex,cmdReg,dataReg,addrReg,dacCfgReg,gainSel,ducGain,compGain,
		ducGainOffset,compGainOffset, dacSel, iCompGain, ampSel,iWrittenBytes,boardIndex;
	switch (bt)
	{
	case TX1:
		cmdReg = TX1_DATABUS_CMD_REG_ADDR;
		dataReg = TX1_DATABUS_DATA_REG_ADDR;
		addrReg = TX1_DATABUS_ADDR_REG_ADDR;
		dacCfgReg = TX1_DAC_CFG;
		gainSel = TX1_GAIN_SEL;
		ampSel = TX1_PWD;
		ducGainOffset = TX1_C1_DUC_GAIN_DB;
		compGainOffset = TX1_C1_COMP_GAIN_DB;
		break;
	case TX2:
		cmdReg = TX2_DATABUS_CMD_REG_ADDR;
		dataReg = TX2_DATABUS_DATA_REG_ADDR;
		addrReg = TX2_DATABUS_ADDR_REG_ADDR;
		dacCfgReg = TX2_DAC_CFG;
		gainSel = TX2_GAIN_SEL;
		ducGainOffset = TX2_C1_DUC_GAIN_DB;
		compGainOffset = TX2_C1_COMP_GAIN_DB;
		ampSel = TX2_PWD;
		break;
	default:
		PrintStr(PRINT_LEVEL_DEBUG,"Config Tx Reg,board param Error");
		return;
	}
	boardIndex = bt - TX1;
	//firste write cali then dac per ch
	char *buf = new char[BUF_STANDARD_SIZE * 4];
	memcpy(buf, "$ETS", 4);
	buf[4] = DATABUS_READ_WRITE_READ;
	buf[7] = DUMMY_CMD;
	buf[8] = DUMMY_CMD;
	buf[9] = DUMMY_CMD;
	ConfigReg(&buf[10], ampSel, amp);
	ConfigReg(&buf[18], gainSel, 0);
	ConfigReg(&buf[26], dacCfgReg, 0);
	ConfigReg(&buf[34], dacCfgReg, 0xF00);
	//ConfigReg(&buf[18], dacCfgReg, 0xFF0);
	ConfigReg(&buf[42], cmdReg, 0);
	regIndex = 50;
	for (i = 0; i < cali.size(); i++)
	{
		iCompGain = (int)round(cali.at(i) * 32);
		ducGain = (ducGainOffset + i*2) * 2;
		compGain = (compGainOffset+i*2)*2;
		regIndex+=ConfigReg(&buf[regIndex], ducGain, 0);
		regIndex += ConfigReg(&buf[regIndex], compGain, iCompGain);
		if (i % 2)
		{
			dacSel = i / 2;
			//dacSel <<= 13;
			for (j = 0; j < theApp.dacRegsSize[boardIndex][dacSel]; j++)
			{
				regIndex += ConfigReg(&buf[regIndex], dataReg, regs[dacSel][j].regval);
				regIndex += ConfigReg(&buf[regIndex], addrReg, regs[dacSel][j].regaddr| (dacSel<<13));
			}
		}
	
	}
	
	regIndex++;
	buf[5] = HIBYTE(regIndex);
	buf[6] = LOBYTE(regIndex);

	buf[regIndex - 1] = theApp.CheckSum(buf, regIndex - 1);
	iWrittenBytes = theApp.Send(bx, buf, regIndex);
	theApp.PrintToLog(BoxToString(bx)+" 发送设置发射板硬件参数指令：", buf, iWrittenBytes);
	
	//query

	memcpy(buf, "$ETR", 4);
	buf[4] = DATABUS_READ_WRITE_READ;
	buf[7] = bt-TX1;
	buf[8] = DUMMY_CMD;
	buf[9] = DUMMY_CMD;
	//ConfigReg(&buf[10], gainSel, 0);
	//ConfigReg(&buf[18], dacCfgReg, 0);
	//ConfigReg(&buf[26], dacCfgReg, 0xF00);
	ConfigReg(&buf[10], cmdReg, 1);
	//ConfigReg(&buf[10], cmdReg, 1);
	regIndex = 18;
	for (i = 0; i < cali.size(); i++)
	{
		if (i % 2)
		{
			dacSel = i / 2;
			dacSel <<= 13;
			
			regIndex += ConfigReg(&buf[regIndex], addrReg, 0x5 | dacSel);
		
		}
	}
	regIndex++;
	buf[5] = HIBYTE(regIndex);
	buf[6] = LOBYTE(regIndex);

	buf[regIndex - 1] = theApp.CheckSum(buf, regIndex - 1);
	//iWrittenBytes = theApp.Send(M, buf, regIndex);
	//theApp.PrintToLog("发送查询发射板DAC FIFO ALARM指令：", buf, iWrittenBytes);
	delete[] buf;
}
void ConfigRxRegs(int bx, boardType bt, reg_info_t *regs, int regCount, vector<rxCHCali> cali)
{
	
	int i, regIndex, cmdReg, dataReg, addrReg, adcCfgReg,
	ampReg, rGain, rG1, rG2, rG3, rATT, rDc,
	rAmpCtrl,iG1,iG2,iG3, iAtt, iWrittenBytes;
	rxCHCali rc;
	switch (bt)
	{
	case RX1:
		cmdReg = RX_B1_SPI1_CMD;
		dataReg = RX_B1_SPI1_DATA;
		addrReg = RX_B1_SPI1_ADDR;
		adcCfgReg = RX1_ADC_CFG;
		ampReg = RX1_AMP_CFG_START;
		break;
	case RX2:
		cmdReg = RX_B2_SPI1_CMD;
		dataReg = RX_B2_SPI1_DATA;
		addrReg = RX_B2_SPI1_ADDR;
		adcCfgReg = RX2_ADC_CFG;
		ampReg = RX2_AMP_CFG_START;
		break;
	case RX3:
		cmdReg = RX_B3_SPI1_CMD;
		dataReg = RX_B3_SPI1_DATA;
		addrReg = RX_B3_SPI1_ADDR;
		adcCfgReg = RX3_ADC_CFG;
		ampReg = RX3_AMP_CFG_START;
		break;
	case RX4:
		cmdReg = RX_B4_SPI1_CMD;
		dataReg = RX_B4_SPI1_DATA;
		addrReg = RX_B4_SPI1_ADDR;
		adcCfgReg = RX4_ADC_CFG;
		ampReg = RX4_AMP_CFG_START;
		break;
	default:
		PrintStr(PRINT_LEVEL_DEBUG,"Config Rx Reg,board param Error");
		return;
	}
	char *buf = new char[BUF_STANDARD_SIZE * 2];
	memcpy(buf, "$ETS", 4);
	buf[4] = DATABUS_READ_WRITE_READ;
	buf[7] = DUMMY_CMD;
	buf[8] = DUMMY_CMD;
	buf[9] = DUMMY_CMD;
	//first reg adc 
	ConfigReg(&buf[10], adcCfgReg, 0);
	ConfigReg(&buf[18], cmdReg, 0);
	regIndex = 26;
	for (i = 0; i < regCount; i++)
	{
		regIndex += ConfigReg(&buf[regIndex], dataReg, regs[i].regval);
		regIndex += ConfigReg(&buf[regIndex], addrReg, regs[i].regaddr);
	}
	for (i = 0; i < cali.size(); i++)
	{
		rc = cali.at(i);
		rGain = (RX1_C1_GAIN_DB + i+ (bt-RX1)*0x200) * 2;
		rG1 = (RX1_C1_G1 + i+ (bt - RX1) * 0x200) * 2;
		rG2 = (RX1_C1_G2 + i+ (bt - RX1) * 0x200) * 2;
		rG3 = (RX1_C1_G3 + i+ (bt - RX1) * 0x200) * 2;
		rATT = (RX1_C1_ATT0 + i+ (bt - RX1) * 0x200) * 2;
		rDc= (RX1_C1_ADC_DC_ADJ + i + (bt - RX1) * 0x200) * 2;
		rAmpCtrl = ampReg + i * 2;
		iG1 = round(rc.amp1 * 32);
		iG2 = round(rc.amp2 * 32);
		iG3 = round(rc.amp3 * 32);
		iAtt = round(rc.att * 32);
		regIndex += ConfigReg(&buf[regIndex], rAmpCtrl, rc.swi);
		regIndex += ConfigReg(&buf[regIndex], rGain, 0);
		regIndex += ConfigReg(&buf[regIndex], rG1,iG1 );
		regIndex += ConfigReg(&buf[regIndex], rG2, iG2);
		regIndex += ConfigReg(&buf[regIndex], rG3, iG3);
		regIndex += ConfigReg(&buf[regIndex], rATT, iAtt);
		regIndex += ConfigReg(&buf[regIndex], rDc, rc.dc);
	}
	regIndex++;
	buf[5] = HIBYTE(regIndex);
	buf[6] = LOBYTE(regIndex);

	buf[regIndex - 1] = theApp.CheckSum(buf, regIndex - 1);
	iWrittenBytes = theApp.Send(bx, buf, regIndex);
	theApp.PrintToLog("发送设置接收板硬件参数指令：", buf, iWrittenBytes);
	delete[] buf;

}
int parse_boxinf(int bx,string filename,bool save,string pwd)
{
  string readline,substr,suffix,filestr;
  string::size_type pos;
  ostringstream outstr;
  string sfilenmae = pwd;
  sfilenmae += filename;
  ifstream file(sfilenmae,ios::in);
  
  reg_info_t *adcRegs = new reg_info_t[47];
  int amp[2] = {0};
  boxCaliInfo bci;
  vector<float> txCali;
  vector<rxCHCali>  rxCali;
  txBoardCaliInfo  tbc;
  rxBoardCaliInfo  rbc;
  boardType  bt;
  int boardIndex, dacIndex,regCountRx=0;
  if (!file.is_open())
  {

	  FormatOutPut(PRINT_LEVEL_DEBUG,"%s open failed", sfilenmae.c_str());
	  return ERROR_IN_PROCESS;
  }
  //first mainctrl reset open
  /*
  int regCount = 19, iWrittenBytes;
  char  buf[19];
  memcpy(buf, "$ETS", 4);
  buf[4] = DATABUS_READ_WRITE_READ;
  buf[5] = HIBYTE(regCount);
  buf[6] = LOBYTE(regCount);
  buf[7] = DUMMY_CMD;
  buf[8] = DUMMY_CMD;
  buf[9] = DUMMY_CMD;
  ConfigReg(&buf[10], SYS_CTRL_REG, 0);
  buf[regCount - 1] = theApp.CheckSum(buf, regCount - 1);
  iWrittenBytes = theApp.Send(M, buf, regCount);
  theApp.PrintToLog("发送设置主控复位指令：", buf, iWrittenBytes);
  Sleep(2000);
  */

  while(getline(file,readline))
  {
	  //comment
	  if(!readline.compare(0,1,"#")||readline.empty())
		  continue;
	  pos=readline.find(':');
	  filestr= pwd+readline.substr(pos + 1, string::npos);
      substr=readline.substr(0,pos);
	  suffix = readline.substr(2,pos-2);
	  if(!substr.compare("gra"))
	  {
	      ParseGradInf(filestr.c_str());

	  }
	  else if (!substr.compare("9520"))
	  {
		  if (Parse9520Stp(bx, filestr.c_str()))
			  return ERROR_IN_PROCESS;

	  }
	  else if(!substr.compare(0,2,"tx"))
	  {
		  //is tx1 aha
		  if (substr.size() == 3)
		  {
			  boardIndex = atoi(suffix.c_str());
			  if (boardIndex > 2)
			  {
				  PrintStr(PRINT_LEVEL_DEBUG, "发射板最多有2块");
				  return ERROR_IN_PROCESS;
			  }
			  bt = boardType(TX1 + boardIndex - 1);
			  txCali.clear();
			  parseTxCali(filestr.c_str(), txCali);
			  if (save)
			  {
				  tbc.filename = filestr;
				  tbc.txBoardCali = txCali;
				  bci.txCalis.insert(pair<boardType, txBoardCaliInfo>(bt, tbc));
			  }
			  ConfigTxRegs(bx, bt, theApp.dacRegs[boardIndex-1], txCali,amp[boardIndex-1]);
		  }
		  else
		  {
			  boardIndex = suffix.at(0) - 0x30;
			  suffix = suffix.substr(1);
			  if (!suffix.compare(0, 4, "_dac"))
			  {
				  dacIndex = suffix.at(4) - 0x30;
				  if ((theApp.dacRegsSize[boardIndex-1][dacIndex-1] = parseDacFile(filestr.c_str(), theApp.dacRegs[boardIndex - 1][dacIndex - 1], 47)) == ERROR_IN_PARSE)
				  {
					  PrintStr(PRINT_LEVEL_DEBUG, "解析DAC文件出错");
					  return ERROR_IN_PROCESS;
				  }
			  }
			  else  if (!suffix.compare("_amp"))
			  {
				  if (( parseAmpFile(filestr.c_str(), amp[boardIndex - 1])) == ERROR_IN_PARSE)
				  {
					  PrintStr(PRINT_LEVEL_DEBUG, "解析AMP文件出错");
					  return ERROR_IN_PROCESS;
				  }
			  }
			  else
			  {
				  PrintStr(PRINT_LEVEL_DEBUG, "invalid suffix");
				  return ERROR_IN_PROCESS;


			  }
		  }
	  }
	  else if(!substr.compare(0,2,"rx"))
	  {
		  if (!suffix.compare("_adc"))
		  {
			  if ((regCountRx = parseDacFile(filestr.c_str(), adcRegs, 10)) == ERROR_IN_PARSE)
			  {
				  PrintStr(PRINT_LEVEL_DEBUG,"解析ADC文件出错");
				  return ERROR_IN_PROCESS;
			  }
		  }
		  else
		  {
			  boardIndex = atoi(suffix.c_str());
			  if (boardIndex > 4)
			  {
				  PrintStr(PRINT_LEVEL_DEBUG,"接收板最多有4块");
				  return ERROR_IN_PROCESS;
			  }
			  bt =  boardType(RX1+boardIndex-1);
			  rxCali.clear();
			  parseRxCali(filestr.c_str(), rxCali);
			  if (save)
			  {
				  rbc.filename = filestr;
				  rbc.rxBoardCali = rxCali;
				  bci.rxCalis.insert(pair<boardType, rxBoardCaliInfo>(bt, rbc));
			  }
			  
			  ConfigRxRegs(bx, bt, adcRegs, regCountRx, rxCali);

		  }
		  }
	    

  }
  if (save)
  {
	  theApp.nmrCali.insert(pair<int, boxCaliInfo>(bx,bci));
  }
  file.close();
 // delete[] dacRegs;
  delete[] adcRegs;
  return RIGHT_IN_PROCESS;

}
typedef struct tcp_keepalive
{
	u_long onoff;
	u_long keepalivetime;
	u_long keepaliveinterval;
}TCP_KEEPALIVE2;
/*
bool SoftWareApp::checkBoxName(string name)
{


}
*/
typedef float(*MAXTRIX_ARRAY)[9];
class SampleServer : public AbstractServer<SampleServer>
{
public:
	SampleServer(TcpSocketServer &server) :
		AbstractServer<SampleServer>(server)
	{
		//this->bindAndAddMethod(Procedure("sayHello", PARAMS_BY_NAME, JSON_STRING, "name", JSON_STRING, NULL), &SampleServer::sayHello);
		//this->bindAndAddNotification(Procedure("notifyServer", PARAMS_BY_NAME, NULL), &SampleServer::notifyServer);
		this->bindAndAddMethod(Procedure("SetParameterFileIn", PARAMS_BY_NAME, JSON_INTEGER, "filename", JSON_STRING, "isedit",JSON_BOOLEAN,NULL), &SampleServer::SetParameterFileIn);
		this->bindAndAddMethod(Procedure("SetTxPhaseTableIn", PARAMS_BY_NAME, JSON_INTEGER, "channel", JSON_INTEGER, "len", JSON_INTEGER, NULL), &SampleServer::SetTxPhaseTableIn);
		this->bindAndAddMethod(Procedure("SetTxGainTableIn", PARAMS_BY_NAME, JSON_INTEGER, "channel", JSON_INTEGER, "len", JSON_INTEGER, NULL), &SampleServer::SetTxGainTableIn);
		this->bindAndAddMethod(Procedure("SetTxFreOffsetTableIn", PARAMS_BY_NAME, JSON_INTEGER, "channel", JSON_INTEGER, "len", JSON_INTEGER, NULL), &SampleServer::SetTxFreOffsetTableIn);
		this->bindAndAddMethod(Procedure("SetRxPhaseTableIn", PARAMS_BY_NAME, JSON_INTEGER, "len", JSON_INTEGER, NULL), &SampleServer::SetRxPhaseTableIn);
		this->bindAndAddMethod(Procedure("SetRxFreOffsetTableIn", PARAMS_BY_NAME, JSON_INTEGER,  "len", JSON_INTEGER, NULL), &SampleServer::SetRxFreOffsetTableIn);
		this->bindAndAddMethod(Procedure("SetGradientAllScaleIn", PARAMS_BY_NAME, JSON_INTEGER, "channel", JSON_INTEGER, "len", JSON_INTEGER, NULL), &SampleServer::SetGradientAllScaleIn);
		this->bindAndAddMethod(Procedure("SetAllMaxtrixValueIn", PARAMS_BY_NAME, JSON_INTEGER, "len", JSON_INTEGER, NULL), &SampleServer::SetAllMaxtrixValueIn);
		this->bindAndAddMethod(Procedure("SaveParameterFileIn", PARAMS_BY_NAME, JSON_INTEGER, "name", JSON_STRING,  NULL), &SampleServer::SaveParameterFileIn);
		this->bindAndAddMethod(Procedure("SetParameterIn", PARAMS_BY_NAME, JSON_INTEGER, "name", JSON_STRING, "value", JSON_REAL, NULL), &SampleServer::SetParameterIn);
		
		//scan
		this->bindAndAddNotification(Procedure("ContinueIn", PARAMS_BY_NAME, NULL), &SampleServer::ContinueIn);
		this->bindAndAddNotification(Procedure("PauseIn", PARAMS_BY_NAME, NULL), &SampleServer::PauseIn);
		this->bindAndAddNotification(Procedure("AbortIn", PARAMS_BY_NAME, NULL), &SampleServer::AbortIn);
		this->bindAndAddMethod(Procedure("RunIn", PARAMS_BY_NAME, JSON_INTEGER, NULL), &SampleServer::RunIn);

		//instrument
		this->bindAndAddNotification(Procedure("SetChannelValueIn", PARAMS_BY_NAME, "channel", JSON_INTEGER, "value", JSON_REAL, NULL), &SampleServer::SetChannelValueIn);
		this->bindAndAddMethod(Procedure("GetChannelValueIn", PARAMS_BY_NAME, JSON_REAL, "channel", JSON_INTEGER, "channel", JSON_INTEGER, NULL), &SampleServer::GetChannelValueIn);
		this->bindAndAddMethod(Procedure("SaveShimValuesIn", PARAMS_BY_NAME, JSON_INTEGER, NULL), &SampleServer::SaveShimValuesIn);
		this->bindAndAddMethod(Procedure("SetPreempValueIn", PARAMS_BY_NAME, JSON_INTEGER, "channel", JSON_INTEGER, "keys", JSON_INTEGER, "value", JSON_REAL, NULL), &SampleServer::SetPreempValueIn);
		this->bindAndAddMethod(Procedure("GetPreempValueIn", PARAMS_BY_NAME, JSON_REAL, "channel", JSON_INTEGER, "keys", JSON_INTEGER,NULL), &SampleServer::GetPreempValueIn);
		this->bindAndAddMethod(Procedure("SavePreempValueIn", PARAMS_BY_NAME, JSON_INTEGER, NULL), &SampleServer::SavePreempValueIn);
		
		this->bindAndAddMethod(Procedure("SetRxCenterFreIn", PARAMS_BY_NAME, JSON_INTEGER,"box", JSON_INTEGER,"boardno", JSON_INTEGER,"channel", JSON_INTEGER, "freq",JSON_REAL,"isAllSet",JSON_BOOLEAN, NULL), &SampleServer::SetRxCenterFreIn);
		this->bindAndAddMethod(Procedure("SetTxCenterFreIn", PARAMS_BY_NAME, JSON_INTEGER, "box", JSON_INTEGER, "boardno", JSON_INTEGER, "channel", JSON_INTEGER, "freq", JSON_REAL, NULL), &SampleServer::SetTxCenterFreIn);
		this->bindAndAddMethod(Procedure("GetCenterFreIn", PARAMS_BY_NAME, JSON_REAL, "sel", JSON_INTEGER,  NULL), &SampleServer::GetCenterFreIn);
		//recon
		this->bindAndAddNotification(Procedure("SetOutputPrefixIn", PARAMS_BY_NAME, "prefix", JSON_STRING, NULL), &SampleServer::SetOutputPrefixIn);
		this->bindAndAddNotification(Procedure("SetSaveModeIn", PARAMS_BY_NAME, "saveMode", JSON_INTEGER, NULL), &SampleServer::SetSaveModeIn);
		this->bindAndAddNotification(Procedure("SetRawDataFormatIn", PARAMS_BY_NAME, "sel", JSON_INTEGER, NULL), &SampleServer::SetRawDataFormatIn);
		this->bindAndAddMethod(Procedure("SetOutputPathIn", PARAMS_BY_NAME, JSON_INTEGER, "path",JSON_STRING, NULL), &SampleServer::SetOutputPathIn);
		this->bindAndAddMethod(Procedure("SetScanLinesIn", PARAMS_BY_NAME, JSON_INTEGER, "lines", JSON_INTEGER, "mode", JSON_INTEGER ,NULL), &SampleServer::SetScanLinesIn);
		this->bindAndAddMethod(Procedure("GetStatusIn", PARAMS_BY_NAME, JSON_OBJECT, NULL), &SampleServer::GetStatusIn);
	}

	//method
	void sayHello(const Json::Value& request, Json::Value& response)
	{
		response = "Hello: " + request["name"].asString();
	}

	//notification
	void notifyServer(const Json::Value& request)
	{
		(void)request;
		cout << "server received some Notification" << endl;
	}

	// real function
	void  SetParameterFileIn(const Json::Value& request, Json::Value& response)
	{
		if (SetParameterFile(request["filename"].asCString(), request["isedit"].asBool()))
		{
			throw JsonRpcException(errorno, theApp.message);
		}
		else response = 0;
	}
	void  SaveParameterFileIn(const Json::Value& request, Json::Value& response)
	{
		if (SaveParameterFile(request["name"].asCString()))
		{
			throw JsonRpcException(errorno, theApp.message);
		}
		else response = 0;
	}
	void SetTxPhaseTableIn(const Json::Value& request, Json::Value& response)
	{
		if (SetTxPhaseTable(request["channel"].asInt(), (float *)theApp.GetBuff(),request["len"].asInt()))
		{
			throw JsonRpcException(errorno, theApp.message);
		}
		else response = 0;
	}
	void SetTxGainTableIn(const Json::Value& request, Json::Value& response)
	{
		if (SetTxGainTable(request["channel"].asInt(), (float *)theApp.GetBuff(), request["len"].asInt()))
		{
			throw JsonRpcException(errorno, theApp.message);
		}
		else response = 0;
	}
	void  SetTxFreOffsetTableIn(const Json::Value& request, Json::Value& response)
	{
		if (SetTxFreOffsetTable(request["channel"].asInt(), (float *)theApp.GetBuff(), request["len"].asInt()))
		{
			throw JsonRpcException(errorno, theApp.message);
		}
		else response = 0;
	}
	void  SetRxPhaseTableIn(const Json::Value& request, Json::Value& response)
	{
		if (SetRxPhaseTable((float *)theApp.GetBuff(), request["len"].asInt()))
		{
			throw JsonRpcException(errorno, theApp.message);
		}
		else response = 0;
	}
	void  SetRxFreOffsetTableIn(const Json::Value& request, Json::Value& response)
	{
		if (SetRxFreOffsetTable((float *)theApp.GetBuff(), request["len"].asInt()))
		{
			throw JsonRpcException(errorno, theApp.message);
		}
		else response = 0;
	}
	void  SetGradientAllScaleIn(const Json::Value& request, Json::Value& response)
	{
		if (SetGradientAllScale(request["channel"].asInt(), (float *)theApp.GetBuff(), request["len"].asInt()))
		{
			throw JsonRpcException(errorno, theApp.message);
		}
		else response = 0;
	}
	void  SetAllMaxtrixValueIn(const Json::Value& request, Json::Value& response)
	{
		
//		if (SetAllMaxtrixValue((MAXTRIX_ARRAY)(theApp.GetBuff()), request["len"].asInt()))
//		{
//			throw JsonRpcException(errorno, theApp.message);
//		}
//		else response = 0;
	}
	void  SetParameterIn(const Json::Value& request, Json::Value& response)
	{
		if (SetParameter(request["name"].asCString(), request["value"].asDouble()))
		{
			throw JsonRpcException(errorno, theApp.message);
		}
		else response = 0;
	}
	
	// Scan
	void  ContinueIn(const Json::Value& request)
	{
		Continue();
	}
	void  PauseIn(const Json::Value& request)
	{
		Pause();
	}
	void  AbortIn(const Json::Value& request)
	{
		Abort();
	}
	void   RunIn(const Json::Value& request, Json::Value& response)
	{
		if (Run())
		{
			throw JsonRpcException(errorno, theApp.message);
		}
		else response = 0;
	}


	//Instrument
	void   SetChannelValueIn(const Json::Value& request)
	{
			SetChannelValue(request["channel"].asInt(), request["value"].asFloat());
	}
	void  GetChannelValueIn(const Json::Value& request, Json::Value& response)
	{
		response = GetChannelValue(request["channel"].asInt());
		
	}
	void  SaveShimValuesIn(const Json::Value& request, Json::Value& response)
	{
		if (SaveShimValues())
		{
			throw JsonRpcException(errorno, theApp.message);
		}
		else response = 0;
	}
	void  SetPreempValueIn(const Json::Value& request, Json::Value& response)
	{
		if (SetPreempValue(request["channel"].asInt(), request["keys"].asInt(), request["value"].asFloat()))
		{
			throw JsonRpcException(errorno, theApp.message);
		}
		else response = 0;
	}
	void  GetPreempValueIn(const Json::Value& request, Json::Value& response)
	{
		int correct;
		float value;
		value=GetPreempValue(request["channel"].asInt(), request["keys"].asInt(),&correct);
		if (correct)
		{
			response = value;
		}
		else
			throw JsonRpcException(1013, theApp.message);
		
	}
	void  SavePreempValueIn(const Json::Value& request, Json::Value& response)
	{
		if (SavePreempValue())
		{
			throw JsonRpcException(errorno, theApp.message);
		}
		else response = 0;
	}
	void  SetRxCenterFreIn(const Json::Value& request, Json::Value& response)
	{
		if (SetRxCenterFre(request["box"].asInt(), request["boardno"].asInt(), request["channel"].asInt(), request["freq"].asFloat(), request["isAllSet"].asBool()))
		{
			throw JsonRpcException(errorno, theApp.message);
		}
		else response = 0;
		
	}
	void  SetTxCenterFreIn(const Json::Value& request, Json::Value& response)
	{
		if (SetTxCenterFre(request["box"].asInt(), request["boardno"].asInt(), request["channel"].asInt(), request["freq"].asFloat()))
		{
			throw JsonRpcException(errorno, theApp.message);
		}
		else response = 0;
	}
	void  GetCenterFreIn(const Json::Value& request, Json::Value& response)
	{
		response = GetCenterFre(request["sel"].asInt());
	}
	//recon
	void   SetOutputPrefixIn(const Json::Value& request)
	{
		SetOutputPrefix(request["prefix"].asCString());
	}
	void   SetSaveModeIn(const Json::Value& request)
	{
		SetSaveMode(request["saveMode"].asInt());
	}
	void  SetRawDataFormatIn(const Json::Value& request)
	{
		SetRawDataFormat(request["sel"].asInt());
	}
	void  SetOutputPathIn(const Json::Value& request, Json::Value& response)
	{
		if (SetOutputPath(request["path"].asCString()))
		{
			throw JsonRpcException(errorno, theApp.message);
		}
		else response = 0;
	}
	void   SetScanLinesIn(const Json::Value& request, Json::Value& response)
	{
		theApp.linesset = true;
		theApp.lines = request["lines"].asInt();
		theApp.tiggermode = request["mode"].asInt();
		response = 0;
	}
	void GetStatusIn(const Json::Value& request, Json::Value& response)
	{
		response["status"] = theApp.status;
		response["num"] = theApp.triggerNum;
		response["size"] = theApp.onelinesize;
	}
};
//init rpc server
DWORD WINAPI InitServer(LPVOID lpParameter)
{
	try
	{
		TcpSocketServer server("127.0.0.1", 6543);
		SampleServer serv(server);
		if (serv.StartListening())
		{
			FormatOutPut(PRINT_LEVEL_INFO, "RPC Server started successfully");
			if(theApp.stoprpc||getchar())
			serv.StopListening();
			FormatOutPut(PRINT_LEVEL_INFO, "RPC Server closed");
		}
		else
		{
			FormatOutPut(PRINT_LEVEL_INFO, "Error starting Server" );
		}
	}
	catch (jsonrpc::JsonRpcException& e)
	{
		FormatOutPut(PRINT_LEVEL_ERROR, e.what());
	}
	return 0;

}

int Init(const char *initfile)
{
	string filename= initfile,pwd;
	string::size_type pos;
	int boxindex=0;
	WORD wVersionRequested;
	WSADATA wsaData;
	int err, port;
	//connect 
	SOCKET m_SockClient;
	SOCKADDR_IN addrSrv;
	map<int, Spectrometer>::iterator iter;
	std::string boxname;
	int  extendType=0;
	std::string serverIP;
	std::string hostIP;
	//Initialize occur error 
	if (error)
	{
		return ERROR_IN_PROCESS;
	}
	theApp.rpcthread = CreateThread(NULL, 0, InitServer, NULL, 0, NULL);
	wVersionRequested = MAKEWORD(1, 1);

	err = WSAStartup(wVersionRequested, &wsaData);
	if (err != 0)
	{
		PrintStr(PRINT_LEVEL_ERROR,"WSA start up error");
		return 1;
	}
	pos = filename.find_last_of('\\');
	pwd = filename.substr(0,pos+1);
	theApp.pwd = pwd;
	if (LOBYTE(wsaData.wVersion) != 1 || HIBYTE(wsaData.wVersion) != 1)
	{
		WSACleanup();
		PrintStr(PRINT_LEVEL_ERROR,"socket version error");
		return 1;
	}

	ifstream file;
	string linestr,substr;
	string::size_type startpos,endpos;
	 if(initfile)
	 file.open(initfile);
	 else
	 {
	  file.open(DEFAULT_INIT_FILE);
	 }
	 if (!file.is_open())
	 {
		 PrintStr(PRINT_LEVEL_ERROR, "fail to open init file: ");
		 return ERROR_IN_PROCESS;
	 }
	 theApp.configPwd = pwd;
	 
	 //
	 while (getline(file, linestr))
	 {
		 if (!linestr.compare(0, 1, "#") || linestr.empty())
			 continue;
		 startpos =linestr.find(':');
		 if (startpos != string::npos)
		 {
			 substr= linestr.substr(0, startpos);
			 if (!substr.compare("M"))
				 boxindex = 0;
			 else if (!substr.compare(0,1,"E"))
			 {
				 boxindex = atoi(substr.substr(1).c_str());
			 }
			 else
			 {
				 FormatOutPut(PRINT_LEVEL_ERROR, "invalid box prefix %s", substr.c_str());
				 return ERROR_IN_PROCESS;
			 }
			 
		     boxname = substr;
			 startpos = linestr.find("fiber");
			 if (startpos != string::npos)
				 extendType = 2;
			 startpos = linestr.find("ServerIP");
			 startpos = linestr.find(':', startpos);
			 endpos = linestr.find(',', startpos);
			 substr = linestr.substr(startpos+1, endpos-startpos-1);
			 serverIP = substr;
			 hostIP = substr;
			 inet_pton(AF_INET, substr.c_str(), (void *)&addrSrv.sin_addr);
			 startpos = linestr.find(':', endpos);
			 endpos = linestr.find(' ', startpos);
			 substr = linestr.substr(startpos + 1, endpos - startpos - 1);
			 
			 port = atoi(substr.c_str());
			 addrSrv.sin_family = AF_INET;
			 addrSrv.sin_port = htons(port);//进程监听端口
			 m_SockClient = socket(AF_INET, SOCK_STREAM, 0);
			 //set keep alive
			 if (m_SockClient == INVALID_SOCKET)
			 {
				 PrintStr(PRINT_LEVEL_ERROR, "create socket error");
				 return ERROR_IN_PROCESS;
			 }
#if 1
			 int iKeepAlive = 1;
			 int iOptLen = sizeof(iKeepAlive);
			 setsockopt(m_SockClient, SOL_SOCKET, SO_KEEPALIVE, (char *)&iKeepAlive, iOptLen);

			 TCP_KEEPALIVE2 inKeepAlive = { 0, 0, 0 };
			 unsigned long ulInLen = sizeof(TCP_KEEPALIVE2);
			 TCP_KEEPALIVE2 outKeepAlive = { 0, 0, 0 };
			 unsigned long ulOutLen = sizeof(TCP_KEEPALIVE2);
			 unsigned long ulBytesReturn = 0;

			 // 设置心跳参数
			 inKeepAlive.onoff = 1;                  // 是否启用
			 inKeepAlive.keepalivetime = 3000;       // 在tcp通道空闲1000毫秒后， 开始发送心跳包检测
			 inKeepAlive.keepaliveinterval = 500;    // 心跳包的间隔时间是500毫秒

													 /*
													 补充上面的"设置心跳参数"：
													 当没有接收到服务器反馈后，对于不同的Windows版本，客户端的心跳尝试次数是不同的，
													 比如， 对于Win XP/2003而言, 最大尝试次数是5次， 其它的Windows版本也各不相同。
													 当然啦， 如果是在Linux上， 那么这个最大尝试此时其实是可以在程序中设置的。
													 */


													 // 调用接口， 启用心跳机制
			 WSAIoctl(m_SockClient, SIO_KEEPALIVE_VALS,
				 &inKeepAlive, ulInLen,
				 &outKeepAlive, ulOutLen,
				 &ulBytesReturn, NULL, NULL);
#endif
			
			 err = connect(m_SockClient, (SOCKADDR*)&addrSrv, sizeof(SOCKADDR));
			 if (err == SOCKET_ERROR)
			 {
				 theApp.PrintToLogNC("谱仪连接失败", NULL, 0);
				 PrintStr(PRINT_LEVEL_DEBUG, "network connect error\n");
				 return ERROR_IN_PROCESS;
			 }
			 theApp.PrintToLogNC("谱仪连接成功", NULL, 0);
			 theApp.boxs.insert(pair<int, Spectrometer>(boxindex, Spectrometer()));
			 iter = theApp.boxs.find(boxindex);
			 iter->second.serverIP = serverIP;
			 iter->second.boxname = boxname;
			 iter->second.extendType = extendType;
			 iter->second.hostIP = hostIP;
			 iter->second.b_index = boxindex;
			 iter->second.m_SockClient = m_SockClient;
			 iter->second.addrSrv = addrSrv;
			 //创建接收线程
			 iter->second.connect_status = 0;
			 iter->second.m_Buf = new char[REV_BUF_LEN];
			 iter->second.m_hReadThread = CreateThread(NULL, 0, Spectrometer::ReadPortThread, (LPVOID)(&(iter->second)), 0, NULL);
			// boxinfo.configured = true;
			 iter->second.cfgFileName = linestr.substr(endpos+1);
			 theApp.boxNameHash.insert(make_pair(boxname, boxindex));
		 }

	 }
	 file.close();
	 return 0;
}
/*
* add grad delay @2016/9/29
* revise for set extend box rx freq and in slim version rxboard is only one,tx is only two @2017/3/30
* 
*  
*
*/
void ConfigureSF(string pwd)
{
	int i,j,regCount,iWrittenBytes;
	string linestr,subs;
	string::size_type pos;
	map<int, Spectrometer>::iterator box_iter;
	float dtemp;
	ifstream file;
	ostringstream temp;
	temp << pwd << "fre.hw";
	file.open(temp.str());
	if (!file.is_open())
	{
		theApp.PrintToLogNC("open fre.hw failed", NULL, 0);
		return;
	}
	getline(file, linestr);
	dtemp = atof(linestr.c_str());
	theApp.SYSFREQ = dtemp;
	for (j = 0; j < 8; j++)
		SetTxCenterFre(0,TX1,j,dtemp);
	for (box_iter = theApp.boxs.begin(); box_iter != theApp.boxs.end(); box_iter++)
	{
				SetRxCenterFre(box_iter->first, RX1, 0, dtemp, true);
	}
	file.close();
	temp.str("");
	temp << pwd << "\\shimming.txt";
	file.open(temp.str());
	if (!file.is_open())
	{
		theApp.PrintToLogNC("open shimming.txt failed", NULL, 0);
		return;
	}
	
	while (getline(file, linestr))
	{
		pos = linestr.find('=',0);
		if (pos != string::npos)
		{
			subs = linestr.substr(0,pos);
			dtemp = atof(linestr.substr(pos+1,string::npos).c_str());

			if (!subs.compare("X"))
				SetChannelValue(CHANNEL_X,dtemp);
			else if(!subs.compare("Y"))
				SetChannelValue(CHANNEL_Y, dtemp);
			else if(!subs.compare("Z"))
				SetChannelValue(CHANNEL_Z, dtemp);
			else if(!subs.compare("B0"))
				SetChannelValue(CHANNEL_B0, dtemp);
		}
	}
	file.close();

	temp.str("");
	temp << pwd << "\\gra_gmax.cfg";
	file.open(temp.str());
	if (!file.is_open())
	{
		theApp.PrintToLogNC("open gra_gmax.cfg failed", NULL, 0);
		return;
	}
	while (getline(file, linestr))
	{
		if (!linestr.compare(0, 1, "#") || linestr.empty())
			continue;
		vector<string> result=split(linestr,",");
		if (result.size() != 3)
		{
			theApp.PrintToLogNC("gra_gmax.cfg error", NULL, 0);
			return;
		}
		theApp.matrixG[0] = 1 / atof(result[0].c_str());
		theApp.matrixG[1] = 1 / atof(result[1].c_str());
		theApp.matrixG[2] = 1 / atof(result[2].c_str());
	}

	if (SetAllPreempValue())
	{
		theApp.PrintToLogNC("process gra_pef.txt failed", NULL, 0);
	
	}
	file.close();

	temp.str("");
	temp << pwd << "\\gra_analog_delay.txt";
	file.open(temp.str());
	if (!file.is_open())
	{
		theApp.PrintToLogNC("open gra_analog_delay.txt failed", NULL, 0);
		return;
	}
	vector<string> result;
	while (getline(file, linestr))
	{
		if (!linestr.compare(0, 1, "#") || linestr.empty())
			continue;
		result = split(linestr, ",");
		break;
	}
	if (result.size() != 4)
	{
		theApp.PrintToLogNC("gra_analog_delay.txt param is not equal to 4", NULL, 0);
		return;
	}
	char* buf = new char[BUF_STANDARD_SIZE*2];
	memcpy(buf, "$ETS", 4);
	buf[4] = DIRECT_REG_WRITE_READ;
	buf[7] = DUMMY_CMD;
	buf[8] = DUMMY_CMD;
	buf[9] = DUMMY_CMD;
	regCount = 10;
	regCount+=ConfigReg(&buf[regCount], GRAD_DELAY_X,atoi(result[0].c_str()));
	regCount+=ConfigReg(&buf[regCount], GRAD_DELAY_Y, atoi(result[1].c_str()));
	regCount+=ConfigReg(&buf[regCount], GRAD_DELAY_Z, atoi(result[2].c_str()));
	regCount+=ConfigReg(&buf[regCount], GRAD_DELAY_B0, atoi(result[3].c_str()));
	regCount++;
	buf[5] = HIBYTE(regCount);
	buf[6] = LOBYTE(regCount);
	buf[regCount - 1] = theApp.CheckSum(buf, regCount - 1);
	iWrittenBytes = theApp.Send(M, buf, regCount);
	theApp.PrintToLog("设置梯度延时指令：", buf, iWrittenBytes);
	file.close();

	//down att ph formula ph/360*2^28
	
	int  ph, ph_addr_low, ph_addr_high;
	
	for (i = 1; i <= 4; i++)
	{
		temp.str("");
		temp << pwd << "TX"<<i<<"_att_cal_table.txt";
		file.open(temp.str());
		if (i < 3)
		{
			ph_addr_low = TX1_C1_ATT_PHASE_RAM_DATA_L + 4 * (i - 1);
		}
		else
		{
			ph_addr_low = TX2_C1_ATT_PHASE_RAM_DATA_L + 4 * (i - 3);
		}
		ph_addr_high = ph_addr_low + 2;
		if (file.is_open())
		{ 
			regCount = 10;
			while (getline(file, linestr))
			{
				if (!linestr.compare(0, 1, "#") || linestr.empty())
					continue;
				result = split(linestr, ",");
				ph = round(atof(result[2].c_str())/360*pow(2,28));
				regCount += ConfigReg(&buf[regCount], ph_addr_low, ph&0xFFFF);
				regCount += ConfigReg(&buf[regCount], ph_addr_high, ph>>16);
			}
			regCount++;
			buf[5] = HIBYTE(regCount);
			buf[6] = LOBYTE(regCount);
			buf[regCount - 1] = theApp.CheckSum(buf, regCount - 1);
			iWrittenBytes = theApp.Send(M, buf, regCount);
			theApp.PrintToLog("设置衰减器校准指令：", buf, iWrittenBytes);
			file.close();
		}
	}
	delete[] buf;

}
int ConfigFile(const char *filename)
{
	string filenames = filename, pwd;
	string::size_type pos;
	unsigned int i, j,k;
	boardType brdt;
	map<int, boxCaliInfo>::iterator iter;
	map<boardType, txBoardCaliInfo>::iterator txIter;
	map<boardType, rxBoardCaliInfo>::iterator rxIter;
	boxCaliInfo bci;
	txBoardCaliInfo tbi;
	rxBoardCaliInfo rbi;
	rxCHCali        rcc;
	string  jasondata,temp;
	map<int, Spectrometer>::iterator box_iter;
	
	char floatstr[100];
	int bt;
	//clear map
	theApp.nmrCali.clear();

	pos = filenames.find_last_of('\\');
	pwd = filenames.substr(0, pos + 1);
	
	for (box_iter = theApp.boxs.begin(); box_iter != theApp.boxs.end(); box_iter++)
	{
		//Get Arm version
			//analysis
		i = box_iter->first;
			ResetFPGA(i);
			LedControl(i);
			if (ERROR_IN_PROCESS == parse_boxinf(i, box_iter->second.cfgFileName, true, pwd))
			{
				PrintStr(PRINT_LEVEL_ERROR, "error occur while parse box  file");
				return NULL;
			}
			if (ArmVerQeruy(i) == ERROR_IN_PROCESS)
			{
				theApp.PrintToLogNC("Get Arm Version failded", NULL, 0);
			}
			if (i == 0)
				ConfigSingleReg_i(i, MAINCTRL_TRIGGER_SEL_REG,0);
			else if(box_iter->second.extendType==2)
				ConfigSingleReg_i(i, MAINCTRL_TRIGGER_SEL_REG, 2);
				else 
				ConfigSingleReg_i(i, MAINCTRL_TRIGGER_SEL_REG, 1);
		
	}
	ConfigureSF(pwd);
	///organize to JASON data
	jasondata = "{";
	for (iter = theApp.nmrCali.begin(),k=0; iter != theApp.nmrCali.end(); iter++,k++)
	{
		bt = iter->first;
		bci = iter->second;
		jasondata += "\"" + BoxToString(bt) + "\"";
		ostringstream ostrtemp;
		jasondata += ":{";
		/*add serverip,localip,arminfo*/
		jasondata += "\"arminfo\":[";
		
		ostrtemp << "\"" << theApp.boxs[bt].armAppVer << "\"" << ",\"" << theApp.boxs[bt].armDriverVer << "\"";
		jasondata += ostrtemp.str();
		jasondata += "],";
		ostrtemp.str(""); /*clear ostrtemp*/
		jasondata += "\"serverip\":";
		ostrtemp <<"\""<< theApp.boxs[bt].serverIP << "\""<<",";
		jasondata += ostrtemp.str();
		ostrtemp.str("");
		jasondata += "\"localip\":";
		ostrtemp << "\"" << theApp.boxs[bt].hostIP << "\""<<",";
		jasondata += ostrtemp.str();
		ostrtemp.str("");
		if (!bci.txCalis.empty())
		{
			jasondata+= "\"""TX""\"";
			jasondata += ":{";
			for (txIter = bci.txCalis.begin(),j=0; txIter != bci.txCalis.end(); txIter++,j++)
			{
				brdt = txIter->first;
				tbi = txIter->second;
				switch (brdt)
				{
				case TX1:
					jasondata += "\"""TX1""\"";
					break;
				case TX2:
					jasondata += "\"""TX2""\"";
					break;
				default:
					break;
				}
				jasondata += ":";
				jasondata += "[";
				for (i = 0; i < tbi.txBoardCali.size(); i++)
				{

					sprintf_s(floatstr,"%.2f", tbi.txBoardCali.at(i));
					temp = floatstr;
					jasondata += temp;
					if(i!= tbi.txBoardCali.size()-1)
					jasondata += ",";
				}
				jasondata += "]";
				if(j!= bci.txCalis.size()-1)
				jasondata += ",";
			}
			jasondata += "}";
		}
		if (!bci.rxCalis.empty())
		{
			jasondata += ",";
			jasondata += "\"""RX""\"";
			jasondata += ":{";
			for (rxIter = bci.rxCalis.begin(), j = 0; rxIter != bci.rxCalis.end(); rxIter++, j++)
			{
				brdt = rxIter->first;
				rbi = rxIter->second;
				switch (brdt)
				{
				case RX1:
					jasondata += "\"""RX1""\"";
					break;
				case RX2:
					jasondata += "\"""RX2""\"";
					break;
				case RX3:
					jasondata += "\"""RX3""\"";
					break;
				case RX4:
					jasondata += "\"""RX4""\"";
					break;
				default:
					break;
				}
				
				SetRxDCOffset(bt, brdt);
				jasondata += ":";
				jasondata += "[";
				for (i = 0; i < rbi.rxBoardCali.size(); i++)
				{
					rcc = rbi.rxBoardCali.at(i);
					jasondata += "[";
					sprintf_s(floatstr, "%.2f,%.2f,%.2f,%.2f,%d,%d", rcc.att, rcc.amp1, rcc.amp2, rcc.amp3, rcc.swi,rcc.dc);
					
					temp = floatstr;
					jasondata += temp;
					jasondata += "]";
					if (i != rbi.rxBoardCali.size() - 1)
						jasondata += ",";
					SetRxATT(bt, brdt,i, rcc.att, rcc.amp1, rcc.amp2, rcc.amp3, rcc.swi);
				}
				jasondata += "]";
				if (j != bci.rxCalis.size() - 1)
					jasondata += ",";

			}
			jasondata += "}";
			
		}
		jasondata += "}";
		if (k != theApp.nmrCali.size() - 1)
			jasondata += ",";
	}
	jasondata += "}";
	//int size = jasondata.size();
	//char *jasonstr = new char[size +1];
	//strcpy_s(jasonstr, jasondata.size()+1, jasondata.c_str());
	//return jasondata.c_str();
	filenames = pwd + "cali_anke.txt";
	ofstream file(filenames,ios::out);
	file << jasondata << endl;
	file.close();
    return RIGHT_IN_PROCESS;
	
}
bool SoftWareApp::CheckRTAddr(LPCTSTR addr,int len)
{
	if ((addr - pBufRealTime + len > RT_BUF_SIZE))
		return false;
	else return true;
			
}
SoftWareApp::SoftWareApp()
{
    char logfilename[30];
	char errfilename[30];
	time_t curtime=time(0);
	tm time;
	localtime_s(&time,&curtime);
	int day,mon,year;
	string str1;
	year=time.tm_year;
	mon=time.tm_mon;
	day=time.tm_mday;
	sprintf_s(logfilename,20,"log_%d.%d.%d.log",year+1900,mon+1,day);
	sprintf_s(errfilename, 20, "error_%d.%d.%d.log", year + 1900, mon + 1, day);
	logfile.open(logfilename,ios::app|ios::out);
	errfile.open(errfilename, ios::app | ios::out);
	if (!errfile.is_open())
	{ 
		error = 1;
		PrintStr(PRINT_LEVEL_DEBUG, "err file open failed\n");
	}
	if(!logfile.is_open())
	{ 
		error = 1;
		PrintStr(PRINT_LEVEL_DEBUG,"log file open failed\n");
	}
	boardNameHash.insert(pair<string,boardType>("tx1",TX1));
	boardNameHash.insert(pair<string,boardType>("tx2",TX2));
	boardNameHash.insert(pair<string,boardType>("rx1",RX1));
	boardNameHash.insert(pair<string,boardType>("rx2",RX2));
	boardNameHash.insert(pair<string,boardType>("rx3",RX3));
	boardNameHash.insert(pair<string,boardType>("rx4",RX4));
	boardNameHash.insert(pair<string,boardType>("main",MAIN_BOARD));
	boardNameHash.insert(pair<string,boardType>("gradP",GRADP));
	boardNameHash.insert(pair<string,boardType>("gradR",GRADR));
	boardNameHash.insert(pair<string,boardType>("gradS",GRADS));
	//m_hReadThread=NULL;
	m_SetupModeThread=NULL;
	sysclosed=false;
	abortSetupModeThread=false;
	seqRun=false;
	
	srcFileDowned = false;
	setupMode = false;
	saveMode = RECEIVED_SAVE;
	
	
	pefload = false;
	
	preDiscard = 0;
	lastDiscard = 0;
	//connect_status = 0;
	
#ifdef ANALYSIS_DMA_DATA
	dma_temp_buf = new char[ONE_PACK_MAX_SIZE];
#endif
	
	jasonstr = new char[BUF_STANDARD_SIZE];	
	dacRegs = new reg_info_t[2][4][47];
	lastSampleBufSize = 0;
	outputFileName = '.';
	outputFilePrefix = "";
	//boxChecked = { false };
	InitializeCriticalSection(&g_updateParmProtect);
	stoprpc = false;
	linesset = false;

	hMapFile = CreateFileMapping(
		INVALID_HANDLE_VALUE,    // use paging file
		NULL,                    // default security
		PAGE_READWRITE,          // read/write access
		0,                       // maximum object size (high-order DWORD)
		BUF_SIZE,                // maximum object size (low-order DWORD)
		szName);                 // name of mapping object
	if (hMapFile == NULL)
	{
		error = 1;
		FormatOutPut(PRINT_LEVEL_ERROR, "Could not create file mapping object %d",GetLastError());

	}
	pBuf = (LPTSTR)MapViewOfFile(hMapFile,   // handle to map object
		FILE_MAP_ALL_ACCESS, // read/write permission
		0,
		0,
		BUF_SIZE);

	if (pBuf == NULL)
	{
		error = 1;
		FormatOutPut(PRINT_LEVEL_ERROR, "Could not map view of file %d", GetLastError());
		CloseHandle(hMapFile);
	}

	hMapFileRecon = CreateFileMapping(
		INVALID_HANDLE_VALUE,    // use paging file
		NULL,                    // default security
		PAGE_READWRITE,          // read/write access
		0,                       // maximum object size (high-order DWORD)
		RT_BUF_SIZE,                // maximum object size (low-order DWORD)
		rtName);                 // name of mapping object
	if (hMapFileRecon == NULL)
	{
		error = 1;
		FormatOutPut(PRINT_LEVEL_ERROR, "Could not create rt file mapping object %d", GetLastError());

	}
	pBufRealTime = (LPTSTR)MapViewOfFile(hMapFileRecon,   // handle to map object
		FILE_MAP_ALL_ACCESS, // read/write permission
		0,
		0,
		RT_BUF_SIZE);

	if (pBufRealTime == NULL)
	{
		error = 1;
		FormatOutPut(PRINT_LEVEL_ERROR, "Could not map view of rt file %d", GetLastError());
		CloseHandle(hMapFileRecon);
	}
}
SoftWareApp::~SoftWareApp()
{


	delete[] dacRegs;
	delete[] jasonstr;
	
	

#ifdef ANALYSIS_DMA_DATA
	delete[] dma_temp_buf;
#endif
	logfile.close();
	errfile.close();
	stoprpc = true;
	UnmapViewOfFile(pBuf);
	CloseHandle(hMapFile);

	UnmapViewOfFile(pBufRealTime);
	CloseHandle(hMapFileRecon);
	CloseHandle(rpcthread);

}
DWORD WINAPI  SoftWareApp::UpdateParamThreadStatic(LPVOID lpParameter)
{
   return ((SoftWareApp *)lpParameter)->UpdateParamThread();
}
LPCTSTR SoftWareApp::GetBuff()
{
	return pBuf;
		
}
LPCTSTR SoftWareApp::GetRTBuff()
{
	return pBufRealTime;

}
DWORD  SoftWareApp::UpdateParamThread()
{
   int   regCount=0,j,iWrittenBytes,packLength = 15,config1,config2
   ,config3,config4,config5,allocat_size;
   
   //default size
   char* buf=new char[BUF_STANDARD_SIZE];
   allocat_size=BUF_STANDARD_SIZE;
   map<string,paramInfo>::iterator paramMapIterator;
   string varname;
   paramInfo pi;
   varBoxInfo vboxi;
   varBoardInfo vboardi;
   long long int updateValue;
   while(!abortSetupModeThread)
   {
   
   if(hasParamUpdate&& setupMode&&boxs[0].processEnable)
   {
       
	   memcpy(buf,"$ETS",4);
	   buf[4] = 0x1;
	   buf[7] = DUMMY_CMD;
	   buf[8] = DUMMY_CMD;
	   buf[9] = DUMMY_CMD;

	   buf[10]= MAIN_CTRL_ID0_ADDR >>24;
	   buf[11]= MAIN_CTRL_ID0_ADDR >>16;
	   buf[12]= MAIN_CTRL_ID0_ADDR >>8;
	   buf[13]= MAIN_CTRL_ID0_ADDR & 0xff;

	   buf[14]= 0x00;
	   buf[15]= 0x00;
	   buf[16]= 0x00;
	   buf[17]= 0x01; //写寄存器
	   regCount=18;

	   regCount++;
	   buf[5] = HIBYTE(regCount);
	   buf[6] = LOBYTE(regCount);
	   buf[regCount-1] = theApp.CheckSum(buf,regCount-1);
	   iWrittenBytes = theApp.Send(M,buf,regCount);

	   theApp.PrintToLog("设置ID[0]指令：",buf,iWrittenBytes);
	   while((!theApp.boxs[0].m_WaitState))
	   {
		   if (!boxs[0].processEnable) goto end;
		   memcpy(buf,"$ETR",4);
		   buf[4] = 0x01;
		   buf[5] = HIBYTE(packLength);
		   buf[6] = LOBYTE(packLength);


		   buf[7] = DUMMY_CMD;
		   buf[8] = DUMMY_CMD;
		   buf[9] = SINGLE_READ;


		   buf[10] = MAIN_CTRL_WAITE_STATE_REG >> 24;
		   buf[11] = MAIN_CTRL_WAITE_STATE_REG >> 16;
		   buf[12] = MAIN_CTRL_WAITE_STATE_REG >> 8;
		   buf[13] = MAIN_CTRL_WAITE_STATE_REG & 0xff;
		   buf[packLength-1] = theApp.CheckSum(buf,packLength-1);

		   iWrittenBytes = theApp.Send(M,buf,packLength);

		   //打印发送信息,判断iWrittenBytes的
		   theApp.PrintToLog("查询是否处于wait状态：",buf,iWrittenBytes);
		   Sleep(900);

	   }
	   //发送更新数据
	  
	   memcpy(buf,"$ETS",4);
	   buf[4] = DATABUS_READ_WRITE_READ;
	   buf[7] = DUMMY_CMD;
	   buf[8] = DUMMY_CMD;
	   buf[9] = DUMMY_CMD;
	   regCount =10;
	   EnterCriticalSection(&(theApp.g_updateParmProtect));
	   for(paramMapIterator = paramMap.begin(); paramMapIterator!= paramMap.end(); paramMapIterator++)
	   {  
          pi=paramMapIterator->second;
		  if (!pi.needUpdate) continue;
		  if(pi.pt==TYPE_DOUBLE)
          updateValue=*(long long int *)(&(pi.value.doublelit));
		  else 
		  updateValue=pi.value.intlit;
		  vboxi=pi.varBoxInfos.at(0);
		  for(j=0;j<vboxi.varBoardInfos.size();j++)
		  {
		     vboardi=vboxi.varBoardInfos.at(j);
			 
			 switch(vboardi.bt)
			 {
			 case MAIN_BOARD:
				 config1=MAIN_CTRL_COM_FIFO_CONFIG1;
				 config2=MAIN_CTRL_COM_FIFO_CONFIG2;
				 config3=MAIN_CTRL_COM_FIFO_CONFIG3;
				 config4=MAIN_CTRL_COM_FIFO_CONFIG4;
				 config5=MAIN_CTRL_COM_FIFO_CONFIG5;
				 break;
			 case GRADP:
				 config1=GRADP_COM_FIFO_CONFIG1;
				 config2=GRADP_COM_FIFO_CONFIG2;
				 config3=GRADP_COM_FIFO_CONFIG3;
				 config4=GRADP_COM_FIFO_CONFIG4;
				 config5=GRADP_COM_FIFO_CONFIG5;
				 break;
			 case GRADR:
				 config1=GRADR_COM_FIFO_CONFIG1;
				 config2=GRADR_COM_FIFO_CONFIG2;
				 config3=GRADR_COM_FIFO_CONFIG3;
				 config4=GRADR_COM_FIFO_CONFIG4;
				 config5=GRADR_COM_FIFO_CONFIG5;
				 break;
             case GRADS:
				 config1=GRADS_COM_FIFO_CONFIG1;
				 config2=GRADS_COM_FIFO_CONFIG2;
				 config3=GRADS_COM_FIFO_CONFIG3;
				 config4=GRADS_COM_FIFO_CONFIG4;
				 config5=GRADS_COM_FIFO_CONFIG5;
				 break;
			 case TX1:
				 config1=TX_B1_COM_FIFO_CONFIG1;
				 config2=TX_B1_COM_FIFO_CONFIG2;
				 config3=TX_B1_COM_FIFO_CONFIG3;
				 config4=TX_B1_COM_FIFO_CONFIG4;
				 config5=TX_B1_COM_FIFO_CONFIG5;
				 break;
			 case TX2:
				 config1=TX_B2_COM_FIFO_CONFIG1;
				 config2=TX_B2_COM_FIFO_CONFIG2;
				 config3=TX_B2_COM_FIFO_CONFIG3;
				 config4=TX_B2_COM_FIFO_CONFIG4;
				 config5=TX_B2_COM_FIFO_CONFIG5;
				 break;
			 case RX1:
				 config1=RX_B1_COM_FIFO_CONFIG1;
				 config2=RX_B1_COM_FIFO_CONFIG2;
				 config3=RX_B1_COM_FIFO_CONFIG3;
				 config4=RX_B1_COM_FIFO_CONFIG4;
				 config5=RX_B1_COM_FIFO_CONFIG5;
				 break;
			 case RX2:
				 config1=RX_B2_COM_FIFO_CONFIG1;
				 config2=RX_B2_COM_FIFO_CONFIG2;
				 config3=RX_B2_COM_FIFO_CONFIG3;
				 config4=RX_B2_COM_FIFO_CONFIG4;
				 config5=RX_B2_COM_FIFO_CONFIG5;
				 break;
			 case RX3:
				 config1=RX_B3_COM_FIFO_CONFIG1;
				 config2=RX_B3_COM_FIFO_CONFIG2;
				 config3=RX_B3_COM_FIFO_CONFIG3;
				 config4=RX_B3_COM_FIFO_CONFIG4;
				 config5=RX_B3_COM_FIFO_CONFIG5;
				 break;
			 case RX4:
				 config1=RX_B4_COM_FIFO_CONFIG1;
				 config2=RX_B4_COM_FIFO_CONFIG2;
				 config3=RX_B4_COM_FIFO_CONFIG3;
				 config4=RX_B4_COM_FIFO_CONFIG4;
				 config5=RX_B4_COM_FIFO_CONFIG5;
				 break;
			 default:
				 PrintStr(PRINT_LEVEL_DEBUG,"Impossible:board not find");
				 break;
			  
			 }
			
			 if(regCount>=allocat_size)
			 {
			   //double the size;
			   buf=(char *)realloc(buf,allocat_size*2);
			   allocat_size = allocat_size * 2;
			 }
			 buf[regCount++] = config1>>24;
			 buf[regCount++] = config1>>16;
			 buf[regCount++] = config1>>8;
			 buf[regCount++] = config1 &0xff;

			 buf[regCount++] = 0;
			 buf[regCount++] = 0;
			 buf[regCount++] = updateValue>>8;
			 buf[regCount++] = updateValue &0xff;

			 buf[regCount++] = config2>>24;
			 buf[regCount++] = config2>>16;
			 buf[regCount++] = config2>>8;
			 buf[regCount++] = config2 &0xff;

			 buf[regCount++] = 0;
			 buf[regCount++] = 0;
			 buf[regCount++] = updateValue>>24;
			 buf[regCount++] = updateValue>>16;

			 buf[regCount++] = config3>>24;
			 buf[regCount++] = config3>>16;
			 buf[regCount++] = config3>>8;
			 buf[regCount++] = config3 &0xff;

			 buf[regCount++] = 0;
			 buf[regCount++] = 0;
			 buf[regCount++] = updateValue>>40;
			 buf[regCount++] = updateValue>>32;

			 buf[regCount++] = config4>>24;
			 buf[regCount++] = config4>>16;
			 buf[regCount++] = config4>>8;
			 buf[regCount++] = config4 &0xff;

			 buf[regCount++] = 0;
			 buf[regCount++] = 0;
			 buf[regCount++] = updateValue>>56;
			 buf[regCount++] = updateValue>>48;

			 buf[regCount++] = config5>>24;
			 buf[regCount++] = config5>>16;
			 buf[regCount++] = config5>>8;
			 buf[regCount++] = config5 &0xff;

			 buf[regCount++] = 0;
			 buf[regCount++] = 0;
			 buf[regCount++] = vboardi.updateAddr>>8;
			 buf[regCount++] = vboardi.updateAddr&0xFF;	   
		  } 
		  
          
	   }
	   pi.needUpdate = false;
	   hasParamUpdate = false;
	   LeaveCriticalSection(&(theApp.g_updateParmProtect));
	   //End use updateParm
	   regCount++;
	   buf[5] = HIBYTE(regCount);
	   buf[6] = LOBYTE(regCount);
	   buf[regCount-1] = theApp.CheckSum(buf,regCount-1);

	   iWrittenBytes = theApp.Send(M,buf,regCount);
	   theApp.PrintToLog("发送更新数据指令：",buf,iWrittenBytes);

	   //清零标志位
	   memcpy(buf,"$ETS",4);
	   buf[4] = 0x1;
	   buf[7] = DUMMY_CMD;
	   buf[8] = DUMMY_CMD;
	   buf[9] = DUMMY_CMD;

	   buf[10]= MAIN_CTRL_ID0_ADDR >>24;
	   buf[11]= MAIN_CTRL_ID0_ADDR >>16;
	   buf[12]= MAIN_CTRL_ID0_ADDR >>8;
	   buf[13]= MAIN_CTRL_ID0_ADDR & 0xff;



	   buf[14]= 0x00;
	   buf[15]= 0x00;
	   buf[16]= 0x00;
	   buf[17]= 0x00; //写寄存器
	   regCount=18;

	   regCount++;
	   buf[5] = HIBYTE(regCount);
	   buf[6] = LOBYTE(regCount);
	   buf[regCount-1] = theApp.CheckSum(buf,regCount-1);
	   iWrittenBytes = theApp.Send(M,buf,regCount);
	   theApp.PrintToLog("清零ID[0]指令：",buf,iWrittenBytes);
       
   }
   end:
   Sleep(100);
   }
   delete[] buf;
   return 0;
}


void SetVerboseLevel(int vlevel)
{
  theApp.verboseLevel=vlevel;
}
//no condition print to log
string GetCurrentTimeFormat()
{
	char timeinfo[30];
	time_t curtime = time(0);
	tm time;
	string strtmp;
	localtime_s(&time, &curtime);
	//int hour,min,second;
	sprintf_s(timeinfo, 20, "%d:%d:%d", time.tm_hour, time.tm_min, time.tm_sec);
	strtmp = timeinfo;
	return strtmp;
}
//no condition print to log
string GetCurrentTimeFormat1()
{
	char timeinfo[30];
	time_t curtime = time(0);
	tm time;
	string strtmp;
	localtime_s(&time, &curtime);
	//int hour,min,second;
	sprintf_s(timeinfo, 20, "%d.%d.%d", time.tm_hour, time.tm_min, time.tm_sec);
	strtmp = timeinfo;
	return strtmp;
}
void SoftWareApp::PrintToLogNC(string prefix, char *buf, int length)
{
	int i;
	char format[5];
	string strtmp(""),logstr("");
	logstr += GetCurrentTimeFormat();
	prefix += "\n";
	logstr += prefix;
	if (buf)
	{

		for (i = 1; i <= length; i++)
		{

			sprintf_s(format, 5, "%02X ", (BYTE)buf[i - 1]);
			strtmp = format;
			logstr += strtmp;
			if (!(i%MAX_CHAR_PER_LINE))
				logstr += "\n";
		}
	}
	logstr += "\n";

	theApp.logfile.write(logstr.c_str(), logstr.length());
	theApp.logfile.flush();

}
void SoftWareApp::PrintToLog(string prefix,char *buf,int length)
{
	if (theApp.verboseLevel)
	{
		//operate time 
		int i;
		char timeinfo[30], format[5];
		string strtmp(""), logstr("");
		time_t curtime = time(0);
		tm time;
		localtime_s(&time, &curtime);
		//int hour,min,second;
		sprintf_s(timeinfo, 20, "%d:%d:%d ", time.tm_hour, time.tm_min, time.tm_sec);
		strtmp = timeinfo;
		logstr += strtmp;
		prefix += "\n";
		logstr += prefix;
		if (buf)
		{

			for (i = 1; i <= length; i++)
			{

				sprintf_s(format, 5, "%02X ", (BYTE)buf[i - 1]);
				strtmp = format;
				logstr += strtmp;
				if (!(i%MAX_CHAR_PER_LINE))
					logstr += "\n";
			}
		}
		logstr += "\n";

		theApp.logfile.write(logstr.c_str(), logstr.length());
		theApp.logfile.flush();
	}
}
DWORD SoftWareApp::SendFile(string fileName)
{
	DWORD dwRegCount = 0;
	DWORD dwPackageCnt = 0,dwLastPackage = 0;
	DWORD dwPackageLength = 0;
	DWORD iWrittenBytes = 0;
	int i,boxIndex;
	vector<reg_info_t> *temp;
	map<int,vector<reg_info_t >> regConfigInfo;
	map<int, vector<reg_info_t >>::iterator iter;
	if(theApp.ParseFile(fileName, regConfigInfo))
	{
		PrintStr(PRINT_LEVEL_DEBUG,"配置文件内容为空！");
		return ERROR_IN_PROCESS;
	}
	for (iter = regConfigInfo.begin(); iter!= regConfigInfo.end(); iter++)
	{
		temp = &(iter->second);
		if (temp->size() == 0) continue;
		boxIndex = iter->first;
		dwRegCount = temp->size();
		dwPackageCnt = dwRegCount / REG_CNT_PER_PACKAGE;
		dwLastPackage = dwRegCount % REG_CNT_PER_PACKAGE;

		for (DWORD pIndex = 0; pIndex < dwPackageCnt; pIndex++)
		{
			//分包发送配置信息，每包REG_CNT_PER_PACKAGE个寄存器
			dwPackageLength = REG_CNT_PER_PACKAGE * REG_INFO_WITH + 11;
			char* buf = new char[dwPackageLength];
			memset(buf, 0, dwPackageLength);
			memcpy(buf, "$ETS", 4);
			buf[4] = DATABUS_READ_WRITE_READ;
			buf[5] = HIBYTE(dwPackageLength);
			buf[6] = LOBYTE(dwPackageLength);
			buf[7] = DUMMY_CMD;
			buf[8] = DUMMY_CMD;
			buf[9] = DUMMY_CMD;

			for (i = 0; i < REG_CNT_PER_PACKAGE; i++)
			{
				//memcpy(&buf[6 + dwRegIndex*REG_INFO_WITH],&regConfigInfo[i].regInfo,REG_INFO_WITH);
				buf[10 + (i*REG_INFO_WITH)] = (*temp)[i + (pIndex*REG_CNT_PER_PACKAGE)].regaddr >> 24;
				buf[11 + (i*REG_INFO_WITH)] = (*temp)[i + (pIndex*REG_CNT_PER_PACKAGE)].regaddr >> 16;
				buf[12 + (i*REG_INFO_WITH)] = (*temp)[i + (pIndex*REG_CNT_PER_PACKAGE)].regaddr >> 8;
				buf[13 + (i*REG_INFO_WITH)] = (*temp)[i + (pIndex*REG_CNT_PER_PACKAGE)].regaddr & 0xff;
				buf[14 + (i*REG_INFO_WITH)] = (*temp)[i + (pIndex*REG_CNT_PER_PACKAGE)].regval >> 24;
				buf[15 + (i*REG_INFO_WITH)] = (*temp)[i + (pIndex*REG_CNT_PER_PACKAGE)].regval >> 16;
				buf[16 + (i*REG_INFO_WITH)] = (*temp)[i + (pIndex*REG_CNT_PER_PACKAGE)].regval >> 8;
				buf[17 + (i*REG_INFO_WITH)] = (*temp)[i + (pIndex*REG_CNT_PER_PACKAGE)].regval & 0xff;
			}

			buf[dwPackageLength - 1] = theApp.CheckSum(buf, dwPackageLength - 1);

			iWrittenBytes = theApp.Send(boxIndex, buf, dwPackageLength);

			if (iWrittenBytes == 0 || iWrittenBytes == -1)
			{
				PrintStr(PRINT_LEVEL_DEBUG, "文件发送失败!");
				return 1;
			}
			if (theApp.verboseLevel)
				PrintToLog(BoxToString(boxIndex)+" 发送配置信息：", buf, iWrittenBytes);


			delete[] buf;
		}

		if (dwLastPackage > 0)
		{
			//发送最后一包配置信息
			dwPackageLength = dwLastPackage * REG_INFO_WITH + 11;
			char* buf = new char[dwPackageLength];
			memset(buf, 0, dwPackageLength);
			memcpy(buf, "$ETS", 4);
			buf[4] = DATABUS_READ_WRITE_READ;
			buf[5] = HIBYTE(dwPackageLength);
			buf[6] = LOBYTE(dwPackageLength);
			buf[7] = DUMMY_CMD;
			buf[8] = DUMMY_CMD;
			buf[9] = DUMMY_CMD;

			for (i = 0; i < dwLastPackage; i++)
			{
				//memcpy(&buf[6 + dwRegIndex*REG_INFO_WITH],&regConfigInfo[i].regInfo,REG_INFO_WITH);
				buf[10 + (i*REG_INFO_WITH)] = (*temp)[i + (dwPackageCnt*REG_CNT_PER_PACKAGE)].regaddr >> 24;
				buf[11 + (i*REG_INFO_WITH)] = (*temp)[i + (dwPackageCnt*REG_CNT_PER_PACKAGE)].regaddr >> 16;
				buf[12 + (i*REG_INFO_WITH)] = (*temp)[i + (dwPackageCnt*REG_CNT_PER_PACKAGE)].regaddr >> 8;
				buf[13 + (i*REG_INFO_WITH)] = (*temp)[i + (dwPackageCnt*REG_CNT_PER_PACKAGE)].regaddr & 0xff;
				buf[14 + (i*REG_INFO_WITH)] = (*temp)[i + (dwPackageCnt*REG_CNT_PER_PACKAGE)].regval >> 24;
				buf[15 + (i*REG_INFO_WITH)] = (*temp)[i + (dwPackageCnt*REG_CNT_PER_PACKAGE)].regval >> 16;
				buf[16 + (i*REG_INFO_WITH)] = (*temp)[i + (dwPackageCnt*REG_CNT_PER_PACKAGE)].regval >> 8;
				buf[17 + (i*REG_INFO_WITH)] = (*temp)[i + (dwPackageCnt*REG_CNT_PER_PACKAGE)].regval & 0xff;
			}

			buf[dwPackageLength - 1] = theApp.CheckSum(buf, dwPackageLength - 1);

			iWrittenBytes = theApp.Send(boxIndex, buf, dwPackageLength);

			if (iWrittenBytes == 0 || iWrittenBytes == -1)
			{
				PrintStr(PRINT_LEVEL_DEBUG, "文件发送失败!");
				return 1;
			}
			if (theApp.verboseLevel)
				PrintToLog(BoxToString(boxIndex)+" 发送配置信息：", buf, iWrittenBytes);
			delete[] buf;
		}
	}
	return 0;
}
int SoftWareApp::Send(int boxtype,char *data,int length)
{
	char errorPrint[40];
	int sendReturn;
	map<int, Spectrometer>::iterator box_iter;
	box_iter = boxs.find(boxtype);
	if (box_iter == boxs.end())
	{	
		//sprintf_s(errorPrint, "send error ocurr %d\n", GetLastError());
		PrintToLogNC("BOX not configured",NULL,0);
		return -1;
	}
	if ((sendReturn=send(box_iter->second.m_SockClient, data, length, 0)) == SOCKET_ERROR)
	{
		sprintf_s(errorPrint, "send error ocurr %d\n", GetLastError());
		PrintStr(PRINT_LEVEL_DEBUG,errorPrint);
		
	}
	return sendReturn;

}
//2016/6/28 support GRADWAVE_i
int SoftWareApp::SendParConfig()
{

	int   regCount,i, j, iWrittenBytes, config1, config2
		, config3, config4, config5, allocat_size,varIndex,boardi,chi,brami;
	//default size
	char* buf = (char *)malloc(BUF_STANDARD_SIZE*sizeof(char)); 
	allocat_size = BUF_STANDARD_SIZE;
	long long int updateValue;
	string::size_type pos;
	paramInfo pi;
	string varname;
	double temp;
	map<string, paramInfo>::iterator paramMapIterator;
	varBoxInfo vboxi;
	varBoardInfo vboardi;
	ostringstream output;
	txWaveParas txwtemp;
	

	for (paramMapIterator = paramMap.begin(); paramMapIterator != paramMap.end(); paramMapIterator++)
	{
		varname = paramMapIterator->first;
		pi = paramMapIterator->second;
		if (pi.pt == TYPE_DOUBLE)
			updateValue = *(long long int *)(&(pi.value.doublelit));
		else
			updateValue = pi.value.intlit;
		if (!varname.compare(0, 9, "GRADWAVE_"))
		{
			varIndex = atoi(varname.substr(9, string::npos).c_str());
			if (varIndex >= 1 && varIndex <= 1024) //valid value
			{
				if (gradwaveCfged)
				{
					if (varIndex > paramCount)
					{
						theApp.PrintToLogNC("var exceed exist wavenumber", NULL, 0);
						goto Lerror;
					}
					temp = gpr[varIndex].param2;
					updateValue = *(long long int *)(&temp);
				}
				else
				{
					theApp.PrintToLogNC("never configured gradwave", NULL, 0);
					goto Lerror;
				}
			}
		}
		else if (!varname.compare(0, 2, "TX"))
		{
			boardi = atoi(varname.substr(2, 1).c_str());

			if (boardi >= 1 && boardi <= 2 && !varname.compare(3, 1, "C"))
			{
				chi = atoi(varname.substr(4, 1).c_str());

				if (chi >= 1 && chi <= 8 && !varname.compare(5, 1, "B"))
				{
					pos = varname.find('_', 0);
					//check
					brami = atoi(varname.substr(6, pos - 6).c_str());
					if (brami >= 1 && brami <= 16)
					{
						txwtemp = theApp.twp[boardi - 1][chi - 1][brami - 1];
						if (txwtemp.set&&theApp.txwaveCfged)
						{
							if (!varname.compare(pos + 1, string::npos, "POINTS"))
								updateValue = *(long long int *)(&txwtemp.points);
							else if (!varname.compare(pos + 1, string::npos, "PERIOD"))
								updateValue = *(long long int *)(&txwtemp.period);
							else if (!varname.compare(pos + 1, string::npos, "BW"))
								updateValue = *(long long int *)(&txwtemp.bw);
						}
						else
						{
							output << varname << "is not configured";
							theApp.PrintToLogNC(output.str(), NULL, 0);
							goto Lerror;
						}
					}
				}
			}

		}
		for (i = 0; i < pi.varBoxInfos.size(); i++)
		{
			vboxi = pi.varBoxInfos.at(i);
			memcpy(buf, "$ETS", 4);
			buf[4] = DATABUS_READ_WRITE_READ;
			buf[7] = DUMMY_CMD;
			buf[8] = DUMMY_CMD;
			buf[9] = DUMMY_CMD;
			regCount = 10;
			for (j = 0; j < vboxi.varBoardInfos.size(); j++)
			{
				vboardi = vboxi.varBoardInfos.at(j);

				switch (vboardi.bt)
				{
				case MAIN_BOARD:
					config1 = MAINCTRL_CTRL_RAM_DATA_1;
					config2 = MAINCTRL_CTRL_RAM_DATA_2;
					config3 = MAINCTRL_CTRL_RAM_DATA_3;
					config4 = MAINCTRL_CTRL_RAM_DATA_4;
					config5 = MAINCTRL_CTRL_RAM_ADDR;
					break;
				case GRADP:
					config1 = GRADP_RAM_DATA_1;
					config2 = GRADP_RAM_DATA_2;
					config3 = GRADP_RAM_DATA_3;
					config4 = GRADP_RAM_DATA_4;
					config5 = GRADP_RAM_ADDR;
					break;
				case GRADR:
					config1 = GRADR_RAM_DATA_1;
					config2 = GRADR_RAM_DATA_2;
					config3 = GRADR_RAM_DATA_3;
					config4 = GRADR_RAM_DATA_4;
					config5 = GRADR_RAM_ADDR;
					break;
				case GRADS:
					config1 = GRADS_RAM_DATA_1;
					config2 = GRADS_RAM_DATA_2;
					config3 = GRADS_RAM_DATA_3;
					config4 = GRADS_RAM_DATA_4;
					config5 = GRADS_RAM_ADDR;
					break;
				case TX1:
					config1 = TX1_RAM_DATA_1;
					config2 = TX1_RAM_DATA_2;
					config3 = TX1_RAM_DATA_3;
					config4 = TX1_RAM_DATA_4;
					config5 = TX1_RAM_ADDR;
					break;
				case TX2:
					config1 = TX2_RAM_DATA_1;
					config2 = TX2_RAM_DATA_2;
					config3 = TX2_RAM_DATA_3;
					config4 = TX2_RAM_DATA_4;
					config5 = TX2_RAM_ADDR;
					break;
				case RX1:
					config1 = RX1_RAM_DATA_1;
					config2 = RX1_RAM_DATA_2;
					config3 = RX1_RAM_DATA_3;
					config4 = RX1_RAM_DATA_4;
					config5 = RX1_RAM_ADDR;
					break;
				case RX2:
					config1 = RX2_RAM_DATA_1;
					config2 = RX2_RAM_DATA_2;
					config3 = RX2_RAM_DATA_3;
					config4 = RX2_RAM_DATA_4;
					config5 = RX2_RAM_ADDR;
					break;
				case RX3:
					config1 = RX3_RAM_DATA_1;
					config2 = RX3_RAM_DATA_2;
					config3 = RX3_RAM_DATA_3;
					config4 = RX3_RAM_DATA_4;
					config5 = RX3_RAM_ADDR;
					break;
				case RX4:
					config1 = RX4_RAM_DATA_1;
					config2 = RX4_RAM_DATA_2;
					config3 = RX4_RAM_DATA_3;
					config4 = RX4_RAM_DATA_4;
					config5 = RX4_RAM_ADDR;
					break;
				default:
					PrintStr(PRINT_LEVEL_DEBUG, "Impossible:board not find");
					break;

				}

				if ((regCount + 50) >= allocat_size)
				{
					//double the size;
					buf = (char *)realloc(buf, allocat_size * 2);
					allocat_size = allocat_size * 2;
				}
				buf[regCount++] = config1 >> 24;
				buf[regCount++] = config1 >> 16;
				buf[regCount++] = config1 >> 8;
				buf[regCount++] = config1 & 0xff;

				buf[regCount++] = 0;
				buf[regCount++] = 0;
				buf[regCount++] = updateValue >> 8;
				buf[regCount++] = updateValue & 0xff;

				buf[regCount++] = config2 >> 24;
				buf[regCount++] = config2 >> 16;
				buf[regCount++] = config2 >> 8;
				buf[regCount++] = config2 & 0xff;

				buf[regCount++] = 0;
				buf[regCount++] = 0;
				buf[regCount++] = updateValue >> 24;
				buf[regCount++] = updateValue >> 16;

				buf[regCount++] = config3 >> 24;
				buf[regCount++] = config3 >> 16;
				buf[regCount++] = config3 >> 8;
				buf[regCount++] = config3 & 0xff;

				buf[regCount++] = 0;
				buf[regCount++] = 0;
				buf[regCount++] = updateValue >> 40;
				buf[regCount++] = updateValue >> 32;

				buf[regCount++] = config4 >> 24;
				buf[regCount++] = config4 >> 16;
				buf[regCount++] = config4 >> 8;
				buf[regCount++] = config4 & 0xff;

				buf[regCount++] = 0;
				buf[regCount++] = 0;
				buf[regCount++] = updateValue >> 56;
				buf[regCount++] = updateValue >> 48;

				buf[regCount++] = config5 >> 24;
				buf[regCount++] = config5 >> 16;
				buf[regCount++] = config5 >> 8;
				buf[regCount++] = config5 & 0xff;

				buf[regCount++] = 0;
				buf[regCount++] = 0;
				buf[regCount++] = vboardi.updateAddr >> 8;
				buf[regCount++] = vboardi.updateAddr & 0xFF;
			}

		}
		regCount++;
		buf[5] = HIBYTE(regCount);
		buf[6] = LOBYTE(regCount);
		buf[regCount - 1] = theApp.CheckSum(buf, regCount - 1);

		iWrittenBytes = theApp.Send(vboxi.boxt, buf, regCount);
		theApp.PrintToLog(BoxToString(vboxi.boxt)+" 发送par中参数指令：", buf, iWrittenBytes);
	}
	   free(buf);
	   return RIGHT_IN_PROCESS;
   Lerror:
	   free(buf);
	   return ERROR_IN_PROCESS;
}
DWORD SoftWareApp::ParseFile(string fileName, map<int, vector<reg_info_t >> &regconfiginfo)
{

	string readline;
	ifstream file;
	file.open(fileName.c_str(),ios::in);
	int boxindex = 0;
	reg_info_t ri ={0};
	unordered_map<std::string, int>::iterator iter;
	pair<map<int, vector<reg_info_t>>::iterator, bool> ret;
	vector<reg_info_t> b_reg_info;
	vector<reg_info_t> *p_reg_info;
	//compatible with last fcode default is Main Box
	ret = regconfiginfo.insert(make_pair(0, b_reg_info));
	p_reg_info = &(ret.first->second);
	if(!file.is_open())
	{
		FormatOutPut(PRINT_LEVEL_DEBUG,"%s open failed", fileName.c_str());
		return ERROR_IN_PROCESS;
	} 
	while(getline(file, readline))
	{
		if (!readline.compare(0, 1, "#") || readline.empty())
			continue;
		if (readline[0] == '$')
		{
			iter = boxNameHash.find(readline.substr(1));
			if (iter == boxNameHash.end())
			{
				
				FormatOutPut(PRINT_LEVEL_DEBUG, "in fcode box  %s not find", readline.c_str());

				return ERROR_IN_PROCESS;
			}
			boxindex = iter->second;
			ret=regconfiginfo.insert(make_pair(boxindex, b_reg_info));
			p_reg_info = &(ret.first->second);
			continue;
		}
		    ri.regaddr = StringToHex_S(readline);
			ri.regval = StringToHex_S(&readline[9]);
			p_reg_info->push_back(ri);
	}

	file.close();
	return RIGHT_IN_PROCESS;
}
char SoftWareApp::CheckSum(char *buf, DWORD length)
{
	char sum = 0;

	for(DWORD i=0;i<length;i++)
	{
		sum ^= buf[i];
	}

	return sum;
}
DWORD SoftWareApp::StringToHex_S(string strTemp)
{
	int hexdata,lowhexdata,data=0;
	int hexdatalen=0;
	int hexdatacount=0;
	
	string::size_type len=strTemp.length();
	unsigned char HexBuf[10];
	for(int i=0;i<len;i++)
	{
		if((strTemp[i]==',')||(strTemp[i]==' ')||(strTemp[i]=='\r')||(strTemp[i]=='\n'))
		{
			break;
		}
		else
			hexdatacount++;
	}

	for(int i=0;i<hexdatacount;)
	{
		char lstr,hstr;

		if(i==0 && (hexdatacount%2 != 0))
		{
			hstr = 0x30;
			lstr=strTemp[i];
			i++;
		}
		else
		{
			hstr=strTemp[i];
			i++;
			lstr=strTemp[i];
			i++;
		}
		hexdata=ConvertHexData(hstr);
		lowhexdata=ConvertHexData(lstr);
		if((hexdata==16)||(lowhexdata==16))
			break;
		else
			hexdata=hexdata*16+lowhexdata;

		HexBuf[hexdatalen]=(unsigned char)hexdata;
		hexdatalen++;
	}
	for (UINT i = 0; i<hexdatalen; i++)
	{
		data <<= 8;
		data += HexBuf[i];
	}
	return data;
}
char  SoftWareApp::ConvertHexData(char ch)
{
	if((ch>='0')&&(ch<='9'))
		return ch-0x30;
	if((ch>='A')&&(ch<='F'))
		return ch-'A'+10;
	if((ch>='a')&&(ch<='f'))
		return ch-'a'+10;
	else return(-1);
}
char  SoftWareApp::ConvertItoS(char ch)
{
	string temp;
	if ((ch >= 0) && (ch <= 9))
		return ch + 0x30;
	if ((ch >= 10) && (ch <= 15))
		return ch - 10 + 'A';
	else return 0;
}
void SoftWareApp::InitSample(int sampleSize,int sample ,int echo)
{
	map<int, Spectrometer>::iterator box_iter;
	for (box_iter = boxs.begin(); box_iter != boxs.end(); box_iter++)
	{
		(box_iter->second).SInitSample(sampleSize, sample,echo);
	}

}
void SoftWareApp::reConfigDAC(int addr,boardType bt)
{
	int  j, regIndex, cmdReg, dataReg, addrReg,iWrittenBytes, dacSel,boardIndex=0;
	switch (bt)
	{
	case TX1:
		cmdReg = TX1_DATABUS_CMD_REG_ADDR;
		dataReg = TX1_DATABUS_DATA_REG_ADDR;
		addrReg = TX1_DATABUS_ADDR_REG_ADDR;
		break;
	case TX2:
		cmdReg = TX2_DATABUS_CMD_REG_ADDR;
		dataReg = TX2_DATABUS_DATA_REG_ADDR;
		addrReg = TX2_DATABUS_ADDR_REG_ADDR;
		break;
	default:
		PrintStr(PRINT_LEVEL_DEBUG,"Config Tx Reg,board param Error");
		return;
	}
	boardIndex = bt - TX1;
	char *buf = new char[BUF_STANDARD_SIZE * 4];
	memcpy(buf, "$ETS", 4);
	buf[4] = DATABUS_READ_WRITE_READ;
	buf[7] = DUMMY_CMD;
	buf[8] = DUMMY_CMD;
	buf[9] = DUMMY_CMD;
	ConfigReg(&buf[10], cmdReg, 0);
	regIndex = 18;
	dacSel = addr&0xE000;
	for (j = 0; j < dacRegsSize[boardIndex][dacSel >> 13]; j++)
	{
		regIndex += ConfigReg(&buf[regIndex], dataReg, dacRegs[boardIndex][dacSel>>13][j].regval);
		regIndex += ConfigReg(&buf[regIndex], addrReg, dacRegs[boardIndex][dacSel >> 13][j].regaddr | dacSel);
	}
	regIndex++;
	buf[5] = HIBYTE(regIndex);
	buf[6] = LOBYTE(regIndex);

	buf[regIndex - 1] = theApp.CheckSum(buf, regIndex - 1);
	iWrittenBytes = theApp.Send(M, buf, regIndex);
	theApp.PrintToLog("重新发送设置发射板DAC指令：", buf, iWrittenBytes);

	//query

	memcpy(buf, "$ETR", 4);
	buf[4] = DATABUS_READ_WRITE_READ;
	buf[7] = bt - TX1;
	buf[8] = DUMMY_CMD;
	buf[9] = DUMMY_CMD;
	//ConfigReg(&buf[10], gainSel, 0);
	//ConfigReg(&buf[18], dacCfgReg, 0);
	//ConfigReg(&buf[26], dacCfgReg, 0xF00);
	ConfigReg(&buf[10], cmdReg, 1);
	//ConfigReg(&buf[10], cmdReg, 1);
	regIndex = 18;
	regIndex += ConfigReg(&buf[regIndex], addrReg, 0x5 | dacSel);
	regIndex++;
	buf[5] = HIBYTE(regIndex);
	buf[6] = LOBYTE(regIndex);
	buf[regIndex - 1] = theApp.CheckSum(buf, regIndex - 1);
	iWrittenBytes = theApp.Send(M, buf, regIndex);
	theApp.PrintToLog("重新发送查询发射板ADC FIFO ALARM指令：", buf, iWrittenBytes);
	delete[] buf;

}

void SoftWareApp::CloseDmaFile()
{
	map<int, Spectrometer>::iterator box_iter;
	for (box_iter = boxs.begin(); box_iter != boxs.end(); box_iter++)
	{
		box_iter->second.SCloseDmaFile();
	}
}
//default for ANKE 
//1 for Firstech Aha
void WINAPI SetRawDataFormat(int sel)
{
	theApp.raw_data_format = sel;
}

int SoftWareApp:: itoaline(int value,char *str)
{
	const char table[]="0123456789";
	int i=0,j,start,length;
	bool negative =FALSE;
	// 	if(value==0)
	// 	{
	//        
	// 	}
	if(value<0)
	{
		str[0]='-';
		value=0-value;
		negative=TRUE;
		i=1;
	}
	do 
	{
		str[i++]=table[value%10];
	} while ((value/=10));
	// 	for(i=1;value>0;i++,value/=10)
	// 		str[i]=value%10+'0';
	str[i]='\r';
	str[i+1]='\n';
	str[i+2]='\0';
	start=negative?1:0;
	length=i+2;
	for(j=i-1;start<j;j--,start++)
	{
		str[start]=str[start]^str[j];
		str[j]=str[start]^str[j];
		str[start]=str[start]^str[j];
	}

	return length;

}
/*
int NewParFile(char * filename)
{
   if(!filename)
   {
     PrintStr(PRINT_LEVEL_ERROR,"没有指定文件名");
	 return 1;
   }
   string inputfilename=filename,pwd;
   string cmd="p2f -dump par ",rawfilename;
   string::size_type pos;
   ifstream file;
   cmd+=inputfilename;
   system(cmd.c_str());
   rawfilename=inputfilename.substr(0,inputfilename.find(".",0));
   rawfilename+=".par";
   file.open(rawfilename.c_str(),ios::in);
   if(!file.is_open())
   {
        PrintStr(PRINT_LEVEL_ERROR,"文件创建失败，检查SRC文件是否编译通过");
		return 1;

   }
   pos = inputfilename.find_last_of('\\');
   pwd = inputfilename.substr(0, pos + 1);
   theApp.paramMap.clear();
   return theApp.loadParFile( file);

}
*/
bool CheckBoxNameValid(string boxname,int &boxindex)
{
	int i;
	if (boxname.compare("M"))
	{
		if (!boxname.compare(0,1,"E"))
		{
			for (i = 1; i < boxname.length(); i++)
				if (boxname[i] > '9' || boxname[i] < '0')
					return false;
		}
		else return false;
	}
	else
	{
		boxindex = 0;
		return true;
	}
	boxindex = atoi(boxname.substr(1).c_str());
	return true;
}
int SoftWareApp::loadParFile(ifstream &filename)
{
	int lineindex, i,boxindex;
	paramInfo pi;
	paramType pt;
	string::size_type posstart, posend, pos, pos1, splitcharpos;
	map<string, boardType>::iterator boardt;
	unordered_map<string, int>::iterator boxt;
	string linestr(""), strtemp, varname;
	varBoardInfo varBoardI;
	varBoxInfo varBI;
	getline(filename, linestr); ///filter first line
	//find src name
	posstart = linestr.find('\t');
	theApp.srcfilename = linestr.substr(posstart + 1, string::npos);
	//first check src file with '\\' 
	pos = linestr.find_last_of('\\');
	pos1 = theApp.srcfilename.find_last_of('.');
	if (pos == string::npos)
		theApp.downfilename = theApp.srcfilename.substr(0, pos1) + ".fcode";
	else
		theApp.downfilename = theApp.srcfilename.substr(0, pos1) + ".fcode";
	//
	lineindex = 2;
	while (getline(filename, linestr))
	{
		if (!linestr.length()) break;
		posstart = linestr.find('\t');
		strtemp = linestr.substr(1, posstart - 1);
		if (!strtemp.compare("double"))
			pt = TYPE_DOUBLE;
		else if (!strtemp.compare("int"))
			pt = TYPE_INT;
		else
		{			
			FormatOutPut(PRINT_LEVEL_ERROR, "In the PAR file ,line:%d,type is not recognise", lineindex);
			errorno = EPARPARSE;
			return ERROR_IN_PROCESS;
		}
		pi.pt = pt;
		posend = linestr.find(',', posstart);
		strtemp = linestr.substr(posstart + 1, posend - posstart - 1);
		varname = strtemp;
		posstart = posend;
		posend = linestr.find(' ', posstart);
		strtemp = linestr.substr(posstart + 1, posend - posstart - 1);
		if (pt == TYPE_DOUBLE)
			pi.value.doublelit = atof(strtemp.c_str());
		else if (pt == TYPE_INT)
			pi.value.intlit = atoi(strtemp.c_str());
		if (!varname.compare("samplePeriod"))
		{
			if (!theApp.CheckSamplePeriod(pi.value.doublelit, true))
			{
				PrintStr(PRINT_LEVEL_ERROR, "not suppert this sample period");
				
				return ERROR_IN_PROCESS;
			}
		}
		pi.needUpdate = false;
		//means current box decl end
		splitcharpos = linestr.find(':', posend + 2);

		while ((posstart = linestr.find(',', posend)) != string::npos)
		{
			posend = linestr.find('(', posstart);
			//means the end of board
			if (posend >= splitcharpos)
			{

				if (splitcharpos == string::npos)
					//line is parse done
				{
					strtemp = linestr.substr(posstart + 1, linestr.size() - posstart - 1);

				}
				else
				{

					strtemp = linestr.substr(posstart + 1, splitcharpos - posstart - 2);
					splitcharpos = linestr.find(':', posend);

				}
				if (isEdit)
				{
					if (!CheckBoxNameValid(strtemp, boxindex))
					{
						FormatOutPut(PRINT_LEVEL_ERROR, "In the PAR file ,line:%d,box %s is not recognise", lineindex, strtemp.c_str());
						errorno = EPARPARSE;
						return ERROR_IN_PROCESS;
					}
					varBI.boxt = boxindex;
				}
				else
				{
					boxt = theApp.boxNameHash.find(strtemp);
					if (boxt == theApp.boxNameHash.end())
					{
						FormatOutPut(PRINT_LEVEL_ERROR, "In the PAR file ,line:%d,BOX is not recognise", lineindex);
						errorno = EPARPARSE;
						return ERROR_IN_PROCESS;
					}
					varBI.boxt = boxt->second;
				}
				
				pi.varBoxInfos.push_back(varBI);
				if (posend == string::npos) break;
				else
				{
					//shift posend to befor pos
					posend = posstart + 1;
					varBI.varBoardInfos.clear();
					continue;
				}

			}
			strtemp = linestr.substr(posstart + 1, posend - posstart - 1);
			
			
				boardt = theApp.boardNameHash.find(strtemp);
				if (boardt == theApp.boardNameHash.end())
				{
					FormatOutPut(PRINT_LEVEL_ERROR, "In the PAR file ,line:%d,board is not recognise", lineindex);
					errorno = EPARPARSE;
					return ERROR_IN_PROCESS;
				}
			
			else
				varBoardI.bt = boardt->second;
			posstart = posend;
			posend = linestr.find(')', posstart);
			strtemp = linestr.substr(posstart + 1, posend - posstart - 1);
			varBoardI.updateAddr = atoi(strtemp.c_str());
			varBI.varBoardInfos.push_back(varBoardI);
		}

		lineindex++;
		theApp.paramMap.insert(pair<string, paramInfo>(varname, pi));
		pi.varBoxInfos.clear();
		varBI.varBoardInfos.clear();
	}
	//load the wave map
	wavefile temp;
	std::vector<std::string> result;
	while (getline(filename, linestr))
	{
		if (!linestr.length()) break;
		result = split(linestr, " ");
		if (!result[0].compare("GRAD"))
		{
			temp.wavefile_tag = "GRAD";
			temp.wavefile_name = result[1];
			if (!isEdit)
			{
				if (SetGradientWave(result[1].c_str()) == ERROR_IN_PROCESS)
				{
					errorno = EPARPARSEGRADWAVE;
					return ERROR_IN_PROCESS;
				}
			}
			theApp.waveinfo.push_back(temp);
		}
			else if (!result[0].compare("TX"))
			{
				temp.wavefile_tag = "TX";
				temp.ch_no = atoi(result[1].c_str());
				temp.bram_no = atoi(result[2].c_str());
				temp.wavefile_name = result[3];
				temp.wavename = result[4];
				if (!isEdit)
				{
					if (SetRFWaveTable(M, TX1, temp.ch_no, temp.bram_no, temp.wavefile_name, temp.wavename) == ERROR_IN_PROCESS)
					{
						errorno = EPARPARSERFWAVE;
						return ERROR_IN_PROCESS;
					}
				}
				theApp.waveinfo.push_back(temp);
			}
			
	}
	if (!filename.eof())
	{
		Json::Reader reader;
		if (!reader.parse(filename, root))
		{
			PrintStr(PRINT_LEVEL_ERROR,"Error parse RAM data");
			errorno = EPARPARSE;
			return ERROR_IN_PROCESS;
		}
		else if (!isEdit)
		{
			//down
			for (i = 0; i < 2; i++)
			{ 
				if (!root["TX_RAM"][i]["phase"].isNull())
					DownTxPhaseTable(i);
				if (!root["TX_RAM"][i]["freq"].isNull())
					DownTxFreqTable(i);
				if (!root["TX_RAM"][i]["gain"].isNull())
					DownTxGainTable(i);
			}
			if (!root["RX_RAM"]["phase"].isNull())
				DownRxPhaseTable();
			if (!root["RX_RAM"]["freq"].isNull())
				DownRxFreqTable();
			if (!root["ReadGain"].isNull())
				DownGradientGain(CHANNEL_X);
			if (!root["PhaseGain"].isNull())
				DownGradientGain(CHANNEL_Y);
			if (!root["SliceGain"].isNull())
				DownGradientGain(CHANNEL_Z);
		}
	}
	filename.close();
	return RIGHT_IN_PROCESS;
}
int WINAPI SaveShimValues()
{
	string filename = theApp.configPwd + "shimming.txt";
	ofstream file(filename, ios::out );
	
	if (!file.is_open())
	{
		PrintStr(PRINT_LEVEL_ERROR, "shim file open failed");
		errorno = EFILEOPENFAILED;
		return ERROR_IN_PROCESS;
	}
	file << "X=" << theApp.shimValues[CHANNEL_X] << endl;
	file << "Y=" << theApp.shimValues[CHANNEL_Y] << endl;
	file << "Z=" << theApp.shimValues[CHANNEL_Z] << endl;
	file << "B0=" << theApp.shimValues[CHANNEL_B0] << endl;
	file.close();
	return RIGHT_IN_PROCESS;

}
void SetChannelValue(int channel,float value)
{
	int  dwPackageLength = 0,iWrittenBytes;
	int  channel_low_addr,channel_high_addr,channelvalue;
	channelvalue=*(int *)&value;
	switch(channel)
	{
	case CHANNEL_X:
		channel_low_addr=GRADIENT_OFFSET_LW_X;
		channel_high_addr=GRADIENT_OFFSET_HI_X;
		break;
	case CHANNEL_Y:
		channel_low_addr=GRADIENT_OFFSET_LW_Y;
		channel_high_addr=GRADIENT_OFFSET_HI_Y;
		break;
	case CHANNEL_Z:
		channel_low_addr=GRADIENT_OFFSET_LW_Z;
		channel_high_addr=GRADIENT_OFFSET_HI_Z;
		break;
	case CHANNEL_B0:
		channel_low_addr=GRADIENT_OFFSET_LW_B0;
		channel_high_addr=GRADIENT_OFFSET_HI_B0;
		break;
	default:
		PrintStr(PRINT_LEVEL_ERROR,"channel No must in scope (0~3)");
        return; 
	}
	/*save values*/
	theApp.shimValues[channel] = value;
	dwPackageLength = REG_INFO_WITH*2 + 11;
	char* buf=new char[dwPackageLength];
	memcpy(buf,"$ETS",4);
	buf[4] = DIRECT_REG_WRITE_READ;
	buf[5] = HIBYTE(dwPackageLength);
	buf[6] = LOBYTE(dwPackageLength);

	buf[7] = DUMMY_CMD;
	buf[8] = DUMMY_CMD;
	buf[9] = DUMMY_CMD;


	buf[10] = channel_low_addr >> 24;
	buf[11] = channel_low_addr >> 16;
	buf[12] = channel_low_addr >> 8;
	buf[13] = channel_low_addr & 0xff;
    
	buf[14] = 0;
	buf[15] = 0;
	buf[16] = channelvalue >> 8;
	buf[17] = channelvalue &0xff;

	buf[18] = channel_high_addr >> 24;
	buf[19] = channel_high_addr >> 16;
	buf[20] = channel_high_addr >> 8;
	buf[21] = channel_high_addr & 0xff;

	buf[22] = 0;
	buf[23] = 0;
	buf[24] = channelvalue >> 24;
	buf[25] = channelvalue >>16;
     
	buf[dwPackageLength -1] = theApp.CheckSum(buf,dwPackageLength -1);

	iWrittenBytes = theApp.Send(M,buf,dwPackageLength);
	theApp.PrintToLog("发送设置匀场参数指令：",buf,iWrittenBytes);
	delete[] buf;

	 
}
/******************************
*This function is used to parse packet
*
*******************************/
void ParsePack(int boxtype,char *databuf,int dataLength,string prefix)
{
	int packNum, LastPack, i, iWrittenBytes, index;
	char* buf = new char[ONE_PACK_MAX_SIZE];
	packNum = dataLength / (ONE_PACK_MAX_SIZE - 11);
	LastPack = dataLength % (ONE_PACK_MAX_SIZE - 11);
	index = 0;
	for (i = 0; i < packNum; i++)
	{
		memcpy(buf, "$ETS", 4);
		buf[4] = DATABUS_READ_WRITE_READ;
		buf[5] = 0xFF;
		buf[6] = 0xFB;
		buf[7] = DUMMY_CMD;
		buf[8] = DUMMY_CMD;
		buf[9] = DUMMY_CMD;
		memcpy(&buf[10], &databuf[index], 65520);
		index += 65520;
		buf[ONE_PACK_MAX_SIZE - 1] = theApp.CheckSum(buf, ONE_PACK_MAX_SIZE - 1);
		iWrittenBytes = theApp.Send(M, buf, ONE_PACK_MAX_SIZE);
		theApp.PrintToLog(prefix, buf, iWrittenBytes);
	}
	if (LastPack)
	{
		memcpy(buf, "$ETS", 4);
		buf[4] = DATABUS_READ_WRITE_READ;
		buf[5] = HIBYTE(LastPack + 11);
		buf[6] = LOBYTE(LastPack + 11);
		buf[7] = DUMMY_CMD;
		buf[8] = DUMMY_CMD;
		buf[9] = DUMMY_CMD;
		memcpy(&buf[10], &databuf[index], LastPack);
		buf[LastPack + 10] = theApp.CheckSum(buf, LastPack + 10);
		iWrittenBytes = theApp.Send(boxtype, buf, LastPack + 11);
		theApp.PrintToLog(prefix, buf, iWrittenBytes);
	}
	delete[] buf;

}
void DownGradientGain(int channel)
{
	int  len,totalLength, gradient_start_addr, i, channel_low_addr, channel_high_addr, intvalue, regCount, clrvalue;
	string key;
	float temp;
	switch (channel)
	{
	case CHANNEL_X:
		clrvalue = 0x8;
		gradient_start_addr = GRAIDENT_SCALE_X;
		key = "ReadGain";
		break;
	case CHANNEL_Y:
		clrvalue = 0x10;
		gradient_start_addr = GRAIDENT_SCALE_Y;
		key = "PhaseGain";
		break;
	case CHANNEL_Z:
		clrvalue = 0x20;
		gradient_start_addr = GRAIDENT_SCALE_Z;
		key = "SliceGain";
		break;
	default:
		PrintStr(PRINT_LEVEL_ERROR, "set gradient scale error:channel is not in scope");
		return;
	}
	channel_low_addr = gradient_start_addr * 2;
	channel_high_addr = (gradient_start_addr + 1) * 2;
	len = theApp.root[key].size();
	totalLength = len * 16 + 16;//last 16 for clr reg
	char* buf = new char[totalLength];
	regCount = 0;
	//load data 
	regCount += clrBeforeData(&buf[regCount], GRAIDENT_CLR_REG * 2, clrvalue);
	for (i = 0; i < len; i++)
	{
		temp = theApp.root[key][i].asFloat();
		intvalue = *(int *)&temp;
		regCount += ConfigReg(&buf[regCount], channel_low_addr, intvalue & 0xFFFF);
		regCount += ConfigReg(&buf[regCount], channel_high_addr, intvalue >> 16);
	}
	ParsePack(M, buf, totalLength, "发送设置 "+key+" 梯度增益列表：");
	delete[] buf;
}
int WINAPI SetGradientAllScale(int channel,float *bufIn,int len)
{ 
	int i;
	string key;
	int limit;
	switch (channel)
	{
	case CHANNEL_X:
		key = "ReadGain";
		limit = GRADIENT_MAX_SCALE;
		break;
	case CHANNEL_Y:
		key = "PhaseGain";
		limit = 16384;
		break;
	case CHANNEL_Z:
		key = "SliceGain";
		limit = 16384;
		break;
	default:
		PrintStr(PRINT_LEVEL_ERROR, "set gradient scale error:channel is not in scope");
		return ERROR_IN_PROCESS;
	}
   if (len > limit)
   {
	   FormatOutPut(PRINT_LEVEL_ERROR, "max scale  is %d", GRADIENT_MAX_SCALE);
	   return ERROR_IN_PROCESS;
   }
   Json::Value temp;
   for (i = 0; i < len; i++)
	   temp.append(bufIn[i]);
   theApp.root[key] = temp;
   //if used offline return directly
   if (theApp.isEdit||theApp.boxs.count(0)<0)  return RIGHT_IN_PROCESS;
   DownGradientGain(channel);
   return RIGHT_IN_PROCESS;
}
int WINAPI SetAllMaxtrixValueForJava(float *bufIn, int len)
{
	int i, j;

	if (len > MAX_MATRIX_NUM*9)
	{
		FormatOutPut(PRINT_LEVEL_ERROR, " max maxtrix  is %d", MAX_MATRIX_NUM);
		return ERROR_IN_PROCESS;
	}
	
	//if used offline return directly
	Json::Value temp;
	for (i = 0; i<len/9; i++)
		for (j = 0; j < 9; j++)
		{
			theApp.maxtrixvalue[i][j] = bufIn[i * 9 + j];
			temp[i].append(bufIn[i * 9 + j]);
		}
	
	theApp.root["Matrix"] = temp;
	theApp.matrixRCount = len;
	return RIGHT_IN_PROCESS;

}
//one line is 9 element
int WINAPI SetAllMaxtrixValue(float bufIn[][9],int len)
{
	int i, j;

		if (len > MAX_MATRIX_NUM )
		{
			FormatOutPut(PRINT_LEVEL_ERROR, " max maxtrix  is %d", MAX_MATRIX_NUM);
			return ERROR_IN_PROCESS;
		}
		Json::Value temp;
		for (i = 0; i<len; i++)
			for (j = 0; j<9; j++)
			temp[i].append(bufIn[i][j]);
		theApp.root["Matrix"] = temp;
	return RIGHT_IN_PROCESS;
}
void SoftWareApp::DownMatrix()
{
   if (root["Matrix"].isNull()) return;
   int i, j, channel_low_addr, channel_high_addr, intvalue, regCount, iWrittenBytes;
   float temp[9];
   char* buf=new char[144*root["Matrix"].size() +100];
   memcpy(buf,"$ETS",4);
   buf[4] = DIRECT_REG_WRITE_READ;
   buf[7] = DUMMY_CMD;
   buf[8] = DUMMY_CMD;
   buf[9] = DUMMY_CMD;
   //regCount=10;
   regCount = 10 + clrBeforeData(&buf[10], GRAIDENT_CLR_REG * 2, 0x4);
   for(i=0;i<root["Matrix"].size();i++)
   {
	   for (j = 0; j < 9; j++)
		   temp[j] = root["Matrix"][i][j].asFloat();
	   Matrix::MatrxiCalc(matrixT, temp, matrixP, matrixG);
	   for(j=0;j<9;j++)
	   {
	   intvalue=*(int *)&temp[j];
	   if(j<3)
	   {
		   channel_low_addr=(GRAIDENT_ROTA_X+j*2)<<1;
		   channel_high_addr=(GRAIDENT_ROTA_X+1+j*2)<<1;	
	   }
	   else if(j>=3&&j<6)
	   {
		   channel_low_addr=(GRAIDENT_ROTA_Y+(j-3)*2)<<1;
		   channel_high_addr=(GRAIDENT_ROTA_Y+1+(j-3)*2)<<1;	
	   }
	   else 
	   {
		   channel_low_addr=(GRAIDENT_ROTA_Z+(j-6)*2)<<1;
		   channel_high_addr=(GRAIDENT_ROTA_Z+1+(j-6)*2)<<1;	

	   } 


	   buf[regCount++] = channel_low_addr >> 24;
	   buf[regCount++] = channel_low_addr >> 16;
	   buf[regCount++] = channel_low_addr >> 8;
	   buf[regCount++] = channel_low_addr & 0xff;

	   buf[regCount++] = 0;
	   buf[regCount++] = 0;
	   buf[regCount++] = intvalue >> 8;
	   buf[regCount++] = intvalue &0xff;

	   buf[regCount++] = channel_high_addr >> 24;
	   buf[regCount++] = channel_high_addr >> 16;
	   buf[regCount++] = channel_high_addr >> 8;
	   buf[regCount++] = channel_high_addr & 0xff;

	   buf[regCount++] = 0;
	   buf[regCount++] = 0;
	   buf[regCount++] = intvalue >> 24;
	   buf[regCount++] = intvalue >>16;
   }
   }
   regCount++;
   buf[5] = HIBYTE(regCount);
   buf[6] = LOBYTE(regCount);

   buf[regCount -1] = theApp.CheckSum(buf,regCount -1);
   iWrittenBytes = theApp.Send(M,buf,regCount);
   theApp.PrintToLog("发送设置所有旋转矩阵参数指令：",buf,iWrittenBytes);
   delete[] buf;
}
int ConfigQuiry(char *buf, int addr)
{
	buf[0] = addr >> 24;
	buf[1] = addr >> 16;
	buf[2] = addr >> 8;
	buf[3] = addr & 0xFF;
	return 4;
}
int ConfigQuiry2(char *buf, int addr)
{
	buf[0] = addr >> 8;
	buf[1] = addr & 0xFF;
	return 2;
}
int ConfigReg(char *buf,int addr,int value)
{
	buf[0]=addr>>24;
	buf[1]=addr>>16;
	buf[2]=addr>>8;
	buf[3]=addr&0xFF;
	buf[4]=value>>24;
	buf[5]=value>>16;
	buf[6]=value>>8;
	buf[7]=value&0xFF;
	return 8;

}
int clrBeforeData(char *buf,int addr,int value)
{
  
  buf[0]=addr>>24;
  buf[1]=addr>>16;
  buf[2]=addr>>8;
  buf[3]=addr&0xFF;
  buf[4]=value>>24;
  buf[5]=value>>16;
  buf[6]=value>>8;
  buf[7]=value&0xFF;

  buf[8]=addr>>24;
  buf[9]=addr>>16;
  buf[10]=addr>>8;
  buf[11]=addr&0xFF;
  buf[12]=0;
  buf[13]=0;
  buf[14]=0;
  buf[15]=0;
  return 16;


}
int SetGradientWave(const char *filename)
{
	string readline,pefvaluestr;
	float  temp;
	
	float  *waveRamValues = new float[GRADIENT_MAX_WAVE_RAM];
	
	int i=0,tempvalue,intvalue,regCount,
	iWrittenBytes, realwaveRamCount,waveRamCount,state,singleCount,points,allcate_size;
	string::size_type startpos,endpos;
	ifstream preempfile(filename,ios::in);
	if(!preempfile.is_open())
	{
		PrintStr(PRINT_LEVEL_ERROR,"gradient wave file is not exist");
		return ERROR_IN_PROCESS;
	}
	// 1 start 
	state=1;
	paramCount=0;
	waveRamCount=0;
	while(getline(preempfile,readline))
	{
		//comment
		if(!readline.compare(0,1,"#")||readline.empty())
			continue;
		startpos=0;
		switch(state)
		{
        //start 
		case 1:
			{
				i = 0;
				while((endpos=readline.find(',',startpos))!=string::npos)
				{
					if (paramCount == GRADIENT_MAX_PARAM)
					{
						PrintStr(PRINT_LEVEL_ERROR,"gradient param is exceed 1024");
						return ERROR_IN_PROCESS;
					}
					pefvaluestr=readline.substr(startpos,endpos-startpos);
					startpos=++endpos;

					if(!pefvaluestr.compare("lut"))
					{
						gpr[paramCount].paramiscalc=false;
					    state=2;
						singleCount=0;
						
					}
					else if(!pefvaluestr.compare("calc"))
					{
					   gpr[paramCount].paramiscalc=true;
					   state=1;
					}else 
					{
						if (!i)
						{
							PrintStr(PRINT_LEVEL_ERROR,"gradient wave file param tag error (lut/calc)");
							return ERROR_IN_PROCESS;
						}
					   if(gpr[paramCount].paramiscalc)
					   {
                        temp=atof(pefvaluestr.c_str()); 
                        gpr[paramCount].param1=*(int *)&temp;
					   }
					   else
                        gpr[paramCount].param1=atoi(pefvaluestr.c_str());
					          
					
					}
					i++;
				}
				if(startpos==0)
				{
				   PrintStr(PRINT_LEVEL_ERROR,"lut points is not match the wave points");
				   return ERROR_IN_PROCESS;
				 
				}
				if(startpos<readline.size())
				{
				    pefvaluestr=readline.substr(startpos,readline.size()-startpos);
					gpr[paramCount].param2=atoi(pefvaluestr.c_str());
					points=gpr[paramCount].param2;
					paramCount++;
				}
				
			
			}	
             break;
		case 2:
            singleCount++;
			if (waveRamCount == GRADIENT_MAX_WAVE_RAM)
			{
				PrintStr(PRINT_LEVEL_ERROR,"gradient wave ram is exceed 256K");
				return ERROR_IN_PROCESS;
			}
			waveRamValues[waveRamCount++]=atof(readline.c_str());
		    if(singleCount==points)
            state=1;
			break;
		default:
			break;
		}	 

	}
	preempfile.close();
	allcate_size=(waveRamCount*16)>(32*paramCount)?(waveRamCount*16):(32*paramCount);
	char* buf=new char[allcate_size+100];
	memcpy(buf,"$ETS",4);
	buf[4] = DIRECT_REG_WRITE_READ;

	buf[7] = DUMMY_CMD;
	buf[8] = DUMMY_CMD;
	buf[9] = DUMMY_CMD;
	//clr reg 
	
	regCount=10+clrBeforeData(&buf[10],GRAIDENT_CLR_REG*2,0x2);
	//first send param ram
    for(i=0;i<paramCount;i++)
	{
	    if(gpr[i].paramiscalc)
		{
			buf[regCount++] = GRAIDENT_PARAM_RAM_REG1 >> 24;
			buf[regCount++] = GRAIDENT_PARAM_RAM_REG1 >> 16;
			buf[regCount++] = GRAIDENT_PARAM_RAM_REG1 >> 8;
			buf[regCount++] = GRAIDENT_PARAM_RAM_REG1 & 0xff; 
		    
			buf[regCount++]=0;
			buf[regCount++]=0;
			buf[regCount++]=gpr[i].param1>>8;
			buf[regCount++]=gpr[i].param1&0xFF;

			buf[regCount++] = GRAIDENT_PARAM_RAM_REG2 >> 24;
			buf[regCount++] = GRAIDENT_PARAM_RAM_REG2 >> 16;
			buf[regCount++] = GRAIDENT_PARAM_RAM_REG2 >> 8;
			buf[regCount++] = GRAIDENT_PARAM_RAM_REG2 & 0xff; 

			buf[regCount++]=0;
			buf[regCount++]=0;
			buf[regCount++]=gpr[i].param1>>24;
			buf[regCount++]=gpr[i].param1>>16;

			buf[regCount++] = GRAIDENT_PARAM_RAM_REG3 >> 24;
			buf[regCount++] = GRAIDENT_PARAM_RAM_REG3 >> 16;
			buf[regCount++] = GRAIDENT_PARAM_RAM_REG3 >> 8;
			buf[regCount++] = GRAIDENT_PARAM_RAM_REG3 & 0xff; 

			buf[regCount++]=0;
			buf[regCount++]=0;
			buf[regCount++]=gpr[i].param2>>8;
			buf[regCount++]=gpr[i].param2&0xFF;

			buf[regCount++] = GRAIDENT_PARAM_RAM_REG4 >> 24;
			buf[regCount++] = GRAIDENT_PARAM_RAM_REG4 >> 16;
			buf[regCount++] = GRAIDENT_PARAM_RAM_REG4 >> 8;
			buf[regCount++] = GRAIDENT_PARAM_RAM_REG4 & 0xff; 

			buf[regCount++]=0;
			buf[regCount++]=0;
			buf[regCount++]=0;
			buf[regCount++]=gpr[i].param2>>16|0x10;
		}else
		{
			buf[regCount++] = GRAIDENT_PARAM_RAM_REG1 >> 24;
			buf[regCount++] = GRAIDENT_PARAM_RAM_REG1 >> 16;
			buf[regCount++] = GRAIDENT_PARAM_RAM_REG1 >> 8;
			buf[regCount++] = GRAIDENT_PARAM_RAM_REG1 & 0xff; 

			buf[regCount++]=0;
			buf[regCount++]=0;
			buf[regCount++]=gpr[i].param1>>8;
			buf[regCount++]=gpr[i].param1&0xFF;

			buf[regCount++] = GRAIDENT_PARAM_RAM_REG2 >> 24;
			buf[regCount++] = GRAIDENT_PARAM_RAM_REG2 >> 16;
			buf[regCount++] = GRAIDENT_PARAM_RAM_REG2 >> 8;
			buf[regCount++] = GRAIDENT_PARAM_RAM_REG2 & 0xff; 
            tempvalue=gpr[i].param1>>16|(gpr[i].param2<<10&0xFFFF);
			buf[regCount++]=0;
			buf[regCount++]=0;
			buf[regCount++]=tempvalue>>8;
			buf[regCount++]=tempvalue&0xFF;

			buf[regCount++] = GRAIDENT_PARAM_RAM_REG3 >> 24;
			buf[regCount++] = GRAIDENT_PARAM_RAM_REG3 >> 16;
			buf[regCount++] = GRAIDENT_PARAM_RAM_REG3 >> 8;
			buf[regCount++] = GRAIDENT_PARAM_RAM_REG3 & 0xff; 

			buf[regCount++]=0;
			buf[regCount++]=0;
			buf[regCount++]=(gpr[i].param2 >>14)& 0xFF;
			buf[regCount++]=(gpr[i].param2>>6)&0xFF;

			buf[regCount++] = GRAIDENT_PARAM_RAM_REG4 >> 24;
			buf[regCount++] = GRAIDENT_PARAM_RAM_REG4 >> 16;
			buf[regCount++] = GRAIDENT_PARAM_RAM_REG4 >> 8;
			buf[regCount++] = GRAIDENT_PARAM_RAM_REG4 & 0xff; 

			buf[regCount++]=0;
			buf[regCount++]=0;
			buf[regCount++]=0;
			buf[regCount++]= (gpr[i].param2>>22)&0xF;
		
		}
	
	}
	regCount++;
	buf[5] = HIBYTE(regCount);
	buf[6] = LOBYTE(regCount);

	buf[regCount -1] = theApp.CheckSum(buf,regCount -1);
	iWrittenBytes = theApp.Send(M,buf,regCount);
	theApp.PrintToLog("发送设置梯度参数RAM指令：",buf,iWrittenBytes);
	
	
	//second send wave ram
	realwaveRamCount = (waveRamCount % 16 == 0) ? waveRamCount : (waveRamCount / 16 + 1) * 16;
	if (waveRamCount)
	{
		regCount = 10 + clrBeforeData(&buf[10], GRAIDENT_CLR_REG * 2, 1);
		for (i = 0; i < realwaveRamCount; i++)
		{
			if(i<waveRamCount)
			intvalue = *(int *)&waveRamValues[i];
			else intvalue = 0;

			buf[regCount++] = GRADIENT_WAVE_RAM_LOW_REG >> 24;
			buf[regCount++] = GRADIENT_WAVE_RAM_LOW_REG >> 16;
			buf[regCount++] = GRADIENT_WAVE_RAM_LOW_REG >> 8;
			buf[regCount++] = GRADIENT_WAVE_RAM_LOW_REG & 0xff;

			buf[regCount++] = 0;
			buf[regCount++] = 0;
			buf[regCount++] = intvalue >> 8;
			buf[regCount++] = intvalue & 0xff;

			buf[regCount++] = GRADIENT_WAVE_RAM_HIGH_REG >> 24;
			buf[regCount++] = GRADIENT_WAVE_RAM_HIGH_REG >> 16;
			buf[regCount++] = GRADIENT_WAVE_RAM_HIGH_REG >> 8;
			buf[regCount++] = GRADIENT_WAVE_RAM_HIGH_REG & 0xff;

			buf[regCount++] = 0;
			buf[regCount++] = 0;
			buf[regCount++] = intvalue >> 24;
			buf[regCount++] = intvalue >> 16;
		}
		regCount++;
		buf[5] = HIBYTE(regCount);
		buf[6] = LOBYTE(regCount);

		buf[regCount - 1] = theApp.CheckSum(buf, regCount - 1);
		iWrittenBytes = theApp.Send(0, buf, regCount);
		theApp.PrintToLog("发送设置梯度波表指令：", buf, iWrittenBytes);
	}
	ConfigWriteEnd(0);
	delete[] buf;
	delete[] waveRamValues;
	return RIGHT_IN_PROCESS;
}
int TransferTxSample(int sample)
{
	switch (sample)
	{
	case us32:
		return 32;
	case us16:
		return 16;
	case us8:
		return  8;
	case us4:
		return 4;
	case us2:
		return 2;
	default:
		PrintStr(PRINT_LEVEL_ERROR,"tx wave file period set error");
		return ERROR_IN_PROCESS;
	}
}
int SetRFWaveFile(string filename)
{

	//parse file
	
	string readline,tempstr,wavename;
	txWaveInfo twi;
	int 
		state,sample,points,singleCount;
	string::size_type startpos,endpos;
	/*first I use hash but may confict so quit*/
	//hashcode = theApp.String_hashCode(filename);
	map<string, bool>::iterator iter;
	pair<map<string, txWaveInfo>::iterator, bool> ret;
	if ((iter = theApp.txwaveStatus.find(filename)) != theApp.txwaveStatus.end())
	{
		return RIGHT_IN_PROCESS;
	}

	ifstream preempfile(filename.c_str(),ios::in);
	if(!preempfile.is_open())
	{
		//PrintStr(PRINT_LEVEL_ERROR,"tx wave file is not exist");
		theApp.PrintToLogNC("tx wave file is not exist",NULL,0);
		return ERROR_IN_PROCESS;
	}
	state = 1;
	while(getline(preempfile,readline))
	{
		//comment
		if(!readline.compare(0,1,"#")||readline.empty())
			continue;
		startpos=0;
		switch(state)
		{
			//start 
		case 1:
			{
				if((endpos=readline.find(',',startpos))!=string::npos)
				{
					twi.pos = preempfile.tellg();
					wavename=readline.substr(startpos,endpos-startpos);
					startpos=++endpos;
					endpos=readline.find(',',startpos);
					tempstr=readline.substr(startpos,endpos-startpos);
					sample=atoi(tempstr.c_str());
					switch (sample)
					{
					case 32:
						twi.sampleRate = us32;
						break;
					case 16:
						twi.sampleRate = us16;
						break;
					case 8:
						twi.sampleRate = us8;
						break;
					case 4:
						twi.sampleRate = us4;
						break;
					case 2:
						twi.sampleRate = us2;
						break;
					default:
						PrintStr(PRINT_LEVEL_ERROR, "tx wave file period set error");
						return ERROR_IN_PROCESS;
					}
						//points
					startpos = ++endpos;
					endpos = readline.find(',', startpos);
					tempstr = readline.substr(startpos, endpos - startpos);
					points = atoi(tempstr.c_str());
					twi.points = points;
					//bw
					startpos=++endpos;
					tempstr=readline.substr(startpos,readline.size()-startpos);
					twi.bw=atof(tempstr.c_str());
					//


					if(points>TX_WAVE_RAM_MAX)
					{
						//PrintStr(PRINT_LEVEL_ERROR,"tx wave points exceed 4K");
						theApp.PrintToLogNC("tx wave points exceed 4K",NULL,0);
						return ERROR_IN_PROCESS;					
					}
				}
				else
				{
					theApp.PrintToLogNC("tx points is not match the wave points", NULL, 0);
					//PrintStr(PRINT_LEVEL_ERROR,"tx points is not match the wave points");
					return ERROR_IN_PROCESS;
				}
				singleCount = 0;
				//insert txwaveinfo
				ret=theApp.txWaveMap.insert(pair<string,txWaveInfo>(wavename,twi));
				if (ret.second == false)
				{
					theApp.PrintToLogNC(wavename+"already exist",NULL,0);
					return ERROR_IN_PROCESS;
				}
				state=2;

			}	
			break;
		case 2:
			singleCount++;
			if(singleCount==points)
				state=1;
			break;
		default:
			break;
		}
	}
	theApp.txwaveStatus.insert(pair<string,bool>(filename,true));
	//theApp.txWaveFileName=filename;
    preempfile.close();
	return RIGHT_IN_PROCESS;


}
void DownTxFreqTable(int channel)
{
	int totalLength, clr_reg, ramWriteL, ramWriteH, i, regCount, len, tempInt;
	float temp;
	clr_reg = TX1_NCO_GAIN_CONFG_CLR;
	ramWriteL = (TX1_C1_NCO_FREQ_I + channel * 3) * 2;
	ramWriteH = (TX1_C1_NCO_FREQ_I + channel * 3 + 1) * 2;
	len = theApp.root["TX_RAM"][channel]["freq"].size();
	totalLength = len * 16 + 8;
	char* buf = new char[totalLength];
	regCount = 0;
	regCount += ConfigReg(&buf[regCount], clr_reg, TX_FREQ_CLR);
	for (i = 0; i < len; i++)
	{
		temp = theApp.root["TX_RAM"][channel]["freq"][i].asFloat();
		tempInt = round(temp /  TX_SYS_CLK*pow(2.0, 28));
		regCount += ConfigReg(&buf[regCount], ramWriteL, tempInt & 0xFFFF);
		regCount += ConfigReg(&buf[regCount], ramWriteH, tempInt >> 16);
	}
	ParsePack(M, buf, totalLength, "发送设置发射频率参数指令： ");
	delete[] buf;

}
int WINAPI SetTxFreOffsetTable(int channel,float *bufIn ,int len)
{
	int i;
	if (channel > 1 || channel < 0)
	{
		PrintStr(PRINT_LEVEL_ERROR, "rf channel in [0,1]");
		return ERROR_IN_PROCESS;
	}
	if (len>TX_NCO_OFFSET_MAX)
	{
		PrintStr(PRINT_LEVEL_ERROR, "rf offset param exceed 1024");
		return ERROR_IN_PROCESS;
	}
	Json::Value temp;	
	for (i = 0; i < len; i++)
		temp.append(bufIn[i]);
	theApp.root["TX_RAM"][channel]["freq"]= temp;
	//if used offline return directly
	if(theApp.isEdit||theApp.boxs.count(0)<0)  return RIGHT_IN_PROCESS;
	DownTxFreqTable(channel);
	return RIGHT_IN_PROCESS;
}
void DownTxGainTable(int channel)
{
	int totalLength, clr_reg, ramWriteL, i, regCount, len, tempInt;
	float temp;
	clr_reg = TX1_NCO_GAIN_CONFG_CLR;
	ramWriteL = (TX1_C1_GAIN_I + channel * 2) * 2;
	len = theApp.root["TX_RAM"][channel]["gain"].size();
	totalLength = len * 8 + 8;
	char* buf = new char[totalLength];
	regCount = 0;
	regCount += ConfigReg(&buf[regCount], clr_reg, TX_GAIN_CLR);
	for (i = 0; i < len; i++)
	{
		temp = theApp.root["TX_RAM"][channel]["gain"][i].asFloat();
		tempInt = round(temp * 32);
		regCount += ConfigReg(&buf[regCount], ramWriteL, tempInt & 0xFFFF);
	}
	ParsePack(M, buf, totalLength, "发送设置发射增益参数指令：");
	delete[] buf;
}

void DownTxPhaseTable(int channel)
{
	int totalLength, clr_reg, ramWriteL, ramWriteH, i, regCount,len,tempInt;
	float temp;
	clr_reg = TX1_NCO_GAIN_CONFG_CLR;
	ramWriteL = (TX1_C1_NCO_PHASE_I + channel * 3) * 2;
	ramWriteH = (TX1_C1_NCO_PHASE_I + channel * 3 + 1) * 2;
	len = theApp.root["TX_RAM"][channel]["phase"].size();
	totalLength = len * 16 + 8;
	char* buf = new char[totalLength];
	regCount = 0;
	regCount += ConfigReg(&buf[regCount], clr_reg, TX_PH_CLR);
	for (i = 0; i < len; i++)
	{
		temp=theApp.root["TX_RAM"][channel]["phase"][i].asFloat();
		tempInt = round(temp / 360.0*pow(2.0, 28));
		 regCount += ConfigReg(&buf[regCount], ramWriteL, tempInt & 0xFFFF);
		 regCount += ConfigReg(&buf[regCount], ramWriteH, tempInt >> 16);
	}
	ParsePack(M, buf, totalLength, "发送设置发射相位参数指令：");
	delete[] buf;
}
int WINAPI SetTxPhaseTable(int channel, float *bufIn, int len)
{
	int i;
	if (channel > 1 || channel < 0)
	{
		PrintStr(PRINT_LEVEL_ERROR, "rf channel in [0,1]");
		return ERROR_IN_PROCESS;
	}
	if (len>TX_NCO_OFFSET_MAX)
	{
		FormatOutPut(PRINT_LEVEL_ERROR, "rf offset param exceed %d", TX_NCO_OFFSET_MAX);
		return ERROR_IN_PROCESS;
	}
	Json::Value temp;
	for (i = 0; i < len; i++)
		temp.append(bufIn[i]);
	theApp.root["TX_RAM"][channel]["phase"] = temp;
	//if used offline return directly
	if (theApp.isEdit||theApp.boxs.count(0)<0)  return RIGHT_IN_PROCESS;
	DownTxPhaseTable(channel);
	return RIGHT_IN_PROCESS;
}
/************************************
*james@2016/8/10 change the db calc fomula 
*from pow(10,x/20)*2^32 to round(x*32)
*
*
*************************************/
int WINAPI SetTxGainTable(int channel,float *bufIn, int len)
{
	int i;
	if (channel > 1 || channel < 0)
	{
		PrintStr(PRINT_LEVEL_ERROR, "rf channel in [0,1]");
		return ERROR_IN_PROCESS;
	}
		if (len>=TX_NCO_OFFSET_MAX)
		{
			PrintStr(PRINT_LEVEL_ERROR,"rf offset param exceed 1024");
			return ERROR_IN_PROCESS;
		}
	Json::Value temp;
	for (i = 0; i < len; i++)
		temp.append(bufIn[i]);
	theApp.root["TX_RAM"][channel]["gain"] = temp;
	//if used offline return directly
	if (theApp.isEdit||theApp.boxs.count(0)<0)  return RIGHT_IN_PROCESS;
	DownTxGainTable(channel);
	return RIGHT_IN_PROCESS;
}
//sel=0 tx  
//sel=1 rx
float WINAPI GetCenterFre(int sel)
{
	if (sel) return theApp.rxCenterFre;
	else   return theApp.txCenterFre;

}
///ostr value change from 390625-->390625*4
int WINAPI SetTxCenterFre(int box,int boardno,int channel,float freq)
{

	static bool dac_test_nco = false;
	int ramWriteL,ramWriteH,iWrittenBytes,
     dacSel,dacNcoLw,dacNcoHi,iNco,ifiq,dacCfg,cmdReg,dataReg,addrReg,regCount;
   double n,fnco,fiq,ostr=390625*4;
   dacSel=channel/2;
   dacSel=(dacSel)<<13;
   n=floor(freq*pow(10.0,6)/ostr);
   theApp.txCenterFre = freq;
   //fnco
   if(channel%2)
   {
	   dacNcoLw = TX_DAC_CH1_LW;
	   dacNcoHi = TX_DAC_CH1_HI;
	 
   }
   else 
   {
	   dacNcoLw = TX_DAC_CH2_LW;
	   dacNcoHi = TX_DAC_CH2_HI;
   }
   
   fnco=n*ostr/pow(10.0,6);
   iNco=round(fnco/TX_DAC_CLK*pow(2.0,32));
   //fiq
   fiq=freq-fnco;
   ifiq=round(fiq/TX_SYS_CLK*pow(2.0,28));
   switch(boardno)
   {
   case TX1:
	   ramWriteL=(TX1_C1_NCO_CENTER_FREQ_I+channel*2)*2;
	   ramWriteH=(TX1_C1_NCO_CENTER_FREQ_I+channel*2+1)*2;
	   dacCfg = TX1_DAC_CFG;
	   cmdReg = TX1_DATABUS_CMD_REG_ADDR;
	   dataReg = TX1_DATABUS_DATA_REG_ADDR;
	   addrReg = TX1_DATABUS_ADDR_REG_ADDR;
	   break;
   case TX2:

	   ramWriteL=(TX2_C1_NCO_CENTER_FREQ_I+channel*2)*2;
	   ramWriteH=(TX2_C1_NCO_CENTER_FREQ_I+channel*2+1)*2;
	   dacCfg = TX2_DAC_CFG;
	   cmdReg = TX2_DATABUS_CMD_REG_ADDR;
	   dataReg = TX2_DATABUS_DATA_REG_ADDR;
	   addrReg = TX2_DATABUS_ADDR_REG_ADDR;
	   break;
   default:
	   errorno = EBOARDNOTFOUND;
	   PrintStr(PRINT_LEVEL_ERROR,"set rf wave error,board no incorrect");
	   return ERROR_IN_PROCESS;
   }

   
   char* buf=new char[BUF_STANDARD_SIZE];
   memcpy(buf,"$ETS",4);
   buf[4] = DATABUS_READ_WRITE_READ;
  

   buf[7] = DUMMY_CMD;
   buf[8] = DUMMY_CMD;
   buf[9] = DUMMY_CMD;
   regCount = 10;
   if (!dac_test_nco)
   {
	   regCount += ConfigReg(&buf[regCount], DAC_TEST_NCO_FREQ_I*2, ifiq & 0xFFFF);
	   regCount += ConfigReg(&buf[regCount], DAC_TEST_NCO_FREQ_Q * 2, ifiq >> 16);
	   dac_test_nco = true;
   }
   regCount += ConfigReg(&buf[regCount], ramWriteL, ifiq&0xFFFF);
   regCount += ConfigReg(&buf[regCount], ramWriteH, ifiq >>16);
  /*
   buf[10] = ramWriteL >> 24;
   buf[11] = ramWriteL >> 16;
   buf[12] = ramWriteL >> 8;
   buf[13] = ramWriteL & 0xff; 

   buf[14]=0;
   buf[15]=0;
   buf[16]=ifiq>>8;
   buf[17]=ifiq&0xFF;

   buf[18] = ramWriteH >> 24;
   buf[19] = ramWriteH >> 16;
   buf[20] = ramWriteH >> 8;
   buf[21] = ramWriteH & 0xff; 

   buf[22]=0;
   buf[23]=0;
   buf[24]=ifiq>>24;
   buf[25]=ifiq>>16;
   */
   //unset reset dac sleep   and tx_analog output disable
   regCount+=ConfigReg(&buf[regCount],dacCfg,0xF00);
   //write reg
   regCount+=ConfigReg(&buf[regCount], cmdReg,0x0);
   regCount+=ConfigReg(&buf[regCount],dataReg,iNco&0xFFFF);	
   regCount+=ConfigReg(&buf[regCount],addrReg,dacNcoLw|dacSel);
   
   regCount+=ConfigReg(&buf[regCount], dataReg,iNco>>16);
   regCount+=ConfigReg(&buf[regCount], addrReg,dacNcoHi|dacSel);
   regCount++;
   buf[5] = HIBYTE(regCount);
   buf[6] = LOBYTE(regCount);	
   
   buf[regCount -1] = theApp.CheckSum(buf,regCount-1);
   iWrittenBytes = theApp.Send(box,buf,regCount);
   theApp.PrintToLog("发送设置发射中心频率参数指令：",buf,iWrittenBytes);
   
   memcpy(buf, "$ETS", 4);
   buf[4] = DATABUS_READ_WRITE_READ;

   buf[7] = DUMMY_CMD;
   buf[8] = DUMMY_CMD;
   buf[9] = DUMMY_CMD;
   regCount = 10;
   regCount += ConfigReg(&buf[regCount], dacCfg, 0xF0F);
   regCount++;
   buf[5] = HIBYTE(regCount);
   buf[6] = LOBYTE(regCount);

   buf[regCount - 1] = theApp.CheckSum(buf, regCount - 1);
   iWrittenBytes = theApp.Send(box, buf, regCount);
   theApp.PrintToLog("发送设置DAC enable指令：", buf, iWrittenBytes);
   delete[] buf;
   theApp.SYSFREQ = freq;
   return RIGHT_IN_PROCESS;
}
int SetRFWaves(const char *filename)
{
	string readline,waveFile,waveName,strtemp,path;
	string filestr(filename);
	
	string::size_type pos,newpos;
	pos = filestr.find_last_of('\\');
	path = filestr.substr(0,pos+1);
	char str[100];
	ifstream file(filename, ios::in);
	if (!file.is_open())
	{
		PrintStr(PRINT_LEVEL_ERROR, "fail to open  file: ");
		return ERROR_IN_PROCESS;
	}
	int box, brd, ch, bramno,lineno;
	lineno = 1;
	//clear last record
	theApp.txWaveMap.clear();
	theApp.txwaveStatus.clear();
	while (getline(file, readline))
	{
		//comment
		if (!readline.compare(0, 1, "#") || readline.empty())
		{
			lineno++;
			continue;
		}
		pos = readline.find(',');
		strtemp = readline.substr(0,pos);
		box = atoi(strtemp.c_str());
		newpos = ++pos;

		pos = readline.find(',',newpos);
		strtemp = readline.substr(newpos, pos-newpos);
		brd= atoi(strtemp.c_str());
		newpos = ++pos;

		pos = readline.find(',', newpos);
		strtemp = readline.substr(newpos, pos - newpos);
		ch = atoi(strtemp.c_str());
		newpos = ++pos;

		pos = readline.find(',', newpos);
		strtemp = readline.substr(newpos, pos - newpos);
		bramno = atoi(strtemp.c_str());
		newpos = ++pos;

		pos = readline.find(',', newpos);
		strtemp = readline.substr(newpos, pos - newpos);
		waveFile = strtemp;
		PathUriToWin(waveFile);
		waveFile = path + waveFile;
		newpos = ++pos;

		waveName =readline.substr(newpos, string::npos);
		
		if (SetRFWaveTable(box, brd, ch, bramno, waveFile.c_str(), waveName.c_str()))
		{
			file.close();
			sprintf_s(str,"in %s,parse line error,%d", filename, lineno);
			//print to log
			theApp.PrintToLogNC(str,NULL,0);
			return ERROR_IN_PROCESS;
		}
		lineno++;

	}
	ConfigWriteEnd(0);
	file.close();
	return RIGHT_IN_PROCESS;
}
/*
*DATA             RECOMMET       AUTHOR
*2016.3.15        first write sel then point type .... 
*2016.4.20        change the ramclr to first step line<4554>
*
*
*/
int SetRFWaveTable(int box,int boardno,int channel,int bramno, string filename, string wavename)
{
    
	string readline, strtemp;
	string::size_type pos;
	int dataLength,i, ramClrReg,selReg,regCount,pointReg,realPoint,periodReg,ramWriteL,ramWriteH,value;
	map<string,txWaveInfo>::iterator waveMapIter;
	txWaveInfo twi;
	if (SetRFWaveFile(filename))
	{
		PrintStr(PRINT_LEVEL_ERROR,"parse wave file error ");
		return ERROR_IN_PROCESS;
	}
    switch(boardno)
	{
	case TX1:
      selReg=TX1_BRAM_WR_SEL;
	  pointReg=(TX1_C1_BRAM_RD_POINT+channel)*2;
	  periodReg=(TX1_C1_TYPE+channel)*2;	
	  ramWriteL=TX1_BRAM_WDATA_I;
	  ramWriteH=TX1_BRAM_WDATA_H;
	  ramClrReg = TX1_BRAM_WR_ADDR_CLR;
	  break;
	case TX2:
		selReg=TX2_BRAM_WR_SEL;
		pointReg=(TX2_C1_BRAM_RD_POINT+channel)*2;
		periodReg=(TX2_C1_TYPE+channel)*2;	
		ramWriteL=TX2_BRAM_WDATA_I;
		ramWriteH=TX2_BRAM_WDATA_H;
		ramClrReg = TX2_BRAM_WR_ADDR_CLR;
		break;
	default:
	 PrintStr(PRINT_LEVEL_ERROR,"set rf wave error,board no incorrect");
	 return ERROR_IN_PROCESS;
	}
	
	if(theApp.txWaveMap.empty())
	{
		PrintStr(PRINT_LEVEL_ERROR,"need set the wave file or have problem with wave file");
		return ERROR_IN_PROCESS;
	}
    if((waveMapIter=theApp.txWaveMap.find(wavename))==theApp.txWaveMap.end())
	{
		FormatOutPut(PRINT_LEVEL_ERROR,"wave name %s is not find", wavename.c_str());
		return ERROR_IN_PROCESS;
	} 
    twi=waveMapIter->second;
	//external 6 reg  per reg
	realPoint = (twi.points % 16 == 0) ? twi.points : (twi.points / 16 + 1) * 16;
	dataLength = realPoint * 32+48;
	//char* buf = new char[ONE_PACK_MAX_SIZE];
	char *databuf = new char[dataLength];
	//packNum = dataLength /(ONE_PACK_MAX_SIZE - 11);
	//LastPack = dataLength % (ONE_PACK_MAX_SIZE - 11);
	ifstream file(filename);
	file.seekg(twi.pos);
	int *idata = new int[twi.points];
	int *qdata = new int[twi.points];
	for (i = 0; i<twi.points; i++)
	{
		if (getline(file, readline))
		{
			pos = readline.find(',');
			strtemp = readline.substr(0, pos);
			idata[i] = atoi(strtemp.c_str());
			strtemp = readline.substr(pos + 1, string::npos);
			qdata[i] = atoi(strtemp.c_str());
		}
		else
		{
			FormatOutPut(PRINT_LEVEL_ERROR, "error while read wave file line %d",i+1);
			return ERROR_IN_PROCESS;
		}
	}
	//store info 
	memset(theApp.twp,0,sizeof(txWaveParas)*2*8*16);
	theApp.twp[boardno - TX1][channel][bramno].bw = twi.bw;
	theApp.twp[boardno - TX1][channel][bramno].period = TransferTxSample(twi.sampleRate);
	theApp.twp[boardno - TX1][channel][bramno].points = twi.points;
	theApp.twp[boardno - TX1][channel][bramno].set = true;

	//
	regCount = 0;
	value = (channel << 5) + (bramno << 1);
	regCount += ConfigReg(&databuf[regCount], ramClrReg, 1);
	regCount += ConfigReg(&databuf[regCount], selReg, value);
	regCount += ConfigReg(&databuf[regCount], pointReg, twi.points);
	regCount += ConfigReg(&databuf[regCount], periodReg, twi.sampleRate);
	
	for (i = 0; i < realPoint; i++)
	{
		if (i < twi.points)
		{
			regCount += ConfigReg(&databuf[regCount], ramWriteL, idata[i] & 0xFFFF);
			regCount += ConfigReg(&databuf[regCount], ramWriteH, idata[i] >> 16);
		}
		else
		{
			regCount += ConfigReg(&databuf[regCount], ramWriteL, 0);
			regCount += ConfigReg(&databuf[regCount], ramWriteH, 0);
		}
	}
	value = (channel << 5) + (bramno << 1) + 1;
	regCount += ConfigReg(&databuf[regCount], ramClrReg, value);
	regCount += ConfigReg(&databuf[regCount], selReg, value);
	for (i = 0; i < realPoint; i++)
	{
		if (i < twi.points)
		{
			regCount += ConfigReg(&databuf[regCount], ramWriteL, qdata[i] & 0xFFFF);
			regCount += ConfigReg(&databuf[regCount], ramWriteH, qdata[i] >> 16);
		}
		else
		{
		regCount += ConfigReg(&databuf[regCount], ramWriteL, 0);
		regCount += ConfigReg(&databuf[regCount], ramWriteH, 0);
		}
	}
	ParsePack(box,databuf, dataLength, "发送设置发送波形参数指令：");
	/*	
	index = 0;
		for (i = 0; i < packNum; i++)
		{
			memcpy(buf, "$ETS", 4);
			buf[4] = DATABUS_READ_WRITE_READ;
			buf[5] = 0xFF;
			buf[6] = 0xFB;
			buf[7] = DUMMY_CMD;
			buf[8] = DUMMY_CMD;
			buf[9] = DUMMY_CMD;
			memcpy(&buf[10],&databuf[index], 65520);
			index += 65520;
			buf[ONE_PACK_MAX_SIZE - 1] = theApp.CheckSum(buf, ONE_PACK_MAX_SIZE -1);
			iWrittenBytes = theApp.Send(box, buf, ONE_PACK_MAX_SIZE);
			theApp.PrintToLog("发送设置发送波形参数指令：", buf, iWrittenBytes);
		}
		if (LastPack)
		{
			memcpy(buf, "$ETS", 4);
			buf[4] = DATABUS_READ_WRITE_READ;
			buf[5] = HIBYTE(LastPack+11);
			buf[6] = LOBYTE(LastPack+11);
			buf[7] = DUMMY_CMD;
			buf[8] = DUMMY_CMD;
			buf[9] = DUMMY_CMD;
			memcpy(&buf[10],&databuf[index], LastPack);
			buf[LastPack +10] = theApp.CheckSum(buf, LastPack+10);
			iWrittenBytes = theApp.Send(box, buf, LastPack+11);
			theApp.PrintToLog("发送设置发送波形参数指令：", buf, iWrittenBytes);
		}

	delete[] buf;
	*/
	delete[] databuf;
	delete[] idata;
	delete[] qdata;
	file.close();
	return RIGHT_IN_PROCESS;
}
int SetRxBW(int box,int boardno,int channel,int bandwith)
{
	string readline;
	int typeReg,iWrittenBytes,dwPackageLength=19;
	
	switch(boardno)
	{
	case RX1:
		typeReg=(RX1_C1_TYPE+channel)*2;
		break;
	case RX2:
		typeReg=(RX2_C1_TYPE+channel)*2;
		break;
	case RX3:
		typeReg=(RX3_C1_TYPE+channel)*2;
	    break;
	case RX4:
		typeReg=(RX4_C1_TYPE+channel)*2;
		break;
	default:
		PrintStr(PRINT_LEVEL_ERROR,"set rx filter bandwidth,board no incorrect");
		return ERROR_IN_PROCESS;
	}
	
	char* buf=new char[dwPackageLength];
	memcpy(buf,"$ETS",4);
	buf[4] = DIRECT_REG_WRITE_READ;
	buf[5] = HIBYTE(dwPackageLength);
	buf[6] = LOBYTE(dwPackageLength);

	buf[7] = DUMMY_CMD;
	buf[8] = DUMMY_CMD;
	buf[9] = DUMMY_CMD;

	buf[10]=0;
	buf[11]=0;
	buf[12]=typeReg>>24;
	buf[13]=typeReg>>16;

	buf[14]=0;
	buf[15]=0;
	buf[16]=0;
	buf[17]=bandwith;
	buf[dwPackageLength -1] = theApp.CheckSum(buf,dwPackageLength);
	iWrittenBytes = theApp.Send(box,buf,dwPackageLength);
	theApp.PrintToLog("发送设置接收滤波器参数指令：",buf,iWrittenBytes);
	delete[] buf;
	return RIGHT_IN_PROCESS;
}
void DownRxFreqTable()
{
	int tempInt, totalLength, clr_reg, ramWriteL, ramWriteH, i, j, regCount, len;
	int  channel = 0;
	float temp;
	vector<int> temp_vector;
	ramWriteL = (RX1_C1_NCO_FREQ_I + channel * 3) * 2;
	ramWriteH = (RX1_C1_NCO_FREQ_I + channel * 3 + 1) * 2;
	clr_reg = RX1_FRE_PH_CONFG_CLR;
	len = theApp.root["RX_RAM"]["freq"].size();
	int channels = GetRxChannels(0, RX1);
	totalLength = channels*len * 16 + 8;
	char* buf = new char[totalLength];
	regCount = 0;
	regCount += ConfigReg(&buf[regCount], clr_reg, RX_FREQ_CLR);
	for (j = 0; j < channels; j++)
	{
		for (i = 0; i < len; i++)
		{
			if (j == 0)
			{
				temp = theApp.root["RX_RAM"]["freq"][i].asFloat();
				tempInt = round(temp / TX_SYS_CLK*pow(2.0, 28));
				temp_vector.push_back(tempInt);
			}
			else tempInt = temp_vector[i];
			regCount += ConfigReg(&buf[regCount], ramWriteL, tempInt & 0xFFFF);
			regCount += ConfigReg(&buf[regCount], ramWriteH, tempInt >> 16);

		}
		ramWriteL = ramWriteL + 6;
		ramWriteH = ramWriteL + 2;
	}
	ParsePack(M, buf, totalLength, "发送设置接收频率参数指令：");
	delete[] buf;
}
int WINAPI SetRxFreOffsetTable(float *bufIn,int len)
{

	int i;
	if (len>TX_NCO_OFFSET_MAX)
	{
		PrintStr(PRINT_LEVEL_ERROR, "rf offset param exceed 1024");
		return ERROR_IN_PROCESS;
	}
	Json::Value temp;
	for (i = 0; i < len; i++)
		temp.append(bufIn[i]);
	theApp.root["RX_RAM"]["freq"] = temp;
	//if used offline return directly
	if (theApp.isEdit||theApp.boxs.count(0)<0)  return RIGHT_IN_PROCESS;
	DownRxFreqTable();
	return RIGHT_IN_PROCESS;
}
void DownRxPhaseTable()
{
	int tempInt,totalLength, clr_reg, ramWriteL, ramWriteH, i, j, regCount,len;
	int channel = 0;
	float temp;
	vector<int> temp_vector;
	ramWriteL = (RX1_C1_NCO_PHASE_I + channel * 3) * 2;
	ramWriteH = (RX1_C1_NCO_PHASE_I + channel * 3 + 1) * 2;
	clr_reg = RX1_FRE_PH_CONFG_CLR;
	len = theApp.root["RX_RAM"]["phase"].size();
	int channels = GetRxChannels(0, RX1);
	totalLength = channels*len * 16 + 8;
	char* buf = new char[totalLength];
	regCount = 0;
	regCount += ConfigReg(&buf[regCount], clr_reg, RX_PH_CLR);
	for (j = 0; j < channels; j++)
	{
		for (i = 0; i < len; i++)
		{ 
			if(j==0)
			{ 
			temp = theApp.root["RX_RAM"]["phase"][i].asFloat();
			tempInt=round(temp / 360.0*pow(2.0, 28));
			temp_vector.push_back(tempInt);
			}
			else tempInt = temp_vector[i];
			regCount += ConfigReg(&buf[regCount], ramWriteL, tempInt & 0xFFFF);
			regCount += ConfigReg(&buf[regCount], ramWriteH, tempInt >> 16);

		}
		ramWriteL = ramWriteL + 6;
		ramWriteH = ramWriteL + 2;
	}
	ParsePack(M, buf, totalLength, "发送设置接收相位参数指令：");
	delete[] buf;
}
int WINAPI SetRxPhaseTable(float *bufIn, int len)
{
	int i;
	if (len>TX_NCO_OFFSET_MAX)
	{
		PrintStr(PRINT_LEVEL_ERROR, "rf offset param exceed 1024");
		return ERROR_IN_PROCESS;
	}
	Json::Value temp;
	for (i = 0; i < len; i++)
		temp.append(bufIn[i]);
	theApp.root["RX_RAM"]["phase"] = temp;
	//if used offline return directly
	if (theApp.isEdit||theApp.boxs.count(0)<0)  return RIGHT_IN_PROCESS;
	DownRxPhaseTable();
	return RIGHT_IN_PROCESS;
}
////////////////////////////
///
///isAllSet:use to indicate if set all channel to the same freq
///true: Set all channel to same freq
///false: Set the special channel to the freq
///////////////////////////
int WINAPI SetRxCenterFre(int box, int boardno, int channel, float freq,bool isAllSet)
{
	int ncoFreqStartL, ncoFreqStartH,n,iFreq,regCount, iWrittenBytes,i;
	double dFreq;
	theApp.rxCenterFre = freq;
	if (isAllSet)
		channel = 0;
	switch (boardno)
	{
	case RX1:
		ncoFreqStartL = (RX1_C1_NCO_CENTER_FREQ_L+channel*2)*2;
		ncoFreqStartH= (RX1_C1_NCO_CENTER_FREQ_L + channel * 2+1) * 2;
		break;
	case RX2:
		ncoFreqStartL = (RX2_C1_NCO_CENTER_FREQ_L + channel * 2) * 2;
		ncoFreqStartH = (RX2_C1_NCO_CENTER_FREQ_L + channel * 2 + 1) * 2;
		break;
	case RX3:
		ncoFreqStartL = (RX3_C1_NCO_CENTER_FREQ_L + channel * 2) * 2;
		ncoFreqStartH = (RX3_C1_NCO_CENTER_FREQ_L + channel * 2 + 1) * 2;
		break;
	case RX4:
		ncoFreqStartL = (RX4_C1_NCO_CENTER_FREQ_L + channel * 2) * 2;
		ncoFreqStartH = (RX4_C1_NCO_CENTER_FREQ_L + channel * 2 + 1) * 2;
		break;
	default:
		errorno = EBOARDNOTFOUND;
		PrintStr(PRINT_LEVEL_ERROR,"set rx ceter freq board no incorrect\n");
		return ERROR_IN_PROCESS;
	}
	n = floor(2 * freq / 50);
	if (n % 2)
	
		//n is odd
		dFreq=freq-((n + 1) / 2 * 50);
	else dFreq = freq - ((n ) / 2 * 50);
	iFreq=round(dFreq / RX_SYS_CLK*pow(2.0, 28));
	char* buf = new char[BUF_STANDARD_SIZE];
	memcpy(buf, "$ETS", 4);
	buf[4] = DIRECT_REG_WRITE_READ;

	buf[7] = DUMMY_CMD;
	buf[8] = DUMMY_CMD;
	buf[9] = DUMMY_CMD;
	regCount = 10;
	if (isAllSet)
	{
		for (i = 0; i < 8; i++)
		{
			regCount += ConfigReg(&buf[regCount],ncoFreqStartL, iFreq&0xFFFF);
			regCount += ConfigReg(&buf[regCount],ncoFreqStartH, iFreq>>16);
			ncoFreqStartL += 4;
			ncoFreqStartH += 4;
		}
	}
	else
	{
		regCount += ConfigReg(&buf[regCount],ncoFreqStartL, iFreq & 0xFFFF);
		regCount += ConfigReg(&buf[regCount],ncoFreqStartH, iFreq >> 16);
	}
	regCount++;
	buf[5] = HIBYTE(regCount);
	buf[6] = LOBYTE(regCount);

	buf[regCount - 1] = theApp.CheckSum(buf, regCount - 1);
	iWrittenBytes = theApp.Send(box,buf, regCount);
	theApp.PrintToLog("发送设置接收中心频率参数指令：", buf, iWrittenBytes);
	delete[] buf;
	return RIGHT_IN_PROCESS;
}

int SetAllPreempValue()
{
	float pefvalues;
	int i=0,channel_low_addr,channel_high_addr,intvalue,regCount,iWrittenBytes;
	if (!theApp.pefload && (LoadPreempValue() == NULL))
	{
		return ERROR_IN_PROCESS;
	}
	char* buf=new char[BUF_STANDARD_SIZE];
	
	memcpy(buf,"$ETS",4);
	buf[4] = DIRECT_REG_WRITE_READ;
	
	buf[7] = DUMMY_CMD;
	buf[8] = DUMMY_CMD;
	buf[9] = DUMMY_CMD;
    regCount=10;
    for(i=0;i<54;i++)
	{
		if ((i >= 6 && i < 12) || (i >= 18 && i < 24) || (i >= 30 && i < 36) || (i >= 39 && i < 42) || (i >= 45 && i < 48) || (i >= 51 && i < 54))
		{
			pefvalues = exp(-theApp.ts / theApp.pefvalues[i]);
		}
		else
			pefvalues = theApp.pefvalues[i];
		intvalue = *(int *)&pefvalues;
	if(i<12)
	{
	  channel_low_addr=(GRADIENT_PEF_X+i*2)<<1;
	  channel_high_addr=(GRADIENT_PEF_X+1+i*2)<<1;	
	}
	else if(i>=12&&i<24)
	{
		channel_low_addr=(GRADIENT_PEF_Y+(i-12)*2)<<1;
		channel_high_addr=(GRADIENT_PEF_Y+1+(i-12)*2)<<1;	
	}
	else if(i>=24&&i<36)
	{
		channel_low_addr=(GRADIENT_PEF_Z+(i-24)*2)<<1;
		channel_high_addr=(GRADIENT_PEF_Z+1+(i-24)*2)<<1;	
	
	} else
	{
		channel_low_addr=(GRADIENT_PEF_B0+(i-36)*2)<<1;
		channel_high_addr=(GRADIENT_PEF_B0+1+(i-36)*2)<<1;	
	}

	regCount+=ConfigReg(&buf[regCount], channel_low_addr, intvalue&0xFFFF);
	regCount += ConfigReg(&buf[regCount], channel_high_addr, intvalue >>16);
	}
	regCount++;
	buf[5] = HIBYTE(regCount);
	buf[6] = LOBYTE(regCount);
	buf[regCount -1] = theApp.CheckSum(buf,regCount -1);
	iWrittenBytes = theApp.Send(0,buf,regCount);
	theApp.PrintToLog("发送设置所有预加重参数指令：",buf,iWrittenBytes);
	delete[] buf;
	return RIGHT_IN_PROCESS;
}
//change LoadPreempValue param to no param
char *LoadPreempValue()
{
	string pefvaluestr,readline,filename = theApp.configPwd + DEFAULT_PEF_FILE;
	ifstream preempfile(filename, ios::binary);
	bool errSet = false, tsget = false;
	int i=0;
	string::size_type startpos, endpos;
	if (!preempfile.is_open())
	{
		PrintStr(PRINT_LEVEL_ERROR, "preemp file open failed");
		return NULL;
	}
	preempfile.seekg(0, preempfile.end);
	int length = preempfile.tellg();
	preempfile.seekg(0, preempfile.beg);
	if(length>BUF_STANDARD_SIZE-1)
	{
		errorno = EBUFOVERFLOW;
		preempfile.close();
		return NULL;
	}
	preempfile.read(theApp.jasonstr, length);
	theApp.jasonstr[length] = '\0';//end
								   // hava problem 
	if (!preempfile)
	{
		//int state=preempfile.rdstate();	
		errSet = true;
		errorno = EREADWRITEFILEDATA;
		goto END;
	}
	preempfile.seekg(0, preempfile.beg);
	while (getline(preempfile, readline))
	{
		//comment
		if (!readline.compare(0, 1, "#") || readline.empty())
			continue;
		if (!tsget)
		{
			theApp.ts = atoi(readline.c_str());
			tsget = true;
			continue;
		}
		startpos = 0;
		while ((endpos = readline.find(',', startpos)) != string::npos)
		{
			pefvaluestr = readline.substr(startpos, endpos - startpos);
			startpos = ++endpos;
			theApp.pefvalues[i++] = atof(pefvaluestr.c_str());
		}
		if (startpos<readline.size())
		{
			pefvaluestr = readline.substr(startpos, readline.size() - startpos);
			theApp.pefvalues[i++] = atof(pefvaluestr.c_str());
		}
	}
	if (i != 54)
	{
		PrintStr(PRINT_LEVEL_ERROR, "preemp file param is not 54,please check");
		errSet=true;
	}
	END:
	preempfile.close();
	if (errSet)
		return NULL;
	theApp.pefload = true;
	return theApp.jasonstr;
}
int SavePreempValue()
{
	string filename = theApp.configPwd + DEFAULT_PEF_FILE;
	int i;
	ofstream preempfile(filename);
	if (!preempfile.is_open())
	{
		PrintStr(PRINT_LEVEL_ERROR, "preemp file open failed");
		return ERROR_IN_PROCESS;
	}
	preempfile << theApp.ts << endl;
	for (i = 0; i < 54; i++)
	{
		preempfile << theApp.pefvalues[i] ;
		if (i != 53)
		{
			if (i == 11 || i == 23 || i == 35)
				preempfile << endl;
			else preempfile << ",";
		}
	}
	preempfile.close();
	return RIGHT_IN_PROCESS;

}
void SetPreempTs(int ts)
{
	theApp.ts = ts;
}
int SetPreempValue(int channel,int keys,float value)
{
	int index = 0;
	switch (channel)
	{
	case CHANNEL_X:
		index = keys;
		break;
	case CHANNEL_Y:
		index = 12+keys;
		break;
	case CHANNEL_Z:
		index = 24+keys;
		break;
	case CHANNEL_B0:
		index = 36+keys-A1X;
		break;
	default:
		PrintStr(PRINT_LEVEL_ERROR, "Invalid channel");
		return ERROR_IN_PROCESS;
	}
	if (index > 53 || index < 0)
	{
		PrintStr(PRINT_LEVEL_ERROR, "Invalid keys");
		return ERROR_IN_PROCESS;
	}
	theApp.pefvalues[index] = value;
	return RIGHT_IN_PROCESS;
	/*
	int channelvalue,dwPackageLength,channel_addr,channel_low_addr,channel_high_addr,iWrittenBytes;
    channel_addr=theApp.GetPreempAddr(channel,keys);
	if(!channel_addr) return;
	channel_low_addr=channel_addr*2;
	channel_high_addr=(channel_addr+1)*2;
	channelvalue=*(int *)&value;
	dwPackageLength = REG_INFO_WITH*2 + 11;
	char* buf=new char[dwPackageLength];
	memcpy(buf,"$ETS",4);
	buf[4] = DIRECT_REG_WRITE_READ;
	buf[5] = HIBYTE(dwPackageLength);
	buf[6] = LOBYTE(dwPackageLength);

	buf[7] = DUMMY_CMD;
	buf[8] = DUMMY_CMD;
	buf[9] = DUMMY_CMD;


	buf[10] = channel_low_addr >> 24;
	buf[11] = channel_low_addr >> 16;
	buf[12] = channel_low_addr >> 8;
	buf[13] = channel_low_addr & 0xff;

	buf[14] = 0;
	buf[15] = 0;
	buf[16] = channelvalue >> 8;
	buf[17] = channelvalue &0xff;

	buf[18] = channel_high_addr >> 24;
	buf[19] = channel_high_addr >> 16;
	buf[20] = channel_high_addr >> 8;
	buf[21] = channel_high_addr & 0xff;

	buf[22] = 0;
	buf[23] = 0;
	buf[24] = channelvalue >> 24;
	buf[25] = channelvalue >>16;

	buf[dwPackageLength -1] = theApp.CheckSum(buf,dwPackageLength -1);

	iWrittenBytes = theApp.Send(0,buf,dwPackageLength);
	theApp.PrintToLog("发送设置预加重参数指令：",buf,iWrittenBytes);
	delete[] buf;
	*/

}
int   SoftWareApp::GetPreempAddr(int channel,int keys)
{
	int preemp_start_addr;
	switch(channel)
	{
	case CHANNEL_X:
		preemp_start_addr=GRADIENT_PEF_X;
			break;
	case CHANNEL_Y:
		preemp_start_addr=GRADIENT_PEF_Y;
			break;
	case CHANNEL_Z:
		preemp_start_addr=GRADIENT_PEF_Z;
		break;
	case CHANNEL_B0:
		preemp_start_addr=GRADIENT_PEF_B0;
		break;
	default:
      PrintStr(PRINT_LEVEL_ERROR,"Impossible PEF args");
	  return 0;
	}
	if(channel!=CHANNEL_B0)
	{
	   return preemp_start_addr+keys*2;
	}
	return preemp_start_addr+(keys-A1X)*2;

}
float GetPreempValue(int channel,int keys,int *iscorrect)
{
	int dwPackageLength,channel_addr,channel_low_addr,channel_high_addr,iWrittenBytes,channel_value;
	channel_addr=theApp.GetPreempAddr(channel,keys);
	if (!channel_addr)
	{
		*iscorrect = 0;
		return 0.0;
	}
	DWORD waitReturn;
	channel_low_addr=channel_addr*2;
	channel_high_addr=(channel_addr+1)*2;
	dwPackageLength = 19;
	char* buf=new char[dwPackageLength];
	memcpy(buf,"$ETS",4);
	buf[4] = DIRECT_REG_WRITE_READ;
	buf[5] = HIBYTE(dwPackageLength);
	buf[6] = LOBYTE(dwPackageLength);

	buf[7] = DUMMY_CMD;
	buf[8] = DUMMY_CMD;
	buf[9] = DUMMY_CMD;


	buf[10] = channel_low_addr >> 24;
	buf[11] = channel_low_addr >> 16;
	buf[12] = channel_low_addr >> 8;
	buf[13] = channel_low_addr & 0xff;
	buf[14] = channel_high_addr >> 24;
	buf[15] = channel_high_addr >> 16;
	buf[16] = channel_high_addr >> 8;
	buf[17] = channel_high_addr & 0xff;

	buf[dwPackageLength -1] = theApp.CheckSum(buf,dwPackageLength -1);
	iWrittenBytes = theApp.Send(0,buf,dwPackageLength);
	theApp.PrintToLog("发送查询预加重参数指令：",buf,iWrittenBytes);
	theApp.boxs[0].queryPreempOrChannel=true;
	theApp.boxs[0].queryLowAddr=channel_low_addr;
	theApp.boxs[0].queryHighAddr=channel_high_addr;
	delete[] buf;
    
	waitReturn=WaitForSingleObject(theApp.boxs[0].syncEvent,2000);
	theApp.boxs[0].queryPreempOrChannel=false;
	if(waitReturn==WAIT_OBJECT_0)
	{

		channel_value=	theApp.boxs[0].channel_value_high<<16|(theApp.boxs[0].channel_value_low&0xFFFF);
		theApp.boxs[0].channel_hi_receive=false;
		theApp.boxs[0].channel_lw_receive=false;
		*iscorrect = 1;
		return *(float *)&channel_value;


	}
	else if(waitReturn==WAIT_TIMEOUT)
	{
		PrintStr(PRINT_LEVEL_ERROR,"read time out");
		*iscorrect = 0;
		return 0.0;
	}
	else if(waitReturn==WAIT_FAILED)
	{
		PrintStr(PRINT_LEVEL_ERROR,"read failed");
		*iscorrect = 0;
		return 0.0;
		// GetLastError();
	}


}
/***********************************
*@2016/8/3 change GetChannelValue read from file
*
***********************************/
float GetChannelValue(int channel)
{
	Json::Value root;
	
	if (channel > 3 || channel < 0)
	{
		PrintStr(PRINT_LEVEL_ERROR, "channel No must in scope [0-3]");
		return 0.0;
	}
	return theApp.shimValues[channel];
	/*
	int  dwPackageLength = 0,iWrittenBytes;
	int  channel_low_addr,channel_high_addr,channel_value;
	int  waitReturn;
	switch(channel)
	{
	case CHANNEL_X:
		channel_low_addr=GRADIENT_OFFSET_LW_X;
		channel_high_addr=GRADIENT_OFFSET_HI_X;
		break;
	case CHANNEL_Y:
		channel_low_addr=GRADIENT_OFFSET_LW_Y;
		channel_high_addr=GRADIENT_OFFSET_HI_Y;
		break;
	case CHANNEL_Z:
		channel_low_addr=GRADIENT_OFFSET_LW_Z;
		channel_high_addr=GRADIENT_OFFSET_HI_Z;
		break;
	case CHANNEL_B0:
		channel_low_addr=GRADIENT_OFFSET_LW_B0;
		channel_high_addr=GRADIENT_OFFSET_HI_B0;
		break;
	default:
		PrintStr(PRINT_LEVEL_ERROR,"channel No must in scope (0~2)");
		return 0.0; 
	}
	dwPackageLength = REG_INFO_WITH + 11;
	char* buf=new char[dwPackageLength];
	memcpy(buf,"$ETR",4);
	buf[4] = DIRECT_REG_WRITE_READ;
	buf[5] = HIBYTE(dwPackageLength);
	buf[6] = LOBYTE(dwPackageLength);

	buf[7] = DUMMY_CMD;
	buf[8] = DUMMY_CMD;
	buf[9] = DUMMY_CMD;


	buf[10] = channel_low_addr >> 24;
	buf[11] = channel_low_addr >> 16;
	buf[12] = channel_low_addr >> 8;
	buf[13] = channel_low_addr & 0xff;

	buf[14] = channel_high_addr >> 24;
	buf[15] = channel_high_addr >> 16;
	buf[16] = channel_high_addr >> 8;
	buf[17] = channel_high_addr & 0xff;

	buf[dwPackageLength -1] = theApp.CheckSum(buf,dwPackageLength -1);

	iWrittenBytes = theApp.Send(0,buf,dwPackageLength);
	theApp.PrintToLog("发送查询匀场参数指令：",buf,iWrittenBytes);
	theApp.queryPreempOrChannel=true;
	theApp.queryLowAddr=channel_low_addr;
	theApp.queryHighAddr=channel_high_addr;
	delete[] buf;

	//

	waitReturn=WaitForSingleObject(theApp.syncEvent,2000);

	theApp.queryPreempOrChannel=false;
	if(waitReturn==WAIT_OBJECT_0)
	{

			channel_value=	theApp.channel_value_high<<16|(theApp.channel_value_low&0xFFFF);
			theApp.channel_hi_receive=false;
			theApp.channel_lw_receive=false;
			return *(float *)&channel_value;
		 
	
	}
	else if(waitReturn==WAIT_TIMEOUT)
	{
          PrintStr(PRINT_LEVEL_ERROR,"read time out");
		  return 0.0;
	}
	else if(waitReturn==WAIT_FAILED)
	{
		 PrintStr(PRINT_LEVEL_ERROR,"read failed");
		 return 0.0;
		// GetLastError();
	}
	*/
}
//Terminate SCAN
void Abort()
{
	int i,regCount, dwPackageLength=19, iWrittenBytes;
	   //add for test

   //need clear arm intrept and then reset fpga
	map<int, Spectrometer>::iterator box_iter;
	Spectrometer *p;
	theApp.setupMode = false;
    theApp.srcFileDowned = false;
   char* buf = new char[dwPackageLength];
   memcpy(buf, "$ETS", 4);
   buf[4] = DIRECT_REG_WRITE_READ;
   buf[5] = HIBYTE(dwPackageLength);
   buf[6] = LOBYTE(dwPackageLength);

   buf[7] = DUMMY_CMD;
   buf[8] = DUMMY_CMD;
   buf[9] = DUMMY_CMD;
   regCount = 10;
   regCount+=ConfigReg(&buf[regCount], MAINCTRL_ID_VALUE0,1);
   regCount++;
   buf[5] = HIBYTE(regCount);
   buf[6] = LOBYTE(regCount);
   buf[regCount - 1] = theApp.CheckSum(buf, regCount - 1);
   iWrittenBytes = theApp.Send(0, buf, regCount);
   theApp.PrintToLog("发送暂停序列指令：", buf, iWrittenBytes);
   delete[] buf;
  
   for (box_iter = theApp.boxs.begin(); box_iter != theApp.boxs.end(); box_iter++)
   {
	   i = box_iter->first;
	   ReceiveEnvironmentClear(i);
	   ResetFPGA_ForTest(i); 
	   p = &(box_iter->second);
	   box_iter->second.processEnable = false;
   }
   int try_times = 3;
   
   for (box_iter = theApp.boxs.begin(); box_iter != theApp.boxs.end(); box_iter++)
   {
	   
		   while (box_iter->second.dmaDataProcess&&try_times-->0)
			   Sleep(1000);
		   if (box_iter->second.dmaDataProcess == false)
			   box_iter->second.SCloseDmaFile();
		   else
		   {
			   theApp.PrintToLogNC("Abort Problem：", NULL,0);
		   }
		   
   }
   theApp.lock = false;
   //check if file is not closed normaly 
	
}
//Pause Scan
void Pause()
{
   int regCount,dwPackageLength,iWrittenBytes;
   dwPackageLength=100;
   if(theApp.seqRun==true)
   {
	   char* buf=new char[dwPackageLength];
	   memcpy(buf,"$ETS",4);
	   buf[4] = DIRECT_REG_WRITE_READ;
	   buf[5] = HIBYTE(dwPackageLength);
	   buf[6] = LOBYTE(dwPackageLength);

	   buf[7] = DUMMY_CMD;
	   buf[8] = DUMMY_CMD;
	   buf[9] = DUMMY_CMD;
       regCount=10;
	   regCount += ConfigReg(&buf[regCount], MAINCTRL_ID_VALUE0, 1);
	   regCount++;
	   buf[5] = HIBYTE(regCount);
	   buf[6] = LOBYTE(regCount);
	   buf[regCount-1] = theApp.CheckSum(buf,regCount-1);
	   iWrittenBytes = theApp.Send(0,buf,regCount);
	   theApp.PrintToLog("发送暂停序列指令：",buf,iWrittenBytes);
	   delete[] buf;
	   theApp.seqRun=false;
   } 
}
//Continue 
void Continue()
{
	int regCount,dwPackageLength,iWrittenBytes;
	dwPackageLength=100;
	if(theApp.seqRun==false)
	{
		char* buf=new char[dwPackageLength];
		memcpy(buf,"$ETS",4);
		buf[4] = DIRECT_REG_WRITE_READ;
		buf[5] = HIBYTE(dwPackageLength);
		buf[6] = LOBYTE(dwPackageLength);

		buf[7] = DUMMY_CMD;
		buf[8] = DUMMY_CMD;
		buf[9] = DUMMY_CMD;
		regCount=10;
		regCount += ConfigReg(&buf[regCount], MAINCTRL_ID_VALUE0, 0);		
		regCount++;
		buf[5] = HIBYTE(regCount);
		buf[6] = LOBYTE(regCount);
		buf[regCount-1] = theApp.CheckSum(buf,regCount-1);
		iWrittenBytes = theApp.Send(0,buf,regCount);
		theApp.PrintToLog("发送继续序列指令：",buf,iWrittenBytes);
		delete[] buf;
		theApp.seqRun=true;

	} 
	 
}
void EnableTx()
{
	int regCount = 27, iWrittenBytes;
	char buf[27];
	memcpy(buf, "$ETS", 4);
	buf[4] = DIRECT_REG_WRITE_READ;
	buf[5] = HIBYTE(regCount);
	buf[6] = LOBYTE(regCount);
	buf[7] = DUMMY_CMD;
	buf[8] = DUMMY_CMD;
	buf[9] = DUMMY_CMD;
	ConfigReg(&buf[10], TX1_DAC_CFG,0xF0F);
	ConfigReg(&buf[18], TX2_DAC_CFG, 0xF0F);
	buf[26] = theApp.CheckSum(buf, 26);
	iWrittenBytes = theApp.Send(0, buf, regCount);
	theApp.PrintToLog("发送使能发射DAC指令：", buf, iWrittenBytes);
}
void DisableTx()
{
	int regCount = 27, iWrittenBytes;
	char buf[27];
	memcpy(buf, "$ETS", 4);
	buf[4] = DIRECT_REG_WRITE_READ;
	buf[5] = HIBYTE(regCount);
	buf[6] = LOBYTE(regCount);
	buf[7] = DUMMY_CMD;
	buf[8] = DUMMY_CMD;
	buf[9] = DUMMY_CMD;
	ConfigReg(&buf[10], TX1_DAC_CFG, 0xFF0);
	ConfigReg(&buf[18], TX2_DAC_CFG, 0xFF0);
	buf[26] = theApp.CheckSum(buf, 26);
	iWrittenBytes = theApp.Send(0, buf, regCount);
	theApp.PrintToLog("发送非使能发射DAC指令：", buf, iWrittenBytes);

}
int  Run()
{
   //First down the file
	if (theApp.lock)
	{
		theApp.PrintToLogNC("Run is Lock",NULL,0);
		return ERROR_IN_PROCESS;
	}
	if (PrepareRun(false))
	{
		return ERROR_IN_PROCESS;
	}
	//for test
	//PrintF("1.raw:2.raw:3.raw:4.raw:5.raw:");
	theApp.setupMode = false;
	theApp.lock = true;
	return RIGHT_IN_PROCESS;  
}
int SetDiscard(int preDiscard,int lastDiscard)
{
	int noSamples = findParam("noSamples",NULL);
	if (preDiscard + lastDiscard >= noSamples)
	{
		PrintStr(PRINT_LEVEL_ERROR,"the discard value exceeds noSamples");
		return 1;

	}
	theApp.preDiscard = preDiscard;
	theApp.lastDiscard = lastDiscard;
	return 0;

}
//
int findParam(string varname,bool *exist)
{
	paramInfo pi;
	int value;
	map<string, paramInfo>::iterator paramMapIterator;
	paramMapIterator = theApp.paramMap.find(varname);
	if (paramMapIterator == theApp.paramMap.end())
	{	
		if (exist) *exist = false;
		//preDiscard or postDiscard return 0
		if (!varname.compare("preDiscard") || !varname.compare("postDiscard"))
			return 0;
		return 1;
		
	}
	else
		pi = paramMapIterator->second;
	if (pi.pt == TYPE_DOUBLE)
		value = pi.value.doublelit;
	else if (pi.pt == TYPE_INT)
		value = pi.value.intlit;
	return value;
}
void  ResetFPGA(int boxindex)
{
	unsigned int i,regCount;
	int iWrittenBytes;
	char *buf = new char[BUF_STANDARD_SIZE];
	memcpy(buf, "$ETS", 4);
	buf[4] = DATABUS_READ_WRITE_READ;
	buf[7] = DUMMY_CMD;
	buf[8] = DUMMY_CMD;
	buf[9] = DUMMY_CMD;
	regCount = 10;
	for (i = 0; i < sizeof(ctrl_clr_regs) / sizeof(int); i++)
	{
		regCount += clrBeforeData(&buf[regCount], ctrl_clr_regs[i], 3);
	}
	regCount++;
	buf[5] = HIBYTE(regCount);
	buf[6] = LOBYTE(regCount);
	buf[regCount - 1] = theApp.CheckSum(buf, regCount - 1);
	iWrittenBytes = theApp.Send(boxindex, buf, regCount);
	theApp.PrintToLog(BoxToString(boxindex)+" 发送Reset FPGA指令：", buf, iWrittenBytes);
	delete[] buf;
	//sleep 1s
	Sleep(1000);
}
//for test 
void  ResetFPGA_ForTest(int boxindex)
{
	unsigned int i, regCount;
	int iWrittenBytes;
	char *buf = new char[BUF_STANDARD_SIZE];
	memcpy(buf, "$ETS", 4);
	buf[4] = DATABUS_READ_WRITE_READ;
	buf[7] = DUMMY_CMD;
	buf[8] = DUMMY_CMD;
	buf[9] = DUMMY_CMD;
	regCount = 10;
	for (i = 0; i < sizeof(ctrl_clr_regs) / sizeof(int); i++)
	{
		regCount += clrBeforeData(&buf[regCount], ctrl_clr_regs[i], 1);
	}
	regCount++;
	buf[5] = HIBYTE(regCount);
	buf[6] = LOBYTE(regCount);
	buf[regCount - 1] = theApp.CheckSum(buf, regCount - 1);
	iWrittenBytes = theApp.Send(boxindex, buf, regCount);
	theApp.PrintToLog(BoxToString(boxindex)+" 发送Reset FPGA For Test 指令：", buf, iWrittenBytes);
	delete[] buf;
}
void LedControl(int boxindex)
{
	unsigned int i, regCount;
	int iWrittenBytes;
	char *buf = new char[BUF_STANDARD_SIZE];
	memcpy(buf, "$ETS", 4);
	buf[4] = DATABUS_READ_WRITE_READ;
	buf[7] = DUMMY_CMD;
	buf[8] = DUMMY_CMD;
	buf[9] = DUMMY_CMD;
	regCount = 10;
	for (i = 0; i < sizeof(led_regs) / sizeof(int); i++)
	{
		regCount += ConfigReg(&buf[regCount], led_regs[i], 1);
	}
	regCount++;
	buf[5] = HIBYTE(regCount);
	buf[6] = LOBYTE(regCount);
	buf[regCount - 1] = theApp.CheckSum(buf, regCount - 1);
	iWrittenBytes = theApp.Send(boxindex, buf, regCount);
	theApp.PrintToLog(BoxToString(boxindex)+" 发送LED Control指令：", buf, iWrittenBytes);
	delete[] buf;
}
void SetTime()
{
	time_t curtime = time(0);
	tm time;
	localtime_s(&time, &curtime);
	int sec, min, day, mon, year, hour;
	string str1;
	year = time.tm_year;
	mon = time.tm_mon;
	day = time.tm_mday;
	min = time.tm_min;
	hour = time.tm_hour;
	sec = time.tm_sec;
	sprintf_s(theApp.runtime, 40, "_%d.%d.%d.%d.%d.%d", year + 1900, mon + 1, day, hour, min, sec);
}
void ClrSeq()
{
	unsigned int i, regCount, iWrittenBytes;
	char *buf = new char[BUF_STANDARD_SIZE];
	memcpy(buf, "$ETS", 4);
	buf[4] = DATABUS_READ_WRITE_READ;
	buf[7] = DUMMY_CMD;
	buf[8] = DUMMY_CMD;
	buf[9] = DUMMY_CMD;
	regCount = 10;
	
	for (i = 0; i < sizeof(control_regs) / sizeof(int); i++)
	{
		regCount += ConfigReg(&buf[regCount], control_regs[i], 0);
	}
	regCount++;
	buf[5] = HIBYTE(regCount);
	buf[6] = LOBYTE(regCount);
	buf[regCount - 1] = theApp.CheckSum(buf, regCount - 1);
	iWrittenBytes = theApp.Send(M, buf, regCount);
	theApp.PrintToLog("发送设置seq clr指令：", buf, iWrittenBytes);
	delete[] buf;
}
/*************************************
*
*comment line[5581,5584]  2016.4.19  YJ
*uncommet line[5581,5584] 2016.4.20  YJ
/*************************************/
int PrepareRun(bool setupmode)
{
	int noEchoes, noSlices, noSliceBlock, noViewBlock, noAverages, noViews, noViewsSec, noScans,sampleSize,realnoSamples;
	//First down the file
	string filename;
	ifstream file;
	unsigned int i,regCount, iWrittenBytes;
	char *buf = new char[BUF_STANDARD_SIZE];
	map<int, Spectrometer>::iterator box_iter;
	SetTime();
	//add for test
	for (box_iter = theApp.boxs.begin(); box_iter != theApp.boxs.end(); box_iter++)
	{ 
	ResetFPGA_ForTest(box_iter->first);
	}
	if (!theApp.downfilename.empty())
	{

		theApp.PrintToLog(theApp.downfilename+" downed",NULL,0);
		//file.open(theApp.downfilename,ios::in);
		if (_access(theApp.downfilename.c_str(), 0) == 0)
		{
			//file.close();
			//ResetFPGA();
			///1.clr seq ram 2.seq stop
			memcpy(buf, "$ETS", 4);
			buf[4] = DATABUS_READ_WRITE_READ;
		
			buf[7] = DUMMY_CMD;
			buf[8] = DUMMY_CMD;
			buf[9] = DUMMY_CMD;
			regCount = 10;
			
			//set rx bypass to default value
			//I find theApp.rx_bypass is zero @2017.3.29
			//regCount += ConfigReg(&buf[regCount], theApp.rx_bypass, 0x00);
			for (i = 0; i<sizeof(seq_clr_regs) / sizeof(int); i++)
			{
				regCount+=clrBeforeData(&buf[regCount], seq_clr_regs[i],1);
			}
			
			for (i = 0; i < sizeof(control_regs) / sizeof(int); i++)
			{
				regCount+= ConfigReg(&buf[regCount], control_regs[i],0);
			}
			//add for clr wait aha
			regCount += ConfigReg(&buf[regCount], MAINCTRL_ID_VALUE0, 0);
			if(setupmode)
				regCount += ConfigReg(&buf[regCount], MAIN_SETUP_RUN_MODE, 1);
			else 
				regCount += ConfigReg(&buf[regCount], MAIN_SETUP_RUN_MODE, 0);
			regCount++;
			buf[5] = HIBYTE(regCount);
			buf[6] = LOBYTE(regCount);
			buf[regCount - 1] = theApp.CheckSum(buf, regCount - 1);
			iWrittenBytes = theApp.Send(M, buf, regCount);
			theApp.PrintToLog("发送设置seq clr指令：", buf, iWrittenBytes);
			for (box_iter = theApp.boxs.begin(); box_iter != theApp.boxs.end(); box_iter++)
			{
				if (box_iter->first)
				{
					memcpy(buf, "$ETS", 4);
					buf[4] = DATABUS_READ_WRITE_READ;

					buf[7] = DUMMY_CMD;
					buf[8] = DUMMY_CMD;
					buf[9] = DUMMY_CMD;
					regCount = 10;
					for (i = 0; i < sizeof(seq_clr_regs) / sizeof(int); i++)
					{
						regCount += clrBeforeData(&buf[regCount], seq_clr_regs[i], 1);
					}

					for (i = 0; i < sizeof(control_regs) / sizeof(int); i++)
					{
						regCount += ConfigReg(&buf[regCount], control_regs[i], 0);
					}
					regCount++;
					buf[5] = HIBYTE(regCount);
					buf[6] = LOBYTE(regCount);
					buf[regCount - 1] = theApp.CheckSum(buf, regCount - 1);
					iWrittenBytes = theApp.Send(box_iter->first, buf, regCount);
					theApp.PrintToLog(BoxToString(box_iter->first) + " 发送设置seq clr指令：", buf, iWrittenBytes);
				}
			}
			theApp.SendFile(theApp.downfilename);
			//downed the new value from par file
			if (theApp.SendParConfig())
				return ERROR_IN_PROCESS;
		}
		else
		{
			PrintStr(PRINT_LEVEL_ERROR,"please compile first");
			return ERROR_IN_PROCESS;
		}
		
		//theApp.srcFileDowned = true;
		//theApp.downfilename = filename;
	}
	else 
	{
		PrintStr(PRINT_LEVEL_ERROR,"please set par file first");
		return ERROR_IN_PROCESS;
	}
	theApp.preDiscard = findParam("preDiscard",NULL);
	theApp.lastDiscard = findParam("postDiscard",NULL);
	noEchoes = findParam("noEchoes",NULL);
	theApp.noSamples = findParam("noSamples",NULL)+ theApp.preDiscard+ theApp.lastDiscard;
	//realnoSamples = theApp.noSamples -theApp.preDiscard  - theApp.lastDiscard;
	realnoSamples = findParam("noSamples",NULL);
	memcpy(buf, "$ETS", 4);
	buf[4] = DATABUS_READ_WRITE_READ;

	buf[7] = DUMMY_CMD;
	buf[8] = DUMMY_CMD;
	buf[9] = DUMMY_CMD;
	regCount = 10;
	regCount += clrBeforeData(&buf[regCount], RX_FIR_RESET, 1);
	regCount += ConfigReg(&buf[regCount], RX_CIC_DECIMIMATIN_FACOR, theApp.filterP.cic);
	regCount += ConfigReg(&buf[regCount], RX_FIR_DECIMIMATIN_FACOR, theApp.filterP.fir);
	if (theApp.filterP.iir == 1)
		regCount += ConfigReg(&buf[regCount], RX_FIR_IIR_BYPASS, 0x10);
	else
	{
		regCount += ConfigReg(&buf[regCount], RX_IIR_DECIMIMATIN_FACOR, theApp.filterP.iir);
		regCount += ConfigReg(&buf[regCount], RX_FIR_IIR_BYPASS, 0x00);
	}
	regCount += ConfigReg(&buf[regCount], RX_CIC_GAIN_L, theApp.filterP.cicgain3 & 0xFFFF);
	regCount += ConfigReg(&buf[regCount], RX_CIC_GAIN_M, theApp.filterP.cicgain2 & 0xFFFF);
	regCount += ConfigReg(&buf[regCount], RX_CIC_GAIN_H, theApp.filterP.cicgain1 & 0xFFFF);
	for (i = 0; i < 11; i++)
	{
		regCount += ConfigReg(&buf[regCount], COEF_WDATA_L, theApp.filterP.fircof[i] & 0xFFFF);
		if (i == 10)
			regCount += ConfigReg(&buf[regCount], COEF_WDATA_H, (theApp.filterP.fircof[i] >> 16) & 0x3 | 0x8000);
		else
			regCount += ConfigReg(&buf[regCount], COEF_WDATA_H, (theApp.filterP.fircof[i] >> 16) & 0x3);

	}
	for (i = 0; i < 16; i++)
	{
		regCount += ConfigReg(&buf[regCount], COEF_WDATA_L, theApp.filterP.iircof[i] & 0xFFFF);
		regCount += ConfigReg(&buf[regCount], COEF_WDATA_H, (theApp.filterP.iircof[i] >> 16) & 0x3 | 0x4);
	}
	for (i = 0; i < 4; i++)
	{
		regCount += ConfigReg(&buf[regCount], COEF_WDATA_L, theApp.filterP.iirgain[i] & 0xFFFF);
		if (i == 3)
			regCount += ConfigReg(&buf[regCount], COEF_WDATA_H, (theApp.filterP.iirgain[i] >> 16) & 0x3 | 0x8004);
		else
			regCount += ConfigReg(&buf[regCount], COEF_WDATA_H, (theApp.filterP.iirgain[i] >> 16) & 0x3 | 0x4);
	}
	regCount++;
	buf[5] = HIBYTE(regCount);
	buf[6] = LOBYTE(regCount);
	buf[regCount - 1] = theApp.CheckSum(buf, regCount - 1);
	for (box_iter = theApp.boxs.begin(); box_iter != theApp.boxs.end(); box_iter++)
	{ 
	if (theApp.filterSet)
	{
		iWrittenBytes = theApp.Send(box_iter->first, buf, regCount);
		theApp.PrintToLog(BoxToString(box_iter->first)+" 发送设置滤波器系数指令：", buf, iWrittenBytes);
	}
	}
	theApp.singleSampleNum = realnoSamples*noEchoes;
	theApp.DownMatrix();
	if (!setupmode)
	{
		//calc samples
		
		noSlices = findParam("noSlices",NULL);
		noSliceBlock = findParam("noSliceBlock",NULL);
		noViewBlock = findParam("noViewBlock",NULL);
		noAverages = findParam("noAverages",NULL);
		noViews = findParam("noViews",NULL);
		noViewsSec = findParam("noViewsSec",NULL);
		
		noScans = findParam("noScans",NULL);
		theApp.sampleNum = theApp.singleSampleNum *noSlices*noSliceBlock*noViewBlock*noAverages*noViews*noViewsSec*noScans;
	}
	//complex 
	if(theApp.raw_data_format)
	theApp.fileSaveTag[3] = 0;
	else 
	theApp.fileSaveTag[3] = 2;
	theApp.fileSaveTag[4] = theApp.sampleNum >> 24;
	theApp.fileSaveTag[5] = theApp.sampleNum >> 16;
	theApp.fileSaveTag[6] = theApp.sampleNum >> 8;
	theApp.fileSaveTag[7] = theApp.sampleNum;
	sampleSize = theApp.singleSampleNum * 6 * 8 + 11 + 8;
	

	theApp.sampleType = 0;
	//trigger data
	theApp.status = 0;
	theApp.triggerNum = 0;
	theApp.firstdata = true;
	//init sample parameter
	theApp.InitSample(sampleSize, realnoSamples, noEchoes);

	//Enable Tx
	EnableTx();
	for (box_iter = theApp.boxs.begin(); box_iter != theApp.boxs.end(); box_iter++)
	{
			ReceiveEnvironmentSet(box_iter->first);
	}
	//wait data clr  100ms
	Sleep(100);
#ifdef  REC_DATA
	theApp.recData.open("rec_data.txt",ios::out|ios::binary);  
#endif
	memcpy(buf, "$ETS", 4);
	buf[4] = DATABUS_READ_WRITE_READ;

	buf[7] = DUMMY_CMD;
	buf[8] = DUMMY_CMD;
	buf[9] = DUMMY_CMD;
	regCount = 10;
	//change 0xFF to 0x0 @2016/9/13 9/14 change back
	//regCount += ConfigReg(&buf[regCount], 0x2048, 0x0);
	regCount += ConfigReg(&buf[regCount], 0x2048, 0xFF);
	for (i = 0; i < sizeof(control_regs) / sizeof(int); i++)
	{
		regCount += ConfigReg(&buf[regCount], control_regs[i], 0);
		regCount += ConfigReg(&buf[regCount], control_regs[i], 1);
	}
	regCount++;
	buf[5] = HIBYTE(regCount);
	buf[6] = LOBYTE(regCount);
	buf[regCount - 1] = theApp.CheckSum(buf, regCount - 1);
	for (box_iter = theApp.boxs.begin(); box_iter != theApp.boxs.end(); box_iter++)
	{
		if (box_iter->first)
		{
			iWrittenBytes = theApp.Send(box_iter->first, buf, regCount);
			theApp.PrintToLog(BoxToString(box_iter->first) + " 发送设置seq start指令：", buf, iWrittenBytes);
		}
	}
	iWrittenBytes = theApp.Send(0, buf, regCount);
	theApp.PrintToLog(M + " 发送设置seq start指令：", buf, iWrittenBytes);
	/*
	memcpy(buf, "$ETS", 4);
	buf[4] = DATABUS_READ_WRITE_READ;

	buf[7] = DUMMY_CMD;
	buf[8] = DUMMY_CMD;
	buf[9] = DUMMY_CMD;
	regCount = 10;
	//change 0xFF to 0x0 @2016/9/13 9/14 change back
	//regCount += ConfigReg(&buf[regCount], 0x2048, 0x0);
	regCount += ConfigReg(&buf[regCount], 0x2048, 0xFF);
	for (i = 0; i < sizeof(control_regs) / sizeof(int); i++)
	{
		regCount += ConfigReg(&buf[regCount], control_regs[i], 0);
		regCount += ConfigReg(&buf[regCount], control_regs[i], 1);
	}
	regCount++;
	buf[5] = HIBYTE(regCount);
	buf[6] = LOBYTE(regCount);
	buf[regCount - 1] = theApp.CheckSum(buf, regCount - 1);
	iWrittenBytes = theApp.Send(0, buf, regCount);
	theApp.PrintToLog(M + " 发送设置seq start指令：", buf, iWrittenBytes);
	*/
	for (box_iter = theApp.boxs.begin(); box_iter != theApp.boxs.end(); box_iter++)
	{
		box_iter->second.processEnable = true;
	}
	delete[] buf;
	//set seqrun changes the seq state
	theApp.seqRun = true;
	return RIGHT_IN_PROCESS;
   
}
int SetupModeRun()
{
//	PrintStr("In Setup Mode");
	if (PrepareRun(false))
	{  
		return ERROR_IN_PROCESS;
	}
	
   if(!theApp.m_SetupModeThread)
   theApp.m_SetupModeThread = CreateThread(NULL,0,theApp.UpdateParamThreadStatic,(LPVOID)&theApp,0,NULL);
   //create Thread
   theApp.setupMode = true;
   return RIGHT_IN_PROCESS;
   
}
int SetOutputPath(const char *path)
{
  //check exist
	if (_access(path, 0) == 0)
	{
	theApp.outputFileName = path;
	
	return RIGHT_IN_PROCESS;
	}
	else
	{
		errorno = EFILEOPENFAILED;
		PrintStr(PRINT_LEVEL_ERROR, "path is not exist");
		return ERROR_IN_PROCESS;
	}
}
int WINAPI SetOutputPrefix(const char *prefix)
{
   
	theApp.outputFilePrefix = prefix;
	theApp.outputFilePrefix += "_";
	return RIGHT_IN_PROCESS;
}
//0: don't save to file 1: received and save
void WINAPI SetSaveMode(int saveMode)
{
	theApp.saveMode = saveMode;
}

char *GetOutputFile()
{
   return const_cast<char *>(theApp.outputFileName.c_str()); 
}
// iseidt true only open paramterFile for edit
// else open paramterfile for Run
int WINAPI SetParameterFile(const char * filename,bool isedit)
{
   string pwd,sname=filename;
   string::size_type pos;
   ifstream file;
   file.open(filename,ios::in);
   if(!file.is_open())
   {
	   PrintStr(PRINT_LEVEL_ERROR,"文件打开失败");
	   errorno = EFILEOPENFAILED;
	   return ERROR_IN_PROCESS;
   } 
   pos = sname.find_last_of('\\');
  // pwd = sname.substr(0, pos + 1);
   theApp.parFileName=sname;
   theApp.paramMap.clear();
   theApp.root.clear();
   theApp.waveinfo.clear();
   theApp.isEdit = isedit;
   theApp.linesset = false;
   return theApp.loadParFile(file);

}
//保存参数文件

int SaveParameterFile(const char *name)
{
	string filename, varname,strtemp;
	char str[40];
	map<std::string, paramInfo>::iterator iter;
	vector<wavefile>::iterator wave_iter;
	paramInfo pi;
	varBoxInfo vb;
	varBoardInfo vbrd;
	unsigned int i, j;
	if (name)
	{
		filename = name;
	}
	else if (theApp.parFileName.size())
	{
		filename = theApp.parFileName;
	}
	else
	{
		PrintStr(PRINT_LEVEL_ERROR,"par file name not set");
		return ERROR_IN_PROCESS;
	}
	ofstream file(filename,ios::out|ios::trunc);
	file << ":SRC\t" << theApp.srcfilename << endl;
	for (iter = theApp.paramMap.begin(); iter != theApp.paramMap.end(); iter++)
	{
		varname = iter->first;
		pi = iter->second;
		if (pi.pt == TYPE_DOUBLE)
		{
			strtemp = "double";
			sprintf_s(str,"%f ",pi.value.doublelit);
		}
		else if (pi.pt == TYPE_INT)
		{
			strtemp = "int";
			sprintf_s(str, "%d ", pi.value.intlit);
		}
		file << ":" << strtemp << "\t" << varname << "," << str;
		for (i = 0; i < pi.varBoxInfos.size(); i++)
		{
			vb = pi.varBoxInfos.at(i);
			file << ":board,";
			for (j = 0; j < vb.varBoardInfos.size(); j++)
			{
				
				vbrd = vb.varBoardInfos.at(j);
				strtemp = BoardToString(vbrd.bt);
				file << strtemp << "(" << vbrd.updateAddr << ")";
				if (j != vb.varBoardInfos.size() - 1)
					file << ",";
			}
			strtemp= BoxToString(vb.boxt);
			file << " " << "Spectrometer," << strtemp;	
		}
		file << endl;
	}
	if(theApp.waveinfo.size())
		file << endl;
	//write wave info
	for (wave_iter = theApp.waveinfo.begin(); wave_iter != theApp.waveinfo.end(); wave_iter++)
	{
		
		if (!wave_iter->wavefile_tag.compare("GRAD"))
			file << "GRAD " << wave_iter->wavefile_name<<endl;
		else if (!wave_iter->wavefile_tag.compare("TX"))
			file << "TX " << wave_iter->ch_no << " " << wave_iter->bram_no << " " << wave_iter->wavefile_name << " " << wave_iter->wavename << endl;
		
	}
	//write RAM info
	if (theApp.root.size())
	{
		file << endl;
		file<< theApp.root.toStyledString();
	}
	file.close();
	return RIGHT_IN_PROCESS;
}
string  BoxToString(int bx)
{
	ostringstream ostr;
	if (bx == 0)
		return "M";
	else if (bx > 0)
	{
		ostr << "E" << bx;
		return ostr.str();
	}
	else
	{
		PrintStr(PRINT_LEVEL_DEBUG, "Error Impossble in BoxToString");
		return NULL;
	}	
}
string  BoardToString(int brd)
{
	switch (brd)
	{
	case MAIN_BOARD:
		return "main";
	case GRADP:
		return "gradP";
	case GRADS:
		return "gradS";
	case GRADR:
		return "gradR";
	case TX1:
		return "tx1";
	case TX2:
		return "tx2";
	case RX1:
		return "rx1";
	case RX2:
		return "rx2";
	case RX3:
		return "rx3";
	case RX4:
		return "rx4";
	default:
		PrintStr(PRINT_LEVEL_DEBUG,"Error Impossble in BoardToString");
		return NULL;
	}

}
int SetParameter(const char *name,double value)
{
	string sname=name;
	paramInfo pi;
	map<string,paramInfo>::iterator paramMapIterator;
	paramMapIterator=theApp.paramMap.find(sname);
	if(paramMapIterator==theApp.paramMap.end())
	{
		PrintStr(PRINT_LEVEL_ERROR,"NO this var in par,please check");
	    return ERROR_IN_PROCESS; 
	}
	else 
		pi=paramMapIterator->second;
	//check valid
	if (!strcmp("samplePeriod", name))
	{
		if (!theApp.CheckSamplePeriod(value, true))
		{
			PrintStr(PRINT_LEVEL_ERROR, "not suppert this sample period");
			
			return ERROR_IN_PROCESS;
		}
	}
	if(pi.pt==TYPE_DOUBLE)
    pi.value.doublelit=value;
	else if(pi.pt==TYPE_INT)
	pi.value.intlit=value;
	
    if(theApp.setupMode)
	{
    EnterCriticalSection(&(theApp.g_updateParmProtect));  
	pi.needUpdate = true;
	theApp.hasParamUpdate = true;
	paramMapIterator->second = pi;
	LeaveCriticalSection(&(theApp.g_updateParmProtect));
	}
	else 
	paramMapIterator->second = pi;
	return RIGHT_IN_PROCESS;
}
double  GetParameter(char *name)
{
	string sname=name;
	paramInfo pi;
	double value;
	map<string,paramInfo>::iterator paramMapIterator;
	paramMapIterator=theApp.paramMap.find(sname);
	if(paramMapIterator==theApp.paramMap.end())
	{
		PrintStr(PRINT_LEVEL_ERROR,"NO this var in par,please check");
		errorno = EPAR_PARAM_NOT_EXIST;
		return ERROR_IN_PROCESS; 
	}
	else 
		pi=paramMapIterator->second;
	if(pi.pt==TYPE_DOUBLE)
		value=pi.value.doublelit;
	else if(pi.pt==TYPE_INT)
		value=pi.value.intlit;	
	return value;
}

int SoftWareApp::String_hashCode (const char *x)
{
	int h = 0;

	assert(x);
	while (*x) {
		h = h*MULTIPLIER + (unsigned)*x++;
	}
	return h;
}
void CloseScan()
{
	Abort();
  
}
void ReceiveEnvironmentSet(int boxindex)
{
	int regCount = 11, iWrittenBytes;
	char* buf = new char[regCount];
	memcpy(buf, "$STA", 4);
	buf[4] = DIRECT_REG_WRITE_READ;
	buf[5] = HIBYTE(regCount);
	buf[6] = LOBYTE(regCount);
	buf[7] = DUMMY_CMD;
	buf[8] = DUMMY_CMD;
	buf[9] = DUMMY_CMD;
	buf[regCount - 1] = theApp.CheckSum(buf, regCount - 1);
	iWrittenBytes = theApp.Send(boxindex,buf, regCount);
	theApp.PrintToLog(BoxToString(boxindex)+" 发送设置打开中断指令：", buf, iWrittenBytes);
	delete[] buf;

}
void ReceiveEnvironmentClear(int boxindex)
{
	int regCount = 11, iWrittenBytes;
	char* buf = new char[regCount];
	memcpy(buf, "$DON", 4);
	buf[4] = DIRECT_REG_WRITE_READ;
	buf[5] = HIBYTE(regCount);
	buf[6] = LOBYTE(regCount);
	buf[7] = DUMMY_CMD;
	buf[8] = DUMMY_CMD;
	buf[9] = DUMMY_CMD;
	buf[regCount - 1] = theApp.CheckSum(buf, regCount - 1);
	iWrittenBytes = theApp.Send(boxindex,buf, regCount);
	theApp.PrintToLog(BoxToString(boxindex)+" 发送设置关闭中断指令：", buf, iWrittenBytes);
	delete[] buf;
}
void ArmExit()
{
	int regCount = 11, iWrittenBytes;
	char* buf = new char[regCount];
	memcpy(buf, "$EXT", 4);
	buf[4] = 0x04;
	buf[5] = HIBYTE(regCount);
	buf[6] = LOBYTE(regCount);
	buf[7] = DUMMY_CMD;
	buf[8] = DUMMY_CMD;
	buf[9] = DUMMY_CMD;
	buf[regCount - 1] = theApp.CheckSum(buf, regCount - 1);
	iWrittenBytes = theApp.Send(0, buf, regCount);
	theApp.PrintToLog("发送设置ARM退出指令：", buf, iWrittenBytes);
	delete[] buf;
 
}
//2016/11/5 add RestFPGA 
void CloseSys()
{
	int i;
	
	if(!theApp.sysclosed)
	{
    theApp.sysclosed=true;
	ClrSeq();
	map<int, Spectrometer>::iterator box_iter;
	for (box_iter = theApp.boxs.begin(); box_iter != theApp.boxs.end(); box_iter++)
	{
			i = box_iter->first;
			CloseHandle(box_iter->second.m_hReadThread);
			box_iter->second.processEnable = false;
			box_iter->second.m_hReadThread = NULL;
			Sleep(500);
			closesocket(box_iter->second.m_SockClient);
			ResetFPGA(i);
		
	}
	//WaitForSingleObject(theApp.m_hReadThread,500);
	//theApp.m_hReadThread = NULL;
	
//	closesocket(theApp.m_SockClient);
	WSACleanup();
	theApp.PrintToLogNC("谱仪关闭",NULL,0);
	//close the log file
	theApp.logfile.close();
	}
	if(!theApp.abortSetupModeThread)
	{
		theApp.abortSetupModeThread=true; 
		CloseHandle(theApp.m_SetupModeThread);
		theApp.m_SetupModeThread = NULL;
		Sleep(500);
		//WaitForSingleObject(theApp.m_SetupModeThread,500);
		//theApp.m_SetupModeThread = NULL;
		//CloseHandle(theApp.m_SetupModeThread);
		
	}	
	DeleteCriticalSection(&(theApp.g_updateParmProtect));	
}
int ConfigWriteEnd(int bx)
{

	int regCount = 19, iWrittenBytes;
	char* buf = new char[regCount];
	memcpy(buf, "$ETS", 4);
	buf[4] = 0x4;
	buf[5] = HIBYTE(regCount);
	buf[6] = LOBYTE(regCount);
	buf[7] = DUMMY_CMD;
	buf[8] = DUMMY_CMD;
	buf[9] = DUMMY_CMD;
	ConfigReg(&buf[10], WRITE_END, 1);
	buf[regCount - 1] = theApp.CheckSum(buf, regCount - 1);
	iWrittenBytes = theApp.Send(bx, buf, regCount);
	theApp.PrintToLog("发送设置结束寄存器指令：", buf, iWrittenBytes);
	delete[] buf;
	if (SOCKET_ERROR == iWrittenBytes)
		return ERROR_IN_PROCESS;
	return RIGHT_IN_PROCESS;
}
int ConfigSingleReg_i(int  bx, int addr, int value)
{

	int regCount = 19, iWrittenBytes;
	char* buf = new char[regCount];
	memcpy(buf, "$ETS", 4);
	buf[4] = 0x4;
	buf[5] = HIBYTE(regCount);
	buf[6] = LOBYTE(regCount);
	buf[7] = DUMMY_CMD;
	buf[8] = DUMMY_CMD;
	buf[9] = DUMMY_CMD;
	ConfigReg(&buf[10], addr, value);
	buf[regCount - 1] = theApp.CheckSum(buf, regCount - 1);
	iWrittenBytes = theApp.Send(bx, buf, regCount);
	theApp.PrintToLog(BoxToString(bx)+" 发送设置单个寄存器指令：", buf, iWrittenBytes);
	delete[] buf;
	if (SOCKET_ERROR == iWrittenBytes)
		return ERROR_IN_PROCESS;
	return RIGHT_IN_PROCESS;
}
int ConfigSingleReg(int  bx, int addr, int value)
{

	int regCount = 19, iWrittenBytes;
	char* buf = new char[regCount];
	memcpy(buf, "$ETS", 4);
	buf[4] = 0x1;
	buf[5] = HIBYTE(regCount);
	buf[6] = LOBYTE(regCount);
	buf[7] = DUMMY_CMD;
	buf[8] = DUMMY_CMD;
	buf[9] = DUMMY_CMD;
	ConfigReg(&buf[10], addr, value);
	buf[regCount - 1] = theApp.CheckSum(buf, regCount - 1);
	iWrittenBytes = theApp.Send(bx, buf, regCount);
	theApp.PrintToLog("发送设置单个寄存器指令：", buf, iWrittenBytes);
	delete[] buf;
	if (SOCKET_ERROR == iWrittenBytes)
		return ERROR_IN_PROCESS;
	return RIGHT_IN_PROCESS;
}
int QueryReg(int  bx, int addr)
{
	int regCount = 15, iWrittenBytes, waitReturn;
	char* buf = new char[regCount];
	memcpy(buf, "$ETR", 4);
	buf[4] = 0x01;
	buf[5] = HIBYTE(regCount);
	buf[6] = LOBYTE(regCount);
	buf[7] = DUMMY_CMD;
	buf[8] = DUMMY_CMD;
	buf[9] = DUMMY_CMD;
	buf[10] = addr >> 24;
	buf[11] = addr >> 16;
	buf[12] = addr >> 8;
	buf[13] = addr & 0xff;
	buf[regCount - 1] = theApp.CheckSum(buf, regCount - 1);
	theApp.boxs[bx].querySingleReg = true;
	theApp.boxs[bx].queryLowAddr = addr;
	iWrittenBytes = theApp.Send(bx, buf, regCount);
	theApp.PrintToLog("发送设置单个寄存器查询指令：", buf, iWrittenBytes);
	delete[] buf;
	if (SOCKET_ERROR == iWrittenBytes)
		return ERROR_IN_READ_REG;
	waitReturn = WaitForSingleObject(theApp.boxs[bx].syncEvent, 2000);
	theApp.boxs[bx].querySingleReg = false;
	if (waitReturn == WAIT_OBJECT_0)
	{

		return theApp.boxs[bx].single_reg_value;

	}
	else if (waitReturn == WAIT_TIMEOUT)
	{
		PrintStr(PRINT_LEVEL_DEBUG,"read time out");
		return ERROR_IN_READ_REG;
	}
	else if (waitReturn == WAIT_FAILED)
	{
		PrintStr(PRINT_LEVEL_DEBUG,"read failed");
		return ERROR_IN_READ_REG;
	}
	return ERROR_IN_READ_REG;
}
int  QueryRegs(int  boxType, int startaddr, int endaddr, char *file)
{
	/*
	int regCount = 19, iWrittenBytes;
	char* buf = new char[regCount];
	memcpy(buf, "$ETR", 4);
	buf[4] = DIRECT_REG_WRITE_READ;
	buf[5] = HIBYTE(packLength);
	buf[6] = LOBYTE(packLength);
	buf[7] = DUMMY_CMD;
	buf[8] = DUMMY_CMD;
	buf[9] = BAT_READ;

	buf[10] = iBeginAddr >> 24;
	buf[11] = iBeginAddr >> 16;
	buf[12] = iBeginAddr >> 8;
	buf[13] = iBeginAddr & 0xff;
	buf[14] = iEndAddr >> 24;
	buf[15] = iEndAddr >> 16;
	buf[16] = iEndAddr >> 8;
	buf[17] = iEndAddr & 0xff;
	*/
	return 0;
}
int  SetRxDCValue(int  box, int board, int channel, int value)
{
	int dcReg, regCount=19, iWrittenBytes;
	boxCaliInfo bx;
	rxBoardCaliInfo rxCal;
	//vector<rxCHCali> rxCal;
	map<int, boxCaliInfo>::iterator biter;
	map<boardType, rxBoardCaliInfo>::iterator rxiter;
	if ((biter = theApp.nmrCali.find(box)) == theApp.nmrCali.end())
	{
		PrintStr(PRINT_LEVEL_ERROR,"set dc error,box not find ");
		return ERROR_IN_PROCESS;
	}
	bx = biter->second;
	if ((rxiter = bx.rxCalis.find(boardType(board))) == bx.rxCalis.end())
	{
		PrintStr(PRINT_LEVEL_ERROR,"set dc error,board not find ");
		return ERROR_IN_PROCESS;
	}
	rxCal = rxiter->second;
	rxCHCali &rCHCal = rxCal.rxBoardCali.at(channel);
	rCHCal.dc = value;
	//save
	rxiter->second = rxCal;
	biter->second = bx;


	dcReg= (RX1_C1_ADC_DC_ADJ + channel + (board - RX1) * 0x200) * 2;
	char *buf = new char[BUF_STANDARD_SIZE];
	memcpy(buf, "$ETS", 4);
	buf[4] = DIRECT_REG_WRITE_READ;
	buf[5] = HIBYTE(regCount);
	buf[6] = LOBYTE(regCount);
	buf[7] = DUMMY_CMD;
	buf[8] = DUMMY_CMD;
	buf[9] = DUMMY_CMD;
	ConfigReg(&buf[10], dcReg, value);
	buf[regCount - 1] = theApp.CheckSum(buf, regCount - 1);
	iWrittenBytes = theApp.Send(box, buf, regCount);
	theApp.PrintToLog("发送设置DC直流寄存器指令：", buf, iWrittenBytes);
	delete[] buf;
	if (SOCKET_ERROR == iWrittenBytes)
		return ERROR_IN_PROCESS;
	return RIGHT_IN_PROCESS;
}
int  SetTxATT(int  box, int board, int channel, float value)
{
	int regCount = 19, iWrittenBytes, compGainOffset,ivalue;
	boxCaliInfo bx;
	txBoardCaliInfo txCal;
	//vector<rxCHCali> rxCal;
	map<int, boxCaliInfo>::iterator biter;
	map<boardType, txBoardCaliInfo>::iterator txiter;
	if ((biter = theApp.nmrCali.find(box)) == theApp.nmrCali.end())
	{
		PrintStr(PRINT_LEVEL_ERROR,"set tx att error,box not find ");
		return ERROR_IN_PROCESS;
	}
	bx = biter->second;
	if ((txiter = bx.txCalis.find(boardType(board))) == bx.txCalis.end())
	{
		PrintStr(PRINT_LEVEL_ERROR,"set dc error,board not find ");
		return ERROR_IN_PROCESS;
	}
	txCal = txiter->second;
	float &tCHCal = txCal.txBoardCali.at(channel);
	tCHCal = value;
	txiter->second = txCal;
	biter->second = bx;
	ivalue = round(value*32);
	switch (board)
	{
	case TX1:
		compGainOffset = TX1_C1_COMP_GAIN_DB;
		break;
	case TX2:
		compGainOffset = TX2_C1_COMP_GAIN_DB;
		break;
	default:
		break;
	}
	compGainOffset = (compGainOffset + channel * 2) * 2;
	char *buf = new char[BUF_STANDARD_SIZE];
	memcpy(buf, "$ETS", 4);
	buf[4] = DIRECT_REG_WRITE_READ;
	buf[5] = HIBYTE(regCount);
	buf[6] = LOBYTE(regCount);
	buf[7] = DUMMY_CMD;
	buf[8] = DUMMY_CMD;
	buf[9] = DUMMY_CMD;
	ConfigReg(&buf[10], compGainOffset, ivalue);
	buf[regCount - 1] = theApp.CheckSum(buf, regCount - 1);
	iWrittenBytes = theApp.Send(box, buf, regCount);
	theApp.PrintToLog("发送设置发射衰减器寄存器指令：", buf, iWrittenBytes);
	delete[] buf;
	if (SOCKET_ERROR == iWrittenBytes)
		return ERROR_IN_PROCESS;
	return RIGHT_IN_PROCESS;
}
int GetRxChannels(int  box, int board)
{
	boxCaliInfo bx;
	rxBoardCaliInfo rxCal;
	//vector<rxCHCali> rxCal;
	map<int, boxCaliInfo>::iterator biter;
	map<boardType, rxBoardCaliInfo>::iterator rxiter;
	if ((biter = theApp.nmrCali.find(box)) == theApp.nmrCali.end())
	{
		theApp.PrintToLogNC("In GetRxChannels box not find ",NULL,0);
		return 0;
	}
	bx = biter->second;
	if ((rxiter = bx.rxCalis.find(boardType(board))) == bx.rxCalis.end())
	{
		theApp.PrintToLogNC("In GetRxChannels board not find ", NULL, 0);
		return 0;
	}
	rxCal = rxiter->second;
	return  rxCal.rxBoardCali.size();
}
int  SetRxATT(int  box, int board, int channel, float att,float amp1,float amp2,float amp3, int switchValue)
{
	int  regCount, iWrittenBytes, rG1, rG2, rG3, rATT,
		rSwitch,iAmp1,iAmp2,iAmp3,iAtt;
	boxCaliInfo bx;
	rxBoardCaliInfo rxCal;
	//vector<rxCHCali> rxCal;
	map<int, boxCaliInfo>::iterator biter;
	map<boardType, rxBoardCaliInfo>::iterator rxiter;
	if ((biter = theApp.nmrCali.find(box)) == theApp.nmrCali.end())
	{
		PrintStr(PRINT_LEVEL_ERROR,"set rx att error,box not find ");
		return ERROR_IN_PROCESS;
	}
	bx = biter->second;
	if ((rxiter = bx.rxCalis.find(boardType(board))) == bx.rxCalis.end())
	{
		PrintStr(PRINT_LEVEL_ERROR,"set dc error,board not find ");
		return ERROR_IN_PROCESS;
	}
	rxCal = rxiter->second;
	if(channel>=rxCal.rxBoardCali.size())
	{
		PrintStr(PRINT_LEVEL_INFO, "set dc error,channel not find ");
		return ERROR_IN_PROCESS;
	}
	rxCHCali &rCHCal = rxCal.rxBoardCali.at(channel);
	rCHCal.amp1 = amp1;
	rCHCal.amp2 = amp2;
	rCHCal.amp3 = amp3;
	rCHCal.att = att;
	rCHCal.swi = switchValue;
	//save
	rxiter->second = rxCal;
	biter->second = bx;

	iAmp1 =round(amp1 * 32);
    iAmp2= round(amp2 * 32);
    iAmp3= round(amp3 * 32);
	iAtt=  round(att * 32);
	rG1 = (RX1_C1_G1 + channel + (board - RX1) * 0x200) * 2;
	rG2 = (RX1_C1_G2 + channel + (board - RX1) * 0x200) * 2;
	rG3 = (RX1_C1_G3 + channel + (board - RX1) * 0x200) * 2;
	rATT = (RX1_C1_ATT0 + channel + (board - RX1) * 0x200) * 2;
	rSwitch= (RX1_C1_AMP_CTRL + channel + (board - RX1) * 0x200) * 2;
	char *buf = new char[BUF_STANDARD_SIZE];
	memcpy(buf, "$ETS", 4);
	buf[4] = DIRECT_REG_WRITE_READ;
	buf[7] = DUMMY_CMD;
	buf[8] = DUMMY_CMD;
	buf[9] = DUMMY_CMD;
	regCount = 10;
	regCount+=ConfigReg(&buf[regCount], rG1, iAmp1);
	regCount += ConfigReg(&buf[regCount], rG2, iAmp2);
	regCount += ConfigReg(&buf[regCount], rG3, iAmp3);
	regCount += ConfigReg(&buf[regCount], rATT, iAtt);
	regCount += ConfigReg(&buf[regCount], rSwitch, switchValue);
	regCount++;
	buf[5] = HIBYTE(regCount);
	buf[6] = LOBYTE(regCount);
	buf[regCount - 1] = theApp.CheckSum(buf, regCount - 1);
	iWrittenBytes = theApp.Send(box, buf, regCount);
	theApp.PrintToLog("发送设置接收放大链指令：", buf, iWrittenBytes);
	delete[] buf;
	if (SOCKET_ERROR == iWrittenBytes)
		return ERROR_IN_PROCESS;
	return RIGHT_IN_PROCESS;
}
int  SaveTXAllCaliValue(int  box)
{
	unsigned int i;
	float tc;
	boxCaliInfo bx;
	txBoardCaliInfo txCal;
	map<int, boxCaliInfo>::iterator biter;
	map<boardType, txBoardCaliInfo>::iterator txiter;
	if ((biter = theApp.nmrCali.find(box)) == theApp.nmrCali.end())
	{
		PrintStr(PRINT_LEVEL_ERROR,"save all tx error,box not find ");
		return ERROR_IN_PROCESS;
	}
	bx = biter->second;
	for (txiter = bx.txCalis.begin(); txiter != bx.txCalis.end(); txiter++)
	{
	txCal = txiter->second;
	ofstream file(txCal.filename, ios::out | ios::trunc);
	if (file.is_open())
	{
		for (i = 0; i < txCal.txBoardCali.size(); i++)
		{
			tc = txCal.txBoardCali.at(i);
			file << tc << endl;
		}
		file.close();
		
	}
	else
	{
		FormatOutPut(PRINT_LEVEL_ERROR,"%s open failed", txCal.filename.c_str());
		return ERROR_IN_PROCESS;
	}
	}
	return RIGHT_IN_PROCESS;
}
int  SaveTXCaliValue(int  box, int board)
{
	unsigned int i;
	float tc;
	boxCaliInfo bx;
	txBoardCaliInfo txCal;
	//vector<rxCHCali> rxCal;
	map<int, boxCaliInfo>::iterator biter;
	map<boardType, txBoardCaliInfo>::iterator txiter;
	if ((biter = theApp.nmrCali.find(box)) == theApp.nmrCali.end())
	{
		PrintStr(PRINT_LEVEL_ERROR,"save single tx error,box not find ");
		return ERROR_IN_PROCESS;
	}
	bx = biter->second;
	if ((txiter = bx.txCalis.find(boardType(board))) == bx.txCalis.end())
	{
		PrintStr(PRINT_LEVEL_ERROR,"save single tx error,board not find ");
		return ERROR_IN_PROCESS;
	}
	txCal = txiter->second;
	ofstream file(txCal.filename, ios::out | ios::trunc);
	if (file.is_open())
	{
		for (i = 0; i < txCal.txBoardCali.size(); i++)
		{
			tc = txCal.txBoardCali.at(i);
			file <<tc<< endl;
		}
		file.close();
		return RIGHT_IN_PROCESS;
	}
	else
	{
		PrintStr(PRINT_LEVEL_ERROR," save single tx open file failed");
		return ERROR_IN_PROCESS;
	}


}
int  SaveRXAllCaliValue(int  box)
{
	unsigned int i;
	rxCHCali rc;
	boxCaliInfo bx;
	rxBoardCaliInfo rxCal;
	//vector<rxCHCali> rxCal;
	map<int, boxCaliInfo>::iterator biter;
	map<boardType, rxBoardCaliInfo>::iterator rxiter;
	if ((biter = theApp.nmrCali.find(box)) == theApp.nmrCali.end())
	{
		PrintStr(PRINT_LEVEL_ERROR,"save all rx error,box not find ");
		return ERROR_IN_PROCESS;
	}
	bx = biter->second;
	for (rxiter = bx.rxCalis.begin(); rxiter != bx.rxCalis.end(); rxiter++)
	{
		rxCal = rxiter->second;
		ofstream file(rxCal.filename, ios::out | ios::trunc);
		if (file.is_open())
		{
			for (i = 0; i < rxCal.rxBoardCali.size(); i++)
			{
				rc = rxCal.rxBoardCali.at(i);
				file << rc.att << "," << rc.amp1 << "," << rc.amp2 << "," << rc.amp3 << "," <<rc.swi<<","<<rc.dc << endl;
			}
			file.close();
			
		}
		else
		{
			FormatOutPut(PRINT_LEVEL_ERROR,"%s open failed", rxCal.filename.c_str());
			return ERROR_IN_PROCESS;
		}
	}
	return RIGHT_IN_PROCESS;
}
int  SaveRXCaliValue(int  box, int board)
{
	unsigned int i;
	rxCHCali rc;
	boxCaliInfo bx;
	rxBoardCaliInfo rxCal;
	//vector<rxCHCali> rxCal;
	map<int, boxCaliInfo>::iterator biter;
	map<boardType, rxBoardCaliInfo>::iterator rxiter;
	if ((biter = theApp.nmrCali.find(box)) == theApp.nmrCali.end())
	{
		PrintStr(PRINT_LEVEL_ERROR,"save single rx error,box not find ");
		return ERROR_IN_PROCESS;
	}
	bx = biter->second;
	if ((rxiter = bx.rxCalis.find(boardType(board))) == bx.rxCalis.end())
	{
		PrintStr(PRINT_LEVEL_ERROR,"save single rx error,board not find ");
		return ERROR_IN_PROCESS;
	}
	rxCal = rxiter->second;
	ofstream file(rxCal.filename,ios::out|ios::trunc);
	if (file.is_open())
	{
		for (i = 0; i < rxCal.rxBoardCali.size(); i++)
		{   
			rc = rxCal.rxBoardCali.at(i);
			file << rc.att << "," << rc.amp1 << "," << rc.amp2 << "," << rc.amp3 << "," <<rc.swi<<","<<rc.dc << endl;
		}
		file.close();
		return RIGHT_IN_PROCESS;
	}
	else
	{ 
		PrintStr(PRINT_LEVEL_ERROR," save single rx open file failed");
		return ERROR_IN_PROCESS;
	}

}
int ScanCompleted()
{
	if (QueryReg(M, MAINCTRL_SEQ_END_REG)==1)
	{
		if (theApp.boxs[0].processEnable)
			return SCAN_COMPLETE_UPLOADING;
		else
			return SCAN_COMPLETEED;
	}
	return SCAN_UNCOMPLETED;
}
const char *  GetSlotInfo(int  boxType, int slot,int real)
{
	string strtemp;
	int regCount=12, iWrittenBytes, waitReturn, i,slotInfo,slotvalue,txnum,rxnum,offset;
	///first need qeury reg 0x
	ostringstream outputstr;
	char *receiveData;
	char dna[16];//aha 15 dna 
	map<int, Spectrometer>::iterator box_iter;
	box_iter = theApp.boxs.find(boxType);
	if (box_iter==theApp.boxs.end())
	{
		errorno = EBOXTYPE;
		return NULL;
	}
	if (slot > 5 || slot < 0)
	{
		errorno = ESLOT;
		return NULL;
	}
	txnum = 0;
	rxnum = 0;
	if (!box_iter->second.boxChecked)
	{
		if ((slotInfo = QueryReg(boxType, 0x50000000 + CARD_IDENTIFY_REG)) == ERROR_IN_READ_REG)
		{
			errorno = EREADCARDREG;
			return NULL;
		}
		/*anaysis slotInfo*/
		for (i = 1; i <= 5;i++)
		{
			slotvalue = (slotInfo >> (2 * (i - 1))) & 0x03;
			switch (slotvalue)
			{
			case 0:
				box_iter->second.boxcardInfo[i] = -1;/*no card*/
				break;
			case 1:/*tx card*/
				box_iter->second.boxcardInfo[i] = TX1+txnum;/*no card*/
				txnum++;
				break;
			case 2:/*rx card*/
				box_iter->second.boxcardInfo[i] = RX1 + rxnum;/*no card*/
				rxnum++;
				break;
			default:
				errorno = ESLOTVALUE;
				return NULL;
			}
		}
		if (txnum > 2)
		{
			errorno = ESLOTTXNUM;
			return NULL;
		}
		if (rxnum > 4)
		{
			errorno = ESLOTRXNUM;
			return NULL;
		}
		box_iter->second.boxcardInfo[0] = MAIN_BOARD;
		box_iter->second.boxChecked = true;
	}
	
	if (box_iter->second.boxcardInfo[slot] == -1)
	{
		errorno = ENOCARD;
		return NULL;
	}
	
	Json::Value root;
	if (real)
	{
		char* buf = new char[BUF_STANDARD_SIZE];
		memcpy(buf, "$FSQ", 4);
		buf[4] = QERUY_FPGA_STATUS;
		buf[5] = HIBYTE(regCount);
		buf[6] = LOBYTE(regCount);
		buf[7] = DUMMY_CMD;
		buf[8] = DUMMY_CMD;
		buf[9] = DUMMY_CMD;
		buf[10] = slot;
		buf[regCount - 1] = theApp.CheckSum(buf, regCount - 1);
		theApp.boxs[boxType].queryFPGAInfo = true;
		theApp.boxs[boxType].queryLowAddr = slot;
		iWrittenBytes = theApp.Send(boxType, buf, regCount);
		theApp.PrintToLog("发送查询卡槽信息指令：", buf, iWrittenBytes);
		delete[] buf;
		if (SOCKET_ERROR == iWrittenBytes)
			return NULL;
		
		waitReturn = WaitForSingleObject(theApp.boxs[boxType].readsyncEvent, 2000);
		theApp.boxs[boxType].queryFPGAInfo = false;
		
		if (waitReturn == WAIT_OBJECT_0)
		{
			/*set value in jason data*/
			string str(theApp.boxs[boxType].receiveData);
			root["NAME"] = Json::Value(str);
		}
		else if (waitReturn == WAIT_TIMEOUT)
		{
			errorno = EREADTIMEOUT;
			PrintStr(PRINT_LEVEL_DEBUG, "read time out");
			return NULL;
		}
		else if (waitReturn == WAIT_FAILED)
		{
			errorno = EREADFAILED;
			PrintStr(PRINT_LEVEL_DEBUG, "read failed");
			return NULL;
		}
	}
	receiveData = box_iter->second.receiveData;
	if (GetBoardInfo(boxType, box_iter->second.boxcardInfo[slot], real))
	return NULL;
	else
	{
		
		offset = 0;
		strtemp = BoardToString(box_iter->second.boxcardInfo[slot]);
		root["TYPE"] = Json::Value(strtemp);
		if (real)
		{
			offset = 2;
			root["FPGAver"] = Json::Value((receiveData[0]<<8)| receiveData[1]);
			i = 0;

			for (i = 0; i < 15; )
			{
			
				if (i ==0)
				{
					dna[i++] = theApp.ConvertItoS(receiveData[offset++]&0x1);
				}
				else
				{
					dna[i++] = theApp.ConvertItoS((unsigned char)receiveData[offset]>>4);
					dna[i++] = theApp.ConvertItoS(receiveData[offset] &0xF);
					offset++;
				}		
			}
			dna[i] = '\0';
			root["FPGADNA"] = Json::Value(dna);
		}
			root["warn"]= (receiveData[offset] << 8) |
				(receiveData[offset+1]);
			offset += 2;
			root["coretemp"]= (unsigned int)((unsigned char)receiveData[offset]) << 8
				| (unsigned int)((unsigned char)receiveData[offset+1]);
			offset += 2;
			root["vccint"]= (unsigned int)((unsigned char)receiveData[offset]) << 8 | (unsigned int)((unsigned char)receiveData[offset+1]);
			offset += 2;
			root["vccaux"]= (unsigned int)((unsigned char)receiveData[offset]) << 8 | (unsigned int)((unsigned char)receiveData[offset+1]);
			offset += 2;
			root["vccram"]= (unsigned int)((unsigned char)receiveData[offset]) << 8 | (unsigned int)((unsigned char)receiveData[offset+1]);
	}
	if (root.toStyledString().size()>BUF_STANDARD_SIZE)
	{
		errorno = EBUFOVERFLOW;
		return NULL;
	}
	strcpy_s(theApp.jasonstr, root.toStyledString().size() + 1, root.toStyledString().c_str());
	//ofstream file("2.txt");
	//file << root.toStyledString().c_str();
	//file.close();
	//const char *temp= root.toStyledString().c_str();
	return theApp.jasonstr;
}
int  GetBoardInfo(int boxType, int boardType, int  real)
{
	int regCount , iWrittenBytes, waitReturn,warnaddr,dnaaddr,chipid,i,addrTemp;
	
	switch (boardType)
	{
	case MAIN_BOARD:
		chipid = CTRL_BR_CHIP_ID;
		warnaddr = CTRL_FPGA_MONITOR_ALARM;
		break;
	case TX1:
		chipid = TX1_B1_CHIP_ID;
		warnaddr = TX1_FPGA_MONITOR_ALARM;
		break;
	case TX2:
		chipid = TX2_B1_CHIP_ID;
		warnaddr = TX2_FPGA_MONITOR_ALARM;
		break;
	case RX1:
		chipid = RX1_B1_CHIP_ID;
		warnaddr = RX1_FPGA_MONITOR_ALARM;
		break;
	case RX2:
		chipid = RX2_B1_CHIP_ID;
		warnaddr = RX2_FPGA_MONITOR_ALARM;
		break;
	case RX3:
		chipid = RX3_B1_CHIP_ID;
		warnaddr = RX3_FPGA_MONITOR_ALARM;
		break;
	case RX4:
		chipid = RX4_B1_CHIP_ID;
		warnaddr = RX4_FPGA_MONITOR_ALARM;
		break;
	default:
		PrintStr(PRINT_LEVEL_DEBUG, "Impossible:board not find");
		break;
	}
	dnaaddr = warnaddr + 0x10;
	char* buf = new char[BUF_STANDARD_SIZE];
	memcpy(buf, "$ETR", 4);
	buf[4] = DIRECT_BAT_READ; //direct read 
	buf[7] = DUMMY_CMD;
	buf[8] = DUMMY_CMD;
	buf[9] = CHAOS_ADDR;
	regCount = 10;
	//qeruy fpga ver and dna
	if (real)
	{
		regCount += ConfigQuiry(&buf[regCount], chipid);
		for (i = 0; i < 4; i++)
		{
			addrTemp = dnaaddr - i * 2;
			regCount += ConfigQuiry(&buf[regCount], addrTemp);

		}
	}
	for (i = 0; i < 5; i++)
	{
		addrTemp = warnaddr + i * 2;
		regCount+=ConfigQuiry(&buf[regCount], addrTemp);
	}
	regCount++;
	buf[5] = HIBYTE(regCount);
	buf[6] = LOBYTE(regCount);
	buf[regCount - 1] = theApp.CheckSum(buf, regCount - 1);
	theApp.boxs[boxType].queryFPGAInfo = true;
	if (real)
		theApp.boxs[boxType].queryLowAddr = chipid;
	else
		theApp.boxs[boxType].queryLowAddr = warnaddr;
	iWrittenBytes = theApp.Send(boxType, buf, regCount);
	theApp.PrintToLog("发送查询板卡信息指令：", buf, iWrittenBytes);
	delete[] buf;
	if (SOCKET_ERROR == iWrittenBytes)
		return NULL;
	
	waitReturn = WaitForSingleObject(theApp.boxs[boxType].readsyncEvent, 2000);
	theApp.boxs[boxType].queryFPGAInfo = false;
	if (waitReturn == WAIT_OBJECT_0)
	{
		
		return RIGHT_IN_PROCESS;
	}
	else if (waitReturn == WAIT_TIMEOUT)
	{
		errorno = EREADTIMEOUT;
		PrintStr(PRINT_LEVEL_DEBUG, "read time out");
		return ERROR_IN_PROCESS;
	}
	else if (waitReturn == WAIT_FAILED)
	{
		
		errorno = EREADFAILED;
		PrintStr(PRINT_LEVEL_DEBUG, "read failed");
		return ERROR_IN_PROCESS;
	}
	return ERROR_IN_PROCESS;
}
int WriteSN(int  boxType, int slot, char *sn)
{
	int i,strsize, iWrittenBytes;
	strsize = strlen(sn);
	if (strsize > 26)
	{
		PrintStr(PRINT_LEVEL_ERROR, "SN is exceed 26");
		return ERROR_IN_PROCESS;
	}
	char* buf = new char[BUF_STANDARD_SIZE];
	memcpy(buf, "$WFS", 4);
	buf[4] = DUMMY_CMD;
	buf[7] = DUMMY_CMD;
	buf[8] = DUMMY_CMD;
	buf[9] = DUMMY_CMD;
	buf[10] = slot;
	for (i = 0; i < strsize; i++)
		buf[11 + i] = sn[i];
	buf[5] = HIBYTE(strsize+12);
	buf[6] = LOBYTE(strsize+12);
	buf[11 +i] = theApp.CheckSum(buf, strsize + 11);
	iWrittenBytes = theApp.Send(boxType, buf, strsize + 12);
	theApp.PrintToLog("发送设置FPGA串号指令：", buf, iWrittenBytes);
	delete[] buf;
	if (SOCKET_ERROR == iWrittenBytes)
		return ERROR_IN_PROCESS;
	return RIGHT_IN_PROCESS;

}
int  ArmVerQeruy(int boxType)
{

	int regCount = 11, iWrittenBytes, waitReturn;

	char* buf = new char[BUF_STANDARD_SIZE];
	memcpy(buf, "$AVQ", 4);
	buf[4] = DUMMY_CMD;
	buf[5] = HIBYTE(regCount);
	buf[6] = LOBYTE(regCount);
	buf[7] = DUMMY_CMD;
	buf[8] = DUMMY_CMD;
	buf[9] = DUMMY_CMD;
	
	//ConfigQuiry(&buf[10],slot);
	buf[regCount - 1] = theApp.CheckSum(buf, regCount - 1);
	iWrittenBytes = theApp.Send(boxType, buf, regCount);
	theApp.PrintToLog("发送查询ARM版本信息指令：", buf, iWrittenBytes);
	delete[] buf;
	if (SOCKET_ERROR == iWrittenBytes)
	{

		return NULL;
	}
	waitReturn = WaitForSingleObject(theApp.boxs[boxType].readsyncEvent, 2000);
	
	if (waitReturn == WAIT_OBJECT_0)
	{
		errorno = ENOTCONNECTED;
		return RIGHT_IN_PROCESS;
	}
	else if (waitReturn == WAIT_TIMEOUT)
	{
		errorno = EREADTIMEOUT;
		PrintStr(PRINT_LEVEL_DEBUG, "read time out");
		return ERROR_IN_PROCESS;
	}
	else if (waitReturn == WAIT_FAILED)
	{
		errorno = EREADFAILED;
		PrintStr(PRINT_LEVEL_DEBUG, "read failed");
		return ERROR_IN_PROCESS;
	}
	return ERROR_IN_PROCESS;
}
//TODO
int GetConnectStatus(int boxType)
{
	return theApp.boxs[boxType].connect_status;
}
void  PathUriToWin(string &str)
{
	
	string::size_type pos;
	for (pos = 0; pos !=str.length(); pos++)
	{
		if (str[pos] == '/') str[pos] = '\\';
	}
}
/*
int   ParseMappingFile(const char *filename,ostringstream &ss)
{
	ifstream file(filename, ios::in);
	if (!file.is_open())
	{
		PrintStr(PRINT_LEVEL_ERROR, "fail to open  file: ");
		return ERROR_IN_PROCESS;
	}
	string::size_type posstart,posend,pos;
	string filestr(filename);
	string readline,strtemp,path,realfile;
	int box, board, ch;
	pos = filestr.find_last_of('\\');
	path = filestr.substr(0, pos + 1);
	while (getline(file, readline))
	{
			//comment
			if (!readline.compare(0, 1, "#") || readline.empty())
				continue;
			posstart = readline.find(',');
			box = atoi(readline.substr(0, 1).c_str());
			board = atoi(readline.substr(posstart+1, 1).c_str());
			posstart = readline.find(',', posstart + 1);
			ch= atoi(readline.substr(posstart + 1, 1).c_str());
			posstart++;
			posstart = readline.find(',', posstart);
			posend = readline.find(',', posstart+1);
			posstart++;
			if (posend != posstart )
			{
				strtemp = path + readline.substr(posstart , posend - posstart);
				PathUriToWin(strtemp);
				//fre file exist
				if (board >= RX1)
				{
					ss << "	process rx fre offset Table " << strtemp << endl;
					if (SetRxFreOffsetTable(box, board, ch, strtemp.c_str()))
					{
						ss << "	process rx fre offset Table failed"  << endl;
						return ERROR_IN_PROCESS;
					}
					ss << "	process rx fre offset Table success"  << endl;
				}
				else
				{
					ss << "	process tx fre offset Table " << strtemp << endl;
					if (SetTxFreOffsetTable(box, board, ch, strtemp.c_str()))
					{
						ss << "	process tx fre offset Table failed"  << endl;
						return ERROR_IN_PROCESS;
					}
					ss << "	process tx fre offset Table success"  << endl;
				}
				}
			posstart = ++posend;
			posend = readline.find(',', posstart);
			if (posend != posstart)
			{
				strtemp = path + readline.substr(posstart , posend - posstart);
				PathUriToWin(strtemp);
				//fre file exist
				if (board < RX1)
				{
					ss << "	process tx gain Table " << strtemp << endl;
					if (SetTxGainTable(box, board, ch, strtemp.c_str()))
					{
						ss << "	process tx gain Table failed" << endl;
						return ERROR_IN_PROCESS;

					}
					ss << "	process tx gain Table success" << endl;

				}
			}
			posstart = ++posend;
			if (readline.size() != posstart)
			{
				//phase file exist
				strtemp = path + readline.substr(posstart , string::npos);
				PathUriToWin(strtemp);
				if (board >= RX1)
				{
					ss << "	process RX phase Table " << strtemp << endl;
					if (SetRxPhaseTable(box, board, ch, strtemp.c_str()))
					{
						ss << "	process RX phase Table failed" << endl;
						return ERROR_IN_PROCESS;
					}
					ss << "	process RX phase Table success" << endl;
				}
				else
				{
					ss << "	process tx phase Table " << strtemp << endl;
					if (SetTxPhaseTable(box, board, ch, strtemp.c_str()))
					{
						ss << "	process tx phase Table failed" << endl;
						return ERROR_IN_PROCESS;
					}
					ss << "	process tx phase Table success" << endl;
				}
			}
	}
	return RIGHT_IN_PROCESS;
}

int SetConfigure(const char *filename)
{
	string processtr, readline,tag,filetag,path;
	string filestr(filename);
	int ret;
	ifstream file(filename, ios::in);
	string::size_type pos;
	ostringstream out;
	theApp.gradwaveCfged = false;
	theApp.txwaveCfged = false;
	if (!file.is_open())
	{
		out << "open " << filename << " failed" << endl;
		ret = ERROR_IN_PROCESS;
		//processtr = out.str();
		goto reta;
	}
	pos = filestr.find_last_of('\\');
	path = filestr.substr(0, pos + 1);
	processtr = "starting parsing cfg file\n";
	while (getline(file, readline))
	{
		//comment
		if (!readline.compare(0, 1, "#") || readline.empty())
			continue;
		if ((pos = readline.find('=')) == string::npos)
			continue;
		else
		{
			tag = readline.substr(0, pos);
			filetag = readline.substr(pos + 1, string::npos);
			if (filetag.empty()) continue;
			PathUriToWin(filetag);
			filetag = path + filetag;
			if(!tag.compare("read_gain"))
			{ 
				out << "starting process read gain file "<<filetag<<endl;		
				if(SetGradientAllScale(CHANNEL_X,filetag.c_str()))
				{
					out << "process read gain file failed" << endl;
					ret = ERROR_IN_PROCESS;
					goto reta;
				}
				out << "process read gain file success" << endl;
			}else if (!tag.compare("slice_gain"))
			{
				out << "starting process slice gain file "<<filetag<<endl;
				if(SetGradientAllScale(CHANNEL_Z,filetag.c_str()))
				{
					out << "process slice gain file failed" << endl;
					ret = ERROR_IN_PROCESS;
					goto reta;
				}
				out << "process slice gain file success" << endl;
			}
			else if (!tag.compare("phase_gain"))
			{
				out << "starting process phase gain file " << filetag << endl;
				if (SetGradientAllScale(CHANNEL_Y, filetag.c_str()))
				{
					out << "process phase gain file failed" << endl;
					ret = ERROR_IN_PROCESS;
					goto reta;
				}
				out << "process phase gain file success" << endl;
			}
			else if (!tag.compare("gradient_wave"))
			{
				out << "starting process gradient_wave file " << filetag << endl;
				if (SetGradientWave(filetag.c_str()))
				{
					out << "process gradient_wave file failed" << endl;
					ret = ERROR_IN_PROCESS;
					goto reta;
				}
				theApp.gradwaveCfged = true;
				out << "process gradient_wave file success" << endl;
			}
			else if (!tag.compare("gradient_matrix"))
			{
				out << "starting process gradient_matrix file " << filetag << endl;
				if (SetAllMaxtrixValue( filetag.c_str()))
				{
					out << "process gradient_matrix file failed" << endl;
					ret = ERROR_IN_PROCESS;
					goto reta;
				}
				out << "process gradient_matrix file success" << endl;
			}
			else if (!tag.compare("par"))
			{
				out << "starting process par file " << filetag << endl;
				if(SetParameterFile(filetag.c_str()))
				{ 
					out << "process par file failed" << endl;
					ret = ERROR_IN_PROCESS;
					goto reta;
				}
				out << "process par file success" << endl;
			}
			else if (!tag.compare("rxmapping")|| !tag.compare("txmapping"))
			{
				out << "starting process " << tag << " file " << filetag << endl;
				if (ParseMappingFile(filetag.c_str(),out))
				{
					out << "process " << tag << " file failed" << endl;
					ret = ERROR_IN_PROCESS;
					goto reta;
				}
				out << "process " << tag << " file success" << endl;
			}
			else if (!tag.compare("wavemapping"))
			{
				out << "starting process "<<tag<<" file " << filetag << endl;
				if (SetRFWaves(filetag.c_str()))
				{
					out << "process "<< tag<<" file failed" << endl;
					ret = ERROR_IN_PROCESS;
					goto reta;
				}
				out << "process "<<tag <<" file success" << endl;
				theApp.txwaveCfged = true;
			}
			else
			{
				out << "undefined tag " << tag << endl;
			}
		}
	}
	file.close();
	ret = RIGHT_IN_PROCESS;
	
reta:
	processtr += out.str();
	processtr += "parsing cfg file done!\n";
	PrintStr(PRINT_LEVEL_DEBUG,processtr.c_str());
	return ret;
}
*/
int LocationLine(char *list)
{
	return 0;
}
//0 ellipse --default
//1 butterworth-

int SetFilterType(int type)
{
	if (type < 0 || type>1)
		return ERROR_IN_PROCESS;
	theApp.filterType = type;
	return RIGHT_IN_PROCESS;
}
bool SoftWareApp::CheckSamplePeriod(double sampleP, bool load)
{
	    //find file 
	string filePath = pwd + "filter\\";
	string filename;
	ostringstream ostr; 
	if (filterType) filename = "butterworth";
	else filename = "ellipse";
	//us
	ostr << filePath << filename << "_" << sampleP<<"us.dat";
	if (_access(ostr.str().c_str(), 0) == 0)
	{
		if (load)
			 SetFilter(ostr.str().c_str());
		return true;
	}
	return false;
}
double SetFilter(const char *filename)
{
	string readline;
	int i;
	ifstream file(filename, ios::in);
	if (!file.is_open())
	{	
		return  ERROR_IN_PROCESS;
	}
	vector<string> result;
	//use indicate which parm
	int count = 0;
	while (getline(file, readline))
	{
		//comment
		if (!readline.compare(0, 1, "#") || readline.empty())
			continue;
		switch (count)
		{
		case 0:
			theApp.filterP.cic = atoi(readline.c_str());
			break;
		case 1:
			theApp.filterP.cicgain1 = strtol(readline.substr(2,4).c_str(),NULL,16);
			theApp.filterP.cicgain2 = strtol(readline.substr(6,4).c_str(),NULL,16);
			theApp.filterP.cicgain3 = strtol(readline.substr(10,4).c_str(),NULL,16);
			break;
		case 2:
			theApp.filterP.fir = atoi(readline.c_str());
			break;
		case 3:
			result = split(readline, ",");
			if (result.size() != 11)
			{
				PrintStr(PRINT_LEVEL_ERROR, "fir coef is not equal 11");
				return ERROR_IN_PROCESS;
			}
			for (i = 0; i < 11; i++)
				theApp.filterP.fircof[i] = round(atof(result[i].c_str())* 65536);
			break;
		case 4:
			theApp.filterP.iir = atoi(readline.c_str());
			break;
		case 5:
			result = split(readline, ",");
			if (result.size() != 16)
			{
				PrintStr(PRINT_LEVEL_ERROR, "iir coef is not equal 11");
				return ERROR_IN_PROCESS;
			}
			for (i = 0; i < 16; i++)
				theApp.filterP.iircof[i] = round(atof(result[i].c_str())* 65536);

			break;
		case 6:
			result = split(readline, ",");
			if (result.size() != 4)
			{
				PrintStr(PRINT_LEVEL_ERROR, "iir gain is not equal 4");
				return ERROR_IN_PROCESS;
			}
			for (i = 0; i < 4; i++)
				theApp.filterP.iirgain[i] = round(atof(result[i].c_str())*65536);
			goto Parse_done;
		default :
			break;
		}
		count++;
	}
Parse_done:
	theApp.filterP.freq = 50 * 1000.0 / (theApp.filterP.cic*theApp.filterP.fir*theApp.filterP.iir);
	theApp.filterSet = true;
	return theApp.filterP.freq;
}
void  SetMatrixPT(int *matrixP, int *matrixT)
{
	memcpy(theApp.matrixP,matrixP,9*sizeof(int));
	memcpy(theApp.matrixT, matrixT, 9*sizeof(int));
}
#define MAX_CALI 128
int SendParamToArm(int paramType,int channel,float *value,int len)
{
	int i,regCount, iWrittenBytes, waitReturn, valueTemp;

	if (len > MAX_CALI)
	{
		FormatOutPut(PRINT_LEVEL_ERROR,"len exceed %d", MAX_CALI);
		return ERROR_IN_PROCESS;
	}
	char* buf = new char[len*4+11];
	memcpy(buf, "$ETS", 4);
	buf[4] = SEND_PARAM_TO_ARM;
	buf[7] = paramType;
	buf[8] = channel;
	buf[9] = DUMMY_CMD;
	regCount = 10;
	for (i = 0; i < len; i++)
	{
		if (paramType == 0 || paramType == 2)
		{
			valueTemp = round(value[i] / 360 * pow(2, 28));
			regCount+=ConfigQuiry(&buf[regCount], valueTemp);
		}
		else
		{
			valueTemp = round(value[i]  * pow(2, 5));
			regCount += ConfigQuiry2(&buf[regCount], valueTemp);
		}
		
	}
	regCount++;
	buf[5] = HIBYTE(regCount);
	buf[6] = LOBYTE(regCount);
	buf[regCount - 1] = theApp.CheckSum(buf, regCount - 1);
	iWrittenBytes = theApp.Send(M, buf, regCount);
	theApp.PrintToLog("发送参数到ARM指令：", buf, iWrittenBytes);
	delete[] buf; 
	return RIGHT_IN_PROCESS;
}
int LoadARMParamtoFPGA(int paramType, int channel,bool all)
{
	int regCount = 11, iWrittenBytes;
	
	char* buf = new char[BUF_STANDARD_SIZE];
	memcpy(buf, "$ETS", 4);
	buf[4] = CONFIG_PARAM_FROM_ARM;
	buf[5] = HIBYTE(regCount);
	buf[6] = LOBYTE(regCount);
	if (all)
	{
		buf[7] = DUMMY_CMD;
		buf[8] = DUMMY_CMD;
	}
	else
	{
		buf[7] = paramType;
		buf[8] = channel;
	}
	buf[9] = DUMMY_CMD;
	buf[regCount - 1] = theApp.CheckSum(buf, regCount - 1);
	iWrittenBytes = theApp.Send(M, buf, regCount);
	theApp.PrintToLog("发送加载ARM参数到FPGA指令：", buf, iWrittenBytes);
	delete[] buf;
	return RIGHT_IN_PROCESS;
}
int LoadFileFromARM()
{
	int regCount = 11, iWrittenBytes;
	char* buf = new char[BUF_STANDARD_SIZE];
	memcpy(buf, "$ETS", 4);
	buf[4] = DUMP_ARM_FILE;
	buf[5] = HIBYTE(regCount);
	buf[6] = LOBYTE(regCount);
	buf[7] = DUMMY_CMD;
	buf[8] = DUMMY_CMD;
	buf[9] = DUMMY_CMD;
	buf[regCount - 1] = theApp.CheckSum(buf, regCount - 1);
	iWrittenBytes = theApp.Send(M, buf, regCount);
	theApp.PrintToLog("发送将ARM参数读回指令：", buf, iWrittenBytes);
	delete[] buf;
	return RIGHT_IN_PROCESS;
}
int  Simulate(int port, int tr, int loop, const char *simulatePath, const char * prjpath)
{

	PROCESS_INFORMATION pi;
	STARTUPINFO si;
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
	int len = MAX_PATH * 2 + 100;
	char *szCmdLine = new char[len];
	snprintf(szCmdLine, len, "\"%s\\run.bat\" %s", simulatePath, prjpath);
	//because paramter use ansi version ,the CreateProcessA also use A version
	if (CreateProcessA(NULL,
		szCmdLine,
		NULL,
		NULL,
		FALSE,
		0,
		NULL,
		simulatePath,
		&si, &pi))
	{
		WaitForSingleObject(pi.hProcess, INFINITE);
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
	}
	else
	{

		errorno = GetLastError();
		return ERROR_IN_PROCESS;
	}

	snprintf(szCmdLine, len, "\"%s\\mri_c.exe\" %d %d %d %s", simulatePath, tr, loop, port, prjpath);
	if (CreateProcessA(NULL,
		szCmdLine,
		NULL,
		NULL,
		FALSE,
		0,
		NULL,
		simulatePath,
		&si, &pi))

	{
		theApp.thread = pi.hThread;
		theApp.process = pi.hProcess;
	}
	else
		return ERROR_IN_PROCESS;
	return RIGHT_IN_PROCESS;
}
void  SimulatePause()
{
	SuspendThread(theApp.thread);

}
void  SimulateResume()
{
	ResumeThread(theApp.thread);
}
void SimulateAbort()
{
	TerminateProcess(theApp.process, 0);
	CloseHandle(theApp.process);
	CloseHandle(theApp.thread);
}
/*
int   SetScanLines(int lines, int mode)
{
	theApp.linesset = true;
	
	theApp.lines= lines;
	theApp.tiggermode= mode;
	
	return RIGHT_IN_PROCESS;
}
*/