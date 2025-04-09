// ds1307.h

#pragma once

#include "definitions.h"
#include "report.h"

#ifdef __cplusplus
extern "C" {
#endif

// ########################################## Macros ###############################################
// ######################################## Enumerations ###########################################
// ######################################### Structures ############################################

struct i2c_di_t;
typedef struct __attribute__((packed)) ds1307_t {
	union {
		struct __attribute__((packed)) {
/* Sec */	union { u8_t r0; struct { u8_t secL:4; u8_t secH:3; u8_t CH:1; }; };
/* Min */	union { u8_t r1; struct { u8_t minL:4; u8_t minH:4; }; };
/* Hrs */	union {
				u8_t r2;
/* 12H */		struct { u8_t hrsL:4; u8_t hrsH:1; u8_t ampm:1; u8_t hrsX:1; u8_t s2:1; } h12;
/* 24H */		struct { u8_t hrsL:4; u8_t hrsH:2;              u8_t hrsX:1; u8_t s2:1; } h24;
			};
/* DoW */	union { u8_t r3; struct { u8_t wday:3; u8_t s3:5; }; };
/* Day */	union { u8_t r4; struct { u8_t dayL:4; u8_t dayH:2; u8_t s4:2; }; };
/* Mth */	union { u8_t r5; struct { u8_t mthL:4; u8_t mthH:1; u8_t s5:3; }; };
/* Yr */	union { u8_t r6; struct { u8_t yrL:4; u8_t yrH:4; }; };
/* Cfg */	union { u8_t r7; struct { u8_t rs:2; u8_t s6:2; u8_t sqwe:1; u8_t s7:2; u8_t out:1; }; };
/* Data */	u8_t data[56];
		};
		struct __attribute__((packed)) { u8_t sTime[7]; u8_t sConf; u8_t sData[56]; };
		u8_t u8Buf[64];
	};
	struct i2c_di_t * psI2C;
} ds1307_t;
DUMB_STATIC_ASSERT(sizeof(ds1307_t) == (64 + sizeof(void *)));

// ####################################### Global functions ########################################

int	ds1307Identify(struct i2c_di_t * psI2C);
int	ds1307Config(struct i2c_di_t * psI2C);
int	ds1307Diagnostics(struct i2c_di_t * psI2C);

/**
 * @brief	update DS1307 registers with deconstructed epoch seconds values
 * @param	epoch seconds value to be converted
 * @param	psTM optional pointer to structure where components are to be stored
 * @return	result of ds1307WriteData() operation
*/
int ds1307SetTime(seconds_t epoch);

/**
 * @brief	Read RTC time register, populate [optionally] supplied structure
 * @param[in] psTM pointer to [optional] tm_t structure to store time components into
 * @return	number of epoch seconds based on RTC registers
 * @note	if optional structure pointer supplied, deconstructed time values returned there
*/
seconds_t ds1307GetTime(tm_t * psTM);

/**
 * @brief	Report current RTC time (read registers to ensure up to date)
 * @param[in]	psR pointer to report_t structure
 * @param[in]	pcStr optional report label
 * @return	Number of characters output or -1 if device read error
 */
int ds1307ReportTime(report_t * psR, const char * pcStr);

/**
 * @brief	Report current RTC configuration (read registers to ensure up to date)
 * @param[in]	psR pointer to report_t structure
 * @param[in]	pcStr optional report label
 * @return	Number of characters output or (-) error code
 */
int ds1307ReportConfig(report_t * psR, const char * pcStr);

/**
 * @brief	Report current RTC memory buffer (read buffer to ensure up to date)
 * @param[in]	psR pointer to report_t structure
 * @param[in]	pcStr optional report label
 * @return	Number of characters output or (-) error code
 */
int ds1307ReportData(report_t * psR, const char * pcStr);

/**
 * @brief	Report I2C config, RTC time/config/buffer (read registers to ensure up to date)
 * @param[in]	psR pointer to report_t structure
 * @param[in]	pcStr optional report label
 * @return	Number of characters output or (-) error code
 */
int ds1307Report(report_t * psR, const char * pcStr);

#ifdef __cplusplus
}
#endif
