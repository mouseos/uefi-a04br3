##
#  Copyright (c) 2011-2015, ARM Limited. All rights reserved.
#  Copyright (c) 2014, Linaro Limited. All rights reserved.
#  Copyright (c) 2015 - 2016, Intel Corporation. All rights reserved.
#  Copyright (c) 2018 - 2019, Bingxing Wang. All rights reserved.
#
#  SPDX-License-Identifier: BSD-2-Clause-Patent
##

!include a04br3Pkg/WindowsArm32/WindowsSilicium.dsc.inc

[PcdsFixedAtBuild]
  #
  # ACPI
  #
  gEfiMdeModulePkgTokenSpaceGuid.PcdAcpiDefaultOemId|"MTK   "

[LibraryClasses]
  #
  # ARM Libraries
  #
  ArmSmcLib|a04br3Pkg/WindowsArm32/Library/ArmSmcLib/ArmSmcLib.inf

  #
  # Memory Libraries
  #
  RamManagerLib|MediaTekPkg/Library/RamManagerLib/RamManagerLib.inf

[Components]
  #
  # Custom Drivers
  #
  MediaTekPkg/Drivers/PmicWrapperDxe/PmicWrapperDxe.inf
