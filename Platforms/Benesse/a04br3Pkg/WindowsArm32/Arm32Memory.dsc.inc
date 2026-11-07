# Candidate Windows-only layout; not used by the production AArch64 DSC.
# The stack occupies the top 256 KiB of the 0x03dc0000 HOB arena.
[PcdsFixedAtBuild]
  gA04BR3WindowsTokenSpaceGuid.PcdA04WinHobBase|0x4E240000
  gA04BR3WindowsTokenSpaceGuid.PcdA04WinHobSize|0x03DC0000
  gEmbeddedTokenSpaceGuid.PcdPrePiStackBase|0x51FC0000
  gEmbeddedTokenSpaceGuid.PcdPrePiStackSize|0x00040000

[LibraryClasses]
!if $(A04BR3_CAMERA_HANDOFF) == 1
  MemoryMapLib|a04br3Pkg/WindowsArm32/Library/MemoryMapLib/MemoryMapCameraLib.inf
!else
  MemoryMapLib|a04br3Pkg/WindowsArm32/Library/MemoryMapLib/MemoryMapLib.inf
!endif
