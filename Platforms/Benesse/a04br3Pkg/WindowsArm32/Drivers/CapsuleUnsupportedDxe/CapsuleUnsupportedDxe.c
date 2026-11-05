/* ARM32 board capsule runtime: capsule update is not supported.
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 */
#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Protocol/Capsule.h>

STATIC EFI_STATUS EFIAPI
UnsupportedUpdate (EFI_CAPSULE_HEADER **Headers, UINTN Count,
                   EFI_PHYSICAL_ADDRESS ScatterGatherList)
{
  (VOID)ScatterGatherList;
  if (Headers == NULL || Count == 0) return EFI_INVALID_PARAMETER;
  return EFI_UNSUPPORTED;
}

STATIC EFI_STATUS EFIAPI
UnsupportedQuery (EFI_CAPSULE_HEADER **Headers, UINTN Count,
                  UINT64 *MaximumCapsuleSize, EFI_RESET_TYPE *ResetType)
{
  if (Headers == NULL || Count == 0 || MaximumCapsuleSize == NULL || ResetType == NULL)
    return EFI_INVALID_PARAMETER;
  return EFI_UNSUPPORTED;
}

EFI_STATUS EFIAPI
CapsuleUnsupportedEntry (EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
  EFI_HANDLE Handle = NULL;
  EFI_STATUS Status;
  EFI_UPDATE_CAPSULE OldUpdate = gRT->UpdateCapsule;
  EFI_QUERY_CAPSULE_CAPABILITIES OldQuery = gRT->QueryCapsuleCapabilities;
  (VOID)ImageHandle; (VOID)SystemTable;
  gRT->UpdateCapsule = UnsupportedUpdate;
  gRT->QueryCapsuleCapabilities = UnsupportedQuery;
  Status = gBS->InstallMultipleProtocolInterfaces (&Handle, &gEfiCapsuleArchProtocolGuid, NULL, NULL);
  if (EFI_ERROR (Status)) {
    gRT->UpdateCapsule = OldUpdate;
    gRT->QueryCapsuleCapabilities = OldQuery;
  }
  return Status;
}
