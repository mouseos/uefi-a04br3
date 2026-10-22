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
  OUTPUT_DIRECTORY               = Build/a04br3Pkg
  SUPPORTED_ARCHITECTURES        = AARCH64
  BUILD_TARGETS                  = RELEASE|DEBUG
  SKUID_IDENTIFIER               = DEFAULT
  FLASH_DEFINITION               = a04br3Pkg/a04br3.fdf
  USE_CUSTOM_DISPLAY_DRIVER      = 0

  #
  # The UniXNU firmware driver passes only the defines of its board file;
  # default the ones build_uefi.py always passes.
  #
!ifndef ENABLE_SECUREBOOT
  DEFINE ENABLE_SECUREBOOT       = 0
!endif

  #
  # Unattended test cycle (UniXNU board.toml [firmware.defines]
  # A04BR3_TEST_CYCLE = "1"): from early DXE on, A04BR3TestCycleDxe makes
  # every reset return to the stock LK fastboot mode and keeps the watchdog
  # armed (kicked until ExitBootServices, at most 120 s), so a firmware or
  # kernel that stops anywhere still returns to fastboot. Off by default;
  # release images must not include it.
  #
!ifndef A04BR3_TEST_CYCLE
  DEFINE A04BR3_TEST_CYCLE       = 0
!endif

!include MT8167Pkg/MT8167Pkg.dsc.inc

[PcdsFixedAtBuild]
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
  gEmbeddedTokenSpaceGuid.PcdDefaultDtPref|TRUE

[LibraryClasses]
  #
  # Memory Libraries
  #
  MemoryMapLib|a04br3Pkg/Library/MemoryMapLib/MemoryMapLib.inf

  #
  # Input Libraries
  #
  KeypadDeviceLib|a04br3Pkg/Library/KeypadDeviceLib/KeypadDeviceLib.inf

  #
  # UniXNU Libraries
  #
  UniXNULib|UniXNUPkg/Library/UniXNULib/UniXNULib.inf

[Components]
  #
  # Board Drivers (ported from edk2-mtk A04BR3Pkg)
  #
  a04br3Pkg/Drivers/DisplayInitDxe/DisplayInitDxe.inf
  a04br3Pkg/Drivers/EmmcBlockIoDxe/EmmcBlockIoDxe.inf
  a04br3Pkg/Drivers/SdCardBlockIoDxe/SdCardBlockIoDxe.inf

  #
  # Boot image ramdisk (UniXNU payload volume)
  #
  a04br3Pkg/Drivers/BootRamdiskDxe/BootRamdiskDxe.inf

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
  UniXNUPkg/Application/UniXNU/UniXNU.inf
  UniXNUPkg/Driver/UniXNUDxe/UniXNUDxe.inf
  DarwinBootPkg/Driver/DarwinBoot.inf
