##
#  MediaTek MT8167 SoC configuration for Mu-Silicium.
#
#  Values come from the working A04BR3 edk2 port (mouseos/edk2-mtk,
#  Silicon/MediaTek/MT8167Pkg/MT8167.dsc and Platform/Benesse/A04BR3Pkg,
#  commit 1982d888c765) and from the A04BR3 device tree
#  (interrupt-controller@10310000, timer node).
#
#  SPDX-License-Identifier: BSD-2-Clause-Patent
##

[Defines]
  #
  # Timer: edk2-mtk MT8167.dsc uses the physical generic timer.
  #
  USE_PHYSICAL_TIMER                         = 1

!include MediaTekPkg/MediaTekPkg.dsc.inc

[BuildOptions]
  # Cortex-A35 is ARMv8.0-A: no ARMv8.2 extensions.
  *_*_AARCH64_CC_FLAGS = -march=armv8-a+crc
  # MediaTekPkg MsdcDxe (built from MediaTekPkg.dsc.inc, not placed in the
  # A04BR3 FV) has set-but-unused variables that clang 18 rejects with -Werror.
  *_CLANGPDB_AARCH64_CC_FLAGS = -Wno-unused-but-set-variable

[PcdsFixedAtBuild]
  #
  # ACPI (no ACPI tables are shipped; OEM fields only)
  #
  gEfiMdeModulePkgTokenSpaceGuid.PcdAcpiDefaultOemTableId|0x2020373631385443
  gEfiMdeModulePkgTokenSpaceGuid.PcdAcpiDefaultOemRevision|0x00000850

  #
  # CPU: 4x Cortex-A35 in one cluster (edk2-mtk MT8167.dsc).
  #
  gArmPlatformTokenSpaceGuid.PcdCoreCount|4
  gArmPlatformTokenSpaceGuid.PcdClusterCount|1

  #
  # Generic timer: 13 MHz, PPIs 29/30/27/26 (edk2-mtk MT8167.dsc; A04BR3 DT
  # timer "clock-frequency = <13000000>").
  #
  gSiliciumPkgTokenSpaceGuid.PcdTimerFreqOverwrite|13000000
  gArmTokenSpaceGuid.PcdArmArchTimerSecIntrNum|29
  gArmTokenSpaceGuid.PcdArmArchTimerIntrNum|30
  gArmTokenSpaceGuid.PcdArmArchTimerVirtIntrNum|27
  gArmTokenSpaceGuid.PcdArmArchTimerHypIntrNum|26

  #
  # GIC-400 (GICv2). Distributor 0x10310000 (A04BR3 DT
  # interrupt-controller@10310000). CPU interface 0x10320000 as used by the
  # working edk2-mtk MT8167.dsc; the DT lists the 64 KiB aliased window at
  # 0x1032F000, whose pages alias 0x10320000.
  #
  gArmTokenSpaceGuid.PcdGicDistributorBase|0x10310000
  gArmTokenSpaceGuid.PcdGicInterruptInterfaceBase|0x10320000
  gArmTokenSpaceGuid.PcdGicRedistributorsBase|0x0

  #
  # SMBIOS
  #
  gSiliciumPkgTokenSpaceGuid.PcdSmbiosProcessorModel|"MediaTek MT8167"
  gSiliciumPkgTokenSpaceGuid.PcdSmbiosProcessorPartNumber|"MT8167"

[LibraryClasses]
  #
  # SEC Libraries
  #
  PlatformSecLib|MT8167Pkg/Library/PlatformSecLib/PlatformSecLib.inf
  PlatformSecBoardLib|MT8167Pkg/Library/PlatformSecBoardLibNull/PlatformSecBoardLibNull.inf

  #
  # Serial: MT8167 UART0 (16550 compatible, 32-bit register spacing). Used for
  # DEBUG output, SerialDxe (EFI_SERIAL_IO_PROTOCOL) and the serial console in
  # DEBUG and RELEASE builds.
  #
  SerialPortLib|MT8167Pkg/Library/UartSerialPortLib/UartSerialPortLib.inf

  #
  # Custom Libraries (needed to build the MediaTekPkg components)
  #
  GpioImplLib|MT8167Pkg/Library/GpioImplLib/GpioImplLib.inf
  ClockImplLib|MT8167Pkg/Library/ClockImplLib/ClockImplLib.inf
  PmicWrapperImplLib|MT8167Pkg/Library/PmicWrapperImplLib/PmicWrapperImplLib.inf
  MsdcImplLib|MT8167Pkg/Library/MsdcImplLib/MsdcImplLib.inf
