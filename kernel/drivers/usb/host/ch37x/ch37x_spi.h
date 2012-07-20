#ifndef __CH374_SPI_H__
#define __CH374_SPI_H__

#define SPI_BASE_V SSI2_BASE_V

#define    SPI_CONTROL0			(SPI_BASE_V + 0x000)
#define    SPI_CONTROL1			(SPI_BASE_V + 0x004)
#define    SPI_SPIENR			(SPI_BASE_V + 0x008)
#define    SPI_MWCR				(SPI_BASE_V + 0x00C)
#define    SPI_SER				(SPI_BASE_V + 0x010)
#define    SPI_BAUDR			(SPI_BASE_V + 0x014)
#define    SPI_TXFTLR			(SPI_BASE_V + 0x018)
#define    SPI_RXFTLR			(SPI_BASE_V + 0x01C)
#define    SPI_TXFLR			(SPI_BASE_V + 0x020)
#define    SPI_RXFLR			(SPI_BASE_V + 0x024)
#define    SPI_SR				(SPI_BASE_V + 0x028)
#define    SPI_IMR				(SPI_BASE_V + 0x02C)
#define    SPI_ISR				(SPI_BASE_V + 0x030)
#define    SPI_RISR				(SPI_BASE_V + 0x034)
#define    SPI_TXOICR			(SPI_BASE_V + 0x038)
#define    SPI_RXOICR			(SPI_BASE_V + 0x03C)
#define    SPI_RXUICR			(SPI_BASE_V + 0x040)
#define    SPI_ICR				(SPI_BASE_V + 0x048)
#define    SPI_DMACR			(SPI_BASE_V + 0x04C)
#define    SPI_DMATDLR			(SPI_BASE_V + 0x050)
#define    SPI_DMARDLR			(SPI_BASE_V + 0x054)
#define    SPI_DR				(SPI_BASE_V + 0x060)

extern void ch37x_writeb(uint8_t addr, uint8_t data);
extern uint8_t ch37x_readb(uint8_t addr);
extern void ch37x_read_fifo_cpu(uint8_t *buf, int len);
extern void ch37x_write_fifo_cpu(uint8_t *buf, int len);
extern void ch37x_read_fifo_dma(uint8_t *buf, int len);
extern void ch37x_write_fifo_dma(uint8_t *buf, int len);

#endif

