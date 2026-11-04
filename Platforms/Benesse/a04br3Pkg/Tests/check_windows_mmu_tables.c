#define _GNU_SOURCE
#include <sys/mman.h>
#include <string.h>
#include <stdio.h>
#include <Uefi.h>
#include "../WindowsArm32/Library/ArmMmuLib/Arm/ArmMmuLibUpdate.c"
#include "../WindowsArm32/Library/ArmMmuLib/Arm/ArmMmuLibConvert.c"
static UINT32 l1[4096];
static unsigned flushes, updates;
VOID *ArmGetTTBR0BaseAddress(void) { return l1; }
VOID ArmUpdateTranslationTableEntry(VOID *p, VOID *v) {(void)p;(void)v;updates++;}
VOID ArmInvalidateTlb(void) {flushes++;}
VOID *AllocatePages(UINTN n) { void *p=mmap(NULL,n*4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_32BIT,-1,0); return p==MAP_FAILED ? NULL : p; }
int main(void) {
  UINT32 *l2=mmap(NULL,4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_32BIT,-1,0);
  assert(l2 != MAP_FAILED && (uintptr_t)l2 <= UINT32_MAX);
  memset(l1,0,sizeof(l1)); memset(l2,0,4096);
  l1[1]=(UINT32)(uintptr_t)l2 | TT_DESCRIPTOR_SECTION_TYPE_PAGE_TABLE;
  l2[0]=0x00100000 | TT_DESCRIPTOR_PAGE_TYPE_PAGE | TT_DESCRIPTOR_PAGE_AP_RO_RO | TT_DESCRIPTOR_PAGE_CACHE_POLICY_WRITE_BACK_ALLOC;
  l2[1]=0x00101000 | TT_DESCRIPTOR_PAGE_TYPE_PAGE_XN | TT_DESCRIPTOR_PAGE_AP_RW_RW | TT_DESCRIPTOR_PAGE_CACHE_POLICY_WRITE_BACK_ALLOC;
  UINT32 old0=l2[0],old1=l2[1];
  assert(ArmSetMemoryAttributes(0x100000,4096,EFI_MEMORY_XP,EFI_MEMORY_XP)==EFI_SUCCESS);
  assert(l2[0] == (old0 | TT_DESCRIPTOR_PAGE_XN_MASK));
  assert(l2[1] == old1);
  old0=l2[0];
  assert(ArmSetMemoryAttributes(0x100000,4096,0,EFI_MEMORY_RO)==EFI_SUCCESS);
  assert((l2[0] & TT_DESCRIPTOR_PAGE_XN_MASK) == (old0 & TT_DESCRIPTOR_PAGE_XN_MASK));
  assert((l2[0] & TT_DESCRIPTOR_PAGE_CACHE_POLICY_MASK)==(old0 & TT_DESCRIPTOR_PAGE_CACHE_POLICY_MASK));
  assert((l2[0] & TT_DESCRIPTOR_PAGE_AP_MASK)==TT_DESCRIPTOR_PAGE_AP_RW_RW);
  old0=l2[0]; old1=l2[1];
  assert(ArmSetMemoryAttributes(0x100000,8192,EFI_MEMORY_RP,EFI_MEMORY_RP)==EFI_UNSUPPORTED);
  assert(l2[0]==old0 && l2[1]==old1);
  assert(ArmSetMemoryAttributes(0x100000,12288,EFI_MEMORY_XP,EFI_MEMORY_XP)==EFI_NO_MAPPING);
  assert(l2[0]==old0 && l2[1]==old1);
  l1[2]=0x00200000 | TT_DESCRIPTOR_SECTION_TYPE_SECTION | TT_DESCRIPTOR_SECTION_AP_RO_RO | TT_DESCRIPTOR_SECTION_CACHE_POLICY_WRITE_BACK_ALLOC;
  assert(ArmSetMemoryAttributes(0x200000,4096,EFI_MEMORY_XP,EFI_MEMORY_XP)==EFI_SUCCESS);
  assert(TT_DESCRIPTOR_SECTION_TYPE_IS_PAGE_TABLE(l1[2]));
  UINT32 *split=(UINT32 *)(uintptr_t)TT_DESCRIPTOR_PAGE_BASE_ADDRESS(l1[2]);
  assert((split[0] & TT_DESCRIPTOR_PAGE_XN_MASK) != 0);
  assert((split[1] & TT_DESCRIPTOR_PAGE_XN_MASK) == 0);
  assert((split[0] & TT_DESCRIPTOR_PAGE_AP_MASK)==TT_DESCRIPTOR_PAGE_AP_RO_RO);
  assert((split[1] & TT_DESCRIPTOR_PAGE_AP_MASK)==TT_DESCRIPTOR_PAGE_AP_RO_RO);
  assert(updates >= 3 && flushes >= 1);
  puts("actual ARM32 page table update and no-mapping preflight passed (host mock)");
}
