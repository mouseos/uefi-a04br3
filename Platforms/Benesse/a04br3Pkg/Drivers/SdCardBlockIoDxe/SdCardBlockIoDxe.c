/** @file
  Read-only SD-card Block I/O driver for the MT8167 MSDC1 host.

  The driver enumerates the card conservatively, then switches to 4-bit
  transfer mode and SD high-speed mode when the card accepts it.

  Ported unchanged from mouseos/edk2-mtk (Platform/Benesse/A04BR3Pkg/Drivers/
  SdCardBlockIoDxe, commit 1982d888c765). With A04BR3_EMMC_BUILD defined
  (EmmcBlockIoDxe) the same code drives the internal eMMC on MSDC0.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/CacheMaintenanceLib.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/PcdLib.h>
#include <Library/TimerLib.h>
#include <Library/UefiBootServicesTableLib.h>

#include <Protocol/BlockIo.h>
#include <Protocol/DevicePath.h>

#define MSDC_CFG                    0x000
#define MSDC_IOCON                  0x004
#define MSDC_PS                     0x008
#define MSDC_INT                    0x00c
#define MSDC_INTEN                  0x010
#define MSDC_FIFOCS                 0x014
#define MSDC_RXDATA                 0x01c
#define SDC_CFG                     0x030
#define SDC_CMD                     0x034
#define SDC_ARG                     0x038
#define SDC_STS                     0x03c
#define SDC_RESP0                   0x040
#define SDC_RESP1                   0x044
#define SDC_RESP2                   0x048
#define SDC_RESP3                   0x04c
#define SDC_BLK_NUM                 0x050
#define EMMC_CFG0                   0x070
#define MSDC_DMA_SA_HIGH            0x08c
#define MSDC_DMA_SA                 0x090
#define MSDC_DMA_CA                 0x094
#define MSDC_DMA_CTRL               0x098
#define MSDC_DMA_CFG                0x09c
#define MSDC_DMA_LEN                0x0a8

#define MSDC_CFG_MODE               BIT0
#define MSDC_CFG_CKPDN              BIT1
#define MSDC_CFG_RST                BIT2
#define MSDC_CFG_PIO                BIT3
#define MSDC_CFG_CKSTB              BIT7
#define MSDC_CFG_CKDIV_MASK         (0xfffU << 8)
#define MSDC_CFG_CKMOD_MASK         (0x3U << 20)
#define MSDC_CFG_CKMOD_NO_DIV       (0x1U << 20)
#define MSDC_CFG_CKMOD_HS400        BIT22

#define MSDC_INT_CMDRDY             BIT8
#define MSDC_INT_CMDTMO             BIT9
#define MSDC_INT_RSPCRCERR          BIT10
#define MSDC_INT_XFER_COMPL         BIT12
#define MSDC_INT_DATTMO             BIT14
#define MSDC_INT_DATCRCERR          BIT15
#define MSDC_INT_BDCSERR            BIT17
#define MSDC_INT_GPDCSERR           BIT18
#define MSDC_INT_ACMDRDY            BIT3
#define MSDC_INT_ACMDTMO            BIT4
#define MSDC_INT_ACMDCRCERR         BIT5

#define MSDC_FIFOCS_RXCNT_MASK      0xffU
#define MSDC_FIFOCS_CLR             BIT31

#define MSDC_DMA_CTRL_START         BIT0
#define MSDC_DMA_CTRL_STOP          BIT1
#define MSDC_DMA_CTRL_MODE          BIT8
#define MSDC_DMA_CTRL_LASTBUF       BIT10
#define MSDC_DMA_CTRL_BURST_MASK    (0x7U << 12)
#define MSDC_DMA_CTRL_BURST_64B     (0x6U << 12)
#define MSDC_DMA_CFG_STS            BIT0
#define MSDC_DMA_CFG_DECSEN         BIT1

#define SDC_CFG_BUSWIDTH_MASK       (0x3U << 16)
#define SDC_CFG_BUSWIDTH_1          (0x0U << 16)
#define SDC_CFG_BUSWIDTH_4          (0x1U << 16)
#define SDC_CFG_BUSWIDTH_8          (0x2U << 16)
#define SDC_CFG_DTOC_MASK           (0xffU << 24)
#define SDC_CFG_DTOC_MAX            (0xffU << 24)

#define SDC_STS_SDCBUSY             BIT0
#define SDC_STS_CMDBUSY             BIT1
#define MSDC_PS_DAT0                BIT16

#define EMMC_CFG0_BOOTSUPP          BIT15

#define TOPCKGEN_CLK_MUX_REG        0x10000000U
#define MSDC_CLK_GATE_CTRL          0x10000054U
#define MSDC_CLK_UNGATE_CTRL        0x10000084U
#define MSDC_GATE_INFRA_SW_CG       0x1000009cU
#define MSDC_UNGATE_INFRA_SW_CG     0x1000006cU
#ifdef A04BR3_EMMC_BUILD
#define MSDC_HOST_BASE              0x11120000U
#define MSDC1_CLK_SW_CG             BIT17
#define MSDC1_INFRA_SW_CG           BIT28
#define MSDC1_CLK_MUX_SEL_MASK      (0x7U << 11)
#define MSDC1_CLK_MUX_SEL_188MHZ    (0x2U << 11)
#else
#define MSDC1_CLK_SW_CG             BIT18
#define MSDC1_INFRA_SW_CG           BIT29
#define MSDC1_CLK_MUX_SEL_MASK      (0x7U << 20)
#define MSDC1_CLK_MUX_SEL_188MHZ    (0x2U << 20)
#endif
#define MSDC1_SOURCE_CLOCK_HZ       188000000U
#define MSDC_INIT_CLOCK_HZ          260000U
#define MSDC_DEFAULT_TRANSFER_HZ    25000000U
#define MSDC_HIGH_SPEED_HZ          50000000U


#define MSDC_RESP_NONE              0U
#define MSDC_RESP_R1                1U
#define MSDC_RESP_R2                2U
#define MSDC_RESP_R3                3U
#define MSDC_RESP_R7                1U
#define MSDC_RESP_R1B               7U

#define MSDC_DTYPE_NONE             0U
#define MSDC_DTYPE_SINGLE           1U
#define MSDC_DTYPE_MULTIPLE         2U
#define MSDC_AUTOCMD12              BIT28

#define SD_CMD_GO_IDLE              0U
#define SD_CMD_SWITCH_FUNC          6U
#define SD_CMD_STOP_TRANSMISSION    12U
#define SD_CMD_ALL_SEND_CID         2U
#define SD_CMD_SEND_RELATIVE_ADDR   3U
#define SD_CMD_SELECT_CARD          7U
#define SD_CMD_SEND_IF_COND         8U
#define SD_CMD_SEND_CSD             9U
#define SD_CMD_SET_BLOCKLEN         16U
#define SD_CMD_READ_SINGLE          17U
#define SD_CMD_READ_MULTIPLE        18U
#define SD_CMD_APP_CMD              55U
#define SD_ACMD_SET_BUS_WIDTH       6U
#define SD_ACMD_SD_SEND_OP_COND     41U

#define MMC_CMD_SEND_OP_COND        1U
#define MMC_CMD_SWITCH              6U
#define MMC_CMD_SEND_EXT_CSD        8U
#define MMC_CMD_SEND_STATUS         13U
#define MMC_OCR_VOLTAGE_27_36       0x00ff8000U
#define MMC_OCR_SECTOR_MODE         BIT30
#define MMC_EXT_CSD_SIZE            512U
#define MMC_EXT_CSD_BUS_WIDTH       183U
#define MMC_EXT_CSD_HS_TIMING       185U
#define MMC_EXT_CSD_REV             192U
#define MMC_EXT_CSD_CARD_TYPE       196U
#define MMC_EXT_CSD_SEC_COUNT       212U
#define MMC_EXT_CSD_BUS_WIDTH_8     2U
#define MMC_EXT_CSD_HS_TIMING_HS    1U
#define MMC_EXT_CSD_CARD_TYPE_52MHZ BIT1
#define MMC_SWITCH_MODE_WRITE_BYTE  3U
#define MMC_R1_SWITCH_ERROR         BIT7
#define MMC_R1_READY_FOR_DATA       BIT8
#define MMC_R1_CURRENT_STATE_MASK   (0xfU << 9)
#define MMC_R1_CURRENT_STATE_PRG    (0x7U << 9)

#define SD_OCR_BUSY                 BIT31
#define SD_OCR_CCS                  BIT30
#define SD_OCR_HCS                  BIT30
#define SD_OCR_32_34V               (BIT20 | BIT21)

#define SD_BLOCK_SIZE               FixedPcdGet32 (PcdMt8167SdCardBlockSize)
#define SD_SWITCH_STATUS_SIZE       64U
#define SD_SWITCH_CHECK             0U
#define SD_SWITCH_SET               BIT31
#define SD_SWITCH_FUNC_GROUP_1_MASK 0xfffffff0U
#define SD_SWITCH_FUNC_HIGH_SPEED   1U
#define SD_SWITCH_STATUS_HS_BIT     BIT1
#define SD_WAIT_LOOPS               1000000U
#define SD_DATA_WAIT_LOOPS          8000000U
#define SD_ACMD41_RETRIES           1000U
#define SD_DMA_MAX_BYTES            (8U * 1024U * 1024U)
#define SD_DMA_MAX_BLOCKS           (SD_DMA_MAX_BYTES / SD_BLOCK_SIZE)
#define SD_DMA_PROBE_LBA            0x40U
#define SD_DMA_PROBE_BLOCKS         4U

#define MSDC_GPD_HWO                BIT0
#define MSDC_GPD_BDP                BIT1
#define MSDC_DESC_CHECKSUM_SHIFT    8U
#define MSDC_GPD_NEXT_HIGH_SHIFT    24U
#define MSDC_GPD_BD_HIGH_SHIFT      28U
#define MSDC_BD_EOL                 BIT0
#define MSDC_BD_DATA_HIGH_SHIFT     28U
#define MSDC_DESC_HIGH_MASK         0xfU
#define MSDC_DESC_LENGTH_MASK       0x00ffffffU
#define MSDC_DESC_CHECKSUM_BYTES    16U
#define MSDC_DESCRIPTOR_PAGES       1U

#define MSDC_CMD_INTS               (MSDC_INT_CMDRDY | MSDC_INT_CMDTMO | MSDC_INT_RSPCRCERR)
#define MSDC_DATA_INTS              (MSDC_INT_XFER_COMPL | MSDC_INT_DATTMO | MSDC_INT_DATCRCERR | \
                                     MSDC_INT_ACMDRDY | MSDC_INT_ACMDTMO | MSDC_INT_ACMDCRCERR | \
                                     MSDC_INT_BDCSERR | MSDC_INT_GPDCSERR)

typedef struct {
  UINT32  Info;
  UINT32  Next;
  UINT32  Pointer;
  UINT32  BufferLength;
  UINT32  Argument;
  UINT32  BlockCount;
  UINT32  Command;
} MT8167_MSDC_GPD;

typedef struct {
  UINT32  Info;
  UINT32  Next;
  UINT32  Pointer;
  UINT32  BufferLength;
} MT8167_MSDC_BD;

STATIC_ASSERT (sizeof (MT8167_MSDC_GPD) == 28, "MT8167 GPD layout mismatch");
STATIC_ASSERT (sizeof (MT8167_MSDC_BD) == 16, "MT8167 BD layout mismatch");

typedef struct {
  VENDOR_DEVICE_PATH         Vendor;
  EFI_DEVICE_PATH_PROTOCOL   End;
} MT8167_SD_DEVICE_PATH;

STATIC EFI_HANDLE               mSdHandle;
STATIC EFI_BLOCK_IO_MEDIA       mSdMedia;
STATIC EFI_BLOCK_IO_PROTOCOL    mSdBlockIo;
STATIC UINT32                   mSdRca;
STATIC UINT32                   mSdClockHz;
STATIC UINT32                   mSdTargetClockHz = MSDC_INIT_CLOCK_HZ;
STATIC UINT32                   mSdBusWidth      = SDC_CFG_BUSWIDTH_1;
STATIC BOOLEAN                  mSdHighCapacity;
STATIC BOOLEAN                  mSdHighSpeed;
STATIC EFI_PHYSICAL_ADDRESS     mSdDescriptorPageAddress;
STATIC EFI_PHYSICAL_ADDRESS     mSdGpdAddress;
STATIC EFI_PHYSICAL_ADDRESS     mSdBdAddress;
STATIC MT8167_MSDC_GPD          *mSdGpd;
STATIC MT8167_MSDC_BD           *mSdBd;
STATIC UINT8                    mSdDmaProbeReference[SD_DMA_PROBE_BLOCKS * 512U];
STATIC UINT8                    mSdDmaProbeResult[SD_DMA_PROBE_BLOCKS * 512U];
#ifdef A04BR3_EMMC_BUILD
STATIC UINT8                    mEmmcExtCsd[MMC_EXT_CSD_SIZE];
STATIC UINT8                    mEmmcGptProbe[2U * 512U];
#endif
STATIC MT8167_SD_DEVICE_PATH    mSdDevicePath = {
  {
    {
      HARDWARE_DEVICE_PATH,
      HW_VENDOR_DP,
      {
        (UINT8)sizeof (VENDOR_DEVICE_PATH),
        (UINT8)(sizeof (VENDOR_DEVICE_PATH) >> 8)
      }
    },
#ifdef A04BR3_EMMC_BUILD
    { 0x49a08213, 0xc76a, 0x4d35, { 0x90, 0x27, 0x7d, 0x0a, 0xa4, 0x59, 0x4b, 0x31 } }
#else
    { 0x31d49f02, 0x3dc4, 0x4f62, { 0xa1, 0x2e, 0x2c, 0x2f, 0x55, 0xb5, 0x72, 0x84 } }
#endif
  },
  {
    END_DEVICE_PATH_TYPE,
    END_ENTIRE_DEVICE_PATH_SUBTYPE,
    {
      (UINT8)sizeof (EFI_DEVICE_PATH_PROTOCOL),
      (UINT8)(sizeof (EFI_DEVICE_PATH_PROTOCOL) >> 8)
    }
  }
};

STATIC
UINTN
SdBase (
  VOID
  )
{
#ifdef A04BR3_EMMC_BUILD
  return MSDC_HOST_BASE;
#else
  return (UINTN)FixedPcdGet64 (PcdMt8167SdCardBase);
#endif
}

STATIC
UINT32
SdRead32 (
  IN UINTN  Offset
  )
{
  return MmioRead32 (SdBase () + Offset);
}

STATIC
UINT8
SdRead8 (
  IN UINTN  Offset
  )
{
  return MmioRead8 (SdBase () + Offset);
}

STATIC
VOID
SdWrite32 (
  IN UINTN   Offset,
  IN UINT32  Value
  )
{
  MmioWrite32 (SdBase () + Offset, Value);
}

STATIC
VOID
SdSetBits (
  IN UINTN   Offset,
  IN UINT32  Bits
  )
{
  SdWrite32 (Offset, SdRead32 (Offset) | Bits);
}

STATIC
VOID
SdClearBits (
  IN UINTN   Offset,
  IN UINT32  Bits
  )
{
  SdWrite32 (Offset, SdRead32 (Offset) & ~Bits);
}

STATIC
EFI_STATUS
SdWaitClear (
  IN UINTN   Offset,
  IN UINT32  Bits,
  IN UINT32  Loops
  )
{
  while (Loops-- != 0) {
    if ((SdRead32 (Offset) & Bits) == 0) {
      return EFI_SUCCESS;
    }
  }

  return EFI_TIMEOUT;
}

STATIC
EFI_STATUS
SdWaitSet (
  IN UINTN   Offset,
  IN UINT32  Bits,
  IN UINT32  Loops
  )
{
  while (Loops-- != 0) {
    if ((SdRead32 (Offset) & Bits) == Bits) {
      return EFI_SUCCESS;
    }
  }

  return EFI_TIMEOUT;
}

STATIC
EFI_STATUS
SdClearFifo (
  VOID
  )
{
  SdSetBits (MSDC_FIFOCS, MSDC_FIFOCS_CLR);
  return SdWaitClear (MSDC_FIFOCS, MSDC_FIFOCS_CLR, SD_WAIT_LOOPS);
}

STATIC
EFI_STATUS
SdResetHost (
  VOID
  )
{
  EFI_STATUS  Status;

  SdSetBits (MSDC_CFG, MSDC_CFG_RST);
  Status = SdWaitClear (MSDC_CFG, MSDC_CFG_RST, SD_WAIT_LOOPS);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = SdClearFifo ();
  SdWrite32 (MSDC_INT, SdRead32 (MSDC_INT));
  return Status;
}

STATIC
VOID
SdUngateClock (
  VOID
  )
{
  UINT32  Value;

  MmioWrite32 (MSDC_GATE_INFRA_SW_CG, MSDC1_INFRA_SW_CG);
  MmioWrite32 (MSDC_CLK_GATE_CTRL, MSDC1_CLK_SW_CG);

  Value  = MmioRead32 (TOPCKGEN_CLK_MUX_REG);
  Value &= ~MSDC1_CLK_MUX_SEL_MASK;
  Value |= MSDC1_CLK_MUX_SEL_188MHZ;
  MmioWrite32 (TOPCKGEN_CLK_MUX_REG, Value);

  MmioWrite32 (MSDC_CLK_UNGATE_CTRL, MSDC1_CLK_SW_CG);
  MmioWrite32 (MSDC_UNGATE_INFRA_SW_CG, MSDC1_INFRA_SW_CG);
}

STATIC
EFI_STATUS
SdSetClock (
  IN UINT32  TargetHz
  )
{
  UINT32  Div;
  UINT32  ActualHz;
  UINT32  Mode;
  UINT32  Value;

  if (TargetHz == 0) {
    TargetHz = MSDC_INIT_CLOCK_HZ;
  }

  if (TargetHz >= MSDC1_SOURCE_CLOCK_HZ) {
    // MT8167 CKMOD=1 bypasses CKDIV.  The nominal 200 MHz mux source is
    // measured by the vendor clock table as 188 MHz on this controller.
    Mode     = MSDC_CFG_CKMOD_NO_DIV;
    Div      = 0;
    ActualHz = MSDC1_SOURCE_CLOCK_HZ;
  } else if (TargetHz >= (MSDC1_SOURCE_CLOCK_HZ / 2U)) {
    Mode     = 0;
    Div      = 0;
    ActualHz = MSDC1_SOURCE_CLOCK_HZ / 2U;
  } else {
    Mode = 0;
    Div  = (MSDC1_SOURCE_CLOCK_HZ + ((TargetHz << 2) - 1)) / (TargetHz << 2);
    if (Div > 0xfffU) {
      Div = 0xfffU;
    }

    ActualHz = MSDC1_SOURCE_CLOCK_HZ / (4U * Div);
  }

  MmioWrite32 (MSDC_GATE_INFRA_SW_CG, MSDC1_INFRA_SW_CG);
  Value  = SdRead32 (MSDC_CFG);
  Value &= ~(MSDC_CFG_CKMOD_HS400 | MSDC_CFG_CKMOD_MASK | MSDC_CFG_CKDIV_MASK);
  Value |= Mode | (Div << 8);
  SdWrite32 (MSDC_CFG, Value);
  MmioWrite32 (MSDC_UNGATE_INFRA_SW_CG, MSDC1_INFRA_SW_CG);

  mSdTargetClockHz = TargetHz;
  mSdClockHz       = ActualHz;
  return SdWaitSet (MSDC_CFG, MSDC_CFG_CKSTB, SD_WAIT_LOOPS);
}

STATIC
VOID
SdSetHostBusWidth (
  IN UINT32  BusWidth
  )
{
  UINT32  Value;

  Value  = SdRead32 (SDC_CFG);
  Value &= ~SDC_CFG_BUSWIDTH_MASK;
  Value |= BusWidth;
  SdWrite32 (SDC_CFG, Value);
  mSdBusWidth = BusWidth;
}

STATIC
EFI_STATUS
SdRestoreTransferMode (
  VOID
  )
{
  EFI_STATUS  Status;

  Status = SdSetClock (mSdTargetClockHz);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  SdSetHostBusWidth (mSdBusWidth);
  SdSetBits (MSDC_CFG, MSDC_CFG_MODE | MSDC_CFG_PIO);
  return SdClearFifo ();
}

STATIC
UINT32
SdBuildRawCommand (
  IN UINT32  Opcode,
  IN UINT32  ResponseType,
  IN UINT32  DataType,
  IN UINT32  BlockLength
  )
{
  UINT32  Command;

  Command = Opcode | (ResponseType << 7);
  if (DataType != MSDC_DTYPE_NONE) {
    Command |= (DataType << 11) | (BlockLength << 16);
    if (DataType == MSDC_DTYPE_MULTIPLE) {
      Command |= MSDC_AUTOCMD12;
    }
  }

  return Command;
}

STATIC
EFI_STATUS
SdWaitNotBusy (
  IN BOOLEAN  CommandOnly
  )
{
  UINT32  BusyBit;

  BusyBit = CommandOnly ? SDC_STS_CMDBUSY : SDC_STS_SDCBUSY;
  return SdWaitClear (SDC_STS, BusyBit, SD_WAIT_LOOPS);
}

STATIC
EFI_STATUS
SdSendCommand (
  IN  UINT32  Opcode,
  IN  UINT32  Argument,
  IN  UINT32  ResponseType,
  IN  UINT32  DataType,
  IN  UINT32  BlockLength,
  OUT UINT32  Response[4] OPTIONAL
  )
{
  UINT32      Ints;
  UINT32      Loops;
  EFI_STATUS  Status;

  Status = SdWaitNotBusy (Opcode == SD_CMD_APP_CMD);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "[Mt8167Pkg] SD CMD%u busy timeout\n", Opcode));
    return Status;
  }

  SdClearBits (MSDC_INTEN, MSDC_CMD_INTS | MSDC_DATA_INTS);
  SdWrite32 (MSDC_INT, SdRead32 (MSDC_INT));
  SdWrite32 (SDC_ARG, Argument);
  SdWrite32 (SDC_CMD, SdBuildRawCommand (Opcode, ResponseType, DataType, BlockLength));

  Loops = SD_WAIT_LOOPS;
  while (Loops-- != 0) {
    Ints = SdRead32 (MSDC_INT) & MSDC_CMD_INTS;
    if (Ints == 0) {
      continue;
    }

    SdWrite32 (MSDC_INT, Ints);
    if ((Ints & MSDC_INT_CMDRDY) != 0) {
      if (Response != NULL) {
        if (ResponseType == MSDC_RESP_R2) {
          Response[0] = SdRead32 (SDC_RESP3);
          Response[1] = SdRead32 (SDC_RESP2);
          Response[2] = SdRead32 (SDC_RESP1);
          Response[3] = SdRead32 (SDC_RESP0);
        } else {
          Response[0] = SdRead32 (SDC_RESP0);
        }
      }

      if (ResponseType == MSDC_RESP_R1B) {
        Status = SdWaitSet (MSDC_PS, MSDC_PS_DAT0, SD_DATA_WAIT_LOOPS);
        if (EFI_ERROR (Status)) {
          DEBUG ((DEBUG_ERROR, "[Mt8167Pkg] CMD%u R1b DAT0 busy timeout\n", Opcode));
          return Status;
        }
      }

      return EFI_SUCCESS;
    }

    if ((Ints & MSDC_INT_CMDTMO) != 0) {
      DEBUG ((DEBUG_ERROR, "[Mt8167Pkg] SD CMD%u timeout arg=0x%08x\n", Opcode, Argument));
      return EFI_TIMEOUT;
    }

    if ((Ints & MSDC_INT_RSPCRCERR) != 0) {
      DEBUG ((DEBUG_ERROR, "[Mt8167Pkg] SD CMD%u response CRC error arg=0x%08x\n", Opcode, Argument));
      return EFI_CRC_ERROR;
    }
  }

  DEBUG ((DEBUG_ERROR, "[Mt8167Pkg] SD CMD%u software timeout arg=0x%08x\n", Opcode, Argument));
  return EFI_TIMEOUT;
}

#ifndef A04BR3_EMMC_BUILD
STATIC
EFI_STATUS
SdSendAppCommand (
  IN  UINT32  Rca,
  IN  UINT32  Opcode,
  IN  UINT32  Argument,
  IN  UINT32  ResponseType,
  OUT UINT32  Response[4] OPTIONAL
  )
{
  EFI_STATUS  Status;

  Status = SdSendCommand (
             SD_CMD_APP_CMD,
             Rca << 16,
             MSDC_RESP_R1,
             MSDC_DTYPE_NONE,
             0,
             NULL
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  return SdSendCommand (Opcode, Argument, ResponseType, MSDC_DTYPE_NONE, 0, Response);
}
#endif

STATIC
EFI_STATUS
SdReadData (
  OUT UINT8  *Buffer,
  IN  UINTN  Size
  )
{
  UINT32   Ints;
  UINT32   Loops;
  UINT32   FifoCount;
  UINTN    OriginalSize;
  BOOLEAN  TransferDone;

  OriginalSize  = Size;
  TransferDone = FALSE;
  Loops        = SD_DATA_WAIT_LOOPS;

  while (Loops-- != 0) {
    Ints = SdRead32 (MSDC_INT) & MSDC_DATA_INTS;
    if (Ints != 0) {
      SdWrite32 (MSDC_INT, Ints);
      if ((Ints & MSDC_INT_DATTMO) != 0) {
        DEBUG ((DEBUG_ERROR, "[Mt8167Pkg] SD data timeout\n"));
        return EFI_TIMEOUT;
      }

      if ((Ints & MSDC_INT_DATCRCERR) != 0) {
        DEBUG ((DEBUG_ERROR, "[Mt8167Pkg] SD data CRC error\n"));
        return EFI_CRC_ERROR;
      }

      if ((Ints & MSDC_INT_ACMDTMO) != 0) {
        DEBUG ((DEBUG_ERROR, "[Mt8167Pkg] SD auto-CMD timeout\n"));
        return EFI_TIMEOUT;
      }

      if ((Ints & MSDC_INT_ACMDCRCERR) != 0) {
        DEBUG ((DEBUG_ERROR, "[Mt8167Pkg] SD auto-CMD CRC error\n"));
        return EFI_CRC_ERROR;
      }

      if ((Ints & MSDC_INT_XFER_COMPL) != 0) {
        TransferDone = TRUE;
      }
    }

    FifoCount = SdRead32 (MSDC_FIFOCS) & MSDC_FIFOCS_RXCNT_MASK;
    while ((FifoCount != 0) && (Size != 0)) {
      if ((FifoCount >= sizeof (UINT32)) && (Size >= sizeof (UINT32))) {
        WriteUnaligned32 ((UINT32 *)Buffer, SdRead32 (MSDC_RXDATA));
        Buffer    += sizeof (UINT32);
        Size      -= sizeof (UINT32);
        FifoCount -= sizeof (UINT32);
      } else {
        *Buffer++ = SdRead8 (MSDC_RXDATA);
        --Size;
        --FifoCount;
      }
    }

    if (TransferDone && (Size == 0)) {
      return EFI_SUCCESS;
    }
  }

  DEBUG ((
    DEBUG_ERROR,
    "[Mt8167Pkg] SD data software timeout original=0x%lx remaining=0x%lx int=0x%08x fifocs=0x%08x sdc_sts=0x%08x sdc_cmd=0x%08x blk_num=0x%08x\n",
    (UINT64)OriginalSize,
    (UINT64)Size,
    SdRead32 (MSDC_INT),
    SdRead32 (MSDC_FIFOCS),
    SdRead32 (SDC_STS),
    SdRead32 (SDC_CMD),
    SdRead32 (SDC_BLK_NUM)
    ));
  return EFI_TIMEOUT;
}

STATIC
EFI_STATUS
SdReadSingleBlock (
  IN  EFI_LBA  Lba,
  OUT VOID     *Buffer
  )
{
  UINT32      Response[4];
  UINT32      Argument;
  EFI_STATUS  Status;

  Argument = mSdHighCapacity ? (UINT32)Lba : (UINT32)(Lba * SD_BLOCK_SIZE);

  SdWrite32 (SDC_BLK_NUM, 1);
  Status = SdClearFifo ();
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = SdSendCommand (
             SD_CMD_READ_SINGLE,
             Argument,
             MSDC_RESP_R1,
             MSDC_DTYPE_SINGLE,
             SD_BLOCK_SIZE,
             Response
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  return SdReadData (Buffer, SD_BLOCK_SIZE);
}

STATIC
EFI_STATUS
SdReadMultipleBlocks (
  IN  EFI_LBA  Lba,
  IN  UINTN    BlockCount,
  OUT VOID     *Buffer
  )
{
  UINT32      Response[4];
  UINT32      Argument;
  EFI_STATUS  Status;

  if (BlockCount == 1) {
    return SdReadSingleBlock (Lba, Buffer);
  }

  Argument = mSdHighCapacity ? (UINT32)Lba : (UINT32)(Lba * SD_BLOCK_SIZE);

  SdWrite32 (SDC_BLK_NUM, (UINT32)BlockCount);
  Status = SdClearFifo ();
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "[Mt8167Pkg] SD multi clear fifo failed lba=0x%lx blocks=0x%lx: %r int=0x%08x sts=0x%08x\n",
      Lba,
      (UINT64)BlockCount,
      Status,
      SdRead32 (MSDC_INT),
      SdRead32 (SDC_STS)
      ));
    return Status;
  }

  Status = SdSendCommand (
             SD_CMD_READ_MULTIPLE,
             Argument,
             MSDC_RESP_R1,
             MSDC_DTYPE_MULTIPLE,
             SD_BLOCK_SIZE,
             Response
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "[Mt8167Pkg] SD multi CMD18 failed lba=0x%lx blocks=0x%lx: %r int=0x%08x sts=0x%08x\n", Lba, (UINT64)BlockCount, Status, SdRead32 (MSDC_INT), SdRead32 (SDC_STS)));
    return Status;
  }

  Status = SdReadData (Buffer, BlockCount * SD_BLOCK_SIZE);

  if (!EFI_ERROR (Status)) {
    Status = SdWaitNotBusy (FALSE);
  }

  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "[Mt8167Pkg] SD multi transfer failed lba=0x%lx blocks=0x%lx: %r int=0x%08x sts=0x%08x fifocs=0x%08x\n",
      Lba,
      (UINT64)BlockCount,
      Status,
      SdRead32 (MSDC_INT),
      SdRead32 (SDC_STS),
      SdRead32 (MSDC_FIFOCS)
      ));
  }

  return Status;
}

STATIC
UINT8
SdDescriptorChecksum (
  IN CONST VOID  *Descriptor
  )
{
  CONST UINT8  *Bytes;
  UINT32       Index;
  UINT32       Sum;

  Bytes = Descriptor;
  Sum   = 0;
  for (Index = 0; Index < MSDC_DESC_CHECKSUM_BYTES; ++Index) {
    Sum += Bytes[Index];
  }

  return (UINT8)(0xffU - (UINT8)Sum);
}

STATIC
EFI_STATUS
SdBuildDescriptors (
  OUT VOID   *Buffer,
  IN  UINTN  Size
  )
{
  EFI_PHYSICAL_ADDRESS  BufferAddress;
  EFI_PHYSICAL_ADDRESS  NullGpdAddress;
  UINT8                 Checksum;

  if ((mSdGpd == NULL) || (mSdBd == NULL) || (Buffer == NULL) ||
      (Size == 0) || (Size > SD_DMA_MAX_BYTES) ||
      ((Size & ~MSDC_DESC_LENGTH_MASK) != 0))
  {
    return EFI_INVALID_PARAMETER;
  }

  // A04BR3 uses an identity-mapped DXE address space and its 2 GiB DRAM is
  // below 4 GiB.  Keep the MT8167 descriptor high-nibble fields correct so
  // an invalid address fails instead of being silently truncated.
  BufferAddress  = (EFI_PHYSICAL_ADDRESS)(UINTN)Buffer;
  NullGpdAddress = mSdGpdAddress + sizeof (MT8167_MSDC_GPD);
  if (((BufferAddress >> 36) != 0) || (((BufferAddress + Size - 1U) >> 36) != 0) ||
      ((mSdGpdAddress >> 36) != 0) ||
      ((mSdBdAddress >> 36) != 0) || ((NullGpdAddress >> 36) != 0))
  {
    return EFI_DEVICE_ERROR;
  }

  ZeroMem (mSdGpd, sizeof (MT8167_MSDC_GPD) * 2U);
  ZeroMem (mSdBd, sizeof (MT8167_MSDC_BD));

  mSdBd->Info = MSDC_BD_EOL |
                (((UINT32)(BufferAddress >> 32) & MSDC_DESC_HIGH_MASK) << MSDC_BD_DATA_HIGH_SHIFT);
  mSdBd->Pointer      = (UINT32)BufferAddress;
  mSdBd->BufferLength = (UINT32)Size & MSDC_DESC_LENGTH_MASK;
  Checksum            = (UINT8)SdDescriptorChecksum (mSdBd);
  mSdBd->Info        |= (UINT32)Checksum << MSDC_DESC_CHECKSUM_SHIFT;

  mSdGpd[0].Info = MSDC_GPD_HWO | MSDC_GPD_BDP |
                   (((UINT32)(NullGpdAddress >> 32) & MSDC_DESC_HIGH_MASK) << MSDC_GPD_NEXT_HIGH_SHIFT) |
                   (((UINT32)(mSdBdAddress >> 32) & MSDC_DESC_HIGH_MASK) << MSDC_GPD_BD_HIGH_SHIFT);
  mSdGpd[0].Next    = (UINT32)NullGpdAddress;
  mSdGpd[0].Pointer = (UINT32)mSdBdAddress;
  Checksum          = (UINT8)SdDescriptorChecksum (&mSdGpd[0]);
  mSdGpd[0].Info   |= (UINT32)Checksum << MSDC_DESC_CHECKSUM_SHIFT;

  Checksum        = (UINT8)SdDescriptorChecksum (&mSdGpd[1]);
  mSdGpd[1].Info |= (UINT32)Checksum << MSDC_DESC_CHECKSUM_SHIFT;

  WriteBackInvalidateDataCacheRange (mSdBd, sizeof (MT8167_MSDC_BD));
  WriteBackInvalidateDataCacheRange (mSdGpd, sizeof (MT8167_MSDC_GPD) * 2U);
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
SdStopDma (
  VOID
  )
{
  EFI_STATUS  Status;

  SdSetBits (MSDC_DMA_CTRL, MSDC_DMA_CTRL_STOP);
  Status = SdWaitClear (MSDC_DMA_CFG, MSDC_DMA_CFG_STS, SD_WAIT_LOOPS);
  SdClearBits (MSDC_DMA_CFG, MSDC_DMA_CFG_DECSEN);
  SdSetBits (MSDC_CFG, MSDC_CFG_PIO);
  return Status;
}

STATIC
EFI_STATUS
SdWaitDmaComplete (
  VOID
  )
{
  UINT32  Ints;
  UINT32  Loops;

  Loops = SD_DATA_WAIT_LOOPS;
  while (Loops-- != 0) {
    Ints = SdRead32 (MSDC_INT) & MSDC_DATA_INTS;
    if (Ints == 0) {
      continue;
    }

    SdWrite32 (MSDC_INT, Ints);
    if ((Ints & (MSDC_INT_DATTMO | MSDC_INT_ACMDTMO)) != 0) {
      return EFI_TIMEOUT;
    }

    if ((Ints & (MSDC_INT_DATCRCERR | MSDC_INT_ACMDCRCERR)) != 0) {
      return EFI_CRC_ERROR;
    }

    if ((Ints & (MSDC_INT_BDCSERR | MSDC_INT_GPDCSERR)) != 0) {
      DEBUG ((DEBUG_ERROR, "[Mt8167Pkg] SD descriptor checksum error int=0x%08x\n", Ints));
      return EFI_CRC_ERROR;
    }

    if ((Ints & MSDC_INT_XFER_COMPL) != 0) {
      return EFI_SUCCESS;
    }
  }

  DEBUG ((
    DEBUG_ERROR,
    "[Mt8167Pkg] SD DMA software timeout int=0x%08x cfg=0x%08x ctrl=0x%08x ca=0x%08x fifocs=0x%08x\n",
    SdRead32 (MSDC_INT),
    SdRead32 (MSDC_DMA_CFG),
    SdRead32 (MSDC_DMA_CTRL),
    SdRead32 (MSDC_DMA_CA),
    SdRead32 (MSDC_FIFOCS)
    ));
  return EFI_TIMEOUT;
}

STATIC
EFI_STATUS
SdReadDmaBlocks (
  IN  EFI_LBA  Lba,
  IN  UINTN    BlockCount,
  OUT VOID     *Buffer
  )
{
  UINT32      Argument;
  UINT32      Command;
  UINT32      DataType;
  UINT32      DmaControl;
  UINT32      Response[4];
  UINTN       Size;
  EFI_STATUS  Status;
  EFI_STATUS  StopStatus;

  if ((mSdGpd == NULL) || (mSdBd == NULL) || (BlockCount == 0) ||
      (BlockCount > SD_DMA_MAX_BLOCKS))
  {
    return EFI_INVALID_PARAMETER;
  }

  Size     = BlockCount * SD_BLOCK_SIZE;
  Argument = mSdHighCapacity ? (UINT32)Lba : (UINT32)(Lba * SD_BLOCK_SIZE);
  Command  = (BlockCount == 1) ? SD_CMD_READ_SINGLE : SD_CMD_READ_MULTIPLE;
  DataType = (BlockCount == 1) ? MSDC_DTYPE_SINGLE : MSDC_DTYPE_MULTIPLE;

  SdWrite32 (SDC_BLK_NUM, (UINT32)BlockCount);
  Status = SdClearFifo ();
  if (EFI_ERROR (Status)) {
    return Status;
  }

  WriteBackInvalidateDataCacheRange (Buffer, Size);
  Status = SdBuildDescriptors (Buffer, Size);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  // The MT8167 vendor driver selects DMA before issuing the data command.
  SdClearBits (MSDC_CFG, MSDC_CFG_PIO);
  Status = SdSendCommand (
             Command,
             Argument,
             MSDC_RESP_R1,
             DataType,
             SD_BLOCK_SIZE,
             Response
             );
  if (EFI_ERROR (Status)) {
    SdSetBits (MSDC_CFG, MSDC_CFG_PIO);
    return Status;
  }

  SdWrite32 (MSDC_DMA_SA_HIGH, (UINT32)(mSdGpdAddress >> 32) & MSDC_DESC_HIGH_MASK);
  SdWrite32 (MSDC_DMA_SA, (UINT32)mSdGpdAddress);
  SdWrite32 (MSDC_DMA_LEN, 0);
  SdSetBits (MSDC_DMA_CFG, MSDC_DMA_CFG_DECSEN);

  DmaControl  = SdRead32 (MSDC_DMA_CTRL);
  DmaControl &= ~(MSDC_DMA_CTRL_START | MSDC_DMA_CTRL_STOP |
                  MSDC_DMA_CTRL_LASTBUF | MSDC_DMA_CTRL_BURST_MASK);
  DmaControl |= MSDC_DMA_CTRL_MODE | MSDC_DMA_CTRL_BURST_64B;
  SdWrite32 (MSDC_DMA_CTRL, DmaControl);
  SdSetBits (MSDC_DMA_CTRL, MSDC_DMA_CTRL_START);

  Status     = SdWaitDmaComplete ();
  StopStatus = SdStopDma ();
  if (!EFI_ERROR (Status) && EFI_ERROR (StopStatus)) {
    Status = StopStatus;
  }

  if (!EFI_ERROR (Status)) {
    Status = SdWaitNotBusy (FALSE);
  }

  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "[Mt8167Pkg] SD DMA read failed lba=0x%lx blocks=0x%lx: %r int=0x%08x dma_cfg=0x%08x dma_ctrl=0x%08x dma_ca=0x%08x\n",
      Lba,
      (UINT64)BlockCount,
      Status,
      SdRead32 (MSDC_INT),
      SdRead32 (MSDC_DMA_CFG),
      SdRead32 (MSDC_DMA_CTRL),
      SdRead32 (MSDC_DMA_CA)
      ));
    return Status;
  }

  InvalidateDataCacheRange (Buffer, Size);
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
SdInitializeDma (
  VOID
  )
{
  EFI_PHYSICAL_ADDRESS  Address;
  EFI_STATUS            Status;

  Address = MAX_UINT32;
  Status  = gBS->AllocatePages (
                   AllocateMaxAddress,
                   EfiBootServicesData,
                   MSDC_DESCRIPTOR_PAGES,
                   &Address
                   );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "[Mt8167Pkg] SD descriptor allocation failed=%r\n", Status));
    return Status;
  }

  mSdDescriptorPageAddress = Address;
  mSdGpdAddress            = Address;
  mSdBdAddress             = ALIGN_VALUE (
                               Address + (sizeof (MT8167_MSDC_GPD) * 2U),
                               16U
                               );
  mSdGpd = (MT8167_MSDC_GPD *)(UINTN)mSdGpdAddress;
  mSdBd  = (MT8167_MSDC_BD *)(UINTN)mSdBdAddress;

  ZeroMem (mSdDmaProbeReference, sizeof (mSdDmaProbeReference));
  ZeroMem (mSdDmaProbeResult, sizeof (mSdDmaProbeResult));
  Status = SdReadMultipleBlocks (SD_DMA_PROBE_LBA, SD_DMA_PROBE_BLOCKS, mSdDmaProbeReference);
  if (!EFI_ERROR (Status)) {
    Status = SdReadDmaBlocks (SD_DMA_PROBE_LBA, SD_DMA_PROBE_BLOCKS, mSdDmaProbeResult);
  }

  if (!EFI_ERROR (Status) &&
      (CompareMem (mSdDmaProbeReference, mSdDmaProbeResult, sizeof (mSdDmaProbeReference)) != 0))
  {
    Status = EFI_CRC_ERROR;
  }

  if (EFI_ERROR (Status)) {
    (VOID)SdStopDma ();
    (VOID)SdResetHost ();
    (VOID)SdRestoreTransferMode ();
    (VOID)gBS->FreePages (mSdDescriptorPageAddress, MSDC_DESCRIPTOR_PAGES);
    mSdDescriptorPageAddress = 0;
    mSdGpdAddress            = 0;
    mSdBdAddress             = 0;
    mSdGpd                    = NULL;
    mSdBd                     = NULL;
    DEBUG ((DEBUG_ERROR, "[Mt8167Pkg] SD mandatory descriptor DMA probe rejected=%r\n", Status));
    return Status;
  }

  return EFI_SUCCESS;
}

#ifndef A04BR3_EMMC_BUILD
STATIC
EFI_STATUS
SdReadSwitchStatus (
  IN  UINT32  Argument,
  OUT UINT8   StatusBuffer[SD_SWITCH_STATUS_SIZE]
  )
{
  UINT32      Response[4];
  EFI_STATUS  Status;

  SdWrite32 (SDC_BLK_NUM, 1);
  Status = SdClearFifo ();
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = SdSendCommand (
             SD_CMD_SWITCH_FUNC,
             Argument,
             MSDC_RESP_R1,
             MSDC_DTYPE_SINGLE,
             SD_SWITCH_STATUS_SIZE,
             Response
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  return SdReadData (StatusBuffer, SD_SWITCH_STATUS_SIZE);
}

STATIC
UINT32
SdUnstuffBits (
  IN UINT32  Response[4],
  IN UINT32  Start,
  IN UINT32  Size
  )
{
  UINT32  Mask;
  INT32   Offset;
  UINT32  Shift;
  UINT32  Result;

  Mask   = (1U << Size) - 1U;
  Offset = 3 - (INT32)(Start / 32);
  Shift  = Start & 31U;
  Result = Response[Offset] >> Shift;
  if ((Size + Shift >= 32U) && (Offset > 0)) {
    Result |= Response[Offset - 1] << (32U - Shift);
  }

  return Result & Mask;
}

STATIC
EFI_STATUS
SdDecodeCsd (
  IN  UINT32   Csd[4],
  OUT UINT64   *SectorCount
  )
{
  UINT32  CsdStructure;
  UINT32  CSize;
  UINT32  CSizeMult;
  UINT32  ReadBlockLength;

  CsdStructure = SdUnstuffBits (Csd, 126, 2);
  if (CsdStructure == 1) {
    CSize        = SdUnstuffBits (Csd, 48, 22);
    *SectorCount = (UINT64)(CSize + 1U) << 10;
    return EFI_SUCCESS;
  }

  if (CsdStructure == 0) {
    CSize           = SdUnstuffBits (Csd, 62, 12);
    CSizeMult       = SdUnstuffBits (Csd, 47, 3);
    ReadBlockLength = SdUnstuffBits (Csd, 80, 4);
    *SectorCount    = ((UINT64)(CSize + 1U) << (CSizeMult + 2U + ReadBlockLength)) / SD_BLOCK_SIZE;
    return EFI_SUCCESS;
  }

  DEBUG ((DEBUG_ERROR, "[Mt8167Pkg] SD unsupported CSD structure=%u\n", CsdStructure));
  return EFI_UNSUPPORTED;
}
#endif

STATIC
EFI_STATUS
SdPrepareHost (
  VOID
  )
{
  UINT32      Value;
  EFI_STATUS  Status;

  SdUngateClock ();
  SdClearBits (MSDC_CFG, MSDC_CFG_CKPDN);
  Status = SdSetClock (MSDC_INIT_CLOCK_HZ);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "[Mt8167Pkg] SD clock stable timeout: %r\n", Status));
    return Status;
  }

  SdSetBits (MSDC_CFG, MSDC_CFG_MODE | MSDC_CFG_PIO);
  SdClearBits (MSDC_INTEN, 0xffffffffU);
  SdWrite32 (MSDC_INT, SdRead32 (MSDC_INT));

  Status = SdResetHost ();
  if (EFI_ERROR (Status)) {
    return Status;
  }

  SdWrite32 (MSDC_IOCON, 0);
  SdClearBits (EMMC_CFG0, EMMC_CFG0_BOOTSUPP);

  Value  = SdRead32 (SDC_CFG);
  Value &= ~(SDC_CFG_BUSWIDTH_MASK | SDC_CFG_DTOC_MASK);
  Value |= SDC_CFG_BUSWIDTH_1 | SDC_CFG_DTOC_MAX;
  SdWrite32 (SDC_CFG, Value);
  mSdBusWidth      = SDC_CFG_BUSWIDTH_1;
  mSdTargetClockHz = MSDC_INIT_CLOCK_HZ;
  mSdClockHz       = MSDC_INIT_CLOCK_HZ;
  mSdHighSpeed     = FALSE;

  return SdClearFifo ();
}

#ifndef A04BR3_EMMC_BUILD
STATIC
EFI_STATUS
SdSwitchTo4BitBus (
  VOID
  )
{
  UINT32      Response[4];
  EFI_STATUS  Status;

  Status = SdSendAppCommand (
             mSdRca,
             SD_ACMD_SET_BUS_WIDTH,
             2,
             MSDC_RESP_R1,
             Response
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "[Mt8167Pkg] SD ACMD6 4-bit rejected=%r, keeping 1-bit\n", Status));
    SdSetHostBusWidth (SDC_CFG_BUSWIDTH_1);
    return Status;
  }

  SdSetHostBusWidth (SDC_CFG_BUSWIDTH_4);
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
SdSwitchToHighSpeed (
  VOID
  )
{
  EFI_STATUS  Status;
  UINT8       StatusBuffer[SD_SWITCH_STATUS_SIZE];

  ZeroMem (StatusBuffer, sizeof (StatusBuffer));
  Status = SdReadSwitchStatus (
             SD_SWITCH_CHECK | (0x00ffffffU & SD_SWITCH_FUNC_GROUP_1_MASK) | SD_SWITCH_FUNC_HIGH_SPEED,
             StatusBuffer
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "[Mt8167Pkg] SD CMD6 check failed=%r, keeping default speed\n", Status));
    return Status;
  }

  if ((StatusBuffer[13] & SD_SWITCH_STATUS_HS_BIT) == 0) {
    DEBUG ((DEBUG_ERROR, "[Mt8167Pkg] SD high-speed function not advertised status13=0x%02x\n", StatusBuffer[13]));
    return EFI_UNSUPPORTED;
  }

  ZeroMem (StatusBuffer, sizeof (StatusBuffer));
  Status = SdReadSwitchStatus (
             SD_SWITCH_SET | (0x00ffffffU & SD_SWITCH_FUNC_GROUP_1_MASK) | SD_SWITCH_FUNC_HIGH_SPEED,
             StatusBuffer
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "[Mt8167Pkg] SD CMD6 switch failed=%r, keeping default speed\n", Status));
    return Status;
  }

  mSdHighSpeed = TRUE;
  return EFI_SUCCESS;
}
#endif

#ifdef A04BR3_EMMC_BUILD
STATIC
EFI_STATUS
EmmcReadExtCsd (
  OUT UINT8  ExtCsd[MMC_EXT_CSD_SIZE]
  )
{
  UINT32      Response[4];
  EFI_STATUS  Status;

  ZeroMem (ExtCsd, MMC_EXT_CSD_SIZE);
  SdWrite32 (SDC_BLK_NUM, 1);
  Status = SdClearFifo ();
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = SdSendCommand (
             MMC_CMD_SEND_EXT_CSD,
             0,
             MSDC_RESP_R1,
             MSDC_DTYPE_SINGLE,
             MMC_EXT_CSD_SIZE,
             Response
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  return SdReadData (ExtCsd, MMC_EXT_CSD_SIZE);
}

STATIC
EFI_STATUS
EmmcWaitReady (
  VOID
  )
{
  UINT32      Response[4];
  UINT32      Retry;
  UINT32      StatusValue;
  EFI_STATUS  Status;

  for (Retry = 0; Retry < SD_WAIT_LOOPS; ++Retry) {
    Status = SdSendCommand (
               MMC_CMD_SEND_STATUS,
               mSdRca << 16,
               MSDC_RESP_R1,
               MSDC_DTYPE_NONE,
               0,
               Response
               );
    if (EFI_ERROR (Status)) {
      return Status;
    }

    StatusValue = Response[0];
    if ((StatusValue & MMC_R1_SWITCH_ERROR) != 0) {
      DEBUG ((DEBUG_ERROR, "[A04BR3 eMMC] CMD6 switch error status=0x%08x\n", StatusValue));
      return EFI_DEVICE_ERROR;
    }

    if (((StatusValue & MMC_R1_READY_FOR_DATA) != 0) &&
        ((StatusValue & MMC_R1_CURRENT_STATE_MASK) != MMC_R1_CURRENT_STATE_PRG))
    {
      return EFI_SUCCESS;
    }
  }

  DEBUG ((DEBUG_ERROR, "[A04BR3 eMMC] CMD6 status polling timeout\n"));
  return EFI_TIMEOUT;
}

STATIC
EFI_STATUS
EmmcSwitchByte (
  IN UINT8  Index,
  IN UINT8  Value
  )
{
  UINT32      Argument;
  UINT32      Response[4];
  EFI_STATUS  Status;

  Argument = (MMC_SWITCH_MODE_WRITE_BYTE << 24) |
             ((UINT32)Index << 16) |
             ((UINT32)Value << 8);
  Status = SdSendCommand (
             MMC_CMD_SWITCH,
             Argument,
             MSDC_RESP_R1B,
             MSDC_DTYPE_NONE,
             0,
             Response
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "[A04BR3 eMMC] CMD6 index=%u value=%u failed=%r\n",
      Index,
      Value,
      Status
      ));
    return Status;
  }

  return EmmcWaitReady ();
}
#endif

STATIC
EFI_STATUS
SdTuneTransferMode (
  VOID
  )
{
  EFI_STATUS  Status;

#ifdef A04BR3_EMMC_BUILD
  if ((mEmmcExtCsd[MMC_EXT_CSD_CARD_TYPE] & MMC_EXT_CSD_CARD_TYPE_52MHZ) == 0) {
    DEBUG ((
      DEBUG_ERROR,
      "[A04BR3 eMMC] 52MHz High Speed unsupported CARD_TYPE=0x%02x\n",
      mEmmcExtCsd[MMC_EXT_CSD_CARD_TYPE]
      ));
    return EFI_UNSUPPORTED;
  }

  Status = EmmcSwitchByte (MMC_EXT_CSD_BUS_WIDTH, MMC_EXT_CSD_BUS_WIDTH_8);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  SdSetHostBusWidth (SDC_CFG_BUSWIDTH_8);

  Status = EmmcSwitchByte (MMC_EXT_CSD_HS_TIMING, MMC_EXT_CSD_HS_TIMING_HS);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = SdSetClock (MSDC_HIGH_SPEED_HZ);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "[A04BR3 eMMC] 52MHz clock request failed=%r\n", Status));
    return Status;
  }

  Status = EmmcReadExtCsd (mEmmcExtCsd);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "[A04BR3 eMMC] High Speed EXT_CSD verification read failed=%r\n", Status));
    return Status;
  }

  if (mEmmcExtCsd[MMC_EXT_CSD_HS_TIMING] != MMC_EXT_CSD_HS_TIMING_HS) {
    DEBUG ((
      DEBUG_ERROR,
      "[A04BR3 eMMC] High Speed verification mismatch HS_TIMING=%u\n",
      mEmmcExtCsd[MMC_EXT_CSD_HS_TIMING]
      ));
    return EFI_DEVICE_ERROR;
  }

  mSdHighSpeed = TRUE;
  DEBUG ((
    DEBUG_ERROR,
    "[A04BR3 eMMC] High Speed active clk=%u width=8 CARD_TYPE=0x%02x BUS_WIDTH_READBACK=%u\n",
    mSdClockHz,
    mEmmcExtCsd[MMC_EXT_CSD_CARD_TYPE],
    mEmmcExtCsd[MMC_EXT_CSD_BUS_WIDTH]
    ));
  return EFI_SUCCESS;
#else
  (VOID)SdSwitchTo4BitBus ();

  Status = SdSwitchToHighSpeed ();
  if (!EFI_ERROR (Status)) {
    Status = SdSetClock (MSDC_HIGH_SPEED_HZ);
    if (!EFI_ERROR (Status)) {
      return EFI_SUCCESS;
    }

    DEBUG ((DEBUG_WARN, "[Mt8167Pkg] SD high-speed clock set failed=%r\n", Status));
  }

  Status = SdSetClock (MSDC_DEFAULT_TRANSFER_HZ);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "[Mt8167Pkg] SD default transfer clock failed=%r, falling back to init clock\n", Status));
    (VOID)SdSetClock (MSDC_INIT_CLOCK_HZ);
  }
  return EFI_SUCCESS;
#endif

}

STATIC
EFI_STATUS
SdInitializeCard (
  OUT UINT64  *SectorCount
  )
{
#ifdef A04BR3_EMMC_BUILD
  UINT32      Cid[4];
  UINT32      Ocr;
  UINT32      Response[4];
  UINT32      Retry;
  EFI_STATUS  Status;

  Status = SdSendCommand (SD_CMD_GO_IDLE, 0, MSDC_RESP_NONE, MSDC_DTYPE_NONE, 0, NULL);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  gBS->Stall (1000);

  Ocr = 0;
  for (Retry = 0; Retry < SD_ACMD41_RETRIES; ++Retry) {
    Status = SdSendCommand (
               MMC_CMD_SEND_OP_COND,
               MMC_OCR_SECTOR_MODE | MMC_OCR_VOLTAGE_27_36,
               MSDC_RESP_R3,
               MSDC_DTYPE_NONE,
               0,
               Response
               );
    if (EFI_ERROR (Status)) {
      return Status;
    }

    Ocr = Response[0];
    if ((Ocr & SD_OCR_BUSY) != 0) {
      break;
    }

    gBS->Stall (10000);
  }

  if ((Ocr & SD_OCR_BUSY) == 0) {
    DEBUG ((DEBUG_ERROR, "[A04BR3 eMMC] CMD1 power-up timeout OCR=0x%08x\n", Ocr));
    return EFI_TIMEOUT;
  }

  mSdHighCapacity = ((Ocr & MMC_OCR_SECTOR_MODE) != 0);
  if (!mSdHighCapacity) {
    DEBUG ((DEBUG_ERROR, "[A04BR3 eMMC] byte-addressed devices are unsupported OCR=0x%08x\n", Ocr));
    return EFI_UNSUPPORTED;
  }

  Status = SdSendCommand (
             SD_CMD_ALL_SEND_CID,
             0,
             MSDC_RESP_R2,
             MSDC_DTYPE_NONE,
             0,
             Cid
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  mSdRca = 1;
  Status = SdSendCommand (
             SD_CMD_SEND_RELATIVE_ADDR,
             mSdRca << 16,
             MSDC_RESP_R1,
             MSDC_DTYPE_NONE,
             0,
             Response
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = SdSendCommand (
             SD_CMD_SEND_CSD,
             mSdRca << 16,
             MSDC_RESP_R2,
             MSDC_DTYPE_NONE,
             0,
             Response
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = SdSendCommand (
             SD_CMD_SELECT_CARD,
             mSdRca << 16,
             MSDC_RESP_R1,
             MSDC_DTYPE_NONE,
             0,
             Response
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = EmmcReadExtCsd (mEmmcExtCsd);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  *SectorCount =
    ((UINT64)mEmmcExtCsd[MMC_EXT_CSD_SEC_COUNT]) |
    ((UINT64)mEmmcExtCsd[MMC_EXT_CSD_SEC_COUNT + 1U] << 8) |
    ((UINT64)mEmmcExtCsd[MMC_EXT_CSD_SEC_COUNT + 2U] << 16) |
    ((UINT64)mEmmcExtCsd[MMC_EXT_CSD_SEC_COUNT + 3U] << 24);
  if (*SectorCount == 0) {
    DEBUG ((DEBUG_ERROR, "[A04BR3 eMMC] EXT_CSD SEC_COUNT is zero\n"));
    return EFI_DEVICE_ERROR;
  }

  Status = SdSendCommand (
             SD_CMD_SET_BLOCKLEN,
             SD_BLOCK_SIZE,
             MSDC_RESP_R1,
             MSDC_DTYPE_NONE,
             0,
             Response
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  DEBUG ((
    DEBUG_ERROR,
    "[A04BR3 eMMC] identified OCR=0x%08x CID=%08x:%08x:%08x:%08x EXT_CSD_REV=%u sectors=0x%lx\n",
    Ocr,
    Cid[0],
    Cid[1],
    Cid[2],
    Cid[3],
    mEmmcExtCsd[MMC_EXT_CSD_REV],
    *SectorCount
    ));
  return SdTuneTransferMode ();
#else
  UINT32      Response[4];
  UINT32      Retry;
  UINT32      Ocr;
  UINT32      Rca;
  EFI_STATUS  Status;

  Status = SdSendCommand (SD_CMD_GO_IDLE, 0, MSDC_RESP_NONE, MSDC_DTYPE_NONE, 0, NULL);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  gBS->Stall (1000);

  Status = SdSendCommand (SD_CMD_SEND_IF_COND, 0x1aa, MSDC_RESP_R7, MSDC_DTYPE_NONE, 0, Response);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Ocr = 0;
  for (Retry = 0; Retry < SD_ACMD41_RETRIES; ++Retry) {
    Status = SdSendAppCommand (
               0,
               SD_ACMD_SD_SEND_OP_COND,
               SD_OCR_HCS | SD_OCR_32_34V,
               MSDC_RESP_R3,
               Response
               );
    if (EFI_ERROR (Status)) {
      return Status;
    }

    Ocr = Response[0];
    if ((Ocr & SD_OCR_BUSY) != 0) {
      break;
    }

    gBS->Stall (1000);
  }

  if ((Ocr & SD_OCR_BUSY) == 0) {
    return EFI_TIMEOUT;
  }

  mSdHighCapacity = ((Ocr & SD_OCR_CCS) != 0);

  Status = SdSendCommand (SD_CMD_ALL_SEND_CID, 0, MSDC_RESP_R2, MSDC_DTYPE_NONE, 0, Response);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = SdSendCommand (
             SD_CMD_SEND_RELATIVE_ADDR,
             0,
             MSDC_RESP_R1,
             MSDC_DTYPE_NONE,
             0,
             Response
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Rca    = Response[0] >> 16;
  mSdRca = Rca;
  if (Rca == 0) {
    return EFI_DEVICE_ERROR;
  }

  Status = SdSendCommand (
             SD_CMD_SEND_CSD,
             Rca << 16,
             MSDC_RESP_R2,
             MSDC_DTYPE_NONE,
             0,
             Response
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = SdDecodeCsd (Response, SectorCount);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = SdSendCommand (
             SD_CMD_SELECT_CARD,
             Rca << 16,
             MSDC_RESP_R1,
             MSDC_DTYPE_NONE,
             0,
             Response
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = SdSendCommand (
             SD_CMD_SET_BLOCKLEN,
             SD_BLOCK_SIZE,
             MSDC_RESP_R1,
             MSDC_DTYPE_NONE,
             0,
             Response
             );
  if (!EFI_ERROR (Status)) {
    SdTuneTransferMode ();
  }

  return Status;
#endif
}

STATIC
EFI_STATUS
SdReadBlocks (
  IN  EFI_BLOCK_IO_PROTOCOL  *This,
  IN  UINT32                 MediaId,
  IN  EFI_LBA                Lba,
  IN  UINTN                  BufferSize,
  OUT VOID                   *Buffer
  )
{
  EFI_STATUS  Status;
  UINT8       *Bytes;
  UINTN       BlockCount;
  UINTN       BlocksThisPass;

  if (MediaId != mSdMedia.MediaId) {
    return EFI_MEDIA_CHANGED;
  }

  if (Buffer == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (((UINTN)Buffer & (mSdMedia.IoAlign - 1U)) != 0) {
    return EFI_INVALID_PARAMETER;
  }

  if ((BufferSize % mSdMedia.BlockSize) != 0) {
    return EFI_BAD_BUFFER_SIZE;
  }

  if (BufferSize == 0) {
    return EFI_SUCCESS;
  }

  if (Lba > mSdMedia.LastBlock) {
    return EFI_INVALID_PARAMETER;
  }

  if (((BufferSize / mSdMedia.BlockSize) - 1) > (mSdMedia.LastBlock - Lba)) {
    return EFI_INVALID_PARAMETER;
  }

  Bytes = Buffer;
  BlockCount = BufferSize / mSdMedia.BlockSize;
  while (BufferSize != 0) {
    BlocksThisPass = BlockCount;
    if (BlocksThisPass > SD_DMA_MAX_BLOCKS) {
      BlocksThisPass = SD_DMA_MAX_BLOCKS;
    }

    Status = SdReadDmaBlocks (Lba, BlocksThisPass, Bytes);

    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "[Mt8167Pkg] SD descriptor DMA ReadBlocks LBA=0x%lx blocks=0x%lx failed: %r\n",
        Lba,
        (UINT64)BlocksThisPass,
        Status
        ));
      return Status;
    }

    Lba          += BlocksThisPass;
    Bytes        += BlocksThisPass * mSdMedia.BlockSize;
    BufferSize   -= BlocksThisPass * mSdMedia.BlockSize;
    BlockCount   -= BlocksThisPass;
  }

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
SdResetBlocks (
  IN EFI_BLOCK_IO_PROTOCOL  *This,
  IN BOOLEAN                ExtendedVerification
  )
{
  EFI_STATUS  Status;

  Status = SdResetHost ();
  if (EFI_ERROR (Status)) {
    return Status;
  }

  return SdRestoreTransferMode ();
}

STATIC
EFI_STATUS
SdWriteBlocks (
  IN EFI_BLOCK_IO_PROTOCOL  *This,
  IN UINT32                 MediaId,
  IN EFI_LBA                Lba,
  IN UINTN                  BufferSize,
  IN VOID                   *Buffer
  )
{
  return EFI_WRITE_PROTECTED;
}

STATIC
EFI_STATUS
SdFlushBlocks (
  IN EFI_BLOCK_IO_PROTOCOL  *This
  )
{
  return EFI_SUCCESS;
}

STATIC EFI_BLOCK_IO_MEDIA  mSdMedia = {
  1,      // MediaId
#ifdef A04BR3_EMMC_BUILD
  FALSE,  // RemovableMedia
#else
  TRUE,   // RemovableMedia
#endif
  TRUE,   // MediaPresent
  FALSE,  // LogicalPartition
  TRUE,   // ReadOnly
  FALSE,  // WriteCaching
  512,    // BlockSize
  4,      // IoAlign
  0       // LastBlock, filled from CSD
};

STATIC EFI_BLOCK_IO_PROTOCOL  mSdBlockIo = {
  EFI_BLOCK_IO_PROTOCOL_REVISION,
  &mSdMedia,
  SdResetBlocks,
  SdReadBlocks,
  SdWriteBlocks,
  SdFlushBlocks
};

EFI_STATUS
EFIAPI
SdCardBlockIoDxeEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;
  UINT64      SectorCount;

  // The watchdog is owned by SEC (MT8167Pkg PlatformSecLib) and, with the
  // unattended test cycle, by A04BR3TestCycleDxe; this driver leaves it alone.

  Status = SdPrepareHost ();
  if (EFI_ERROR (Status)) {
#ifdef A04BR3_EMMC_BUILD
    DEBUG ((DEBUG_ERROR, "[A04BR3 eMMC] MSDC0 host prepare failed: %r\n", Status));
#else
    DEBUG ((DEBUG_ERROR, "[Mt8167Pkg] SD host prepare failed: %r\n", Status));
#endif
    return EFI_SUCCESS;
  }

  Status = SdInitializeCard (&SectorCount);
  if (EFI_ERROR (Status)) {
#ifdef A04BR3_EMMC_BUILD
    DEBUG ((DEBUG_ERROR, "[A04BR3 eMMC] initialization failed: %r\n", Status));
#else
    DEBUG ((DEBUG_ERROR, "[Mt8167Pkg] SD init failed: %r\n", Status));
#endif
    return EFI_SUCCESS;
  }

  if (SectorCount == 0) {
    DEBUG ((DEBUG_ERROR, "[Mt8167Pkg] SD sector count is zero\n"));
    return EFI_SUCCESS;
  }

  mSdMedia.BlockSize = SD_BLOCK_SIZE;
  mSdMedia.LastBlock = SectorCount - 1;

  Status = SdInitializeDma ();
  if (EFI_ERROR (Status)) {
#ifdef A04BR3_EMMC_BUILD
    DEBUG ((DEBUG_ERROR, "[A04BR3 eMMC] DMA verification failed=%r; BlockIo disabled\n", Status));
#else
    DEBUG ((DEBUG_ERROR, "[Mt8167Pkg] SD descriptor DMA unavailable=%r; BlockIo disabled\n", Status));
#endif
    return Status;
  }

#ifdef A04BR3_EMMC_BUILD
  ZeroMem (mEmmcGptProbe, sizeof (mEmmcGptProbe));
  Status = SdReadDmaBlocks (0, 2, mEmmcGptProbe);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "[A04BR3 eMMC] GPT probe read failed=%r; BlockIo disabled\n", Status));
    return Status;
  }

  if (CompareMem (&mEmmcGptProbe[512], "EFI PART", 8) != 0) {
    DEBUG ((DEBUG_ERROR, "[A04BR3 eMMC] GPT header signature missing; BlockIo disabled\n"));
    return EFI_VOLUME_CORRUPTED;
  }

  DEBUG ((
    DEBUG_ERROR,
    "[A04BR3 eMMC] GPT verified header-lba=0x%lx backup-lba=0x%lx entries-lba=0x%lx\n",
    ReadUnaligned64 ((UINT64 *)&mEmmcGptProbe[512 + 24]),
    ReadUnaligned64 ((UINT64 *)&mEmmcGptProbe[512 + 32]),
    ReadUnaligned64 ((UINT64 *)&mEmmcGptProbe[512 + 72])
    ));
#endif

  mSdHandle = NULL;
  Status = gBS->InstallMultipleProtocolInterfaces (
                  &mSdHandle,
                  &gEfiDevicePathProtocolGuid,
                  &mSdDevicePath,
                  &gEfiBlockIoProtocolGuid,
                  &mSdBlockIo,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
#ifdef A04BR3_EMMC_BUILD
    DEBUG ((DEBUG_ERROR, "[A04BR3 eMMC] BlockIo install failed: %r\n", Status));
#else
    DEBUG ((DEBUG_ERROR, "[Mt8167Pkg] SD BlockIo install failed: %r\n", Status));
#endif
    return Status;
  }

#ifdef A04BR3_EMMC_BUILD
  DEBUG ((
    DEBUG_ERROR,
    "[A04BR3 eMMC] BlockIo ready sectors=0x%lx clk=%u width=%u highspeed=%u read-only=1 dma=descriptor-direct\n",
    SectorCount,
    mSdClockHz,
    (mSdBusWidth == SDC_CFG_BUSWIDTH_8) ? 8U : 1U,
    mSdHighSpeed
    ));
#else
  DEBUG ((
    DEBUG_ERROR,
    "[Mt8167Pkg] SD ready sectors=0x%lx clk=%u width=%u highspeed=%u dma=descriptor-direct max-blocks=%u\n",
    SectorCount,
    mSdClockHz,
    (mSdBusWidth == SDC_CFG_BUSWIDTH_4) ? 4U : 1U,
    mSdHighSpeed,
    SD_DMA_MAX_BLOCKS
    ));
#endif

  return EFI_SUCCESS;
}
