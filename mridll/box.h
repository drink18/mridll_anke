#pragma once
#include <sstream>
#include <vector>
#include <iostream>
#include <fstream>
#include <WS2tcpip.h>
#define  RX_SOURCE_CHANNEL_NUM  16

#define  RX_SOURCE_BOARD_NUM    4 
#define  CARD_NUM 6
typedef struct
{
	int sampleCount;
	// pre discard point
	int preNOD;
	//int write index
	int index;
	int backupIndex;
	bool sampleStart;

}recChannelInfo;

class Spectrometer
{
	//  0:main box ,1:E1,2:E2....
public:
	SOCKET m_SockClient;//�ͻ������Ӷ���
//	bool configured;   // mean this box used
	std::string boxname;
	int  b_index;
	int  extendType;
	std::string hostIP;
	SOCKADDR_IN addrSrv;

	HANDLE m_hReadThread; //�������߳�handle
	int connect_status;
	u_short hostPort;
	u_short serverPort;
	std::string serverIP;
	char armAppVer[20];/*arm app�汾*/
	char armDriverVer[20]; /*arm �����汾*/
	std::string cfgFileName;

	char *m_Buf; //���ڽ��ջ�����

	DWORD m_rBuf_Tail;
	DWORD m_rBuf_Head;

	std::ofstream fileFifoWrite[RX_SOURCE_CHANNEL_NUM / 2 * RX_SOURCE_BOARD_NUM + 1];
	recChannelInfo recInfo[RX_SOURCE_CHANNEL_NUM][RX_SOURCE_BOARD_NUM];
	std::ofstream dmaFile;
	bool alreadyClosed;
	bool   boxChecked ;
	int boxcardInfo[CARD_NUM];
	std::ofstream   adcfile;
	std::ofstream   califile;
	int           receiveChs, iTotal;
	char header[14];
	int    singleChannelSize;
	//
	int    lineNum;
	char  *samplebuf;
	char *m_sample;
	int fifoIRecevieNum;
	int fifoQRecevieNum;
	bool tagConfiged[33];
	LPCTSTR ADDR[33];
	std::vector<int>   reiveData[33];
	//-1:status none,0,erasing,other burning percent
	int                       updataStatus;
	//dma data processing ...
	volatile bool                       dmaDataProcess;
	//adc sample
	int			  sampleBoard;
	int           sampleIndex, sampleTotal;
	volatile bool              processEnable;
	int           sampleChannel;
	std::ostringstream dmaAllFile;
	char *receiveData;
	bool                       queryFPGAInfo;
	unsigned int                        queryLowAddr;
	unsigned int                        queryHighAddr;
	bool                       queryPreempOrChannel;
	bool channel_lw_receive;
	bool channel_hi_receive;
	int channel_value_low;
	int channel_value_high;
	bool querySingleReg;
	int single_reg_value;
	int  m_WaitState;
	int start_chNo;
	int rx_cmd_addr;
	int rx_dc_reg;
	int rc_dc_value;

	HANDLE syncEvent;   // ��ȡ���ݵ��߳������̵߳�ͬ��
	HANDLE readsyncEvent;  // ��ȡFPGA���ݵ��߳������̵߳�ͬ��
	static DWORD WINAPI  ReadPortThread(LPVOID lpParameter);
	//move construt
	
protected:
	//���������̺߳���
	void ProcRevMsg(char *buf, unsigned int length);//���������Ϣ
	void AppendParam(int fileIndex);
public:
	void SCloseDmaFile();
	void SInitSample(int sampleSize, int realnoSamples, int noEchoes);
	Spectrometer();
	Spectrometer(Spectrometer&& other);
	~Spectrometer();
private :
	char *pch;
};