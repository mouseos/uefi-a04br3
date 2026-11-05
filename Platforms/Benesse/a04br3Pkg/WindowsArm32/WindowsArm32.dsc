##
#  Copyright (c) 2011 - 2022, ARM Limited. All rights reserved.
#  Copyright (c) 2014, Linaro Limited. All rights reserved.
#  Copyright (c) 2015 - 2020, Intel Corporation. All rights reserved.
#  Copyright (c) 2018, Bingxing Wang. All rights reserved.
#  Copyright (c) Microsoft Corporation.
#
#  Benesse TAB-A04-BR3 (MediaTek MT8167). Board values and the display,
#  eMMC/SD and UART drivers come from the working A04BR3 edk2 port
#  (mouseos/edk2-mtk, Platform/Benesse/A04BR3Pkg, commit 1982d888c765).
#
#  SPDX-License-Identifier: BSD-2-Clause-Patent
##

################################################################################
#
# Defines Section - statements that will be processed to create a Makefile.
#
################################################################################
[Defines]
  PLATFORM_NAME                  = a04br3
  PLATFORM_GUID                  = 5BBBD0DF-844D-42FF-8CC6-E5A04C8467A9
  PLATFORM_VERSION               = 0.1
  DSC_SPECIFICATION              = 0x00010005
  OUTPUT_DIRECTORY               = $(A04_BUILD_OUTPUT)
  SUPPORTED_ARCHITECTURES        = ARM
  BUILD_TARGETS                  = RELEASE|DEBUG
  SKUID_IDENTIFIER               = DEFAULT
  FLASH_DEFINITION               = a04br3Pkg/WindowsArm32/WindowsArm32.fdf
  USE_CUSTOM_DISPLAY_DRIVER      = 0

  #
  # The UniXNU firmware driver passes only the defines of its board file;
  # default the ones build_uefi.py always passes.
  #
!ifndef ENABLE_SECUREBOOT
  DEFINE ENABLE_SECUREBOOT       = 0
!endif
!ifndef A04BR3_EXPERIMENTAL_THREE_CPU
  DEFINE A04BR3_EXPERIMENTAL_THREE_CPU = 0
!endif
!if $(A04BR3_EXPERIMENTAL_THREE_CPU) != 0 && $(A04BR3_EXPERIMENTAL_THREE_CPU) != 1
  !error A04BR3_EXPERIMENTAL_THREE_CPU must be 0 or 1
!endif
!ifndef A04BR3_EXPERIMENTAL_WORKER_HEARTBEAT
  DEFINE A04BR3_EXPERIMENTAL_WORKER_HEARTBEAT = 0
!endif
!if $(A04BR3_EXPERIMENTAL_WORKER_HEARTBEAT) != 0 && $(A04BR3_EXPERIMENTAL_WORKER_HEARTBEAT) != 1
  !error A04BR3_EXPERIMENTAL_WORKER_HEARTBEAT must be 0 or 1
!endif
!if $(A04BR3_EXPERIMENTAL_WORKER_HEARTBEAT) == 1 && $(A04BR3_EXPERIMENTAL_THREE_CPU) != 1
  !error Worker heartbeat requires the experimental three CPU driver
!endif
!ifndef A04BR3_CACHE_TEST
  DEFINE A04BR3_CACHE_TEST = 0
!endif
!ifndef A04BR3_BOUNDED_RECOVERY
  DEFINE A04BR3_BOUNDED_RECOVERY = 0
!endif
!ifndef A04BR3_BOOT_DIAGNOSTIC
  DEFINE A04BR3_BOOT_DIAGNOSTIC = 0
!endif
!if $(A04BR3_BOOT_DIAGNOSTIC) != 0 && $(A04BR3_BOOT_DIAGNOSTIC) != 1
  !error A04BR3_BOOT_DIAGNOSTIC must be 0 or 1
!endif
!if $(A04BR3_BOOT_DIAGNOSTIC) == 1 && ($(A04BR3_EXPERIMENTAL_THREE_CPU) != 1 || $(A04BR3_EXPERIMENTAL_WORKER_HEARTBEAT) != 1)
  !error Boot diagnostic requires the three CPU worker heartbeat
!endif

# Canonical ARM32 has no legacy MOU0003/MOU0004 table variants.  Reject
# their explicit selection instead of silently publishing overlapping owners.
!ifndef A04BR3_BATTERY_PROVIDER
  DEFINE A04BR3_BATTERY_PROVIDER = 0
!endif
!if $(A04BR3_BATTERY_PROVIDER) != 0 && $(A04BR3_BATTERY_PROVIDER) != 1
  !error A04BR3_BATTERY_PROVIDER must be 0 or 1
!endif
!ifndef A04BR3_SHARED_PARENT
  DEFINE A04BR3_SHARED_PARENT = 0
!endif
!ifndef A04BR3_BUTTON_PROVIDER
  DEFINE A04BR3_BUTTON_PROVIDER = 0
!endif
!ifndef A04BR3_TOUCH_PROVIDER
  DEFINE A04BR3_TOUCH_PROVIDER = 0
!endif
!if $(A04BR3_SHARED_PARENT) != 0 && $(A04BR3_SHARED_PARENT) != 1
  !error A04BR3_SHARED_PARENT must be 0 or 1
!endif
!if $(A04BR3_BUTTON_PROVIDER) != 0 || $(A04BR3_TOUCH_PROVIDER) != 0
  !error canonical ARM32 has no legacy input provider; shared resources must remain exclusive
!endif

  #
  # Unattended test cycle (UniXNU board.toml [firmware.defines]
  # A04BR3_TEST_CYCLE = "1"): from SEC on (A04BR3PlatformSecBoardLib) every
  # reset returns to the stock LK fastboot mode and the watchdog is armed;
  # A04BR3TestCycleDxe kicks it until ExitBootServices (at most 120 s), so a
  # firmware or kernel that stops anywhere still returns to fastboot. Off by default;
  # release images must not include it.
  #
!ifndef A04BR3_TEST_CYCLE
  DEFINE A04BR3_TEST_CYCLE       = 0
!endif

# Build-time exclusion only: this is not proof that every reset source is off.
!ifndef A04BR3_DISK_SERVICING
  DEFINE A04BR3_DISK_SERVICING = 0
!endif
!if $(A04BR3_DISK_SERVICING) != 0 && $(A04BR3_DISK_SERVICING) != 1
  !error A04BR3_DISK_SERVICING must be 0 or 1
!endif
!if $(A04BR3_DISK_SERVICING) == 1 && $(A04BR3_EXPERIMENTAL_THREE_CPU) == 1
  !error The experimental CPU worker permits UART reset and is not safe for disk servicing
!endif
!if $(A04BR3_DISK_SERVICING) == 1 && $(A04BR3_TEST_CYCLE) != 0
  !error A04BR3_DISK_SERVICING forbids A04BR3_TEST_CYCLE
!endif

!ifndef A04BR3_CHARGER_OWNER
  DEFINE A04BR3_CHARGER_OWNER = 0
!endif
!if $(A04BR3_CHARGER_OWNER) != 0 && $(A04BR3_CHARGER_OWNER) != 1
  !error A04BR3_CHARGER_OWNER must be 0 or 1
!endif
# Both paths program I2C2 at ExitBootServices and have no common owner.
!if $(A04BR3_CHARGER_OWNER) == 1 && $(A04BR3_BATTERY_PROVIDER) == 1
  !error The charger owner and battery provider cannot share I2C2
!endif

!ifndef A04BR3_EARLY_FAULT
  DEFINE A04BR3_EARLY_FAULT      = 0
!endif
!if $(A04BR3_EARLY_FAULT) != 0 && $(A04BR3_TEST_CYCLE) != 1
  !error A04BR3_EARLY_FAULT requires A04BR3_TEST_CYCLE=1
!endif
!if $(A04BR3_EARLY_FAULT) != 0 && $(A04BR3_EARLY_FAULT) != 1 && $(A04BR3_EARLY_FAULT) != 2
  !error A04BR3_EARLY_FAULT must be 0, 1 (SEC), or 2 (DXE)
!endif

!ifndef A04BR3_CPU_STAGE_DIAG
  DEFINE A04BR3_CPU_STAGE_DIAG = 0
!endif
!if $(A04BR3_CPU_STAGE_DIAG) != 0 && $(A04BR3_CPU_STAGE_DIAG) != 1
  !error A04BR3_CPU_STAGE_DIAG must be 0 or 1
!endif
!if $(A04BR3_CPU_STAGE_DIAG) == 1 && $(A04BR3_TEST_CYCLE) != 1
  !error A04BR3_CPU_STAGE_DIAG requires A04BR3_TEST_CYCLE=1
!endif
!if $(A04BR3_CPU_STAGE_DIAG) == 1 && $(TARGET) != DEBUG
  !error A04BR3_CPU_STAGE_DIAG requires TARGET=DEBUG
!endif

  #
  # eMMC HS200 (EmmcBlockIoDxe): A04BR3_EMMC_HS200=1 in the build environment
  # switches the eMMC from High Speed (47 MHz) to HS200 (188 MHz) with CMD21
  # tuning after the High Speed checks, and returns to High Speed when tuning
  # or verification fails. Off by default.
  #
  # Boot Ramdisk (MemoryMapLib): A04BR3_RAMDISK_RECLAIMABLE=1 describes the payload FAT as boot services data instead of
  # reserved memory: still never allocated by UEFI, but memory the OS may take after ExitBootServices (DarwinBoot
  # darwinboot-memory-floor). Default 0 keeps it reserved.
!ifndef A04BR3_RAMDISK_RECLAIMABLE
  DEFINE A04BR3_RAMDISK_RECLAIMABLE = 0
!endif
!if $(A04BR3_RAMDISK_RECLAIMABLE) != 0 && $(A04BR3_RAMDISK_RECLAIMABLE) != 1
  !error A04BR3_RAMDISK_RECLAIMABLE must be 0 or 1
!endif
!ifndef A04BR3_EMMC_HS200
  DEFINE A04BR3_EMMC_HS200 = 0
!endif
!if $(A04BR3_EMMC_HS200) != 0 && $(A04BR3_EMMC_HS200) != 1
  !error A04BR3_EMMC_HS200 must be 0 or 1
!endif

  #
  # CPU 1300 MHz (A04BR3PlatformSecBoardLib): A04BR3_CPU_1300=1 in the build
  # environment raises VPROC to 1.30 V and then ARMPLL from the LK 1001 MHz to
  # the stock maximum OPP 1300 MHz once in SEC, after the recovery watchdog and
  # fastboot flag. Without A04BR3_TEST_CYCLE it arms the watchdog only around
  # the switch and stops it again. Off by default.
  #
!ifndef A04BR3_CPU_1300
  DEFINE A04BR3_CPU_1300 = 0
!endif
!if $(A04BR3_DISK_SERVICING) == 1 && $(A04BR3_CPU_1300) != 0
  !error A04BR3_DISK_SERVICING forbids A04BR3_CPU_1300
!endif
!if $(A04BR3_CPU_1300) != 0 && $(A04BR3_CPU_1300) != 1
  !error A04BR3_CPU_1300 must be 0 or 1
!endif
  # Test only: stop the CPU 1300 MHz switch after the VPROC raise with the re-entry guard set (RTC_SPAR0 bit 12).
!ifndef A04BR3_CPU_1300_FAULT
  DEFINE A04BR3_CPU_1300_FAULT = 0
!endif
!if $(A04BR3_CPU_1300_FAULT) != 0 && ($(A04BR3_CPU_1300) != 1 || $(A04BR3_TEST_CYCLE) != 1)
  !error A04BR3_CPU_1300_FAULT requires A04BR3_CPU_1300=1 and A04BR3_TEST_CYCLE=1
!endif

DEFINE DARWINBOOT_BOARD_HANDOFF = $(A04BR3_CHARGER_OWNER)

!include a04br3Pkg/WindowsArm32/WindowsMT8167.dsc.inc

[BuildOptions]
  # Board-only smoke: 1 halts after SEC RTC verification; 2 halts with DXE timer live.
  GCC:*_*_AARCH64_CC_FLAGS = -DA04BR3_EARLY_FAULT=$(A04BR3_EARLY_FAULT) -DA04BR3_CPU_STAGE_DIAG=$(A04BR3_CPU_STAGE_DIAG) -DA04BR3_EMMC_HS200=$(A04BR3_EMMC_HS200) -DA04BR3_CPU_1300=$(A04BR3_CPU_1300) -DA04BR3_TEST_CYCLE=$(A04BR3_TEST_CYCLE) -DA04BR3_CPU_1300_FAULT=$(A04BR3_CPU_1300_FAULT) -DA04BR3_RAMDISK_RECLAIMABLE=$(A04BR3_RAMDISK_RECLAIMABLE)

[PcdsFixedAtBuild]
  # This platform does not ship FormBrowser2; disable the optional health UI.
  gEfiMdeModulePkgTokenSpaceGuid.PcdDriverHealthConfigureForm|{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}
  #
  # DDR Memory: 2 GiB (rank0 + rank1) at 0x40000000 as reported by A04BR3 LK
  # (edk2-mtk A04BR3 PcdSystemMemorySize 0x7F9C0000 plus the 0x640000 MBlock
  # gap at the top). See MemoryMapLib.
  #
  gArmTokenSpaceGuid.PcdSystemMemoryBase|0x40000000

  #
  # UEFI Stack (see MemoryMapLib)
  #
  gArmPlatformTokenSpaceGuid.PcdCPUCoresStackBase|0x4E200000
  gArmPlatformTokenSpaceGuid.PcdCPUCorePrimaryStackSize|0x40000

  #
  # SMBIOS
  #
  gSiliciumPkgTokenSpaceGuid.PcdSmbiosSystemManufacturer|"Benesse"
  gSiliciumPkgTokenSpaceGuid.PcdSmbiosSystemModel|"TAB-A04-BR3"
  gSiliciumPkgTokenSpaceGuid.PcdSmbiosSystemRetailModel|"a04br3"
  gSiliciumPkgTokenSpaceGuid.PcdSmbiosSystemRetailSku|"TAB-A04-BR3"
  gSiliciumPkgTokenSpaceGuid.PcdSmbiosSystemBoardModel|"TAB-A04-BR3"

  #
  # Simple Frame Buffer: 800x1280 AUO WXGA panel, 32 bpp, pitch 3200 bytes
  # (edk2-mtk A04BR3 DisplayInitDxe OVL layer pitch 0x0C80). The surface is
  # the "Display Reserved" region at 0xBE9C0000.
  #
  gSiliciumPkgTokenSpaceGuid.PcdFrameBufferWidth|800
  gSiliciumPkgTokenSpaceGuid.PcdFrameBufferHeight|1280
  gSiliciumPkgTokenSpaceGuid.PcdFrameBufferColorDepth|32

  #
  # Storage
  #
  gMediaTekPkgTokenSpaceGuid.PcdStorageIsEMMC|TRUE

  #
  # UART0 at 921600 baud, 8N1: the stock A04BR3 kernel console rate kept by
  # edk2-mtk A04BR3.
  #
  gEfiMdePkgTokenSpaceGuid.PcdUartDefaultBaudRate|921600
  gEfiMdePkgTokenSpaceGuid.PcdUartDefaultDataBits|8
  gEfiMdePkgTokenSpaceGuid.PcdUartDefaultParity|1
  gEfiMdePkgTokenSpaceGuid.PcdUartDefaultStopBits|1

  #
  # Device Tree: publish the built-in A04BR3 tree (no ACPI tables).
  #
  gEmbeddedTokenSpaceGuid.PcdDefaultDtPref|FALSE

[LibraryClasses]
  #
  # Memory Libraries
  #


  #
  # Input Libraries
  #
  KeypadDeviceLib|a04br3Pkg/Library/KeypadDeviceLib/KeypadDeviceLib.inf
  A04BR3TestCycleLib|a04br3Pkg/Library/A04BR3TestCycleLib/A04BR3TestCycleLib.inf

!if $(A04BR3_TEST_CYCLE) == 1 || $(A04BR3_CPU_1300) == 1
  # Unattended test cycle from SEC on: LK fastboot flag and 31 s watchdog
  # (replaces the empty MT8167Pkg PlatformSecBoardLibNull). Without the test
  # cycle, A04BR3_CPU_1300=1 alone maps it for the CPU switch only.
  PlatformSecBoardLib|a04br3Pkg/Library/A04BR3PlatformSecBoardLib/A04BR3PlatformSecBoardLib.inf
!endif

  #
  # UniXNU Libraries
  #


[Components]
!if $(A04BR3_CHARGER_OWNER) == 1
  a04br3Pkg/Drivers/A04BR3ChargerDxe/A04BR3ChargerDxe.inf
!endif
  #
  # Board Drivers (ported from edk2-mtk A04BR3Pkg)
  #
  a04br3Pkg/Drivers/DisplayInitDxe/DisplayInitDxe.inf
  a04br3Pkg/Drivers/EmmcBlockIoDxe/EmmcBlockIoDxe.inf {
    <BuildOptions>
      GCC:*_*_ARM_CC_FLAGS = -DA04BR3_IO_PROGRESS=$(A04BR3_BOUNDED_RECOVERY)
  }
  a04br3Pkg/Drivers/SdCardBlockIoDxe/SdCardBlockIoDxe.inf

  #
  # Boot image ramdisk (UniXNU payload volume)
  #


!if $(A04BR3_TEST_CYCLE) == 1
  #
  # Unattended test cycle (fastboot return and watchdog from DXE entry on)
  #
  a04br3Pkg/Drivers/A04BR3TestCycleDxe/A04BR3TestCycleDxe.inf
!endif

  #
  # Serial I/O
  #
  MdeModulePkg/Universal/SerialDxe/SerialDxe.inf

  #
  # Input
  #
  SiliciumPkg/Drivers/KeypadDxe/KeypadDxe.inf
  SiliciumPkg/Drivers/KeypadDeviceDxe/KeypadDeviceDxe.inf

  #
  # UniXNU
  #


# Experimental ARM32 integration. Not a validated servicing build.
!include a04br3Pkg/WindowsArm32/Arm32Memory.dsc.inc

[Components.ARM]
  a04br3Pkg/WindowsArm32/Drivers/CapsuleUnsupportedDxe/CapsuleUnsupportedDxe.inf
  a04br3Pkg/WindowsArm32/Drivers/ArmGicDxe/ArmGicDxe.inf {
    <LibraryClasses>
      NULL|a04br3Pkg/WindowsArm32/Library/GicV3Compat/GicV3Compat.inf
  }
  a04br3Pkg/WindowsArm32/PrePi/PrePi.inf {
    <LibraryClasses>
      NULL|MdeModulePkg/Library/LzmaCustomDecompressLib/LzmaCustomDecompressLib.inf
      NULL|a04br3Pkg/WindowsArm32/Library/EarlyStackGuard/EarlyStackGuard.inf
    <BuildOptions>
      GCC:*_*_ARM_CC_FLAGS = -DA04BR3_BOOT_DIAGNOSTIC=$(A04BR3_BOOT_DIAGNOSTIC) -DA04BR3_BOUNDED_RECOVERY=$(A04BR3_BOUNDED_RECOVERY) -DA04BR3_CACHE_TEST=$(A04BR3_CACHE_TEST)
  }
!if $(A04BR3_SHARED_PARENT) == 1
!if $(A04BR3_BATTERY_PROVIDER) == 1
  a04br3Pkg/WindowsArm32/Acpi/AcpiTablesSharedBattery.inf
!else
  a04br3Pkg/WindowsArm32/Acpi/AcpiTablesShared.inf
!endif
!else
!if $(A04BR3_BATTERY_PROVIDER) == 1
  a04br3Pkg/WindowsArm32/Acpi/AcpiTablesBattery.inf
!else
  a04br3Pkg/WindowsArm32/Acpi/AcpiTables.inf
!endif
!endif
  a04br3Pkg/Drivers/I2c1HandoffDxe/I2c1HandoffDxe.inf
!if $(A04BR3_BATTERY_PROVIDER) == 1
  a04br3Pkg/WindowsArm32/Drivers/I2c2HandoffDxe/I2c2HandoffDxe.inf
!endif
!if $(A04BR3_EXPERIMENTAL_THREE_CPU) == 1
  a04br3Pkg/WindowsArm32/Drivers/ThreeCpuDxe/A04BR3BootTraceDxe.inf {
    <BuildOptions>
      GCC:*_*_ARM_CC_FLAGS = -DA04BR3_A64_CPU_TEST=1 -DA04BR3_A64_WDT_TEST=1 -DA04BR3_A32_SECONDARY_TEST=1 -DA04BR3_PARK_TEST=1 -DA04BR3_SMP_TEST=1 -DA04BR3_KERNEL_MARKER=1 -DA04BR3_FB_MONITOR=1 -DA04BR3_FOUR_CPU_TEST=0 -DA04BR3_EXPERIMENTAL_WORKER_HEARTBEAT=$(A04BR3_EXPERIMENTAL_WORKER_HEARTBEAT) -DA04BR3_BOOT_DIAGNOSTIC=$(A04BR3_BOOT_DIAGNOSTIC) -DA04BR3_BOUNDED_RECOVERY=$(A04BR3_BOUNDED_RECOVERY) -DA04BR3_CACHE_TEST=$(A04BR3_CACHE_TEST)
  }
!endif
  MdeModulePkg/Universal/Acpi/AcpiTableDxe/AcpiTableDxe.inf
  MdeModulePkg/Universal/Acpi/AcpiPlatformDxe/AcpiPlatformDxe.inf

[PcdsFeatureFlag.ARM]
  gEfiMdeModulePkgTokenSpaceGuid.PcdInstallAcpiSdtProtocol|TRUE
