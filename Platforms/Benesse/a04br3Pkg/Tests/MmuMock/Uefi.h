#pragma once
#include <stdint.h>
#include <stddef.h>
#include <assert.h>
typedef uint8_t UINT8; typedef uint32_t UINT32; typedef uint64_t UINT64;
typedef uintptr_t UINTN; typedef uint64_t EFI_PHYSICAL_ADDRESS; typedef int EFI_STATUS; typedef int BOOLEAN;
typedef void VOID; typedef UINT32 ARM_FIRST_LEVEL_DESCRIPTOR; typedef UINT32 ARM_PAGE_TABLE_ENTRY;
#define STATIC static
#define IN
#define OUT
#define OPTIONAL
#define TRUE 1
#define FALSE 0
#define EFI_SUCCESS 0
#define EFI_INVALID_PARAMETER 2
#define EFI_UNSUPPORTED 3
#define EFI_OUT_OF_RESOURCES 9
#define EFI_NO_MAPPING 17
#define EFI_ERROR(x) ((x)!=0)
#define EFI_MEMORY_UC 1ULL
#define EFI_MEMORY_WC 2ULL
#define EFI_MEMORY_WT 4ULL
#define EFI_MEMORY_WB 8ULL
#define EFI_MEMORY_UCE 0x10ULL
#define EFI_MEMORY_WP 0x1000ULL
#define EFI_MEMORY_RP 0x2000ULL
#define EFI_MEMORY_XP 0x4000ULL
#define EFI_MEMORY_RO 0x20000ULL
#define MAX_ADDRESS UINT32_MAX
#define MIN(a,b) ((a)<(b)?(a):(b))
#define ASSERT(x) assert(x)
#define DEBUG(x) ((void)0)
#define DEBUG_PAGE 0
VOID *ArmGetTTBR0BaseAddress(void);
VOID ArmUpdateTranslationTableEntry(VOID *, VOID *);
VOID ArmInvalidateTlb(void);
VOID *AllocatePages(UINTN);
UINT32 ConvertSectionAttributesToPageAttributes(UINT32, BOOLEAN);
