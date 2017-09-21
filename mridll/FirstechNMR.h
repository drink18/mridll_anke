/**************************************
*This file is used for offer the core function
*for Firstech NMR software.Include network connect
*data process...
*
*FUCTION            AUTHER            DATA
*first created      yuan,jian         2015.11.30
*delete DLL API     yuan,jian         2016.3.7
*keyword use DEF file 
*
*noSamples meaning change from the Total Points To interest Points()
*so real sample points is noSamples+
*
****************************************/
#pragma once


#ifdef DLL_IMPLENT
//if not use *def file then use keyword below
//#define DLL_API extern "C" _declspec(dllexport)
#define DLL_API 
#else
#define DLL_API _declspec(dllimport)
#endif
#include <fstream>
#include <unordered_map>
#include <map>
#include <vector>
#include "HardwareRegInfo.h"
#include <WS2tcpip.h>
#include "box.h"
#include "json/json.h"
//
#ifdef __cplusplus
//extern "C" {
#endif
//根据TCP/IP协议数据
#define REV_BUF_LEN   (65535*100)//网口接收缓冲区长度

//寄存器信息宽度
#define REG_INFO_WITH  8

//寄存器最大个数
#define REG_MAX_COUNT  0x4000
//实际上TCP包的最大数据量负载是 65535 - 20 -20 =65495 byte
#define MAX_PACKSIZE    65400

//REG is 8 reg + 11
#define ONE_PACK_MAX_SIZE  0xFFFB
#define REV_BUF 65535
#define PACK_MAX_SIZE  65535

//每包发送寄存器个数
#define REG_CNT_PER_PACKAGE  (MAX_PACKSIZE/REG_INFO_WITH)

#define MAX_CHAR_PER_LINE 40


#define PARAM_BOARD_MAIN   0x1
#define PARAM_BOARD_TX1    0x2
#define PARAM_BOARD_TX2    0x4
#define PARAM_BOARD_RX1    0x8
#define PARAM_BOARD_RX2    0x10
#define PARAM_BOARD_RX3    0x20
#define PARAM_BOARD_RX4    0x40
#define PARAM_BOARD_GRADP    0x80
#define PARAM_BOARD_GRADR  0x100
#define PARAM_BOARD_GRADS  0x200

#define ERROR_IN_PROCESS   0x01
#define ERROR_IN_READ_REG   -1
#define ERROR_IN_PARSE   -1

#define RIGHT_IN_PROCESS   0x00

//define HASH 
#define MULTIPLIER 31

typedef enum 
{
 CHANNEL_X,
 CHANNEL_Y,
 CHANNEL_Z,
 CHANNEL_B0
}shimChannel;
typedef enum
{
   A1,
   A2,
   A3,
   A4,
   A5,
   A6,
   T1,
   T2,
   T3,
   T4,
   T5,
   T6,
   A1X,
   A2X,
   A3X,
   T1X,
   T2X,
   T3X,
   A1Y,
   A2Y,
   A3Y,
   T1Y,
   T2Y,
   T3Y,
   A1Z,
   A2Z,
   A3Z,    
   T1Z,
   T2Z,
   T3Z
}PreempKeys;
typedef enum
{
  TYPE_INT,
  TYPE_DOUBLE,
}paramType;
typedef enum
{
   MAIN_BOARD,
   GRADP,
   GRADR,
   GRADS,
   TX1,
   TX2,
   RX1,
   RX2,
   RX3,
   RX4
}boardType;
typedef enum
{
   M,
   E1,
   E2,
   E3,
   E4
}boxType;
enum 
{
   DONT_SAVE, //边接收，边保存
   RECEIVED_SAVE,
};
typedef struct  
{
   	boardType bt;
	int    updateAddr;
}varBoardInfo;

typedef struct 
{
	std::vector<varBoardInfo> varBoardInfos;
   int  boxt;
}varBoxInfo;
typedef struct  
{
	bool paramiscalc;
	int  param1;   //calc_step or lut_start_addr
	int  param2;  //points
}gradientParamRAM;
typedef enum
{
 us32,
 us16,
 us8,
 us4,
 us2
}txDataSample;
typedef struct  
{
	int pos; //pos in file
	int sampleRate;
	int points;
	double bw;
}txWaveInfo;
typedef struct 
{
  paramType pt;
  union {
	  int intlit;
	  double doublelit;
  }value; 
  std::vector<varBoxInfo> varBoxInfos;
  bool needUpdate;
}paramInfo;

/////reg info
//寄存器信息结构
typedef struct reg_info
{
	unsigned int regaddr;
	unsigned int regval;
}reg_info_t;

//寄存器设置信息结构
typedef struct reg_config_info
{
	reg_info_t regInfo;
	bool setFlag;
}reg_config_info_t;
typedef struct
{
	float att;
	float amp1;
	float amp2;
	float amp3;
	int   swi;
	int   dc;

}rxCHCali;
typedef struct
{
	std::string filename;
	std::vector<rxCHCali> rxBoardCali;
}rxBoardCaliInfo;
typedef struct
{
	std::string filename;
	std::vector<float> txBoardCali;
}txBoardCaliInfo;
typedef struct
{
	double points;
	double period;
	double bw;
	bool set; //if true indicate I am set
}txWaveParas;
typedef struct
{
	std::map<boardType,txBoardCaliInfo>  txCalis;
	std::map<boardType,rxBoardCaliInfo>   rxCalis;
}boxCaliInfo;
//receive data info 

typedef struct
{
	int cic;
	int cicgain1;
	int cicgain2;
	int cicgain3;
	int fir;

	int fircof[11];
	int iir;
	int iircof[16];
	int iirgain[4];
	//khz
	double freq;
}FilterParam;
//#define  MAX_BOX (E4-M+1)

#define  MAX_FILE_WRITE_TEMP 4080

#define  DAC_MAX_REG 257
#define  ADC_ATT_REG_NUM 3

///sencond cmd 
#define SINGLE_READ 0x01 
#define BAT_READ    0x02 //批量读

#define SEND_PARAM_TO_ARM 0xB
#define CONFIG_PARAM_FROM_ARM 0xC
#define DUMP_ARM_FILE    0xD
#define DUMMY_CMD 0xFF

//for the arm bug 
//if use 01 then must indicate 0x50000000 for CS1
//0x60000000 for CS2 ...
//#define DIRECT_REG_WRITE_READ 0x01
#define DIRECT_REG_WRITE_READ 0x04
#define DATABUS_READ_WRITE_READ   0x04
#define AD9520_CFG   0x0E
#define DIRECT_BAT_READ   0x02
#define CONTINUE_ADDR     0x02
#define CHAOS_ADDR        0x01
#define DMA_READ 0x05
#define FPGA_UPDATE    0x06
#define ERASING_STATUS   0x7
#define BRUNING_STATUS   0x8
#define QERUY_FPGA_STATUS 0x09
#define TX1_BOARD 0x00
#define TX2_BOARD 0x01
#define RX1_BOARD 0x02
#define RX2_BOARD 0x03
#define RX3_BOARD 0x04
#define RX4_BOARD 0x05
#define  BUF_STANDARD_SIZE  1024
#define  MAX_MATRIX_NUM     4096
#define  GRADIENT_MAX_SCALE  8192

#define  GRADIENT_MAX_WAVE_RAM  (256*1024)
#define  GRADIENT_MAX_PARAM      1024

#define  TX_WAVE_RAM_MAX         (4*1024)
//change from 1024 to 8192
#define  TX_NCO_OFFSET_MAX        8192

//100MHZ
#define  TX_SYS_CLK              100.0

#define  RX_SYS_CLK              100.0
//800MHZ
#define  TX_DAC_CLK              800.0



//user mode print connect status
#define  VERBOSE_LEVEL0       0
//debug mode 1  print send data 
#define  VERBOSE_LEVEL1       1
//debug mode 2  print tx/rx data
#define  VERBOSE_LEVEL2       2 

#define  PRINT_LEVEL_DEBUG    0
#define  PRINT_LEVEL_INFO     1
#define  PRINT_LEVEL_WARN     2
#define  PRINT_LEVEL_ERROR    3
#define  SINGLE_DATA_MAX     (32*1024*2+12)
//#define  SINGLE_DATA_MAX     (32*2+12)
//#define  ANALYSIS_DMA_DATA   1
//#define  REC_DATA

/*scan status*/
#define SCAN_UNCOMPLETED   -1
#define SCAN_COMPLETE_UPLOADING 1
#define SCAN_COMPLETEED 2

/*defaut PEF file*/
#define DEFAULT_PEF_FILE  "gra_pef.txt"
#define DEFAULT_INIT_FILE "init.ini"
#define REG_IMPOSSIBLE    0xFFFFFFFF



typedef struct wavefile
{
	std::string wavefile_tag;
	int bram_no;
	int ch_no;
	std::string wavename;
	std::string wavefile_name;

}wavefile;
class SoftWareApp
{
public:
	SoftWareApp();
	~SoftWareApp();
    char CheckSum(char *buf, DWORD length);  //计算校验和，字节按位异或相加
	DWORD StringToHex_S(std::string strTemp);
	DWORD ParseFile(std::string fileName,std::map<int, std::vector<reg_info_t >> &regconfiginfo);  //解析配置文件内容，并存入reginfo结构体数值
	int  itoaline(int value,char *str);
	static void PrintToLog(std::string prefix,char *buf,int length);
	void PrintToLogNC(std::string prefix, char *buf, int length);
	int SendParConfig();
	int Send(int boxtype,char *data,int length);
	char ConvertHexData(char ch);
	char ConvertItoS(char ch);
	DWORD SendFile(std::string fileName);
	int String_hashCode(const char *p);
	void CloseDmaFile();
	void InitSample(int sampleSize,int sample,int echo);
	void reConfigDAC(int addr,boardType bt);
	reg_info_t (*dacRegs)[4][47];
	int dacRegsSize[2][4];
	//boxindex,boxinfo
	std::map<int,Spectrometer> boxs;
	txWaveParas twp[2][8][16];
	bool gradwaveCfged;
	bool txwaveCfged;
	int           sampleType;//0:complex 1:adc
	float		  shimValues[4];	//Save Shim
	float pefvalues[54];
	bool  pefload;
	int   ts;
	//for test;
#ifdef  REC_DATA
	std::ofstream recData;
#endif
	//std::ofstream dmarawfile;
	std::string  configPwd;	
	
	int  captureNum;
	BOOL CaptureProcess;

	int totalLengthI,totalLengthQ;
	
	u_short verboseLevel;
	
    static DWORD WINAPI UpdateParamThreadStatic(LPVOID lpParameter);
	DWORD WINAPI  UpdateParamThread();
public :
	char runtime[40];
	char fileSaveTag[8];
	
#ifdef ANALYSIS_DMA_DATA
	char* dma_temp_buf;
#endif
	
	char *jasonstr;
	std::string message;
	std::ofstream logfile;
	std::ofstream errfile;
	std::string   outputFileName;
	std::string   outputFilePrefix;
	
	std::string   parFileName;
	//std::string   txWaveFileName;
	std::string   srcfilename;
	std::string   downfilename;
	std::map<std::string,paramInfo> paramMap;
	std::map<std::string,boardType> boardNameHash;
	std::unordered_map<std::string,int>   boxNameHash;
	std::map<std::string,txWaveInfo> txWaveMap;
	std::map<int, boxCaliInfo>  nmrCali;
	//std::vector<std::string>        updateParam;
	std::map<std::string, bool> txwaveStatus;
	bool                       seqRun;
	bool                       setupMode;
	bool                       srcFileDowned;
	bool                       abortSetupModeThread;
	bool                       sysclosed;
	
	int                        sampleNum;
	int                        singleSampleNum;
	int                        saveMode;
	int						   rx_bypass;//bypass addr
	int                        noSamples;
	int						   lastSampleBufSize;
	int                        preDiscard,lastDiscard;
	int raw_data_format;
	HANDLE m_SetupModeThread; //Setup模式的更新线程
	//保护updateParam
    CRITICAL_SECTION    g_updateParmProtect;
	//check if box name is valid,M,E1,E2,true=valid
	//bool checkBoxName(string name);
	int loadParFile(std::ifstream &filename);
	int   GetPreempAddr(int channel,int keys);
	
	LPCTSTR GetBuff();
	LPCTSTR GetRTBuff();
	bool CheckRTAddr(LPCTSTR addr,int len);
	//load ture if file valid load to sys
	bool CheckSamplePeriod(double sampleP,bool load);
	void DownMatrix();
	float SYSFREQ;
	FilterParam filterP;
	bool   filterSet=false;
	int    filterType;
	bool   hasParamUpdate = false;

	int matrixP[9] = {1,0,0,0,1,0,0,0,1};
	int matrixT[9] = {1,0,0,0,1,0,0,0,1};
	float maxtrixvalue[MAX_MATRIX_NUM][9];
	float matrixG[3];
	int matrixRCount = 0;
	float txCenterFre;
	float rxCenterFre;	
	HANDLE thread;
	HANDLE process;
	//params 
	Json::Value root;
	std::vector<wavefile> waveinfo;
	bool isEdit;
	bool lock;
	//RPC2.0 server thread
	HANDLE rpcthread;
	bool   stoprpc;
	unsigned int rtBufSize;
	//
	bool   linesset;
	int    lines;
	int    tiggermode;
	int    status;
	int    triggerNum;
	bool   firstdata;
	int    onelinesize;
	std::vector<LPCTSTR> buffAddr;
	std::string pwd;
	private :
		LPCTSTR pBuf;
		HANDLE hMapFile;

		LPCTSTR pBufRealTime;
		HANDLE hMapFileRecon;
};

void  PrintStr(int level, char *str);
void PrintF(const char *files);
void Display(char **buf, int size);
int  ConfigSampleData(char *buf, int data);
int PrepareRun(bool setupMode);
int findParam(std::string varname,bool *exist);
int ConfigReg(char *buf,int addr,int value);
int ParseGradInf(char *filename);
int parse_boxinf(boxType bx, char *filename, bool save, const char *pwd);
int SetRFWaveFile(char *filename);
int SetRFWaveTable(int box, int boardno, int channel, int bramno, std::string filename, std::string wavename);
void DisableTx();
std::string  BoxToString(int bx);
std::string  BoardToString(int brd);
//double round(double r);
//export functions
std::string GetCurrentTimeFormat();
std::string GetCurrentTimeFormat1();

void  RegisterImageP(void(*callback)(const char *));
void  RegisterAbnormal(void(*callback)(int,char *));
//DLL_API int ParseGradInf(const char *filename);
void HandleAbnormal(int boxindex);
DLL_API int NewParFile(char * filename);
DLL_API void SetChannelValue(int channel,float value);
int GetRxChannels(int  box, int board);
DLL_API int SetGradientWave(const char *filename);
DLL_API int SetRFWaves(char *filename);
DLL_API int SetRxBW(int box,int boardno,int channel,int bandwith);
DLL_API int SetRxCenterFre(int box, int boardno, int channel, float freq, bool isAllSet);
DLL_API int SetAllPreempValue();
DLL_API float GetChannelValue(int channel);
void  FormatOutPut(int level, const char *format, ...);

//for test
void ReceiveEnvironmentClear(int boxindex);
DLL_API void ReceiveEnvironmentSet(int boxindex);
DLL_API void ArmExit();
//for HW  config
DLL_API int  SaveRXCaliValue(int  box, int board);
DLL_API int  SaveRXAllCaliValue(int  box);
DLL_API int  SaveTXCaliValue(int  box, int board);
DLL_API int  SaveTXAllCaliValue(int  box);
DLL_API int  SetRxATT(int  box, int board, int channel, float att, float amp1, float amp2, float amp3, int switchValue);
DLL_API int  SetTxATT(int  box, int board, int channel, float value);
DLL_API int  SetRxDCValue(int  box, int board, int channel, int value);
DLL_API int QueryReg(int  bx, int addr);
DLL_API int ConfigSingleReg(int  bx, int addr, int value);
int ConfigSingleReg_i(int  bx, int addr, int value);

void  ResetFPGA(int boxindex);
int SingleSample(int box, int boardno, int channel, int samplePoints);
void SetTime();
void ParsePack(int box,char *databuf, int dataLength, std::string prefix);
//slot
int UpdateFPGA(boxType bx,int slot, char *filename);
//0: erasing other:burning 
int  GetUpdataSatus();
int ConfigWriteEnd(int bx);
int clrBeforeData(char *buf, int addr, int value);
void ClrSeq();
int ConfigQuiry(char *buf, int addr);
const char *  GetSlotInfo(int  boxType, int slot);
int  GetBoardInfo(int boxType, int boardType, int  real);
int  ArmVerQeruy(int boxType);
int WriteSN(int  boxType, int slot,char *sn);

int GetConnectStatus(int boxType);
void LedControl(int boxindex);
void  PathUriToWin(std:: string &str);
void ConfigureSF(std::string pwd);
char *LoadPreempValue();
char *LoadPreempValue(char *file);
int SavePreempValue();
int LocationLine(char *list);
double SetFilter(char *filename);
void  SetMatrixPT(int *matrixP,int *matrixT);
void  ResetFPGA_ForTest(int boxindex);
double  SetFilter(const char *filename);
void DownTxPhaseTable(int channel);
void DownTxFreqTable(int channel);
void DownTxGainTable(int channel);
void DownRxPhaseTable();
void DownRxFreqTable();
void DownGradientGain(int channel);


#ifdef __cplusplus
//}
#endif

