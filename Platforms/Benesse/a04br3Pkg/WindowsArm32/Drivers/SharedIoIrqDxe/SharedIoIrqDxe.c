/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * MT8167 board-only SYSIRQ handoff for the combined USB/AFE/MSDC2 profile.
 * Linux irq-mtk-sysirq.c converts DTS LEVEL_LOW to GIC LEVEL_HIGH by setting
 * bit (SPI & 31) at intpol_base + (SPI >> 5)*4; irq-gic.c rejects low SPIs.
 * No GIC enable, PMIC, PLL, USB PHY, audio or Wi-Fi power writes here. */
#include <Uefi.h>
#include <Guid/EventGroup.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/UefiBootServicesTableLib.h>
STATIC BOOLEAN Attempted;
STATIC VOID EFIAPI SharedIoBeforeExit(EFI_EVENT Event,VOID *Context)
{
  UINT32 UsbBefore,UsbAfter,MediaBefore,MediaAfter;
  (VOID)Event;(VOID)Context;
  if(Attempted)return;
  Attempted=TRUE;
  /* Board mt8167.dtsi: USB SPI72, MSDC2 SPI109, AFE SPI120, all LEVEL_LOW.
   * Preserve every other interrupt, including GPU SPI185 at a separate word. */
  UsbBefore=MmioRead32(0x10200628U);
  MediaBefore=MmioRead32(0x1020062cU);
  MmioWrite32(0x10200628U,UsbBefore|BIT8);
  MmioWrite32(0x1020062cU,MediaBefore|BIT13|BIT24);
  UsbAfter=MmioRead32(0x10200628U);
  MediaAfter=MmioRead32(0x1020062cU);
  DEBUG((DEBUG_ERROR,"A04IO SYSIRQ USB72 %08x->%08x AFE120/MSDC109 %08x->%08x\n",
    UsbBefore,UsbAfter,MediaBefore,MediaAfter));
  if((UsbAfter&BIT8)==0 || (MediaAfter&(BIT13|BIT24))!=(BIT13|BIT24))
    DEBUG((DEBUG_ERROR,"A04IO SYSIRQ unavailable; integrated IRQ routing unverified\n"));
}
EFI_STATUS EFIAPI SharedIoIrqEntry(EFI_HANDLE ImageHandle,EFI_SYSTEM_TABLE *SystemTable)
{
  EFI_EVENT Event;(VOID)ImageHandle;(VOID)SystemTable;
  return gBS->CreateEventEx(EVT_NOTIFY_SIGNAL,TPL_CALLBACK,SharedIoBeforeExit,NULL,
                            &gEfiEventBeforeExitBootServicesGuid,&Event);
}
