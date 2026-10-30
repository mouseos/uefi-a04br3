##
# Copyright (c) Microsoft Corporation.
# SPDX-License-Identifier: BSD-2-Clause-Patent
##

import datetime
import logging
import os
import uuid

from io import StringIO
from pathlib import Path

from edk2toolext.environment import shell_environment
from edk2toolext.environment.uefi_build import UefiBuilder
from edk2toolext.invocables.edk2_platform_build import BuildSettingsManager
from edk2toolext.invocables.edk2_pr_eval import PrEvalSettingsManager
from edk2toolext.invocables.edk2_setup import (RequiredSubmodule, SetupSettingsManager)
from edk2toolext.invocables.edk2_update import UpdateSettingsManager
from edk2toolext.invocables.edk2_parse import ParseSettingsManager
from edk2toollib.utility_functions import RunCmd

# ####################################################################################### #
#                                Common Configuration                                     #
# ####################################################################################### #
class CommonPlatform ():
    PackagesSupported = ("a04br3Pkg")
    ArchSupported = ("AARCH64")
    TargetsSupported = ("DEBUG", "RELEASE")
    Scopes = ('a04br3', 'gcc_aarch64_linux', 'edk2-build')
    WorkspaceRoot = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
    # UniXNU (https://github.com/UniXNU/UniXNU) supplies UniXNUPkg and
    # DarwinBootPkg. As in UniXNU/uefi-sheng, the checkout next to this
    # repository is used unless UNIXNU_ROOT names another one; its package and
    # drivers/DarwinBoot trees are the {tree}/package and
    # {tree}/drivers/DarwinBoot entries of the UniXNU [firmware] packages.
    UniXNURoot = os.environ.get("UNIXNU_ROOT", os.path.join(WorkspaceRoot, "..", "UniXNU"))
    PackagesPath = (
        os.path.join(UniXNURoot, "package"),
        os.path.join(UniXNURoot, "drivers", "DarwinBoot"),
        "Platforms/Benesse",
        "Common/Mu",
        "Common/Mu_OEM_Sample",
        "Mu_Basecore",
        "Silicon/MediaTek",
        "Silicon/Silicium"
    )

# ####################################################################################### #
#                         Configuration for Update & Setup                                #
# ####################################################################################### #
class SettingsManager (UpdateSettingsManager, SetupSettingsManager, PrEvalSettingsManager, ParseSettingsManager):

    def GetPackagesSupported (self):
        return CommonPlatform.PackagesSupported

    def GetArchitecturesSupported (self):
        return CommonPlatform.ArchSupported

    def GetTargetsSupported (self):
        return CommonPlatform.TargetsSupported

    def GetRequiredSubmodules (self):
        # No ACPI tables, no vendor binaries; OpenSSL only for secure boot.
        return [
            RequiredSubmodule ("Common/Mu", True),
            RequiredSubmodule ("Common/Mu_OEM_Sample", True),
            RequiredSubmodule ("Mu_Basecore", True)
        ]

    def SetArchitectures (self, list_of_requested_architectures):
        unsupported = set(list_of_requested_architectures) - set(self.GetArchitecturesSupported())

        if (len(unsupported) > 0):
            errorString = ("Unsupported Architecture Requested: " + " ".join(unsupported))
            logging.critical (errorString)
            raise Exception (errorString)

        self.ActualArchitectures = list_of_requested_architectures

    def GetWorkspaceRoot (self):
        return CommonPlatform.WorkspaceRoot

    def GetActiveScopes (self):
        return CommonPlatform.Scopes

    def FilterPackagesToTest (self, changedFilesList: list, potentialPackagesList: list) -> list:
        build_these_packages = []
        possible_packages = potentialPackagesList.copy ()

        for f in changedFilesList:
            if "BaseTools" in f:
                if os.path.splitext(f) not in [".txt", ".md"]:
                    build_these_packages = possible_packages
                    break

            if "platform-build-run-steps.yml" in f:
                build_these_packages = possible_packages
                break

        return build_these_packages

    def GetPlatformDscAndConfig (self) -> tuple:
        return ("a04br3Pkg/a04br3.dsc", {})

    def GetName (self):
        return "a04br3"

    def GetPackagesPath (self):
        return CommonPlatform.PackagesPath

# ####################################################################################### #
#                         Actual Configuration for Platform Build                         #
# ####################################################################################### #
class PlatformBuilder (UefiBuilder, BuildSettingsManager):
    def __init__ (self):
        UefiBuilder.__init__ (self)

    def AddCommandLineOptions (self, parserObj):
        parserObj.add_argument('-a', "--arch", dest="build_arch", type=str, default="AARCH64", help="Optional - CSV of architecture to build. AARCH64 is used for PEI and DXE and is the only valid option for this platform.")

    def RetrieveCommandLineOptions (self, args):
        if args.build_arch.upper() != "AARCH64":
            raise Exception("Invalid Arch Specified.  Please see comments in DeviceBuild.py::PlatformBuilder::AddCommandLineOptions")

    def GetWorkspaceRoot (self):
        return CommonPlatform.WorkspaceRoot

    def GetPackagesPath (self):
        result = [ shell_environment.GetBuildVars().GetValue("FEATURE_CONFIG_PATH", "") ]

        for a in CommonPlatform.PackagesPath:
            result.append(a)

        return result

    def GetActiveScopes (self):
        return CommonPlatform.Scopes

    def GetName (self):
        return "a04br3Pkg"

    def GetLoggingLevel (self, loggerType):
        return logging.INFO
        return super().GetLoggingLevel(loggerType)

    def SetPlatformEnv (self):
        logging.debug ("PlatformBuilder SetPlatformEnv")

        self.env.SetValue ("PRODUCT_NAME", "a04br3", "Platform Hardcoded")
        self.env.SetValue ("ACTIVE_PLATFORM", "a04br3Pkg/a04br3.dsc", "Platform Hardcoded")
        self.env.SetValue ("TARGET_ARCH", "AARCH64", "Platform Hardcoded")
        self.env.SetValue ("TOOL_CHAIN_TAG", "CLANGPDB", "set default to clangpdb")
        self.env.SetValue ("EMPTY_DRIVE", "FALSE", "Default to false")
        self.env.SetValue ("RUN_TESTS", "FALSE", "Default to false")
        self.env.SetValue ("SHUTDOWN_AFTER_RUN", "FALSE", "Default to false")
        self.env.SetValue ("BLD_*_BUILDID_STRING", "Unknown", "Default")
        self.env.SetValue ("BUILDREPORTING", "TRUE", "Enabling build report")
        self.env.SetValue ("BUILDREPORT_TYPES", "PCD DEPEX FLASH BUILD_FLAGS LIBRARY FIXED_ADDRESS HASH", "Setting build report types")
        self.env.SetValue ("BLD_*_MEMORY_PROTECTION", "TRUE", "Default")
        self.env.SetValue ("BLD_*_SHIP_MODE", "FALSE", "Default")
        self.env.SetValue ("BLD_*_ENABLE_SECUREBOOT", self.env.GetValue("ENABLE_SECUREBOOT"), "Default")
        self.env.SetValue ("BLD_*_FD_BASE", self.env.GetValue("FD_BASE"), "Default")
        self.env.SetValue ("BLD_*_FD_SIZE", self.env.GetValue("FD_SIZE"), "Default")
        self.env.SetValue ("BLD_*_FD_BLOCKS", self.env.GetValue("FD_BLOCKS"), "Default")
        # Unattended test cycle (A04BR3TestCycleDxe), off by default: A04BR3_TEST_CYCLE=1 on the stuart command line
        # or in the environment of build_uefi.py.
        self.env.SetValue ("BLD_*_A04BR3_TEST_CYCLE", self.env.GetValue("A04BR3_TEST_CYCLE", os.environ.get("A04BR3_TEST_CYCLE", "0")), "Default")
        self.env.SetValue ("BLD_*_A04BR3_CPU_STAGE_DIAG", self.env.GetValue("A04BR3_CPU_STAGE_DIAG", os.environ.get("A04BR3_CPU_STAGE_DIAG", "0")), "Default")
        # eMMC HS200 (SdCardBlockIoDxe EmmcSelectHs200), off by default: A04BR3_EMMC_HS200=1.
        emmc_hs200 = self.env.GetValue("A04BR3_EMMC_HS200", os.environ.get("A04BR3_EMMC_HS200", "0"))
        if emmc_hs200 not in ("0", "1"):
            raise ValueError("A04BR3_EMMC_HS200 must be 0 or 1")
        self.env.SetValue ("BLD_*_A04BR3_EMMC_HS200", emmc_hs200, "Default")
        # CPU 1300 MHz at 1.30 V once in SEC (A04BR3PlatformSecBoardLib), off by default: A04BR3_CPU_1300=1.
        cpu_1300 = self.env.GetValue("A04BR3_CPU_1300", os.environ.get("A04BR3_CPU_1300", "0"))
        if cpu_1300 not in ("0", "1"):
            raise ValueError("A04BR3_CPU_1300 must be 0 or 1")
        self.env.SetValue ("BLD_*_A04BR3_CPU_1300", cpu_1300, "Default")

        charger = self.env.GetValue("A04BR3_CHARGER_OWNER", os.environ.get("A04BR3_CHARGER_OWNER", "0"))
        if charger not in ("0", "1"):
            raise ValueError("A04BR3_CHARGER_OWNER must be 0 or 1")
        self.env.SetValue("BLD_*_A04BR3_CHARGER_OWNER", charger, "Explicit board opt-in")

        return 0

    def PlatformPreBuild (self):
        return 0

    def PlatformPostBuild (self):
        return 0

    def FlashRomImage (self):
        return 0

if __name__ == "__main__":
    import argparse
    import sys

    from edk2toolext.invocables.edk2_platform_build import Edk2PlatformBuild
    from edk2toolext.invocables.edk2_setup import Edk2PlatformSetup
    from edk2toolext.invocables.edk2_update import Edk2Update

    SCRIPT_PATH = os.path.relpath (__file__)

    parser = argparse.ArgumentParser (add_help=False)

    parse_group = parser.add_mutually_exclusive_group()

    parse_group.add_argument ("--update", "--UPDATE", action='store_true', help="Invokes stuart_update")
    parse_group.add_argument ("--setup",  "--SETUP",  action='store_true', help="Invokes stuart_setup")

    args, remaining = parser.parse_known_args()

    new_args = ["stuart", "-c", SCRIPT_PATH]
    new_args = new_args + remaining

    sys.argv = new_args

    if args.setup:
        Edk2PlatformSetup().Invoke()
    elif args.update:
        Edk2Update().Invoke()
    else:
        Edk2PlatformBuild().Invoke()
