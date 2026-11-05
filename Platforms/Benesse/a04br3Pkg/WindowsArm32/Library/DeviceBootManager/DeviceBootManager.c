#include <Library/DeviceBootManagerLib.h>
#include <Library/UefiBootManagerLib.h>
#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/DevicePathLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Protocol/SimpleFileSystem.h>

STATIC VOID
RegisterArmBootFiles (VOID)
{
  EFI_STATUS Status;
  EFI_HANDLE *Handles;
  UINTN Count, Index, ExistingCount;
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *Fs;
  EFI_FILE_PROTOCOL *Root, *File;
  EFI_DEVICE_PATH_PROTOCOL *Path;
  EFI_BOOT_MANAGER_LOAD_OPTION Option, *Existing;

  Status = gBS->LocateHandleBuffer (ByProtocol, &gEfiSimpleFileSystemProtocolGuid, NULL, &Count, &Handles);
  DEBUG ((DEBUG_ERROR, "A04BDS filesystems status=%r count=%u\n", Status, EFI_ERROR (Status) ? 0 : Count));
  if (EFI_ERROR (Status)) return;
  for (Index = 0; Index < Count; ++Index) {
    Status = gBS->HandleProtocol (Handles[Index], &gEfiSimpleFileSystemProtocolGuid, (VOID **)&Fs);
    if (EFI_ERROR (Status)) continue;
    Status = Fs->OpenVolume (Fs, &Root);
    if (EFI_ERROR (Status)) continue;
    Status = Root->Open (Root, &File, L"\\EFI\\BOOT\\BOOTARM.EFI", EFI_FILE_MODE_READ, 0);
    Root->Close (Root);
    if (EFI_ERROR (Status)) continue;
    File->Close (File);
    Path = FileDevicePath (Handles[Index], L"\\EFI\\BOOT\\BOOTARM.EFI");
    if (Path == NULL) continue;
    Status = EfiBootManagerInitializeLoadOption (&Option, LoadOptionNumberUnassigned, LoadOptionTypeBoot, LOAD_OPTION_ACTIVE, L"ARM EFI boot", Path, NULL, 0);
    FreePool (Path);
    if (EFI_ERROR (Status)) continue;
    Existing = EfiBootManagerGetLoadOptions (&ExistingCount, LoadOptionTypeBoot);
    if (Existing == NULL || EfiBootManagerFindLoadOption (&Option, Existing, ExistingCount) == -1) {
      Status = EfiBootManagerAddLoadOptionVariable (&Option, (UINTN)-1);
      DEBUG ((DEBUG_ERROR, "A04BDS register ARM EFI status=%r\n", Status));
    } else {
      DEBUG ((DEBUG_ERROR, "A04BDS ARM EFI already registered\n"));
    }
    if (Existing != NULL) EfiBootManagerFreeLoadOptions (Existing, ExistingCount);
    EfiBootManagerFreeLoadOption (&Option);
  }
  FreePool (Handles);
}

EFI_HANDLE
EFIAPI
DeviceBootManagerBeforeConsole (
  OUT EFI_DEVICE_PATH_PROTOCOL  **DevicePath,
  OUT BDS_CONSOLE_CONNECT_ENTRY **PlatformConsoles)
{
  return NULL;
}

EFI_DEVICE_PATH_PROTOCOL**
EFIAPI
DeviceBootManagerAfterConsole ()
{
  DEBUG ((DEBUG_INFO, "A04BDS: connect and enumerate boot devices\n"));
  EfiBootManagerConnectAll ();
  RegisterArmBootFiles ();
  EfiBootManagerRefreshAllBootOption ();
  DEBUG ((DEBUG_INFO, "A04BDS: boot enumeration complete\n"));
  return NULL;
}

EFI_DEVICE_PATH_PROTOCOL**
EFIAPI
DeviceBootManagerOnDemandConInConnect ()
{
  return NULL;
}

VOID
EFIAPI
DeviceBootManagerProcessBootCompletion (OUT EFI_BOOT_MANAGER_LOAD_OPTION *BootOption)
{
  return;
}

EFI_STATUS
EFIAPI
DeviceBootManagerPriorityBoot (IN OUT EFI_BOOT_MANAGER_LOAD_OPTION *BootOption)
{
  return EFI_SUCCESS;
}

VOID
EFIAPI
DeviceBootManagerUnableToBoot ()
{
  return;
}

VOID
EFIAPI
DeviceBootManagerBdsEntry ()
{
  return;
}
