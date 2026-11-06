/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef MT8167_GPU_CLOCK_RECORD_H
#define MT8167_GPU_CLOCK_RECORD_H
#include <Uefi.h>
#define MT8167_GPU_CLOCK_NAME L"Mt8167GpuClock"
#define MT8167_GPU_CLOCK_GUID {0x9c50bc94,0x6c80,0x4fa2,{0x93,0x9d,0x92,0x78,0xd0,0x7f,0x70,0x10}}
typedef struct {
  UINT32 version;
  UINT32 bytes;
  UINT32 parent;
  UINT32 clock_hz;
  UINT32 con0;
  UINT32 con1;
  UINT32 power;
  UINT32 rail_control;
  UINT32 rail_status;
  UINT32 sel0;
  UINT32 sel1;
} MT8167_GPU_CLOCK_RECORD;
STATIC_ASSERT(sizeof(MT8167_GPU_CLOCK_RECORD) == 44, "GPU clock record ABI");
#endif
