#include <Library/DeviceBootManagerLib.h>
#include <Library/UefiBootManagerLib.h>
#include <Library/DebugLib.h>

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
