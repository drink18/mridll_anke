#pragma once

const char * WINAPI GetDLLVersion();

void WINAPI SetVerboseLevel(int vlevel);
void WINAPI RegisterPrintStr(void(*callback)(int level, const char *));
void WINAPI RegisterDataRecv(void(*callback)(char **, int size));
int WINAPI Init(const char *initfile);
int WINAPI ConfigFile(const char *filename);

//
int WINAPI ScanCompleted();
void WINAPI Abort();
void WINAPI Pause();
void WINAPI Continue();
int WINAPI Run();
int WINAPI SetupModeRun();
int WINAPI SetOutputPath(const char *path);
void WINAPI SetRawDataFormat(int sel);
char * WINAPI GetOutputFile();
int WINAPI SetOutputPrefix(const char *prefix);
void WINAPI SetSaveMode(int saveMode);
int WINAPI SaveParameterFile(const char *name);
int WINAPI SetParameter(const char *name, double value);
double  WINAPI GetParameter(char *name);
int WINAPI SetDiscard(int preDiscard, int lastDiscard);

int WINAPI SetParameterFile(const char * filename, bool isedit);
int WINAPI SetTxPhaseTable(int channel, float *bufIn, int len);
int WINAPI SetTxGainTable(int channel, float *bufIn, int len);
int WINAPI SetTxFreOffsetTable(int channel, float *bufIn, int len);
int WINAPI SetRxPhaseTable(float *bufIn, int len);
int WINAPI SetGradientAllScale(int channel, float *bufIn, int len);
int WINAPI SetAllMaxtrixValue(float bufIn[][9], int len);
int WINAPI SetRxFreOffsetTable(float *bufIn, int len);


//shiming 
void WINAPI SetChannelValue(int channel, float value);
float WINAPI GetChannelValue(int channel);

//preemp 
int WINAPI SetPreempValue(int channel, int keys, float value);
float WINAPI GetPreempValue(int channel, int keys, int *iscorrect);
int WINAPI SaveShimValues();

//center freq
int WINAPI SetRxCenterFre(int box, int boardno, int channel, float freq, bool isAllSet);
int WINAPI SetTxCenterFre(int box, int boardno, int channel, float freq);
float WINAPI GetCenterFre(int sel);

void WINAPI CloseSys();
int WINAPI GetLastErr();

//Unicode version support for MFC call
const wchar_t * WINAPI GetDLLVersionW();
