// ds1307.c - Copyright (c) 2023-25 Andre M. Maree / KSS Technologies (Pty) Ltd.

#include "hal_platform.h"

#if (HAL_DS1307 > 0)
#include "ds1307.h"
#include "hal_i2c_common.h"
#include "errors_events.h"
#include "syslog.h"
#include "systiming.h"

// ########################################## Macros ###############################################

#define	debugFLAG					0xF000
#define	debugTIMING					(debugFLAG_GLOBAL & debugFLAG & 0x1000)
#define	debugTRACK					(debugFLAG_GLOBAL & debugFLAG & 0x2000)
#define	debugPARAM					(debugFLAG_GLOBAL & debugFLAG & 0x4000)
#define	debugRESULT					(debugFLAG_GLOBAL & debugFLAG & 0x8000)

// ####################################### Local variables #########################################

ds1307_t sDS1307 = { 0 };

// ####################################### Local functions #########################################

/**
 * @brief	Read device
 */
static int ds1307ReadData(u8_t addr, u8_t len) {
	IF_myASSERT(debugPARAM, (addr + len) <= sizeof(ds1307_t));
	IF_SYSTIMER_START(debugTIMING, stDS1307);
	int iRV = halI2C_Queue(sDS1307.psI2C, i2cWR_B, &addr, 1, &sDS1307.u8Buf[addr], len, (i2cq_p1_t)NULL, (i2cq_p2_t)NULL);
	IF_SYSTIMER_STOP(debugTIMING, stDS1307);
	return iRV;
}

/**
 * @brief	Write data
 */
static int ds1307WriteData(u8_t addr, u8_t len) {
	IF_myASSERT(debugPARAM, (addr + len) <= 0x40);
	IF_SYSTIMER_START(debugTIMING, stDS1307);
#if 1
	u8_t * pBuf = calloc(1, len+1);
	pBuf[0] = addr;
	memcpy(pBuf+1, &sDS1307.u8Buf[addr], len);
	int iRV = halI2C_Queue(sDS1307.psI2C, i2cW_B, pBuf, len+1, (u8_t *)NULL, 0, (i2cq_p1_t)NULL, (i2cq_p2_t)NULL);
	free(pBuf);
#else
	int iRV = halI2C_Queue(sDS1307.psI2C, i2cW_BD, &addr, sizeof(addr), (u8_t *)NULL, 0, (i2cq_p1_t)NULL, (i2cq_p2_t)NULL);
	if (iRV >= erSUCCESS)
		iRV = halI2C_Queue(sDS1307.psI2C, i2cW_BD, &sDS1307.u8Buf[addr], len, (u8_t *)NULL, 0, (i2cq_p1_t)NULL, (i2cq_p2_t)NULL);
#endif
	IF_SYSTIMER_STOP(debugTIMING, stDS1307);
	if (iRV < erSUCCESS)
		xSyslogError(__FUNCTION__, iRV);
	return iRV;
}

static int ds1307ReportDetail(report_t * psR, tm_t * psTM) {
	int iRV = xReport(psR, "REG: %d%d/%d%d/%d%d %d%d:%d%d:%d%d DoW=%d" strNL, sDS1307.yrH + 197,
		sDS1307.yrL, sDS1307.mthH, sDS1307.mthL, sDS1307.dayH, sDS1307.dayL, sDS1307.h24.hrsH,
		sDS1307.h24.hrsL, sDS1307.minH, sDS1307.minL, sDS1307.secH, sDS1307.secL, sDS1307.wday);
	if (psTM)
		iRV += xReport(psR, "sTM: %d/%02d/%02d %d:%02d:%02d DoW=%d DoY=%d" strNL, psTM->tm_year + YEAR_BASE_MIN,
			psTM->tm_mon+1, psTM->tm_mday, psTM->tm_hour, psTM->tm_min, psTM->tm_sec, psTM->tm_wday, psTM->tm_yday);
	iRV += xReport(psR, "SYS: %R" strNL, sTSZ.usecs);
	return iRV;
}

// ################################## Diagnostics functions ########################################

int ds1307FillData(void) {
	memset(&sDS1307.data, 0, SO_MEM(ds1307_t, data));
	return ds1307WriteData(offsetof(ds1307_t, data), SO_MEM(ds1307_t, data));
}

int	ds1307Identify(i2c_di_t * psI2C) {
	sDS1307.psI2C = psI2C;
	psI2C->TObus = 100;
	psI2C->Test = 1;									// test mode
	psI2C->Type = i2cDEV_DS1307;
	psI2C->Speed = i2cSPEED_100;						// does not support 400KHz
	// Step 1 - read time, control & data registers
	int iRV = ds1307ReadData(offsetof(ds1307_t, u8Buf), SO_MEM(ds1307_t, u8Buf));
	// Step 2 - Check initial default values
	if (iRV >= erSUCCESS && sDS1307.h24.s2 == 0 && sDS1307.s6 == 0 && sDS1307.s7 == 0) {
		psI2C->IDok = 1;
		psI2C->Test = 0;
	}
	return iRV;
}

int	ds1307Config(i2c_di_t * psI2C) {
	int iRV = erINV_STATE;
	if (psI2C->IDok == 0)
		goto exit;
	psI2C->CFGok = 0;
	if (sDS1307.CH) {
		sDS1307.r0 = 0;									// start the clock & set min = 0
		iRV = ds1307WriteData(offsetof(ds1307_t, r0), SO_MEM(ds1307_t, r0));
	} else {
		iRV = erSUCCESS;
	}
	if (iRV < erSUCCESS)
		goto exit;
	if (sDS1307.h24.hrsX) {								// currently in 12Hr mode?
		sDS1307.r2 = 0;									// set 24h mode, set hrs = 0
		iRV = ds1307WriteData(offsetof(ds1307_t, r2), SO_MEM(ds1307_t, r2));
	}
	if (iRV < erSUCCESS)
		goto exit;
	if (sDS1307.out || sDS1307.sqwe || sDS1307.rs) {
		sDS1307.r7 = 0;									// Turn all OFF
		iRV = ds1307WriteData(offsetof(ds1307_t, r7), SO_MEM(ds1307_t, r7));
	}
	if (iRV < erSUCCESS)
		goto exit;
//	iRV = ds1307FillData(); if (iRV < erSUCCESS) goto exit;
	tm_t sTM = { 0 };
	seconds_t SYStime = xTimeStampSeconds(sTSZ.usecs);	// get seconds from current system time 
	if (SYStime < BuildSeconds)							// if older/before build time
		SYStime = BuildSeconds;							// step forward to at least build time...
	seconds_t RTCtime = ds1307GetTime(&sTM);			// read DS1307 registers, calculate seconds
	IF_EXEC_2(debugTRACK && xOptionGet(ioI2Cinit), ds1307ReportDetail, NULL, &sTM);
	if (SYStime < RTCtime) {
		sTSZ.usecs = xTimeMakeTimeStamp(RTCtime, 0);	// Update SYStime with RTCtime
	} else {
		iRV = ds1307SetTime(SYStime);					// update RTC with SYStime
	}
	SL_LOG(RTCtime - SYStime ? SL_SEV_WARNING : SL_SEV_NOTICE, "RTC=%r  SYS=%r  Dif=%!r", RTCtime, SYStime, RTCtime - SYStime);
	if (iRV < erSUCCESS)
		goto exit;
	if (psI2C->CFGerr == 0)								// once off init....
		IF_SYSTIMER_INIT(debugTIMING, stDS1307, stMICROS, "DS1307", 200, 3200);
	psI2C->CFGok = 1;
	IF_EXEC_2(debugTRACK && xOptionGet(ioI2Cinit), ds1307ReportTime, NULL, strNUL);
exit:
	halEventUpdateDevice(devMASK_DS1307, iRV < erSUCCESS ? 0 : 1);
	return iRV;
}

// ################################# Global/public functions #######################################

int ds1307SetTime(seconds_t epoch) {
	tm_t sTM = { 0 };
	tm_t * psTM = &sTM;
	xTimeGMTime(epoch, psTM, 0);
	sDS1307.secL = psTM->tm_sec % 10;
	sDS1307.secH = psTM->tm_sec / 10;
	sDS1307.minL = psTM->tm_min % 10;
	sDS1307.minH = psTM->tm_min / 10;
	if (sDS1307.h12.hrsX) {
		if (psTM->tm_hour >= 12)
			sDS1307.h12.ampm = 1;
		if (psTM->tm_hour > 12) 
			psTM->tm_hour -= 12;
		sDS1307.h12.hrsL = psTM->tm_hour % 10;
		sDS1307.h12.hrsH = psTM->tm_hour / 10;
	} else {
		sDS1307.h24.hrsL = psTM->tm_hour % 10;
		sDS1307.h24.hrsH = psTM->tm_hour / 10;
	}
	sDS1307.wday = psTM->tm_wday;
	sDS1307.dayL = psTM->tm_mday % 10;
	sDS1307.dayH = psTM->tm_mday / 10;
	psTM->tm_mon += 1;				//tm_mon 0=Jan but DS1307 mth 1=Jan
	sDS1307.mthL = psTM->tm_mon % 10;
	sDS1307.mthH = psTM->tm_mon / 10;
	sDS1307.yrL = psTM->tm_year % 10;
	sDS1307.yrH = psTM->tm_year / 10;
	return ds1307WriteData(offsetof(ds1307_t, sTime), SO_MEM(ds1307_t, sTime));
}

seconds_t ds1307GetTime(tm_t * psTM) {
	int iRV = ds1307ReadData(offsetof(ds1307_t, sTime), SO_MEM(ds1307_t, sTime));
	if (iRV < erSUCCESS)
		return 0xFFFFFFFF;
	if (psTM == NULL) {
		tm_t sTM = { 0 };
		psTM = &sTM;
	}
	psTM->tm_year = (sDS1307.yrH * 10) + sDS1307.yrL;
	psTM->tm_mon = (sDS1307.mthH * 10) + sDS1307.mthL;
	psTM->tm_mon -= 1;
	psTM->tm_mday = (sDS1307.dayH * 10) + sDS1307.dayL;
	psTM->tm_wday = sDS1307.wday;
	psTM->tm_hour = sDS1307.h12.hrsX ? (sDS1307.h12.hrsH * 10) + sDS1307.h12.hrsL + (sDS1307.h12.ampm ? 12 :0)
									 : (sDS1307.h24.hrsH * 10) + sDS1307.h24.hrsL;
	psTM->tm_min = (sDS1307.minH * 10) + sDS1307.minL;
	psTM->tm_sec = (sDS1307.secH * 10) + sDS1307.secL;
	psTM->tm_yday = xTimeCalcDaysYTD(psTM);
	seconds_t Sec = xTimeCalcSeconds(psTM, 0);
	#if (appOPTIONS == 1)
		Sec += xOptionGet(ioTZlocal) ? sTSZ.pTZ->timezone : 0;
	#endif
	return Sec;
}

// ######################################## Reporting ##############################################

int ds1307ReportConfig(report_t * psR, const char * pcStr) {
	int iRV = ds1307ReadData(offsetof(ds1307_t, sConf), SO_MEM(ds1307_t, sConf));
	if (iRV >=erSUCCESS)
		iRV = xReport(psR, "%sCH=%c  H12=%c  OUT=%c  SQWE=%c  RS=%c" strNL, pcStr, sDS1307.CH + CHR_0, 
				sDS1307.h24.hrsX + CHR_0, sDS1307.out + CHR_0, sDS1307.sqwe + CHR_0, sDS1307.rs + CHR_0);
	return iRV;
}

int ds1307ReportTime(report_t * psR, const char * pcStr) {
	seconds_t Epoch = ds1307GetTime(NULL);
	if (Epoch < 0xFFFFFFFF)
		return xReport(psR, "%s%r" strNL, pcStr, Epoch);
	return erFAILURE;
}

int ds1307ReportData(report_t * psR, const char * pcStr) {
	int iRV = ds1307ReadData(offsetof(ds1307_t, sData), SO_MEM(ds1307_t, sData));
	if (iRV >=erSUCCESS)
		iRV = xReport(psR, "%s%!'+hhY" strNL, pcStr, SO_MEM(ds1307_t, sData), &sDS1307.data);
	return iRV;
}

int ds1307Report(report_t * psR, const char * pcStr) {
	int iRV = halI2C_DeviceReport(psR, (void *) sDS1307.psI2C);
	iRV += ds1307ReportConfig(psR, pcStr);
	if (sDS1307.psI2C->CFGok)
		iRV += ds1307ReportTime(psR, pcStr);
	iRV += ds1307ReportData(psR, pcStr);
	return iRV;
}

#endif
