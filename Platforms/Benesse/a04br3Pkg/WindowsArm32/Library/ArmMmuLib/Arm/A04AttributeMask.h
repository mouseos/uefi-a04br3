/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_ATTRIBUTE_MASK_H
#define A04_ATTRIBUTE_MASK_H
/* Short descriptors cannot retain independent RO state while RP is set. */
STATIC EFI_STATUS
A04ValidateAttributeRequest (UINT64 Base, UINT64 Length, UINT64 Attr, UINT64 Mask)
{
  UINT64 Cache = Attr & (EFI_MEMORY_UC | EFI_MEMORY_WC | EFI_MEMORY_WT | EFI_MEMORY_WB);
  UINT64 Perm = EFI_MEMORY_RO | EFI_MEMORY_XP | EFI_MEMORY_RP;
  if (((Base | Length) & 0xfff) != 0 || Length == 0 || Base > MAX_ADDRESS ||
      Length > (UINT64)MAX_ADDRESS - Base + 1) return EFI_INVALID_PARAMETER;
  if (Attr & ~(Perm | EFI_MEMORY_UC | EFI_MEMORY_WC | EFI_MEMORY_WT | EFI_MEMORY_WB)) return EFI_UNSUPPORTED;
  if ((Cache != 0) && ((Cache & (Cache-1)) != 0 || Mask != 0)) return EFI_INVALID_PARAMETER;
  if ((Cache == 0) && ((Mask & ~Perm) != 0 || (Attr & ~Mask) != 0)) return EFI_INVALID_PARAMETER;
  if (((Attr | Mask) & EFI_MEMORY_RP) != 0) return EFI_UNSUPPORTED;
  return EFI_SUCCESS;
}
/* Filter new descriptor bits, preserving every permission excluded by mask. */
STATIC VOID
A04FilterDescriptorMask (UINT64 AttributeMask, UINT32 XnBits, UINT32 RoBits,
                         UINT32 *EntryMask, UINT32 *EntryValue)
{
  if ((AttributeMask & EFI_MEMORY_XP) == 0) *EntryMask &= ~XnBits;
  if ((AttributeMask & EFI_MEMORY_RO) == 0) *EntryMask &= ~RoBits;
  *EntryValue &= *EntryMask;
}
#endif
