/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * MT6392 year1968+raw7bit and validation follow the existing BSD Windows
 * calendar core; Gregorian conversion has no timezone/calibration policy. */
#ifndef MT6392_UEFI_CALENDAR_H
#define MT6392_UEFI_CALENDAR_H
STATIC UINT32 MtLeap(UINT32 y){return y%4==0&&(y%100!=0||y%400==0);}
STATIC UINT32 MtDaysBeforeYear(UINT32 y){UINT32 n=y-1;return 365*n+n/4-n/100+n/400;}
STATIC UINT32 MtMonthDays(UINT32 y,UINT32 m){
 STATIC CONST UINT8 days[12]={31,28,31,30,31,30,31,31,30,31,30,31};
 return days[m-1]+(m==2?MtLeap(y):0);
}
STATIC BOOLEAN MtCalendarSeconds(CONST UINT16 raw[7],UINT64 *seconds){
 UINT32 year,days,m;
 if(!raw||!seconds||raw[0]>59||raw[1]>59||raw[2]>23||raw[5]<1||raw[5]>12||raw[6]>127)return FALSE;
 year=1968+raw[6];
 if(raw[3]<1||raw[3]>MtMonthDays(year,raw[5]))return FALSE;
 days=MtDaysBeforeYear(year)-MtDaysBeforeYear(1968)+raw[3]-1;
 for(m=1;m<raw[5];++m)days+=MtMonthDays(year,m);
 *seconds=(UINT64)days*86400+(UINT32)raw[2]*3600+(UINT32)raw[1]*60+raw[0];return TRUE;
}
STATIC BOOLEAN MtSecondsCalendar(UINT64 seconds,EFI_TIME *time){
 UINT64 wholeDays;UINT32 days,low=1968,high=10000,mid,month=1,within;
 wholeDays=DivU64x32(seconds,86400);
 if(wholeDays>=(UINT64)(MtDaysBeforeYear(10000)-MtDaysBeforeYear(1968)))return FALSE;
 days=(UINT32)wholeDays+MtDaysBeforeYear(1968);
 while(low+1<high){mid=low+(high-low)/2;if(MtDaysBeforeYear(mid)<=days)low=mid;else high=mid;}
 days-=MtDaysBeforeYear(low);
 while(days>=MtMonthDays(low,month)){days-=MtMonthDays(low,month);++month;}
 within=(UINT32)(seconds-wholeDays*86400);
 ZeroMem(time,sizeof(*time));time->Year=(UINT16)low;time->Month=(UINT8)month;time->Day=(UINT8)(days+1);
 time->Hour=(UINT8)(within/3600);time->Minute=(UINT8)((within%3600)/60);time->Second=(UINT8)(within%60);
 time->TimeZone=EFI_UNSPECIFIED_TIMEZONE;time->Daylight=0;time->Nanosecond=0;return TRUE;
}
#endif
