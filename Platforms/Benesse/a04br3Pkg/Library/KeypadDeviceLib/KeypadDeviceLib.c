/**
  Benesse TAB-A04-BR3 keys for SiliciumPkg KeypadDeviceDxe.

  The board has three keys:

  - VOLUME UP and HOME sit on the MT8167 matrix keypad (KPD, 0x10002000).
    KP_MEM1 (0x04, 16 bit) holds one bit per matrix position, 0 = pressed;
    bit 0 is VOLUME UP and bit 1 is HOME. KP_DEBOUNCE is 0x18 and KP_EN 0x24.
    Source: the A04BR3 Linux keypad driver
    (a04br3-linux-drivers/keys/a04br3-mt8167-keypad.c), which takes the
    layout from the stock A04BR3 kernel, and the A04BR3 DT keypad node
    (mediatek,kpd-hw-init-map = <0x73 0x66 ...>, mediatek,kpd-key-debounce
    = <0x400>).
  - POWER is the MT6392 PMIC power key, read through the PMIC wrapper:
    MT6392_CHRSTATUS (0x0142) bit 1, 0 = pressed
    (a04br3-linux-drivers/pmic/mtk-pmic-keys.c mt6392_regs and
    include/linux/mfd/mt6392/registers.h).

  UniXNU's menu moves with SCAN_UP/SCAN_DOWN and boots with ENTER, so
  VOLUME UP reports SCAN_UP, HOME reports SCAN_DOWN and POWER reports ENTER.

  The key state machine is the one of Xiaomi/lancelotPkg KeypadDeviceLib.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>

#include <Library/BaseMemoryLib.h>
#include <Library/BitmapLib.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/KeypadDeviceLib.h>
#include <Library/MemoryMapHelperLib.h>
#include <Library/UefiBootServicesTableLib.h>

#include <Protocol/MtkPmicWrapper.h>

//
// MT8167 keypad (A04BR3 Linux keypad driver)
//
#define KP_MEM1                0x04
#define KP_DEBOUNCE            0x18
#define KP_EN                  0x24
#define KP_DEBOUNCE_MASK       0x3FFF
#define KP_DEBOUNCE_A04BR3     0x400

//
// MT6392 power key (mtk-pmic-keys.c mt6392_regs)
//
#define MT6392_CHRSTATUS       0x0142
#define MT6392_PWRKEY_DEB      BIT1

typedef enum {
  KEY_TYPE_UNKNOWN,
  KEY_TYPE_MATRIX,
  KEY_TYPE_PMIC,
} KEY_TYPE;

typedef struct {
  KEY_CONTEXT EfiKeyContext;
  KEY_TYPE    Type;
  UINT32      Mask;
} KEY_CONTEXT_PRIVATE;

STATIC EFI_MEMORY_REGION_DESCRIPTOR  mKeypadRegion;
STATIC BOOLEAN                       mKeypadAvailable;
STATIC MTK_PMIC_WRAPPER_PROTOCOL    *mPmicWrapper;

UINTN gBitmapScanCodes[BITMAP_NUM_WORDS(0x18)]    = {0};
UINTN gBitmapUnicodeChars[BITMAP_NUM_WORDS(0x7f)] = {0};

EFI_KEY_DATA gKeyDataPowerDown      = {.Key = {.ScanCode = SCAN_RIGHT,}};
EFI_KEY_DATA gKeyDataPowerUp        = {.Key = {.ScanCode = SCAN_LEFT,}};
EFI_KEY_DATA gKeyDataPowerLongpress = {.Key = {.ScanCode = SCAN_ESC,}};

#define MS2NS(ms) (((UINT64)(ms)) * 1000000ULL)

STATIC
inline
VOID
KeySetState (
  UINT16  ScanCode,
  CHAR16  UnicodeChar,
  BOOLEAN Value)
{
  if (ScanCode && ScanCode < 0x18) {
    if (Value) {
      BitmapSet(gBitmapScanCodes, ScanCode);
    } else {
      BitmapClear(gBitmapScanCodes, ScanCode);
    }
  }

  if (UnicodeChar && UnicodeChar < 0x7f) {
    if (Value) {
      BitmapSet(gBitmapUnicodeChars, UnicodeChar);
    } else {
      BitmapClear(gBitmapUnicodeChars, UnicodeChar);
    }
  }
}

STATIC
inline
BOOLEAN
KeyGetState (
  UINT16 ScanCode,
  CHAR16 UnicodeChar)
{
  if (ScanCode && ScanCode < 0x18) {
    if (!BitmapTest(gBitmapScanCodes, ScanCode)) {
      return FALSE;
    }
  }

  if (UnicodeChar && UnicodeChar < 0x7f) {
    if (!BitmapTest(gBitmapUnicodeChars, UnicodeChar)) {
      return FALSE;
    }
  }

  return TRUE;
}

STATIC
inline
VOID
LibKeyInitializeKeyContext (KEY_CONTEXT *Context)
{
  SetMem(&Context->KeyData, sizeof(Context->KeyData), 0);

  Context->Time      = 0;
  Context->State     = KEYSTATE_RELEASED;
  Context->Repeat    = FALSE;
  Context->Longpress = FALSE;
}

STATIC
inline
VOID
LibKeyUpdateKeyStatus (
  KEY_CONTEXT       *Context,
  KEYPAD_RETURN_API *KeypadReturnApi,
  BOOLEAN            IsPressed,
  UINT64             Delta)
{
  // Keep Track of the Actual State
  KeySetState (Context->KeyData.Key.ScanCode, Context->KeyData.Key.UnicodeChar, IsPressed);

  // Update Key Time
  Context->Time += Delta;

  switch (Context->State) {
    case KEYSTATE_RELEASED:
      if (IsPressed) {
        // Change to Pressed
        Context->Time  = 0;
        Context->State = KEYSTATE_PRESSED;
      }

      break;

    case KEYSTATE_PRESSED:
      if (IsPressed) {
        // Key Repeat
        if (Context->Repeat && Context->Time >= MS2NS(100)) {
          KeypadReturnApi->PushEfikeyBufTail (KeypadReturnApi, &Context->KeyData);
          Context->Time   = 0;
          Context->Repeat = TRUE;
        } else if (!Context->Longpress && Context->Time >= MS2NS(500)) {
          // Handle Key Combos
          if (Context->KeyData.Key.UnicodeChar == CHAR_CARRIAGE_RETURN) {
            if (KeyGetState (SCAN_DOWN, 0)) {
              // Report SCAN_RIGHT
              KeypadReturnApi->PushEfikeyBufTail (KeypadReturnApi, &gKeyDataPowerDown);
            } else if (KeyGetState (SCAN_UP, 0)) {
              // Report SCAN_LEFT
              KeypadReturnApi->PushEfikeyBufTail (KeypadReturnApi, &gKeyDataPowerUp);
            } else {
              // Report SCAN_ESC
              KeypadReturnApi->PushEfikeyBufTail (KeypadReturnApi, &gKeyDataPowerLongpress);
            }
          } else { // Post first Keyrepeat Event
            // Only Start Keyrepeat if we're not doing a Combo
            if (!KeyGetState (0, CHAR_CARRIAGE_RETURN)) {
              KeypadReturnApi->PushEfikeyBufTail (KeypadReturnApi, &Context->KeyData);
              Context->Time   = 0;
              Context->Repeat = TRUE;
            }
          }

          Context->Longpress = TRUE;
        }
      } else {
        if (!Context->Longpress) {
          // We Supressed Down, so Report it Now
          KeypadReturnApi->PushEfikeyBufTail (KeypadReturnApi, &Context->KeyData);
          Context->State = KEYSTATE_LONGPRESS_RELEASE;
        } else if (Context->Time >= MS2NS(10)) {
          // We Reported another Key Already
          Context->Time      = 0;
          Context->Repeat    = FALSE;
          Context->Longpress = FALSE;
          Context->State     = KEYSTATE_RELEASED;
        }
      }

      break;

    case KEYSTATE_LONGPRESS_RELEASE:
      // Change to Released
      Context->Time      = 0;
      Context->Repeat    = FALSE;
      Context->Longpress = FALSE;
      Context->State     = KEYSTATE_RELEASED;
      break;

    default:
      ASSERT(FALSE);
      break;
  }
}

STATIC KEY_CONTEXT_PRIVATE KeyContextVolumeUp = { .Type = KEY_TYPE_MATRIX, .Mask = BIT0 };
STATIC KEY_CONTEXT_PRIVATE KeyContextHome     = { .Type = KEY_TYPE_MATRIX, .Mask = BIT1 };
STATIC KEY_CONTEXT_PRIVATE KeyContextPower    = { .Type = KEY_TYPE_PMIC,   .Mask = MT6392_PWRKEY_DEB };

STATIC KEY_CONTEXT_PRIVATE *KeyList[] = { &KeyContextVolumeUp, &KeyContextHome, &KeyContextPower };

RETURN_STATUS
EFIAPI
KeypadDeviceConstructor ()
{
  EFI_STATUS Status;

  // Locate the Keypad Registers
  Status = LocateMemoryRegionByName ("Keypad", &mKeypadRegion);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "Failed to Locate Keypad Memory Region! Status = %r\n", Status));
    return RETURN_SUCCESS;
  }

  // Same set-up as the A04BR3 Linux keypad driver probe
  MmioWrite16 (mKeypadRegion.Address + KP_DEBOUNCE, KP_DEBOUNCE_A04BR3 & KP_DEBOUNCE_MASK);
  MmioWrite16 (mKeypadRegion.Address + KP_EN, 1);

  mKeypadAvailable = TRUE;

  return RETURN_SUCCESS;
}

EFI_STATUS
EFIAPI
KeypadDeviceReset (KEYPAD_DEVICE_PROTOCOL *This)
{
  LibKeyInitializeKeyContext (&KeyContextVolumeUp.EfiKeyContext);
  KeyContextVolumeUp.EfiKeyContext.KeyData.Key.ScanCode = SCAN_UP;

  LibKeyInitializeKeyContext (&KeyContextHome.EfiKeyContext);
  KeyContextHome.EfiKeyContext.KeyData.Key.ScanCode     = SCAN_DOWN;

  LibKeyInitializeKeyContext (&KeyContextPower.EfiKeyContext);
  KeyContextPower.EfiKeyContext.KeyData.Key.UnicodeChar = CHAR_CARRIAGE_RETURN;

  return EFI_SUCCESS;
}

EFI_STATUS
KeypadDeviceGetKeys (
  KEYPAD_DEVICE_PROTOCOL *This,
  KEYPAD_RETURN_API      *KeypadReturnApi,
  UINT64                  Delta)
{
  EFI_STATUS Status;
  UINT16     MatrixState = 0xFFFF;
  UINT16     PmicState   = 0xFFFF;
  BOOLEAN    IsPressed;

  // The PMIC wrapper driver may start after this library's constructor.
  if (mPmicWrapper == NULL) {
    Status = gBS->LocateProtocol (&gMediaTekPmicWrapperProtocolGuid, NULL, (VOID **)&mPmicWrapper);
    if (EFI_ERROR (Status)) {
      mPmicWrapper = NULL;
    }
  }

  if (mKeypadAvailable) {
    MatrixState = MmioRead16 (mKeypadRegion.Address + KP_MEM1);
  }

  if (mPmicWrapper != NULL) {
    mPmicWrapper->Read (MT6392_CHRSTATUS, &PmicState);
  }

  for (UINTN Index = 0; Index < ARRAY_SIZE (KeyList); Index++) {
    KEY_CONTEXT_PRIVATE *Context = KeyList[Index];

    if (Context->Type == KEY_TYPE_MATRIX) {
      if (!mKeypadAvailable) {
        continue;
      }

      IsPressed = (MatrixState & Context->Mask) == 0;
    } else if (Context->Type == KEY_TYPE_PMIC) {
      if (mPmicWrapper == NULL) {
        continue;
      }

      IsPressed = (PmicState & Context->Mask) == 0;
    } else {
      continue;
    }

    LibKeyUpdateKeyStatus (&Context->EfiKeyContext, KeypadReturnApi, IsPressed, Delta);
  }

  return EFI_SUCCESS;
}
