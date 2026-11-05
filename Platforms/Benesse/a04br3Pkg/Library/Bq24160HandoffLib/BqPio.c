/* SPDX-License-Identifier: MIT
 * Based on a04br3-windows-drivers BQ24160 mt8167_i2c.c, commit 8353514.
 * Caller owns I2C2, its clock and the full transaction lock.
 */
#include <Library/A04BR3BqPio.h>
static int transfer(struct a04_bq_pio *p, int read, uint8_t reg, uint8_t *value)
{
    uint64_t start, now; uint16_t status; unsigned i;
    if (!p || !p->read16 || !p->write16 || !p->now_ns || !p->delay_us ||
        !value || p->faulted || reg>7 ||
        (!read && (reg || (*value & ~0x88u) || !(*value & 0x80u)))) return -1;
    if (p->read16(p->context,0x24)&1u) { p->faulted=1; return -1; }
    p->write16(p->context,0x08,0);p->write16(p->context,0x0c,7);p->write16(p->context,0x38,1);
    p->write16(p->context,0x10,(uint16_t)((p->read16(p->context,0x10)&~0x3eu)|0x28u|(read?0x12u:0u)));
    p->write16(p->context,0x04,0xd6);p->write16(p->context,0x14,read?1:2);
    p->write16(p->context,0x6c,read?1:0);p->write16(p->context,0x18,read?2:1);
    p->write16(p->context,0,reg);if(!read)p->write16(p->context,0,*value);
    p->write16(p->context,0x24,read?1:0x4001);start=p->now_ns(p->context);
    for(i=0;i<2000;++i){
        now=p->now_ns(p->context);if(now<start || now-start>=10000000ULL)break;
        status=p->read16(p->context,0x0c);now=p->now_ns(p->context);
        if(now<start || now-start>=10000000ULL || (status&6u))break;
        if(status&1u){
            if(read){if(((p->read16(p->context,0x30)>>4)&15u)!=1u)break;*value=(uint8_t)p->read16(p->context,0);}
            p->write16(p->context,0x0c,7);return 0;
        }
        p->delay_us(p->context,5);
    }
    p->write16(p->context,0x0c,7);p->faulted=1;return -1;
}
int a04_bq_pio_read(void *p,uint8_t reg,uint8_t *value){return transfer(p,1,reg,value);}
int a04_bq_pio_kick(void *p,uint8_t value){return transfer(p,0,0,&value);}
