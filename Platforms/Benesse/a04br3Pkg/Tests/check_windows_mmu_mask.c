/* Host test of the same request validator and descriptor-mask filter used by MMU. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#define STATIC static
#define VOID void
typedef uint64_t UINT64; typedef uint32_t UINT32; typedef int EFI_STATUS;
#define MAX_ADDRESS UINT32_MAX
#define EFI_SUCCESS 0
#define EFI_INVALID_PARAMETER 2
#define EFI_UNSUPPORTED 3
#define EFI_MEMORY_UC 1ULL
#define EFI_MEMORY_WC 2ULL
#define EFI_MEMORY_WT 4ULL
#define EFI_MEMORY_WB 8ULL
#define EFI_MEMORY_RP 0x2000ULL
#define EFI_MEMORY_XP 0x4000ULL
#define EFI_MEMORY_RO 0x20000ULL
#include "../WindowsArm32/Library/ArmMmuLib/Arm/A04AttributeMask.h"
static UINT32 apply(UINT32 old, UINT32 value, UINT64 attrmask, UINT32 xn, UINT32 ro) {
  UINT32 mask = xn | ro | 2u;
  A04FilterDescriptorMask(attrmask,xn,ro,&mask,&value);
  return (old & ~mask) | value;
}
int main(void) {
  assert(A04ValidateAttributeRequest(0x1000,0x1000,EFI_MEMORY_XP,EFI_MEMORY_XP)==0);
  assert(A04ValidateAttributeRequest(0x1001,0x1000,0,0)==EFI_INVALID_PARAMETER);
  assert(A04ValidateAttributeRequest(0xfffff000,0x2000,0,0)==EFI_INVALID_PARAMETER);
  assert(A04ValidateAttributeRequest(0x1000,0x1000,EFI_MEMORY_XP,EFI_MEMORY_RO)==EFI_INVALID_PARAMETER);
  assert(A04ValidateAttributeRequest(0x1000,0x1000,EFI_MEMORY_RP,EFI_MEMORY_RP)==EFI_UNSUPPORTED);
  assert(A04ValidateAttributeRequest(0x1000,0x1000,EFI_MEMORY_WB|EFI_MEMORY_WT,0)==EFI_INVALID_PARAMETER);
  assert(A04ValidateAttributeRequest(0x1000,0x1000,EFI_MEMORY_WB,EFI_MEMORY_XP)==EFI_INVALID_PARAMETER);
  /* Real ARM short-descriptor fields: update XP while retaining RO, then reverse. */
  assert(apply(0x232,0x3,EFI_MEMORY_XP,1,0x230)==0x233);
  assert(apply(0x233,0x32,EFI_MEMORY_RO,1,0x230)==0x33);
  assert(apply(0x8832,0x12,EFI_MEMORY_XP,0x10,0x8c00)==0x8832);
  assert(apply(0x8832,0xc02,EFI_MEMORY_RO,0x10,0x8c00)==0xc32);
  puts("ARM32 descriptor permission masks and invalid requests checked");
}
