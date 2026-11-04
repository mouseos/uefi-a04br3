/* SPDX-License-Identifier: MIT
 * The valid marker is published only after timing and complement reach RAM. */
#ifndef A04_I2C1_NVS_H
#define A04_I2C1_NVS_H
STATIC VOID A04I2cInvalidateNvs(volatile UINT32 *Status)
{
  Status[0]=0; Status[1]=0; Status[2]=0;
  WriteBackDataCacheRange((VOID *)Status, 3*sizeof(*Status));
}
STATIC VOID A04I2cPublishNvs(volatile UINT32 *Status, UINT32 Timing)
{
  Status[1]=Timing; Status[2]=~Timing;
  WriteBackDataCacheRange((VOID *)Status, 3*sizeof(*Status));
  Status[0]=0x41303431u;
  WriteBackDataCacheRange((VOID *)Status, 3*sizeof(*Status));
}
#endif
