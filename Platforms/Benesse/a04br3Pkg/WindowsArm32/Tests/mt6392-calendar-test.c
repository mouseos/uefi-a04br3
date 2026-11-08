/* SPDX-License-Identifier: BSD-2-Clause-Patent; host calendar boundary test only. */
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
typedef uint8_t UINT8;typedef uint16_t UINT16;typedef uint32_t UINT32;typedef uint64_t UINT64;typedef int BOOLEAN;
#define STATIC static
#define CONST const
#define TRUE 1
#define FALSE 0
#define EFI_UNSPECIFIED_TIMEZONE 2047
#define ZeroMem(p,n) memset(p,0,n)
#define DivU64x32(a,b) ((a)/(b))
typedef struct {UINT16 Year;UINT8 Month,Day,Hour,Minute,Second,Pad1;UINT32 Nanosecond;int16_t TimeZone;UINT8 Daylight,Pad2;} EFI_TIME;
#include "Calendar.h"
static void tick(UINT16 y,UINT16 m,UINT16 d,UINT16 hh,UINT16 mm,UINT16 ss,unsigned add,UINT16 ey,UINT16 em,UINT16 ed,UINT16 eh,UINT16 emin,UINT16 es){
 UINT16 raw[7]={ss,mm,hh,d,0,m,(UINT16)(y-1968)};UINT64 s;EFI_TIME out;
 assert(MtCalendarSeconds(raw,&s));assert(MtSecondsCalendar(s+add,&out));
 assert(out.Year==ey&&out.Month==em&&out.Day==ed&&out.Hour==eh&&out.Minute==emin&&out.Second==es);
 assert(out.Nanosecond==0&&out.TimeZone==2047&&out.Daylight==0&&out.Pad1==0&&out.Pad2==0);
}
int main(void){UINT16 raw[7]={0,0,0,1,0,1,58};UINT64 s;EFI_TIME out;
 tick(1968,1,1,0,0,0,0,1968,1,1,0,0,0);
 tick(2000,2,28,23,59,59,1,2000,2,29,0,0,0);
 tick(2000,2,29,23,59,59,1,2000,3,1,0,0,0);
 tick(2026,12,31,23,59,59,1,2027,1,1,0,0,0);
 tick(2095,12,31,23,59,59,1,2096,1,1,0,0,0);
 assert(MtCalendarSeconds(raw,&s));assert(!MtSecondsCalendar(UINT64_MAX,&out));
 raw[3]=0;assert(!MtCalendarSeconds(raw,&s));raw[3]=30;raw[5]=2;assert(!MtCalendarSeconds(raw,&s));
 raw[3]=29;assert(!MtCalendarSeconds(raw,&s));raw[6]=32;assert(MtCalendarSeconds(raw,&s));
 raw[6]=128;assert(!MtCalendarSeconds(raw,&s));raw[6]=58;raw[0]=60;assert(!MtCalendarSeconds(raw,&s));
 puts("PASS real calendar core: leap/month/year rollover, RTC raw bounds, no day0, timezone unknown, overflow rejection");}
