#pragma once
/**************************************
*This file is used for define the error NO defined for Internal Functions
*
***************************************/
#define ERRORGEN(major,minor) (major<<16|minor)
#define ENOCARD      1  /*slot not card*/
#define EREADTIMEOUT 2  /*read info timeout*/
#define EREADFAILED  3  /*read failure*/
#define ENOTCONNECTED 4 /* net not connected*/
#define EBOXTYPE     5  /*box type is not in [0,3]*/
#define EBOARDTYPE   6
#define EREADCARDREG 7 /*error occur read reg 0xF*/
#define ESLOT   8
#define ESLOTVALUE 9
#define ESLOTTXNUM   ERRORGEN(1,0)
#define ESLOTRXNUM   ERRORGEN(1,1)
#define ESREADTIMEOUT ERRORGEN(2,EREADTIMEOUT)/*second call*/
#define ESREADFAILED  ERRORGEN(2,EREADFAILED)
#define EBUFOVERFLOW  10
#define EREADWRITEFILEDATA 11
#define EPAR_PARAM_NOT_EXIST 12
#define EFILEOPENFAILED  1009
#define EPARPARSE        1010
#define EPARPARSEGRADWAVE 1011
#define EPARPARSERFWAVE   1012
#define EGETHWVALUE		  1013
#define EBOARDNOTFOUND    1014