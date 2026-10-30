/** @file
  Initialize the A04BR3 AUO WXGA panel and MT8167 display pipeline.

  Register values and sequencing are derived from the A04BR3 stock LK binary,
  the A04BR3 kernel panel driver, and live register readback from this board.

  Ported from mouseos/edk2-mtk (Platform/Benesse/A04BR3Pkg/Drivers/
  DisplayInitDxe, commit 1982d888c765). Mu-Silicium change: the frame buffer
  address comes from the "Display Reserved" memory region and its size from
  gSiliciumPkgTokenSpaceGuid.PcdFrameBufferWidth/Height instead of the
  edk2-mtk MIPI frame buffer PCDs. SimpleFbDxe publishes the GOP on the
  same region after this driver has started the scan-out.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/MemoryMapHelperLib.h>
#include <Library/PcdLib.h>
#include <Library/TimerLib.h>

#define A04BR3_WIDTH                 800U
#define A04BR3_HEIGHT                1280U
#define A04BR3_PITCH                 (A04BR3_WIDTH * sizeof (UINT32))
#define A04BR3_FRAME_SIZE            (A04BR3_PITCH * A04BR3_HEIGHT)

#define MT8167_GPIO_BASE             0x10005000U
#define GPIO_DIR_BASE                (MT8167_GPIO_BASE + 0x000U)
#define GPIO_DOUT_BASE               (MT8167_GPIO_BASE + 0x100U)
#define GPIO_MODE_BASE               (MT8167_GPIO_BASE + 0x300U)
#define GPIO_LCD_BACKLIGHT           1U
#define GPIO_LCD_RESET               66U
#define GPIO_LCD_STANDBY             67U

#define MT8167_PWRAP_BASE            0x1000F000U
#define PWRAP_WACS2_CMD              (MT8167_PWRAP_BASE + 0x0A0U)
#define PWRAP_WACS2_RDATA            (MT8167_PWRAP_BASE + 0x0A4U)
#define PWRAP_WACS2_VLDCLR           (MT8167_PWRAP_BASE + 0x0A8U)
#define PWRAP_FSM_IDLE               0U
#define PWRAP_FSM_WFVLDCLR           6U
#define PWRAP_FSM_MASK               0x7U
#define PWRAP_FSM_SHIFT              16U
#define PWRAP_INIT_DONE              BIT21
#define PWRAP_TIMEOUT_US             10000U
#define MT6392_DIGLDO_CON8           0x050CU
#define MT6392_DIGLDO_CON29          0x0532U
#define MT6392_VGP2_ENABLE           BIT15
#define MT6392_VGP2_VSEL_MASK        (BIT5 | BIT6 | BIT7)
#define MT6392_VGP2_VSEL_1V8         (BIT5 | BIT6)

#define MT8167_TOPCKGEN_BASE         0x10000000U
#define TOP_CG_STA0                  (MT8167_TOPCKGEN_BASE + 0x020U)
#define TOP_CG_SET0                  (MT8167_TOPCKGEN_BASE + 0x050U)
#define TOP_CG_CLR0                  (MT8167_TOPCKGEN_BASE + 0x080U)
#define TOP_CG_SMI_MM                BIT9
#define TOP_CG_MIPI_26M_DBG          BIT4

#define MT8167_INFRACFG_BASE         0x10001000U
#define INFRA_TOPAXI_PROTECTEN       (MT8167_INFRACFG_BASE + 0x220U)
#define INFRA_TOPAXI_PROTECTSTA1     (MT8167_INFRACFG_BASE + 0x228U)
#define DISP_BUS_PROTECT_MASK        (BIT1 | BIT11)

#define MT8167_SPM_BASE              0x10006000U
#define SPM_DIS_PWR_CON              (MT8167_SPM_BASE + 0x23CU)
#define SPM_PWR_STATUS               (MT8167_SPM_BASE + 0x60CU)
#define SPM_PWR_STATUS_2ND           (MT8167_SPM_BASE + 0x610U)
#define DISP_PWR_STATUS              BIT3
#define DISP_PWR_RST_B               BIT0
#define DISP_PWR_ISO                 BIT1
#define DISP_PWR_ON                  BIT2
#define DISP_PWR_ON_2ND              BIT3
#define DISP_PWR_CLK_DIS             BIT4
#define DISP_SRAM_PDN                (BIT8 | BIT9 | BIT10 | BIT11)
#define DISP_SRAM_PDN_ACK            BIT12
#define DISPLAY_POWER_TIMEOUT_US     100000U

#define MT8167_MMSYS_BASE            0x14000000U
#define MMSYS_OVL0_MOUT_EN           (MT8167_MMSYS_BASE + 0x030U)
#define MMSYS_DITHER_MOUT_EN         (MT8167_MMSYS_BASE + 0x038U)
#define MMSYS_COLOR0_SEL_IN          (MT8167_MMSYS_BASE + 0x058U)
#define MMSYS_DSI0_SEL_IN            (MT8167_MMSYS_BASE + 0x064U)
#define MMSYS_RDMA0_SOUT_SEL         (MT8167_MMSYS_BASE + 0x06CU)
#define MMSYS_CG_CON0                (MT8167_MMSYS_BASE + 0x100U)
#define MMSYS_CG_CLR0                (MT8167_MMSYS_BASE + 0x108U)
#define MMSYS_CG_CON1                (MT8167_MMSYS_BASE + 0x110U)
#define MMSYS_CG_CLR1                (MT8167_MMSYS_BASE + 0x118U)
#define A04BR3_MMSYS_CG0             0x0007CC03U
#define A04BR3_MMSYS_CG1             (BIT2 | BIT3)

#define MT8167_OVL0_BASE             0x14007000U
#define OVL_INTEN                    (MT8167_OVL0_BASE + 0x004U)
#define OVL_INTSTA                   (MT8167_OVL0_BASE + 0x008U)
#define OVL_EN                       (MT8167_OVL0_BASE + 0x00CU)
#define OVL_ROI_SIZE                 (MT8167_OVL0_BASE + 0x020U)
#define OVL_DATAPATH_CON             (MT8167_OVL0_BASE + 0x024U)
#define OVL_ROI_BGCLR                (MT8167_OVL0_BASE + 0x028U)
#define OVL_SRC_CON                  (MT8167_OVL0_BASE + 0x02CU)
#define OVL_L0_CON                   (MT8167_OVL0_BASE + 0x030U)
#define OVL_L0_SRC_SIZE              (MT8167_OVL0_BASE + 0x038U)
#define OVL_L0_OFFSET                (MT8167_OVL0_BASE + 0x03CU)
#define OVL_L0_PITCH                 (MT8167_OVL0_BASE + 0x044U)
#define OVL_L0_RDMA_CTRL             (MT8167_OVL0_BASE + 0x0C0U)
#define OVL_L0_ADDR                  (MT8167_OVL0_BASE + 0xF40U)
#define OVL_LAYER_STRIDE             0x20U

#define MT8167_RDMA0_BASE            0x14009000U
#define RDMA_GLOBAL_CON              (MT8167_RDMA0_BASE + 0x010U)
#define RDMA_SIZE_CON_0              (MT8167_RDMA0_BASE + 0x014U)
#define RDMA_SIZE_CON_1              (MT8167_RDMA0_BASE + 0x018U)
#define RDMA_MEM_CON                 (MT8167_RDMA0_BASE + 0x024U)
#define RDMA_MEM_SRC_PITCH           (MT8167_RDMA0_BASE + 0x02CU)
#define RDMA_FIFO_CON                (MT8167_RDMA0_BASE + 0x040U)
#define RDMA_MEM_START_ADDR          (MT8167_RDMA0_BASE + 0xF00U)

#define MT8167_COLOR0_BASE           0x1400C000U
#define COLOR_CFG_MAIN               (MT8167_COLOR0_BASE + 0x400U)
#define COLOR_START                  (MT8167_COLOR0_BASE + 0xC00U)
#define COLOR_INTERNAL_IP_WIDTH      (MT8167_COLOR0_BASE + 0xC50U)
#define COLOR_INTERNAL_IP_HEIGHT     (MT8167_COLOR0_BASE + 0xC54U)
#define COLOR_CM1_EN                 (MT8167_COLOR0_BASE + 0xC60U)
#define COLOR_CM2_EN                 (MT8167_COLOR0_BASE + 0xCA0U)

#define MT8167_CCORR_BASE            0x1400D000U
#define CCORR_EN                     (MT8167_CCORR_BASE + 0x000U)
#define CCORR_CFG                    (MT8167_CCORR_BASE + 0x020U)
#define CCORR_SIZE                   (MT8167_CCORR_BASE + 0x030U)

#define MT8167_AAL_BASE              0x1400E000U
#define AAL_EN                       (MT8167_AAL_BASE + 0x000U)
#define AAL_CFG                      (MT8167_AAL_BASE + 0x020U)
#define AAL_SIZE                     (MT8167_AAL_BASE + 0x030U)

#define MT8167_GAMMA_BASE            0x1400F000U
#define GAMMA_EN                     (MT8167_GAMMA_BASE + 0x000U)
#define GAMMA_SIZE                   (MT8167_GAMMA_BASE + 0x030U)

#define MT8167_DITHER_BASE           0x14010000U
#define DITHER_EN                    (MT8167_DITHER_BASE + 0x000U)
#define DITHER_CFG                   (MT8167_DITHER_BASE + 0x020U)
#define DITHER_SIZE                  (MT8167_DITHER_BASE + 0x030U)
#define DITHER_REG(Index)            (MT8167_DITHER_BASE + 0x100U + ((Index) * 4U))

#define MT8167_DSI0_BASE             0x14012000U
#define DSI_START                    (MT8167_DSI0_BASE + 0x000U)
#define DSI_INTEN                    (MT8167_DSI0_BASE + 0x008U)
#define DSI_INTSTA                   (MT8167_DSI0_BASE + 0x00CU)
#define DSI_CON_CTRL                 (MT8167_DSI0_BASE + 0x010U)
#define DSI_MODE_CTRL                (MT8167_DSI0_BASE + 0x014U)
#define DSI_TXRX_CTRL                (MT8167_DSI0_BASE + 0x018U)
#define DSI_PSCTRL                   (MT8167_DSI0_BASE + 0x01CU)
#define DSI_VSA_NL                   (MT8167_DSI0_BASE + 0x020U)
#define DSI_VBP_NL                   (MT8167_DSI0_BASE + 0x024U)
#define DSI_VFP_NL                   (MT8167_DSI0_BASE + 0x028U)
#define DSI_VACT_NL                  (MT8167_DSI0_BASE + 0x02CU)
#define DSI_HSA_WC                   (MT8167_DSI0_BASE + 0x050U)
#define DSI_HBP_WC                   (MT8167_DSI0_BASE + 0x054U)
#define DSI_HFP_WC                   (MT8167_DSI0_BASE + 0x058U)
#define DSI_CMDQ_SIZE                (MT8167_DSI0_BASE + 0x060U)
#define DSI_PHY_LCPAT                (MT8167_DSI0_BASE + 0x100U)
#define DSI_PHY_LCCON                (MT8167_DSI0_BASE + 0x104U)
#define DSI_PHY_LD0CON               (MT8167_DSI0_BASE + 0x108U)
#define DSI_PHY_TIMECON0             (MT8167_DSI0_BASE + 0x110U)
#define DSI_PHY_TIMECON1             (MT8167_DSI0_BASE + 0x114U)
#define DSI_PHY_TIMECON2             (MT8167_DSI0_BASE + 0x118U)
#define DSI_PHY_TIMECON3             (MT8167_DSI0_BASE + 0x11CU)
#define DSI_PHY_TIMECON4             (MT8167_DSI0_BASE + 0x120U)
#define DSI_VM_CMD_CON               (MT8167_DSI0_BASE + 0x130U)
#define DSI_CMDQ0                    (MT8167_DSI0_BASE + 0x180U)
#define DSI_BUSY                     BIT31

#define MT8167_MIPITX_BASE           0x14018000U
#define MIPITX_DSI0_CON              (MT8167_MIPITX_BASE + 0x000U)
#define MIPITX_CLOCK_LANE            (MT8167_MIPITX_BASE + 0x004U)
#define MIPITX_DATA_LANE0            (MT8167_MIPITX_BASE + 0x008U)
#define MIPITX_DATA_LANE1            (MT8167_MIPITX_BASE + 0x00CU)
#define MIPITX_DATA_LANE2            (MT8167_MIPITX_BASE + 0x010U)
#define MIPITX_DATA_LANE3            (MT8167_MIPITX_BASE + 0x014U)
#define MIPITX_TOP_CON               (MT8167_MIPITX_BASE + 0x040U)
#define MIPITX_BG_CON                (MT8167_MIPITX_BASE + 0x044U)
#define MIPITX_PLL_CON0              (MT8167_MIPITX_BASE + 0x050U)
#define MIPITX_PLL_CON1              (MT8167_MIPITX_BASE + 0x054U)
#define MIPITX_PLL_CON2              (MT8167_MIPITX_BASE + 0x058U)
#define MIPITX_PLL_CON3              (MT8167_MIPITX_BASE + 0x05CU)
#define MIPITX_PLL_CHG               (MT8167_MIPITX_BASE + 0x060U)
#define MIPITX_PLL_TOP               (MT8167_MIPITX_BASE + 0x064U)
#define MIPITX_PLL_PWR               (MT8167_MIPITX_BASE + 0x068U)
#define MIPITX_PHY_SEL               (MT8167_MIPITX_BASE + 0x07CU)
#define MIPITX_SW_CTRL               (MT8167_MIPITX_BASE + 0x080U)
#define MIPITX_SW_CTRL_CON0          (MT8167_MIPITX_BASE + 0x084U)
#define MIPITX_SW_CTRL_CON1          (MT8167_MIPITX_BASE + 0x088U)
#define MIPITX_PLL_POWER_ON          BIT0
#define MIPITX_PLL_ISOLATE           BIT1
#define MIPITX_PLL_POWER_ACK         BIT8

#define MT8167_MUTEX_BASE            0x14015000U
#define MUTEX0_EN                    (MT8167_MUTEX_BASE + 0x020U)
#define MUTEX0_RST                   (MT8167_MUTEX_BASE + 0x028U)
#define MUTEX0_MOD                   (MT8167_MUTEX_BASE + 0x02CU)
#define MUTEX0_SOF                   (MT8167_MUTEX_BASE + 0x030U)

#define A04BR3_FRAME_SIZE_REG        ((A04BR3_HEIGHT << 16) | A04BR3_WIDTH)
#define A04BR3_RELAY_SIZE_REG        ((A04BR3_WIDTH << 16) | A04BR3_HEIGHT)
#define A04BR3_COLOR_CFG_WRITE       0x200032BCU
#define A04BR3_COLOR_CFG_READBACK    0x0000209CU
#define A04BR3_OVL_LAYER_CON         0x000021FFU
//
// OVL_CON bits (Linux drivers/gpu/drm/mediatek/mtk_disp_ovl.c): CLRFMT [15:12]
// (2 = RGBA8888), AEN bit 8 (use the per-pixel alpha byte), ALPHA [7:0]
// (constant alpha). The GOP published on layer 0 is
// PixelBlueGreenRedReserved8BitPerColor: the fourth byte is reserved and UEFI
// drawing code writes 0 there (UniXNU menu, console), so with AEN the whole
// GOP surface is transparent and only the OVL background is scanned out.
// Layer 0 therefore uses the constant alpha 0xFF without AEN, as Linux does
// for the X (no alpha) formats. Layer 3 keeps the stock per-pixel alpha.
//
#define A04BR3_OVL_CON_AEN           BIT8
#define A04BR3_OVL_GOP_LAYER_CON     (A04BR3_OVL_LAYER_CON & ~A04BR3_OVL_CON_AEN)
#define A04BR3_OVL_LAYER_PITCH       0x00FF0C80U
#define A04BR3_OVL_ACTIVE_LAYERS     (BIT0 | BIT3)
#define A04BR3_MUTEX_MODULES         0x0002F940U

STATIC
VOID
FailClosed (
  IN CONST CHAR8  *Stage
  )
{
  DEBUG ((DEBUG_ERROR, "[A04BR3 DISP] FAIL CLOSED: %a\n", Stage));
  CpuDeadLoop ();
}

STATIC
BOOLEAN
WaitForRegister (
  IN UINTN   Address,
  IN UINT32  Mask,
  IN UINT32  Expected
  )
{
  UINTN  Timeout;

  for (Timeout = 0; Timeout < DISPLAY_POWER_TIMEOUT_US; Timeout++) {
    if ((MmioRead32 (Address) & Mask) == Expected) {
      return TRUE;
    }

    MicroSecondDelay (1);
  }

  return FALSE;
}

STATIC
VOID
EnableDisplayPowerAndTopClocks (
  VOID
  )
{
  UINT32  PowerControl;
  UINT32  Protection;

  // MT8167 clk-mt8167: SMI_MM is active-low; MIPI_26M_DBG is active-high.
  MmioWrite32 (TOP_CG_CLR0, TOP_CG_SMI_MM);
  MmioWrite32 (TOP_CG_SET0, TOP_CG_MIPI_26M_DBG);
  if (!WaitForRegister (TOP_CG_STA0, TOP_CG_SMI_MM, 0U) ||
      !WaitForRegister (TOP_CG_STA0, TOP_CG_MIPI_26M_DBG, TOP_CG_MIPI_26M_DBG))
  {
    FailClosed ("display top clocks");
  }

  // Match the MT8167 SCPSYS DISP-domain power-on sequence.
  PowerControl  = MmioRead32 (SPM_DIS_PWR_CON);
  PowerControl |= DISP_PWR_ON;
  MmioWrite32 (SPM_DIS_PWR_CON, PowerControl);
  PowerControl |= DISP_PWR_ON_2ND;
  MmioWrite32 (SPM_DIS_PWR_CON, PowerControl);
  if (!WaitForRegister (SPM_PWR_STATUS, DISP_PWR_STATUS, DISP_PWR_STATUS) ||
      !WaitForRegister (SPM_PWR_STATUS_2ND, DISP_PWR_STATUS, DISP_PWR_STATUS))
  {
    FailClosed ("DISP power ACK");
  }

  PowerControl &= ~DISP_PWR_CLK_DIS;
  MmioWrite32 (SPM_DIS_PWR_CON, PowerControl);
  PowerControl &= ~DISP_PWR_ISO;
  MmioWrite32 (SPM_DIS_PWR_CON, PowerControl);
  PowerControl |= DISP_PWR_RST_B;
  MmioWrite32 (SPM_DIS_PWR_CON, PowerControl);
  PowerControl &= ~DISP_SRAM_PDN;
  MmioWrite32 (SPM_DIS_PWR_CON, PowerControl);
  if (!WaitForRegister (SPM_DIS_PWR_CON, DISP_SRAM_PDN_ACK, 0U)) {
    FailClosed ("DISP SRAM ACK");
  }

  Protection  = MmioRead32 (INFRA_TOPAXI_PROTECTEN);
  Protection &= ~DISP_BUS_PROTECT_MASK;
  MmioWrite32 (INFRA_TOPAXI_PROTECTEN, Protection);
  if (!WaitForRegister (INFRA_TOPAXI_PROTECTSTA1, DISP_BUS_PROTECT_MASK, 0U)) {
    FailClosed ("DISP bus protection");
  }

  PowerControl = MmioRead32 (SPM_DIS_PWR_CON);
  if ((PowerControl & (DISP_PWR_ON | DISP_PWR_ON_2ND | DISP_PWR_RST_B)) !=
      (DISP_PWR_ON | DISP_PWR_ON_2ND | DISP_PWR_RST_B) ||
      ((PowerControl & (DISP_PWR_CLK_DIS | DISP_PWR_ISO | DISP_SRAM_PDN |
                        DISP_SRAM_PDN_ACK)) != 0))
  {
    FailClosed ("DISP power final state");
  }

  DEBUG ((
    DEBUG_ERROR,
    "[A04BR3 DISP] power/top ready pwr=%08x sta=%08x/%08x top=%08x prot=%08x/%08x\n",
    PowerControl,
    MmioRead32 (SPM_PWR_STATUS),
    MmioRead32 (SPM_PWR_STATUS_2ND),
    MmioRead32 (TOP_CG_STA0),
    MmioRead32 (INFRA_TOPAXI_PROTECTEN),
    MmioRead32 (INFRA_TOPAXI_PROTECTSTA1)
    ));
}

STATIC
EFI_STATUS
PwrapWaitForFsm (
  IN UINT32  ExpectedFsm
  )
{
  UINTN  Timeout;

  for (Timeout = 0; Timeout < PWRAP_TIMEOUT_US; Timeout++) {
    if (((MmioRead32 (PWRAP_WACS2_RDATA) >> PWRAP_FSM_SHIFT) & PWRAP_FSM_MASK) ==
        ExpectedFsm)
    {
      return EFI_SUCCESS;
    }

    MicroSecondDelay (1);
  }

  return EFI_TIMEOUT;
}

STATIC
EFI_STATUS
PwrapRead16 (
  IN  UINT16  Address,
  OUT UINT16  *Value
  )
{
  EFI_STATUS  Status;

  Status = PwrapWaitForFsm (PWRAP_FSM_IDLE);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  MmioWrite32 (PWRAP_WACS2_CMD, ((UINT32)Address >> 1) << 16);
  Status = PwrapWaitForFsm (PWRAP_FSM_WFVLDCLR);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  *Value = (UINT16)MmioRead32 (PWRAP_WACS2_RDATA);
  MmioWrite32 (PWRAP_WACS2_VLDCLR, 1U);
  return PwrapWaitForFsm (PWRAP_FSM_IDLE);
}

STATIC
EFI_STATUS
PwrapWrite16 (
  IN UINT16  Address,
  IN UINT16  Value
  )
{
  EFI_STATUS  Status;

  Status = PwrapWaitForFsm (PWRAP_FSM_IDLE);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  MmioWrite32 (
    PWRAP_WACS2_CMD,
    BIT31 | (((UINT32)Address >> 1) << 16) | Value
    );
  return PwrapWaitForFsm (PWRAP_FSM_IDLE);
}

STATIC
EFI_STATUS
SetPanelRegulator (
  IN BOOLEAN  Enable
  )
{
  EFI_STATUS  Status;
  UINT16      Control;
  UINT16      Voltage;

  if ((MmioRead32 (PWRAP_WACS2_RDATA) & PWRAP_INIT_DONE) == 0) {
    return EFI_NOT_READY;
  }

  Status = PwrapRead16 (MT6392_DIGLDO_CON29, &Voltage);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Voltage = (UINT16)((Voltage & ~MT6392_VGP2_VSEL_MASK) | MT6392_VGP2_VSEL_1V8);
  Status  = PwrapWrite16 (MT6392_DIGLDO_CON29, Voltage);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = PwrapRead16 (MT6392_DIGLDO_CON8, &Control);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (Enable) {
    Control |= MT6392_VGP2_ENABLE;
  } else {
    Control &= ~MT6392_VGP2_ENABLE;
  }

  Status = PwrapWrite16 (MT6392_DIGLDO_CON8, Control);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  MicroSecondDelay (264);
  Status = PwrapRead16 (MT6392_DIGLDO_CON29, &Voltage);
  if (!EFI_ERROR (Status)) {
    Status = PwrapRead16 (MT6392_DIGLDO_CON8, &Control);
  }

  if (EFI_ERROR (Status) ||
      ((Voltage & MT6392_VGP2_VSEL_MASK) != MT6392_VGP2_VSEL_1V8) ||
      (((Control & MT6392_VGP2_ENABLE) != 0) != Enable))
  {
    return EFI_DEVICE_ERROR;
  }

  return EFI_SUCCESS;
}

STATIC
VOID
GpioConfigureOutput (
  IN UINT32   Pin,
  IN BOOLEAN  High
  )
{
  UINTN   DirectionPort;
  UINTN   OutputPort;
  UINTN   ModePort;
  UINT16  Bit;
  UINT16  Mode;
  UINTN   ModeShift;

  DirectionPort = GPIO_DIR_BASE + ((Pin / 16U) * 0x10U);
  OutputPort    = GPIO_DOUT_BASE + ((Pin / 16U) * 0x10U);
  ModePort      = GPIO_MODE_BASE + ((Pin / 5U) * 0x10U);
  Bit           = (UINT16)(1U << (Pin % 16U));
  ModeShift     = (Pin % 5U) * 3U;

  Mode  = MmioRead16 (ModePort);
  Mode &= (UINT16)~(7U << ModeShift);
  MmioWrite16 (ModePort, Mode);

  MmioWrite16 (DirectionPort + 4U, Bit);
  MmioWrite16 (OutputPort + (High ? 4U : 8U), Bit);
}

STATIC
BOOLEAN
GpioOutputIs (
  IN UINT32   Pin,
  IN BOOLEAN  High
  )
{
  UINTN   DirectionPort;
  UINTN   OutputPort;
  UINTN   ModePort;
  UINT16  Bit;
  UINTN   ModeShift;

  DirectionPort = GPIO_DIR_BASE + ((Pin / 16U) * 0x10U);
  OutputPort    = GPIO_DOUT_BASE + ((Pin / 16U) * 0x10U);
  ModePort      = GPIO_MODE_BASE + ((Pin / 5U) * 0x10U);
  Bit           = (UINT16)(1U << (Pin % 16U));
  ModeShift     = (Pin % 5U) * 3U;

  return (((MmioRead16 (ModePort) >> ModeShift) & 7U) == 0) &&
         ((MmioRead16 (DirectionPort) & Bit) != 0) &&
         (((MmioRead16 (OutputPort) & Bit) != 0) == High);
}

STATIC
BOOLEAN
DsiWaitForIdle (
  VOID
  )
{
  UINTN  Timeout;

  for (Timeout = 0; Timeout < 50000U; Timeout++) {
    if ((MmioRead32 (DSI_INTSTA) & DSI_BUSY) == 0) {
      return TRUE;
    }

    MicroSecondDelay (10);
  }

  return FALSE;
}

STATIC
BOOLEAN
DsiSendShortCommand (
  IN UINT32  Command
  )
{
  if (!DsiWaitForIdle ()) {
    return FALSE;
  }

  MmioWrite32 (DSI_INTSTA, 0U);
  MmioWrite32 (DSI_CMDQ0, Command);
  MmioWrite32 (DSI_CMDQ_SIZE, 1U);
  MemoryFence ();
  MmioWrite32 (DSI_START, 0U);
  MmioWrite32 (DSI_START, 1U);
  MemoryFence ();

  if (!DsiWaitForIdle ()) {
    return FALSE;
  }

  MmioWrite32 (DSI_INTSTA, 0U);
  return TRUE;
}

STATIC
VOID
StopAndPowerDownPanel (
  IN BOOLEAN  DsiLinkActive
  )
{
  EFI_STATUS  Status;

  GpioConfigureOutput (GPIO_LCD_BACKLIGHT, FALSE);
  if (!GpioOutputIs (GPIO_LCD_BACKLIGHT, FALSE)) {
    FailClosed ("backlight-off readback");
  }

  MmioWrite32 (MUTEX0_EN, 0U);
  if (DsiLinkActive) {
    MmioWrite32 (DSI_MODE_CTRL, 0U);
    MicroSecondDelay (1000);
    if (!DsiSendShortCommand (0x00280500U) ||
        !DsiSendShortCommand (0x00100500U))
    {
      FailClosed ("panel suspend DCS");
    }

    MicroSecondDelay (180000);
  }

  MmioWrite32 (DSI_START, 0U);
  MmioWrite32 (RDMA_GLOBAL_CON, 0U);
  MmioWrite32 (OVL_EN, 0U);
  MmioWrite32 (DSI_CON_CTRL, 0U);

  GpioConfigureOutput (GPIO_LCD_RESET, FALSE);
  MicroSecondDelay (120000);
  GpioConfigureOutput (GPIO_LCD_STANDBY, FALSE);
  Status = SetPanelRegulator (FALSE);
  if (EFI_ERROR (Status) ||
      !GpioOutputIs (GPIO_LCD_RESET, FALSE) ||
      !GpioOutputIs (GPIO_LCD_STANDBY, FALSE))
  {
    FailClosed ("panel power-off");
  }

  MicroSecondDelay (500000);
}

STATIC
VOID
InitializeMipiPhy (
  VOID
  )
{
  UINT32  Bandgap;
  UINT32  Power;
  UINTN   Timeout;

  MmioWrite32 (MIPITX_PLL_CON0, MmioRead32 (MIPITX_PLL_CON0) & ~BIT0);
  MmioWrite32 (MIPITX_PLL_TOP, 0U);
  MmioWrite32 (MIPITX_TOP_CON, BIT11);
  MmioWrite32 (MIPITX_CLOCK_LANE, MmioRead32 (MIPITX_CLOCK_LANE) & ~BIT0);
  MmioWrite32 (MIPITX_DATA_LANE0, MmioRead32 (MIPITX_DATA_LANE0) & ~BIT0);
  MmioWrite32 (MIPITX_DATA_LANE1, MmioRead32 (MIPITX_DATA_LANE1) & ~BIT0);
  MmioWrite32 (MIPITX_DATA_LANE2, MmioRead32 (MIPITX_DATA_LANE2) & ~BIT0);
  MmioWrite32 (MIPITX_DATA_LANE3, MmioRead32 (MIPITX_DATA_LANE3) & ~BIT0);
  MmioWrite32 (MIPITX_PLL_PWR, MIPITX_PLL_POWER_ON | MIPITX_PLL_ISOLATE);
  MicroSecondDelay (1000);
  MmioWrite32 (MIPITX_PLL_PWR, MIPITX_PLL_ISOLATE);
  MicroSecondDelay (1000);

  MmioWrite32 (MIPITX_TOP_CON, 0x00000082U);
  Bandgap = (MmioRead32 (MIPITX_BG_CON) & 0xFF800000U) | 0x00492483U;
  MmioWrite32 (MIPITX_BG_CON, Bandgap);
  MicroSecondDelay (10000);

  MmioWrite32 (MIPITX_DSI0_CON, 0x00000403U);
  MmioWrite32 (MIPITX_PLL_PWR, MIPITX_PLL_POWER_ON | MIPITX_PLL_ISOLATE);
  MicroSecondDelay (10000);
  MmioWrite32 (MIPITX_PLL_PWR, MIPITX_PLL_POWER_ON);

  MmioWrite32 (MIPITX_PLL_CON0, 0x00000008U);
  MmioWrite32 (MIPITX_PLL_CON1, 0x01B10003U);
  MmioWrite32 (MIPITX_PLL_CON2, 0x4B627627U);
  MmioWrite32 (MIPITX_PLL_CON3, 0x08E808E8U);
  MmioWrite32 (MIPITX_PLL_CHG, 1U);
  MmioWrite32 (MIPITX_CLOCK_LANE, 0x00001821U);
  MmioWrite32 (MIPITX_DATA_LANE0, 0x00000401U);
  MmioWrite32 (MIPITX_DATA_LANE1, 0x00000101U);
  MmioWrite32 (MIPITX_DATA_LANE2, 0x00000101U);
  MmioWrite32 (MIPITX_DATA_LANE3, 0x00000101U);
  MmioWrite32 (MIPITX_PHY_SEL, 0x00043210U);
  MmioWrite32 (MIPITX_SW_CTRL, 0U);
  MmioWrite32 (MIPITX_SW_CTRL_CON0, 0U);
  MmioWrite32 (MIPITX_SW_CTRL_CON1, 3U);
  MmioWrite32 (MIPITX_PLL_CON0, 0x00000009U);
  MicroSecondDelay (1000);
  MmioWrite32 (MIPITX_PLL_CON1, 0x01B10007U);
  MmioWrite32 (MIPITX_PLL_TOP, 0x00000300U);
  MicroSecondDelay (1000);

  for (Timeout = 0; Timeout < 10000U; Timeout++) {
    Power = MmioRead32 (MIPITX_PLL_PWR);
    if ((Power & (MIPITX_PLL_POWER_ON | MIPITX_PLL_ISOLATE | MIPITX_PLL_POWER_ACK)) ==
        (MIPITX_PLL_POWER_ON | MIPITX_PLL_POWER_ACK))
    {
      break;
    }

    MicroSecondDelay (1);
  }

  if ((Timeout == 10000U) ||
      (MmioRead32 (MIPITX_PLL_CON0) != 0x00000009U) ||
      (MmioRead32 (MIPITX_PLL_CON1) != 0x01B10007U) ||
      (MmioRead32 (MIPITX_PLL_CON2) != 0x4B627627U) ||
      (MmioRead32 (MIPITX_PLL_CON3) != 0x08E808E8U) ||
      (MmioRead32 (MIPITX_CLOCK_LANE) != 0x00001821U))
  {
    FailClosed ("MIPI PHY readback");
  }
}

STATIC
VOID
InitializeDsiController (
  VOID
  )
{
  MmioWrite32 (DSI_CON_CTRL, BIT0 | BIT1);
  MmioWrite32 (DSI_CON_CTRL, BIT1);
  MmioWrite32 (DSI_INTEN, BIT0 | BIT1);
  MmioWrite32 (DSI_INTSTA, 0U);
  MmioWrite32 (DSI_MODE_CTRL, 0U);
  MmioWrite32 (DSI_TXRX_CTRL, 0x0000003CU);
  MmioWrite32 (DSI_PSCTRL, 0x00030960U);
  MmioWrite32 (DSI_VSA_NL, 4U);
  MmioWrite32 (DSI_VBP_NL, 8U);
  MmioWrite32 (DSI_VFP_NL, 8U);
  MmioWrite32 (DSI_VACT_NL, A04BR3_HEIGHT);
  MmioWrite32 (DSI_HSA_WC, 8U);
  MmioWrite32 (DSI_HBP_WC, 0x190U);
  MmioWrite32 (DSI_HFP_WC, 0x3CU);
  MmioWrite32 (DSI_PHY_LCPAT, 0x55U);
  MmioWrite32 (DSI_PHY_LCCON, 0U);
  MmioWrite32 (DSI_PHY_LD0CON, 0U);
  MmioWrite32 (DSI_PHY_TIMECON0, 0x0F090404U);
  MmioWrite32 (DSI_PHY_TIMECON1, 0x1A140610U);
  MmioWrite32 (DSI_PHY_TIMECON2, 0x0F170000U);
  MmioWrite32 (DSI_PHY_TIMECON3, 0x00081A03U);
  MmioWrite32 (DSI_PHY_TIMECON4, 0x00023000U);
  MmioWrite32 (DSI_VM_CMD_CON, 0x21U);

  if ((MmioRead32 (DSI_CON_CTRL) != BIT1) ||
      (MmioRead32 (DSI_PHY_TIMECON0) != 0x0F090404U) ||
      (MmioRead32 (DSI_PHY_TIMECON3) != 0x00081A03U))
  {
    FailClosed ("DSI controller readback");
  }
}

STATIC
VOID
PowerOnPanel (
  VOID
  )
{
  EFI_STATUS  Status;

  GpioConfigureOutput (GPIO_LCD_STANDBY, FALSE);
  GpioConfigureOutput (GPIO_LCD_BACKLIGHT, FALSE);
  GpioConfigureOutput (GPIO_LCD_RESET, FALSE);

  Status = SetPanelRegulator (TRUE);
  if (EFI_ERROR (Status)) {
    FailClosed ("VGP2 1.8V enable");
  }

  GpioConfigureOutput (GPIO_LCD_STANDBY, TRUE);
  MicroSecondDelay (50000);
  GpioConfigureOutput (GPIO_LCD_RESET, TRUE);
  MicroSecondDelay (10000);

  if (!GpioOutputIs (GPIO_LCD_STANDBY, TRUE) ||
      !GpioOutputIs (GPIO_LCD_RESET, TRUE) ||
      !GpioOutputIs (GPIO_LCD_BACKLIGHT, FALSE))
  {
    FailClosed ("panel GPIO power-on");
  }

  if (!DsiSendShortCommand (0x00110500U)) {
    FailClosed ("panel Sleep Out");
  }

  MicroSecondDelay (180000);
  if (!DsiSendShortCommand (0x00290500U)) {
    FailClosed ("panel Display On");
  }

  MicroSecondDelay (33000);
}

STATIC
VOID
InitializeRelayPath (
  VOID
  )
{
  MmioWrite32 (COLOR_INTERNAL_IP_WIDTH, A04BR3_WIDTH);
  MmioWrite32 (COLOR_INTERNAL_IP_HEIGHT, A04BR3_HEIGHT);
  MmioWrite32 (COLOR_CFG_MAIN, A04BR3_COLOR_CFG_WRITE);
  MmioWrite32 (COLOR_START, 2U);
  MmioWrite32 (COLOR_START, 3U);
  MmioWrite32 (COLOR_CM1_EN, 0U);
  MmioWrite32 (COLOR_CM2_EN, 0U);

  MmioWrite32 (CCORR_SIZE, A04BR3_RELAY_SIZE_REG);
  MmioWrite32 (CCORR_EN, 1U);
  MmioWrite32 (CCORR_CFG, 1U);

  MmioWrite32 (AAL_EN, 0U);
  MmioWrite32 (AAL_CFG, 1U);
  MmioWrite32 (AAL_SIZE, A04BR3_RELAY_SIZE_REG);

  MmioWrite32 (GAMMA_EN, 0U);
  MmioWrite32 (GAMMA_SIZE, A04BR3_RELAY_SIZE_REG);

  MmioWrite32 (DITHER_REG (5U), 0x00000000U);
  MmioWrite32 (DITHER_REG (6U), 0x00003004U);
  MmioWrite32 (DITHER_REG (7U), 0x00000000U);
  MmioWrite32 (DITHER_REG (8U), 0x00000000U);
  MmioWrite32 (DITHER_REG (9U), 0x00000000U);
  MmioWrite32 (DITHER_REG (10U), 0x00000000U);
  MmioWrite32 (DITHER_REG (11U), 0x00000000U);
  MmioWrite32 (DITHER_REG (12U), 0x00000011U);
  MmioWrite32 (DITHER_REG (13U), 0x00000000U);
  MmioWrite32 (DITHER_REG (14U), 0x00000000U);
  MmioWrite32 (DITHER_REG (15U), 0x20200001U);
  MmioWrite32 (DITHER_REG (16U), 0x20202020U);
  MmioWrite32 (DITHER_REG (0U), 0x00000001U);
  MmioWrite32 (DITHER_EN, 1U);
  MmioWrite32 (DITHER_CFG, BIT1);
  MmioWrite32 (DITHER_SIZE, A04BR3_RELAY_SIZE_REG);

  DEBUG ((
    DEBUG_ERROR,
    "[A04BR3 DISP] relay rb cg=%08x color=%08x/%08x/%08x/%08x/%08x/%08x\n",
    MmioRead32 (MMSYS_CG_CON0),
    MmioRead32 (COLOR_INTERNAL_IP_WIDTH),
    MmioRead32 (COLOR_INTERNAL_IP_HEIGHT),
    MmioRead32 (COLOR_CFG_MAIN),
    MmioRead32 (COLOR_START),
    MmioRead32 (COLOR_CM1_EN),
    MmioRead32 (COLOR_CM2_EN)
    ));
  DEBUG ((
    DEBUG_ERROR,
    "[A04BR3 DISP] relay rb ccorr=%08x/%08x/%08x aal=%08x/%08x/%08x gamma=%08x/%08x\n",
    MmioRead32 (CCORR_EN),
    MmioRead32 (CCORR_CFG),
    MmioRead32 (CCORR_SIZE),
    MmioRead32 (AAL_EN),
    MmioRead32 (AAL_CFG),
    MmioRead32 (AAL_SIZE),
    MmioRead32 (GAMMA_EN),
    MmioRead32 (GAMMA_SIZE)
    ));
  DEBUG ((
    DEBUG_ERROR,
    "[A04BR3 DISP] relay rb dither=%08x/%08x/%08x/%08x/%08x\n",
    MmioRead32 (DITHER_EN),
    MmioRead32 (DITHER_CFG),
    MmioRead32 (DITHER_SIZE),
    MmioRead32 (DITHER_REG (15U)),
    MmioRead32 (DITHER_REG (16U))
    ));

  if ((MmioRead32 (COLOR_INTERNAL_IP_WIDTH) != A04BR3_WIDTH) ||
      (MmioRead32 (COLOR_INTERNAL_IP_HEIGHT) != A04BR3_HEIGHT) ||
      (MmioRead32 (COLOR_CFG_MAIN) != A04BR3_COLOR_CFG_READBACK) ||
      (MmioRead32 (COLOR_START) != 3U) ||
      (MmioRead32 (COLOR_CM1_EN) != 0U) ||
      (MmioRead32 (COLOR_CM2_EN) != 0U) ||
      (MmioRead32 (CCORR_EN) != 1U) ||
      (MmioRead32 (CCORR_CFG) != 1U) ||
      (MmioRead32 (CCORR_SIZE) != A04BR3_RELAY_SIZE_REG) ||
      (MmioRead32 (AAL_EN) != 0U) ||
      (MmioRead32 (AAL_CFG) != 1U) ||
      (MmioRead32 (AAL_SIZE) != A04BR3_RELAY_SIZE_REG) ||
      (MmioRead32 (GAMMA_EN) != 0U) ||
      (MmioRead32 (GAMMA_SIZE) != A04BR3_RELAY_SIZE_REG) ||
      (MmioRead32 (DITHER_EN) != 1U) ||
      (MmioRead32 (DITHER_CFG) != BIT1) ||
      (MmioRead32 (DITHER_SIZE) != A04BR3_RELAY_SIZE_REG) ||
      (MmioRead32 (DITHER_REG (15U)) != 0x20200001U) ||
      (MmioRead32 (DITHER_REG (16U)) != 0x20202020U))
  {
    FailClosed ("MMSYS relay path readback");
  }
}

STATIC UINT32  mFrameBufferAddress;

STATIC
BOOLEAN
DisplayConfigurationIsValid (
  VOID
  )
{
  EFI_MEMORY_REGION_DESCRIPTOR  FrameBufferRegion;

  if (EFI_ERROR (LocateMemoryRegionByName ("Display Reserved", &FrameBufferRegion))) {
    return FALSE;
  }

  // LK's primary surface at 0xBE9C0000; the scan-out also uses the next
  // surface (OVL layer 3), so the region must hold two frames.
  if ((FixedPcdGet32 (PcdFrameBufferWidth) != A04BR3_WIDTH) ||
      (FixedPcdGet32 (PcdFrameBufferHeight) != A04BR3_HEIGHT) ||
      (FrameBufferRegion.Address != 0xBE9C0000U) ||
      (FrameBufferRegion.Length < (2U * A04BR3_FRAME_SIZE)))
  {
    return FALSE;
  }

  mFrameBufferAddress = (UINT32)FrameBufferRegion.Address;
  return TRUE;
}

STATIC
BOOLEAN
DsiLinkIsActive (
  VOID
  )
{
  return ((MmioRead32 (DSI_CON_CTRL) & BIT1) != 0) &&
         ((MmioRead32 (MIPITX_PLL_PWR) &
           (MIPITX_PLL_POWER_ON | MIPITX_PLL_POWER_ACK)) ==
          (MIPITX_PLL_POWER_ON | MIPITX_PLL_POWER_ACK));
}

STATIC
VOID
ConfigureA04Br3Scanout (
  VOID
  )
{
  UINT32  FrameBuffer;
  UINT32  Layer3FrameBuffer;

  FrameBuffer       = mFrameBufferAddress;
  Layer3FrameBuffer = FrameBuffer + A04BR3_FRAME_SIZE;

  // Linux installs the OVL IRQ before its CRTC state exists. Do not hand off
  // the frame-complete interrupt left enabled by LK.
  MmioWrite32 (OVL_INTEN, 0U);
  MmioWrite32 (OVL_INTSTA, 0U);
  if (MmioRead32 (OVL_INTEN) != 0U) {
    FailClosed ("OVL interrupt disable");
  }

  MmioWrite32 (MMSYS_CG_CLR0, A04BR3_MMSYS_CG0);
  MmioWrite32 (MMSYS_CG_CLR1, A04BR3_MMSYS_CG1);
  if (((MmioRead32 (MMSYS_CG_CON0) & A04BR3_MMSYS_CG0) != 0) ||
      ((MmioRead32 (MMSYS_CG_CON1) & A04BR3_MMSYS_CG1) != 0))
  {
    FailClosed ("display clocks");
  }

  // A04BR3 stock route: OVL0 through the active MMSYS relay path to RDMA0/DSI0.
  MmioWrite32 (MMSYS_OVL0_MOUT_EN, 1U);
  MmioWrite32 (MMSYS_DITHER_MOUT_EN, 1U);
  MmioWrite32 (MMSYS_COLOR0_SEL_IN, 1U);
  MmioWrite32 (MMSYS_RDMA0_SOUT_SEL, 2U);
  MmioWrite32 (MMSYS_DSI0_SEL_IN, 1U);

  InitializeRelayPath ();

  MmioWrite32 (OVL_ROI_SIZE, A04BR3_FRAME_SIZE_REG);
  MmioWrite32 (OVL_DATAPATH_CON, 0x60000001U);
  MmioWrite32 (OVL_ROI_BGCLR, 0xFF000000U);

  MmioWrite32 (OVL_L0_CON, A04BR3_OVL_GOP_LAYER_CON);
  MmioWrite32 (OVL_L0_SRC_SIZE, A04BR3_FRAME_SIZE_REG);
  MmioWrite32 (OVL_L0_OFFSET, 0U);
  MmioWrite32 (OVL_L0_ADDR, FrameBuffer);
  MmioWrite32 (OVL_L0_PITCH, A04BR3_OVL_LAYER_PITCH);
  MmioWrite32 (OVL_L0_RDMA_CTRL, 0x00760001U);

  MmioWrite32 (OVL_L0_CON + (3U * OVL_LAYER_STRIDE), A04BR3_OVL_LAYER_CON);
  MmioWrite32 (OVL_L0_SRC_SIZE + (3U * OVL_LAYER_STRIDE), A04BR3_FRAME_SIZE_REG);
  MmioWrite32 (OVL_L0_OFFSET + (3U * OVL_LAYER_STRIDE), 0U);
  MmioWrite32 (OVL_L0_ADDR + (3U * OVL_LAYER_STRIDE), Layer3FrameBuffer);
  MmioWrite32 (OVL_L0_PITCH + (3U * OVL_LAYER_STRIDE), A04BR3_OVL_LAYER_PITCH);
  MmioWrite32 (OVL_L0_RDMA_CTRL + (3U * OVL_LAYER_STRIDE), 0x00760001U);
  MmioWrite32 (OVL_SRC_CON, A04BR3_OVL_ACTIVE_LAYERS);
  MmioWrite32 (OVL_EN, 1U);

  MmioWrite32 (RDMA_SIZE_CON_0, A04BR3_WIDTH);
  MmioWrite32 (RDMA_SIZE_CON_1, A04BR3_HEIGHT);
  MmioWrite32 (RDMA_MEM_CON, 0U);
  MmioWrite32 (RDMA_MEM_SRC_PITCH, 0U);
  MmioWrite32 (RDMA_MEM_START_ADDR, 0U);
  MmioWrite32 (RDMA_FIFO_CON, 0x81000010U);
  MmioWrite32 (RDMA_GLOBAL_CON, 0x00000101U);

  MmioWrite32 (DSI_MODE_CTRL, 2U);
  MmioWrite32 (DSI_PHY_LCCON, BIT0);
  MmioWrite32 (MUTEX0_RST, 1U);
  MmioWrite32 (MUTEX0_RST, 0U);
  MmioWrite32 (MUTEX0_MOD, A04BR3_MUTEX_MODULES);
  MmioWrite32 (MUTEX0_SOF, 1U);
  MmioWrite32 (MUTEX0_EN, 1U);

  MemoryFence ();
  MmioWrite32 (DSI_START, 0U);
  MmioWrite32 (DSI_START, 1U);
  MemoryFence ();
  MicroSecondDelay (33000);

  GpioConfigureOutput (GPIO_LCD_BACKLIGHT, TRUE);
}

EFI_STATUS
EFIAPI
A04Br3DisplayInitDxeEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  BOOLEAN  DsiLinkActive;

  (VOID)ImageHandle;
  (VOID)SystemTable;

  if (!DisplayConfigurationIsValid ()) {
    FailClosed ("display platform configuration");
  }

  EnableDisplayPowerAndTopClocks ();
  DsiLinkActive = DsiLinkIsActive ();

  DEBUG ((
    DEBUG_ERROR,
    "[A04BR3 DISP] cold-init begin link=%u ovl=%08x rdma=%08x dsi=%08x/%08x pll=%08x/%08x\n",
    DsiLinkActive,
    MmioRead32 (OVL_EN),
    MmioRead32 (RDMA_GLOBAL_CON),
    MmioRead32 (DSI_CON_CTRL),
    MmioRead32 (DSI_INTSTA),
    MmioRead32 (MIPITX_PLL_CON0),
    MmioRead32 (MIPITX_PLL_PWR)
    ));

  StopAndPowerDownPanel (DsiLinkActive);
  InitializeMipiPhy ();
  InitializeDsiController ();
  PowerOnPanel ();
  ConfigureA04Br3Scanout ();

  if ((MmioRead32 (OVL_L0_ADDR) != mFrameBufferAddress) ||
      (MmioRead32 (OVL_SRC_CON) != A04BR3_OVL_ACTIVE_LAYERS) ||
      ((MmioRead32 (DSI_CON_CTRL) & BIT1) == 0) ||
      (MmioRead32 (DSI_MODE_CTRL) != 2U) ||
      (MmioRead32 (MUTEX0_MOD) != A04BR3_MUTEX_MODULES) ||
      !GpioOutputIs (GPIO_LCD_BACKLIGHT, TRUE))
  {
    FailClosed ("final display readback");
  }

  DEBUG ((
    DEBUG_ERROR,
    "[A04BR3 DISP] cold-init complete fb=%08x l3=%08x ovl=%08x relay=%08x/%08x/%08x/%08x/%08x rdma=%08x dsi=%08x/%08x pll=%08x/%08x gpio=%04x\n",
    MmioRead32 (OVL_L0_ADDR),
    MmioRead32 (OVL_L0_ADDR + (3U * OVL_LAYER_STRIDE)),
    MmioRead32 (OVL_SRC_CON),
    MmioRead32 (COLOR_START),
    MmioRead32 (CCORR_CFG),
    MmioRead32 (AAL_CFG),
    MmioRead32 (GAMMA_EN),
    MmioRead32 (DITHER_CFG),
    MmioRead32 (RDMA_GLOBAL_CON),
    MmioRead32 (DSI_CON_CTRL),
    MmioRead32 (DSI_INTSTA),
    MmioRead32 (MIPITX_PLL_CON0),
    MmioRead32 (MIPITX_PLL_PWR),
    MmioRead16 (GPIO_DOUT_BASE + ((GPIO_LCD_RESET / 16U) * 0x10U))
    ));
  DEBUG ((
    DEBUG_ERROR,
    "[A04BR3 DISP] layer con l0=%08x (want %08x) l3=%08x\n",
    MmioRead32 (OVL_L0_CON),
    A04BR3_OVL_GOP_LAYER_CON,
    MmioRead32 (OVL_L0_CON + (3U * OVL_LAYER_STRIDE))
    ));
  return EFI_SUCCESS;
}
