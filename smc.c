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

#include <IOKit/IOKitLib.h>
#include <Kernel/string.h>
#include <libkern/OSAtomic.h>
#include <stdio.h>
#include <os/lock.h>

#include "smc.h"

static io_connect_t conn;

// Cache the keyInfo to lower the energy impact of SMCReadKey()
#define KEY_INFO_CACHE_SIZE 100

struct
{
  UInt32 key;
  SMCKeyData_keyInfo_t keyInfo;
} g_keyInfoCache[KEY_INFO_CACHE_SIZE];

int g_keyInfoCacheCount = 0;
os_unfair_lock g_keyInfoSpinLock = OS_UNFAIR_LOCK_INIT;

UInt32 _strtoul(char *str, int size, int base)
{
  UInt32 total = 0;
  int i;

  for (i = 0; i < size; i++)
  {
    if (base == 16)
      total += str[i] << (size - 1 - i) * 8;
    else
      total += (unsigned char)(str[i] << (size - 1 - i) * 8);
  }
  return total;
}

void _ultostr(char *str, UInt32 val)
{
  str[0] = '\0';
  sprintf(str, "%c%c%c%c",
          (unsigned int)val >> 24,
          (unsigned int)val >> 16,
          (unsigned int)val >> 8,
          (unsigned int)val);
}

kern_return_t SMCOpen(void)
{
  kern_return_t result;
  io_iterator_t iterator;
  io_object_t device;

  CFMutableDictionaryRef matchingDictionary = IOServiceMatching("AppleSMC");
  // using kIOMainPortDefault instead of IOMasterPort since it's deprecated
  result = IOServiceGetMatchingServices(kIOMainPortDefault, matchingDictionary, &iterator);
  if (result != kIOReturnSuccess)
  {
    printf("Error: IOServiceGetMatchingServices() = %08x\n", result);
    return 1;
  }

  device = IOIteratorNext(iterator);
  IOObjectRelease((io_object_t)iterator);
  if (device == 0)
  {
    printf("Error: no SMC found\n");
    return 1;
  }

  result = IOServiceOpen(device, mach_task_self(), 0, &conn);
  IOObjectRelease(device);
  if (result != kIOReturnSuccess)
  {
    printf("Error: IOServiceOpen() = %08x\n", result);
    return 1;
  }

  return kIOReturnSuccess;
}

kern_return_t SMCClose(void)
{
  return IOServiceClose(conn);
}

kern_return_t SMCCall(io_connect_t conn, int index, SMCKeyData_t *inputStructure, SMCKeyData_t *outputStructure)
{
  size_t structureInputSize;
  size_t structureOutputSize;

  structureInputSize = sizeof(SMCKeyData_t);
  structureOutputSize = sizeof(SMCKeyData_t);

  return IOConnectCallStructMethod(
      conn,
      index,
      inputStructure,
      structureInputSize,
      outputStructure,
      &structureOutputSize);
}

// Provides key info, using a cache to dramatically improve the energy impact of smcFanControl
kern_return_t SMCGetKeyInfo(io_connect_t conn, UInt32 key, SMCKeyData_keyInfo_t *keyInfo)
{
  SMCKeyData_t inputStructure;
  SMCKeyData_t outputStructure;
  kern_return_t result = kIOReturnSuccess;
  int i = 0;

  os_unfair_lock_lock(&g_keyInfoSpinLock);

  for (; i < g_keyInfoCacheCount; ++i)
  {
    if (key == g_keyInfoCache[i].key)
    {
      *keyInfo = g_keyInfoCache[i].keyInfo;
      break;
    }
  }

  if (i == g_keyInfoCacheCount)
  {
    // Not in cache, must look it up.
    memset(&inputStructure, 0, sizeof(inputStructure));
    memset(&outputStructure, 0, sizeof(outputStructure));

    inputStructure.key = key;
    inputStructure.data8 = SMC_CMD_READ_KEYINFO;

    result = SMCCall(conn, KERNEL_INDEX_SMC, &inputStructure, &outputStructure);
    if (result == kIOReturnSuccess)
    {
      *keyInfo = outputStructure.keyInfo;
      if (g_keyInfoCacheCount < KEY_INFO_CACHE_SIZE)
      {
        g_keyInfoCache[g_keyInfoCacheCount].key = key;
        g_keyInfoCache[g_keyInfoCacheCount].keyInfo = outputStructure.keyInfo;
        ++g_keyInfoCacheCount;
      }
    }
  }

  os_unfair_lock_unlock(&g_keyInfoSpinLock);

  return result;
}

kern_return_t SMCReadKey(UInt32Char_t key, SMCVal_t *val)
{
  kern_return_t result;
  SMCKeyData_t inputStructure;
  SMCKeyData_t outputStructure;

  memset(&inputStructure, 0, sizeof(SMCKeyData_t));
  memset(&outputStructure, 0, sizeof(SMCKeyData_t));
  memset(val, 0, sizeof(SMCVal_t));

  inputStructure.key = _strtoul(key, 4, 16);
  memcpy(val->key, key, sizeof(val->key));

  result = SMCGetKeyInfo(conn, inputStructure.key, &outputStructure.keyInfo);
  if (result != kIOReturnSuccess)
    return result;

  val->dataSize = outputStructure.keyInfo.dataSize;
  _ultostr(val->dataType, outputStructure.keyInfo.dataType);
  inputStructure.keyInfo.dataSize = val->dataSize;
  inputStructure.data8 = SMC_CMD_READ_BYTES;

  result = SMCCall(conn, KERNEL_INDEX_SMC, &inputStructure, &outputStructure);
  if (result != kIOReturnSuccess)
    return result;

  memcpy(val->bytes, outputStructure.bytes, sizeof(outputStructure.bytes));

  return kIOReturnSuccess;
}

// Implicit declaration of function 'SMCWriteKeyUnsafe' is invalid in C99
// Uncomment in order to write vals
// kern_return_t SMCWriteKey(io_connect_t conn, const SMCVal_t *val)
//{
//    SMCVal_t readVal;
//
//    IOReturn result = SMCReadKey(val->key, &readVal);
//    if (result != kIOReturnSuccess)
//        return result;
//
//    if (readVal.dataSize != val->dataSize)
//        return kIOReturnError;
//
//    return SMCWriteKeyUnsafe(val);
//}
//
// kern_return_t SMCWriteKeyUnsafe(SMCVal_t *val)
//{
//    kern_return_t result;
//    SMCKeyData_t  inputStructure;
//    SMCKeyData_t  outputStructure;
//
//    memset(&inputStructure, 0, sizeof(SMCKeyData_t));
//    memset(&outputStructure, 0, sizeof(SMCKeyData_t));
//
//    inputStructure.key = _strtoul(val->key, 4, 16);
//    inputStructure.data8 = SMC_CMD_WRITE_BYTES;
//    inputStructure.keyInfo.dataSize = val->dataSize;
//    memcpy(inputStructure.bytes, val->bytes, sizeof(val->bytes));
//
//    result = SMCCall(conn, KERNEL_INDEX_SMC, &inputStructure, &outputStructure);
//    if (result != kIOReturnSuccess)
//        return result;
//
//    return kIOReturnSuccess;
//}

double SMCGetTemperature(char *key)
{
  SMCVal_t val;
  kern_return_t result;

  result = SMCReadKey(key, &val);
  if (result == kIOReturnSuccess)
  {
    // read succeeded - check returned value
    if (val.dataSize > 0)
    {

      if (strcmp(val.dataType, DATATYPE_UINT8) == 0)
      {
        int intValue = val.bytes[0];
        return intValue;
      }
      if (strcmp(val.dataType, DATATYPE_UINT16) == 0)
      {
        int intValue = val.bytes[0] + val.bytes[1];
        return intValue;
      }
      if (strcmp(val.dataType, DATATYPE_UINT32) == 0)
      {
        int intValue = val.bytes[0] + val.bytes[1] + val.bytes[2] + val.bytes[3];
        return intValue;
      }
      if (strcmp(val.dataType, DATATYPE_SP1E) == 0)
      {
        int intValue = val.bytes[0] * 256 + (unsigned char)val.bytes[1];
        return intValue / 16384.0;
      }
      if (strcmp(val.dataType, DATATYPE_SP3C) == 0)
      {
        int intValue = val.bytes[0] * 256 + (unsigned char)val.bytes[1];
        return intValue / 4096.0;
      }
      if (strcmp(val.dataType, DATATYPE_SP4B) == 0)
      {
        int intValue = val.bytes[0] * 256 + (unsigned char)val.bytes[1];
        return intValue / 2048.0;
      }
      if (strcmp(val.dataType, DATATYPE_SP5A) == 0)
      {
        int intValue = val.bytes[0] * 256 + (unsigned char)val.bytes[1];
        return intValue / 1024.0;
      }
      if (strcmp(val.dataType, DATATYPE_SP69) == 0)
      {
        int intValue = val.bytes[0] * 256 + (unsigned char)val.bytes[1];
        return intValue / 512.0;
      }
      if (strcmp(val.dataType, DATATYPE_SP78) == 0)
      {
        // convert sp78 value to temperature
        int intValue = val.bytes[0] * 256 + (unsigned char)val.bytes[1];
        return intValue / 256.0;
      }
      if (strcmp(val.dataType, DATATYPE_SP87) == 0)
      {
        int intValue = val.bytes[0] * 256 + (unsigned char)val.bytes[1];
        return intValue / 128.0;
      }
      if (strcmp(val.dataType, DATATYPE_SP96) == 0)
      {
        int intValue = val.bytes[0] * 256 + (unsigned char)val.bytes[1];
        return intValue / 64.0;
      }
      if (strcmp(val.dataType, DATATYPE_SPB4) == 0)
      {
        int intValue = val.bytes[0] * 256 + (unsigned char)val.bytes[1];
        return intValue / 16.0;
      }
      if (strcmp(val.dataType, DATATYPE_SPF0) == 0)
      {
        int intValue = val.bytes[0] * 256 + (unsigned char)val.bytes[1];
        return intValue;
      }
      if (strcmp(val.dataType, "flt ") == 0)
      {
        return *(float *)val.bytes;
      }
    }
  }
  // read failed
  return 0.0;
}

double SMCGetFanSpeed(char *key)
{
  SMCVal_t val;
  kern_return_t result;

  result = SMCReadKey(key, &val);
  if (result == kIOReturnSuccess)
  {
    // read succeeded - check returned value
    if (val.dataSize > 0)
    {

      if (strcmp(val.dataType, "flt ") == 0)
      {
        // convert flt value to rpm
        int intValue = (unsigned char)val.bytes[0] * 256 + (unsigned char)val.bytes[1];
        return intValue / 4.0;
      }

      if (strcmp(val.dataType, DATATYPE_FPE2) == 0)
      {
        // convert fpe2 value to rpm
        int intValue = (unsigned char)val.bytes[0] * 256 + (unsigned char)val.bytes[1];
        return intValue / 4.0;
      }
    }
  }
  // read failed
  return 0.0;
}

double convertToFahrenheit(double celsius)
{
  return (celsius * (9.0 / 5.0)) + 32.0;
}

// Requires SMCOpen()
void readAndPrintCpuTemp(int show_title, char scale)
{
  double temperature = SMCGetTemperature("Tp09");
  if (scale == 'F')
  {
    temperature = convertToFahrenheit(temperature);
  }

  char *title = "";
  if (show_title)
  {
    title = "CPU: ";
  }
  printf("%s%0.1f °%c\n", title, temperature, scale);
}

// Requires SMCOpen()
void readAndPrintGpuTemp(int show_title, char scale)
{
  double temperature = SMCGetTemperature(SMC_KEY_GPU_TEMP);
  if (scale == 'F')
  {
    temperature = convertToFahrenheit(temperature);
  }

  char *title = "";
  if (show_title)
  {
    title = "GPU: ";
  }
  printf("%s%0.1f °%c\n", title, temperature, scale);
}

float SMCGetFanRPM(char *key)
{
  SMCVal_t val;
  kern_return_t result;

  result = SMCReadKey(key, &val);
  if (result == kIOReturnSuccess)
  {
    // read succeeded - check returned value
    if (val.dataSize > 0)
    {

      if (strcmp(val.dataType, "flt ") == 0)
      {
        return *(float *)val.bytes;
      }
      printf("66 %s %s %s\n", val.dataType, DATATYPE_FPE2, key);
      if (strcmp(val.dataType, DATATYPE_FPE2) == 0)
      {
        // convert fpe2 value to RPM
        return ntohs(*(UInt16 *)val.bytes) / 4.0;
      }
    }
  }
  // read failed
  return -1.f;
}

// Requires SMCOpen()
void readAndPrintFanRPMs(void)
{
  kern_return_t result;
  SMCVal_t val;
  UInt32Char_t key;
  int totalFans, i;

  result = SMCReadKey(SMC_KEY_FAN_COUNT, &val);

  if (result == kIOReturnSuccess)
  {
    totalFans = _strtoul((char *)val.bytes, val.dataSize, 10);

    printf("Num fans: %d\n", totalFans);
    for (i = 0; i < totalFans; i++)
    {
      sprintf(key, "F%dID", i);
      result = SMCReadKey(key, &val);
      if (result != kIOReturnSuccess)
      {
        continue;
      }
      char *name = val.bytes + 4;

      sprintf(key, "F%dAc", i);
      float actual_speed = SMCGetFanRPM(key);
      if (actual_speed < 0.f)
      {
        continue;
      }

      sprintf(key, "F%dMn", i);
      float minimum_speed = SMCGetFanRPM(key);
      if (minimum_speed < 0.f)
      {
        continue;
      }

      sprintf(key, "F%dMx", i);
      float maximum_speed = SMCGetFanRPM(key);
      if (maximum_speed < 0.f)
      {
        continue;
      }

      float rpm = actual_speed - minimum_speed;
      if (rpm < 0.f)
      {
        rpm = 0.f;
      }

      printf("Fan %d - %s at %.0f RPM (%.0f%%)\n", i, name, actual_speed, 100 * actual_speed / maximum_speed);

      // sprintf(key, "F%dSf", i);
      // SMCReadKey(key, &val);
      // printf("    Safe speed   : %.0f\n", strtof(val.bytes, val.dataSize, 2));
      // sprintf(key, "F%dTg", i);
      // SMCReadKey(key, &val);
      // printf("    Target speed : %.0f\n", strtof(val.bytes, val.dataSize, 2));
      // SMCReadKey("FS! ", &val);
      // if ((_strtoul((char *)val.bytes, 2, 16) & (1 << i)) == 0)
      //     printf("    Mode         : auto\n");
      // else
      //     printf("    Mode         : forced\n");
    }
  }
}

int main(int argc, char *argv[])
{
  char scale = 'C';
  int cpu = 0;
  int fan = 0;
  int gpu = 0;

  int c;
  while ((c = getopt(argc, argv, "CFcfgh?")) != -1)
  {
    switch (c)
    {
    case 'F':
    case 'C':
      scale = c;
      break;
    case 'c':
      cpu = 1;
      break;
    case 'f':
      fan = 1;
      break;
    case 'g':
      gpu = 1;
      break;
    case 'h':
    case '?':
      printf("usage: osx-cpu-temp <options>\n");
      printf("Options:\n");
      printf("  -F  Display temperatures in degrees Fahrenheit.\n");
      printf("  -C  Display temperatures in degrees Celsius (Default).\n");
      printf("  -c  Display CPU temperature (Default).\n");
      printf("  -g  Display GPU temperature.\n");
      printf("  -f  Display fan speeds.\n");
      printf("  -h  Display this help.\n");
      printf("\nIf more than one of -c, -f, or -g are specified, titles will be added\n");
      return -1;
    }
  }

  if (!fan && !gpu)
  {
    cpu = 1;
  }

  int show_title = fan + gpu + cpu > 1;

  SMCOpen();

  if (cpu)
  {
    readAndPrintCpuTemp(show_title, scale);
  }
  if (gpu)
  {
    readAndPrintGpuTemp(show_title, scale);
  }
  if (fan)
  {
    readAndPrintFanRPMs();
  }

  SMCClose();
  return 0;
}
