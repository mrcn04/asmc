/*
 * Copyright (C) 2022 Ömürcan Cengiz
 * The source code does not belong to me. I only edited, added,
 * modified, enchanged the codes in a way that I need. You can find the
 * sources on README.md.

 =================================================================

 * Apple System Management Control (SMC) Tool
 * Copyright (C) 2006 devnull
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#include <stdint.h>
#include <mach/kern_return.h>
#include <MacTypes.h>
#define VERSION "0.01"

#define KERNEL_INDEX_SMC 2

#define SMC_CMD_READ_BYTES 5
#define SMC_CMD_WRITE_BYTES 6
#define SMC_CMD_READ_INDEX 8
#define SMC_CMD_READ_KEYINFO 9
#define SMC_CMD_READ_PLIMIT 11
#define SMC_CMD_READ_VERS 12

#define DATATYPE_UINT8 "ui8 "
#define DATATYPE_UINT16 "ui16"
#define DATATYPE_UINT32 "ui32"
#define DATATYPE_SP1E "sp1e"
#define DATATYPE_SP3C "sp3c"
#define DATATYPE_SP4B "sp4b"
#define DATATYPE_SP5A "sp5a"
#define DATATYPE_SP69 "sp669"
#define DATATYPE_SP78 "sp78"
#define DATATYPE_SP87 "sp87"
#define DATATYPE_SP96 "sp96"
#define DATATYPE_SPB4 "spb4"
#define DATATYPE_SPF0 "spf0"

#define DATATYPE_FLT "flt "
#define DATATYPE_FP1F "fp1f"
#define DATATYPE_FPE2 "fpe2"
#define DATATYPE_FP2E "fp2e"
#define DATATYPE_FP4C "fp4c"
#define DATATYPE_FPC4 "fpc4"
#define DATATYPE_CH8 "ch8*"
#define DATATYPE_FDS "{fds"

#define SMC_KEY_CPU_TEMP "TC0P"
// int value is % with
#define SMC_KEY_CPU_CORE_TEMP "TC1C"
#define SMC_KEY_GPU_TEMP "TG0P"
#define SMC_KEY_FAN_COUNT "FNum"
#define SMC_KEY_FAN0_RPM_CUR "F0Ac"
#define SMC_KEY_FAN0_RPM_MIN "F0Mn"
#define SMC_KEY_FAN0_RPM_MAX "F0Mx"
#define SMC_KEY_FAN1_RPM_CUR "F1Ac"
#define SMC_KEY_FAN1_RPM_MIN "F1Mn"
#define SMC_KEY_FAN1_RPM_MAX "F1Mx"
#define SMC_KEY_FAN_TS "F0Tg"
#define SMC_KEY_FAN_POS "F0ID"

// Apple Silicon
#define SMC_KEY_BATTERY_1_TEMP "TB1T"
#define SMC_KEY_BATTERY_2_TEMP "TB2T"

// m1,m1 pro,m1 max, m1 ultra
#define SMC_KEY_CPU_ECORE_1_TEMP_M1 "Tp09"
#define SMC_KEY_CPU_ECORE_2_TEMP_M1 "Tp0T"
#define SMC_KEY_CPU_PCORE_1_TEMP_M1 "Tp01"
#define SMC_KEY_CPU_PCORE_2_TEMP_M1 "Tp05"
#define SMC_KEY_CPU_PCORE_3_TEMP_M1 "Tp0D"
#define SMC_KEY_CPU_PCORE_4_TEMP_M1 "Tp0H"
#define SMC_KEY_CPU_PCORE_5_TEMP_M1 "Tp0L"
#define SMC_KEY_CPU_PCORE_6_TEMP_M1 "Tp0P"
#define SMC_KEY_CPU_PCORE_7_TEMP_M1 "Tp0X"
#define SMC_KEY_CPU_PCORE_8_TEMP_M1 "Tp0b"

#define SMC_KEY_GPU_1_TEMP_M1 "Tg05"
#define SMC_KEY_GPU_2_TEMP_M1 "Tg0D"
#define SMC_KEY_GPU_3_TEMP_M1 "Tg0L"
#define SMC_KEY_GPU_4_TEMP_M1 "Tg0T"

typedef struct
{
  char major;
  char minor;
  char build;
  char reserved[1];
  UInt16 release;
} SMCKeyData_vers_t;

typedef struct
{
  UInt16 version;
  UInt16 length;
  UInt32 cpuPLimit;
  UInt32 gpuPLimit;
  UInt32 memPLimit;
} SMCKeyData_pLimitData_t;

typedef struct
{
  UInt32 dataSize;
  UInt32 dataType;
  char dataAttributes;
} SMCKeyData_keyInfo_t;

typedef char SMCBytes_t[32];

typedef struct
{
  UInt32 key;
  SMCKeyData_vers_t vers;
  SMCKeyData_pLimitData_t pLimitData;
  SMCKeyData_keyInfo_t keyInfo;
  char result;
  char status;
  char data8;
  UInt32 data32;
  SMCBytes_t bytes;
} SMCKeyData_t;

typedef char UInt32Char_t[5];

typedef struct
{
  UInt32Char_t key;
  UInt32 dataSize;
  UInt32Char_t dataType;
  SMCBytes_t bytes;
} SMCVal_t;

// prototypes
double SMCGetTemperature(char *key);
// kern_return_t SMCSetFanRpm(char *key, int rpm);
kern_return_t SMCOpen(void);
void readAndPrintFanRPMs(void);
