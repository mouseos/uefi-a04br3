#include <stdint.h>
#include <stddef.h>
#include <assert.h>
#include <stdio.h>
typedef uint32_t UINT32; typedef void VOID;
#define STATIC static
static UINT32 snapshots[4][3]; static unsigned n;
static void WriteBackDataCacheRange(VOID *address, size_t size) {
  UINT32 *p=address; assert(size==12 && n<4);
  for (unsigned i=0;i<3;i++) snapshots[n][i]=p[i]; n++;
}
#include "../Drivers/I2c1HandoffDxe/I2c1Nvs.h"
int main(void) {
  UINT32 s[3]={0x41303431u,123,456};
  A04I2cInvalidateNvs(s); assert(n==1 && snapshots[0][0]==0 && snapshots[0][1]==0 && snapshots[0][2]==0);
  A04I2cPublishNvs(s,0x205); assert(n==3);
  assert(snapshots[1][0]==0 && snapshots[1][1]==0x205 && snapshots[1][2]==~(UINT32)0x205);
  assert(snapshots[2][0]==0x41303431u && snapshots[2][1]==0x205 && snapshots[2][2]==~(UINT32)0x205);
  puts("I2C NVS invalidation and flush-before-marker ordering passed (host mock)");
}
