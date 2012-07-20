

/***  I2S  regs define ***/
#define    I2S_IER                  0x000
#define    I2S_IRER                 0x004
#define    I2S_ITER                 0x008
#define    I2S_CER                  0x00C
#define    I2S_CCR                  0x010
#define    I2S_RXFFR                0x014
#define    I2S_TXFFR                0x018
// channel 0
#define    I2S_LRBR0                0x020
#define    I2S_LTHR0                0x020
#define    I2S_RRBR0                0x024
#define    I2S_RTHR0                0x024
#define    I2S_RER0                 0x028
#define    I2S_TER0                 0x02C
#define    I2S_RCR0                 0x030
#define    I2S_TCR0                 0x034
#define    I2S_ISR0                 0x038
#define    I2S_IMR0                 0x03C
#define    I2S_ROR0                 0x040
#define    I2S_TOR0                 0x044
#define    I2S_RFCR0                0x048
#define    I2S_TFCR0                0x04C
#define    I2S_RFF0                 0x050
#define    I2S_TFF0                 0x054
// channel 1
#define    I2S_LRBR1                0x060
#define    I2S_LTHR1                0x060
#define    I2S_RRBR1                0x064
#define    I2S_RTHR1                0x064
#define    I2S_RER1                 0x068
#define    I2S_TER1                 0x06C
#define    I2S_RCR1                 0x070
#define    I2S_TCR1                 0x074
#define    I2S_ISR1                 0x078
#define    I2S_IMR1                 0x07C
#define    I2S_ROR1                 0x080
#define    I2S_TOR1                 0x084
#define    I2S_RFCR1                0x088
#define    I2S_TFCR1                0x08C
#define    I2S_RFF1                 0x090
#define    I2S_TFF1                 0x094
// channel 2
#define    I2S_LRBR2                0x0A0
#define    I2S_LTHR2                0x0A0
#define    I2S_RRBR2                0x0A4
#define    I2S_RTHR2                0x0A4
#define    I2S_RER2                 0x0A8
#define    I2S_TER2                 0x0AC
#define    I2S_RCR2                 0x0B0
#define    I2S_TCR2                 0x0B4
#define    I2S_ISR2                 0x0B8
#define    I2S_IMR2                 0x0BC
#define    I2S_ROR2                 0x0C0
#define    I2S_TOR2                 0x0C4
#define    I2S_RFCR2                0x0C8
#define    I2S_TFCR2                0x0CC
#define    I2S_RFF2                 0x0D0
#define    I2S_TFF2                 0x0D4
// channel 3
#define    I2S_LRBR3                0x0E0
#define    I2S_LTHR3                0x0E0
#define    I2S_RRBR3                0x0E4
#define    I2S_RTHR3                0x0E4
#define    I2S_RER3                 0x0E8
#define    I2S_TER3                 0x0EC
#define    I2S_RCR3                 0x0F0
#define    I2S_TCR3                 0x0F4
#define    I2S_ISR3                 0x0F8
#define    I2S_IMR3                 0x0FC
#define    I2S_ROR3                 0x100
#define    I2S_TOR3                 0x104
#define    I2S_RFCR3                0x108
#define    I2S_TFCR3                0x10C
#define    I2S_RFF3                 0x110
#define    I2S_TFF3                 0x114
#define    I2S_SCR		    0x118
// I2S DMA CTL REG
#define    I2S_RXDMA                0x1C0
#define    I2S_RRXDMA               0x1C4
#define    I2S_TXDMA                0x1C8
#define    I2S_RTXDMA               0x1CC
// CONSTANT REG
#define    I2S_COMP_PARAM2          0x1F0
#define    I2S_COMP_PARAM1          0x1F4
#define    I2S_COMP_VERSION         0x1F8
#define    I2S_COMP_TYPE            0x1FC


#define write_reg(reg, data) \
	*(volatile unsigned long*)(reg) = data

#define read_reg(reg) \
	*(volatile unsigned long*)(reg)
//#if 0
#define    DMAC2_SAR0               0x000
#define    DMAC2_DAR0              0x008
#define    DMAC2_LLP0               0x010
#define    DMAC2_CTL0               0x018
#define    DMAC2_SSTAT0            0x020
#define    DMAC2_DSTAT0             0x028
#define    DMAC2_SSTATR0          0x030
#define    DMAC2_DSTATR0            0x038
#define    DMAC2_CFG0              0x040
#define    DMAC2_SGR0              0x048
#define    DMAC2_DSR0              0x050

// DMAC2 channel 1
#define    DMAC2_SAR1               0x058
#define    DMAC2_DAR1               0x060
#define    DMAC2_LLP1              0x068
#define    DMAC2_CTL1               0x070
#define    DMAC2_SSTAT1             0x078
#define    DMAC2_DSTAT1            0x080
#define    DMAC2_SSTATR1           0x088
#define    DMAC2_DSTATR1            0x090
#define    DMAC2_CFG1               0x098
#define    DMAC2_SGR1               0x0A0
#define    DMAC2_DSR1              0x0A8

// DMAC2 channel 2
#define    DMAC2_SAR2               0x0B0
#define    DMAC2_DAR2               0x0B8
#define    DMAC2_LLP2               0x0C0
#define    DMAC2_CTL2              0x0C8
#define    DMAC2_SSTAT2            0x0D0
#define    DMAC2_DSTAT2             0x0D8
#define    DMAC2_SSTATR2            0x0E0
#define    DMAC2_DSTATR2            0x0E8
#define    DMAC2_CFG2              0x0F0
#define    DMAC2_SGR2               0x0F8
#define    DMAC2_DSR2               0x100

// DMAC2 channel 3
#define    DMAC2_SAR3               0x108
#define    DMAC2_DAR3              0x110
#define    DMAC2_LLP3              0x118
#define    DMAC2_CTL3               0x120
#define    DMAC2_SSTAT3            0x128
#define    DMAC2_DSTAT3             0x130
#define    DMAC2_SSTATR3           0x138
#define    DMAC2_DSTATR3           0x140
#define    DMAC2_CFG3               0x148
#define    DMAC2_SGR3               0x150
#define    DMAC2_DSR3               0x158

// DMAC2 channel 4
#define    DMAC2_SAR4               0x160
#define    DMAC2_DAR4               0x168
#define    DMAC2_LLP4               0x170
#define    DMAC2_CTL4               0x178
#define    DMAC2_SSTAT4             0x180
#define    DMAC2_DSTAT4            0x188
#define    DMAC2_SSTATR4           0x190
#define    DMAC2_DSTATR4           0x198
#define    DMAC2_CFG4              0x1A0
#define    DMAC2_SGR4               0x1A8
#define    DMAC2_DSR4              0x1B0

// DMAC2 channel 5
#define    DMAC2_SAR5             0x1B8
#define    DMAC2_DAR5               0x1C0
#define    DMAC2_LLP5               0x1C8
#define    DMAC2_CTL5               0x1D0
#define    DMAC2_SSTAT5            0x1D8
#define    DMAC2_DSTAT5            0x1E0
#define    DMAC2_SSTATR5            0x1E8
#define    DMAC2_DSTATR5            0x1F0
#define    DMAC2_CFG5             0x1F8
#define    DMAC2_SGR5              0x200
#define    DMAC2_DSR5               0x208

// DMAC2 channel 6
#define    DMAC2_SAR6               0x210
#define    DMAC2_DAR6              0x218
#define    DMAC2_LLP6               0x220
#define    DMAC2_CTL6              0x228
#define    DMAC2_SSTAT6            0x230
#define    DMAC2_DSTAT6             0x238
#define    DMAC2_SSTATR6           0x240
#define    DMAC2_DSTATR6           0x248
#define    DMAC2_CFG6                0x250
#define    DMAC2_SGR6               0x258
#define    DMAC2_DSR6               0x260

// DMAC2 channel 7
#define    DMAC2_SAR7             0x268
#define    DMAC2_DAR7               0x270
#define    DMAC2_LLP7               0x278
#define    DMAC2_CTL7              0x280
#define    DMAC2_SSTAT7              0x288
#define    DMAC2_DSTAT7             0x290
#define    DMAC2_SSTATR7            0x298
#define    DMAC2_DSTATR7             0x2A0
#define    DMAC2_CFG7               0x2A8
#define    DMAC2_SGR7               0x2B0
#define    DMAC2_DSR7                0x2B8

#define    DMAC2_RAWTFR             0x2C0
#define    DMAC2_RAWBLK             0x2C8
#define    DMAC2_RAWSRCTR         0x2D0
#define    DMAC2_RAWDSTTR           0x2D8
#define    DMAC2_RAWERR             0x2E0

#define    DMAC2_STATRF           0x2E8
#define    DMAC2_STATBLK           0x2F0
#define    DMAC2_STATSRCTR          0x2F8
#define    DMAC2_STATDSTTR         0x300
#define    DMAC2_STATERR            (0x308

#define    DMAC2_MASKTRF          0x310
#define    DMAC2_MASKBLK             0x318
#define    DMAC2_MASKSRCTR         0x320
#define    DMAC2_MASKDSTTR       0x328
#define    DMAC2_MASKERR           0x330

#define    DMAC2_CLRTFR          0x338
#define    DMAC2_CLRBLK              0x340
#define    DMAC2_CLRSRCTR          0x348
#define    DMAC2_CLRDSTTR            0x350
#define    DMAC2_CLRERR             0x358
#define    DMAC2_STATINT          0x360

#define    DMAC2_SRCREQ             0x368
#define    DMAC2_DSTREQ            0x370
#define    DMAC2_SGSRCREQ            0x378
#define    DMAC2_SGDSTREQ            0x380
#define    DMAC2_LSTSRC              0x388
#define    DMAC2_LSTDST             0x390

#define    DMAC2_CFG                0x398
#define    DMAC2_CHENA            0x3A0
#define    DMAC2_ID                 0x3A8
#define    DMAC2_TEST               0x3B0
#define    DMAC2_COMP_PARAM6         0x3C8
#define    DMAC2_COMP_PARAM5        0x3D0
#define    DMAC2_COMP_PARAM4        0x3D8
#define    DMAC2_COMP_PARAM3       0x3E0
#define    DMAC2_COMP_PARAM2         0x3E8
#define    DMAC2_COMP_PARAM1         0x3F0
#define    DMAC2_COMP_ID            0x3F8
//#endif
