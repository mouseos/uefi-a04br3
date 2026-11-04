#include <stdint.h>
#define STATIC static
#define VOID void
#define EFIAPI
#define BIT0 1u
#define BIT2 4u
#define BIT4 16u
typedef uint32_t UINT32; typedef uint16_t UINT16; typedef uint64_t UINT64;
typedef int EFI_STATUS; typedef void *EFI_HANDLE; typedef void *EFI_EVENT;
typedef void EFI_SYSTEM_TABLE;
#define EFI_SUCCESS 0
#define EFI_UNSUPPORTED 1
#define EFI_NOT_READY 2
#define EFI_DEVICE_ERROR 3
#define EVT_NOTIFY_SIGNAL 0
#define TPL_CALLBACK 0
struct mock_bs { EFI_STATUS (*CreateEventEx)(int,int,void(*)(EFI_EVENT,VOID *),void*,void*,EFI_EVENT*); };
extern struct mock_bs *gBS;
