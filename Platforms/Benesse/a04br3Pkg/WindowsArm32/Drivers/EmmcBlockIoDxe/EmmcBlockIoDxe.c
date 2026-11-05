/** Windows ARM32 board wrapper for eMMC model handoff.
  Only the manufacturer ID and product name are exposed, never the serial.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/
#include <Uefi.h>
#include <Guid/EventGroup.h>
#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>

STATIC UINT8      mEmmcModel[7];
STATIC EFI_EVENT  mEmmcModelEvent;
STATIC EFI_GUID   mEmmcModelGuid = { 0x689e5e54, 0x214e, 0x4df7, { 0xa8, 0x71, 0x8f, 0x31, 0xe1, 0x55, 0x9a, 0x21 } };

STATIC VOID EFIAPI
PublishEmmcModel (EFI_EVENT Event, VOID *Context)
{
  EFI_STATUS Status;
  (VOID)Context;
  Status = gRT->SetVariable (L"A04EmmcModel", &mEmmcModelGuid,
                  EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS,
                  sizeof (mEmmcModel), mEmmcModel);
  DEBUG ((DEBUG_ERROR, "[A04BR3 eMMC] model handoff %r\n", Status));
  gBS->CloseEvent (Event);
  mEmmcModelEvent = NULL;
}

STATIC VOID
ObserveEmmcCid (CONST UINT32 *Cid)
{
  EFI_STATUS Status;
  UINTN Index;
  BOOLEAN NonSpace = FALSE;
  UINT8 Model[7];
  Model[0] = (UINT8)(Cid[0] >> 24);
  Model[1] = (UINT8)Cid[0];
  Model[2] = (UINT8)(Cid[1] >> 24);
  Model[3] = (UINT8)(Cid[1] >> 16);
  Model[4] = (UINT8)(Cid[1] >> 8);
  Model[5] = (UINT8)Cid[1];
  Model[6] = (UINT8)(Cid[2] >> 24);
  if (Model[0] == 0 || Model[0] == 0xff) return;
  for (Index = 1; Index < sizeof (Model); ++Index) {
    if (Model[Index] < 0x20 || Model[Index] > 0x7e) return;
    if (Model[Index] != 0x20) NonSpace = TRUE;
  }
  if (!NonSpace) return;
  for (Index = 0; Index < sizeof (Model); ++Index) mEmmcModel[Index] = Model[Index];
  if (mEmmcModelEvent != NULL) return;
  Status = gBS->CreateEventEx (EVT_NOTIFY_SIGNAL, TPL_CALLBACK, PublishEmmcModel,
                  NULL, &gEfiEventReadyToBootGuid, &mEmmcModelEvent);
  if (EFI_ERROR (Status)) DEBUG ((DEBUG_ERROR, "[A04BR3 eMMC] model event %r\n", Status));
}

#define A04BR3_EMMC_CID_OBSERVED(Cid)  ObserveEmmcCid (Cid)
#include "../../../Drivers/EmmcBlockIoDxe/EmmcBlockIoDxe.c"
