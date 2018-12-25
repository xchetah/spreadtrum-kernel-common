/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/tty.h>
#include <linux/ioport.h>
#include <linux/console.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/tty_flip.h>
#include <linux/serial_core.h>
#include <linux/serial.h>
#include <linux/termios.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <mach/hardware.h>
#include <linux/clk.h>
#include <mach/pinmap.h>
#include <mach/serial_sprd.h>
#include <linux/wakelock.h>

#include <linux/dma-mapping.h>
#include <mach/dma.h>

#define IRQ_WAKEUP 	0

#define UART_NR_MAX			CONFIG_SERIAL_SPRD_UART_NR
#define SP_TTY_NAME			"ttyS"
#define SP_TTY_MINOR_START	64
#define SP_TTY_MAJOR		TTY_MAJOR

#define UART_CLK		48000000

/*offset*/
#define ARM_UART_TXD	0x0000
#define ARM_UART_RXD	0x0004
#define ARM_UART_STS0	0x0008
#define ARM_UART_STS1	0x000C
#define ARM_UART_IEN	0x0010
#define ARM_UART_ICLR	0x0014
#define ARM_UART_CTL0	0x0018
#define ARM_UART_CTL1	0x001C
#define ARM_UART_CTL2	0x0020
#define ARM_UART_CLKD0	0x0024
#define ARM_UART_CLKD1	0x0028
#define ARM_UART_STS2	0x002C
/*UART IRQ num*/

/*UART FIFO watermark*/
#define SP_TX_FIFO		0x40
#define SP_RX_FIFO		0x60
/*UART IEN*/
#define UART_IEN_RX_FIFO_FULL	(0x1<<0)
#define UART_IEN_TX_FIFO_EMPTY	(0x1<<1)
#define UART_IEN_BREAK_DETECT	(0x1<<7)
#define UART_IEN_TIMEOUT     	(0x1<<13)
/*DMA enable bit*/
#define UART_DMA_EN_BIT (0x1 << 15)
#define DMA_MAX_TRSC_LEN (0xFFFFFFF)

/*data length*/
#define UART_DATA_BIT	(0x3<<2)
#define UART_DATA_5BIT	(0x0<<2)
#define UART_DATA_6BIT	(0x1<<2)
#define UART_DATA_7BIT	(0x2<<2)
#define UART_DATA_8BIT	(0x3<<2)
/*stop bit*/
#define UART_STOP_1BIT	(0x1<<4)
#define UART_STOP_2BIT	(0x3<<4)
/*parity*/
#define UART_PARITY		0x3
#define UART_PARITY_EN	0x2
#define UART_EVEN_PAR	0x0
#define UART_ODD_PAR	0x1
/*line status */
#define UART_LSR_OE	(0x1<<4)
#define UART_LSR_FE	(0x1<<3)
#define UART_LSR_PE	(0x1<<2)
#define UART_LSR_BI	(0x1<<7)
#define UART_LSR_DR	(0x1<<8)
/*flow control */
#define RX_HW_FLOW_CTL_THRESHOLD	0x68
#define RX_HW_FLOW_CTL_EN		(0x1<<7)
#define TX_HW_FLOW_CTL_EN		(0x1<<8)
/*status indicator*/
#define UART_STS_RX_FIFO_FULL	(0x1<<0)
#define UART_STS_TX_FIFO_EMPTY	(0x1<<1)
#define UART_STS_BREAK_DETECT	(0x1<<7)
#define UART_STS_TIMEOUT     	(0x1<<13)
/*baud rate*/
#define BAUD_1200_48M	0x9C40
#define BAUD_2400_48M	0x4E20
#define BAUD_4800_48M	0x2710
#define BAUD_9600_48M	0x1388
#define BAUD_19200_48M	0x09C4
#define BAUD_38400_48M	0x04E2
#define BAUD_57600_48M	0x0314
#define BAUD_115200_48M	0x01A0
#define BAUD_230400_48M	0x00D0
#define BAUD_460800_48M	0x0068
#define BAUD_921600_48M	0x0034
#define BAUD_1000000_48M 0x0030
#define BAUD_1152000_48M 0x0029
#define BAUD_1500000_48M 0x0020
#define BAUD_2000000_48M 0x0018
#define BAUD_2500000_48M 0x0013
#define BAUD_3000000_48M 0x0010

#define UART_DMA_BUF_SIZE (SP_RX_FIFO << 3)

struct sprd_uart_chip {
	/*following vals will be init in probe function */
	struct clk *clk;

	bool dma_enable;
	u32 uart_phy_base;
	void *dma_buf_v;
	dma_addr_t dma_buf_p;
	u32 dma_tx_dev_id;
	u32 dma_rx_dev_id;
	spinlock_t uart_dma_lock;

	/*following vals will be init in start up function */
	u32 dma_rx_chn;
	u32 dma_tx_chn;
	u32 dma_buf_read_offset;
	u32 dma_buf_write_offset;
	u32 dma_rx_size;
};

static struct wake_lock uart_rx_lock;	// UART0  RX  IRQ
static bool is_uart_rx_wakeup;
static struct serial_data plat_data;

#define CONFIG_SERIAL_DEBUG 0
#if CONFIG_SERIAL_DEBUG
static void serial_debug_save(int idx, void *data, size_t len);
#else
static void inline serial_debug_save(int idx, void *data, size_t len)
{
};
#endif

static inline unsigned int serial_in(struct uart_port *port, int offset)
{
	return __raw_readl(port->membase + offset);
}

static inline void serial_out(struct uart_port *port, int offset, int value)
{
	__raw_writel(value, port->membase + offset);
}

static unsigned int serial_sprd_tx_empty(struct uart_port *port)
{
	if (serial_in(port, ARM_UART_STS1) & 0xff00)
		return 0;
	else
		return 1;
}

static unsigned int serial_sprd_get_mctrl(struct uart_port *port)
{
	return TIOCM_DSR | TIOCM_CTS;
}

static void serial_sprd_set_mctrl(struct uart_port *port, unsigned int mctrl)
{
}

static void serial_sprd_stop_tx(struct uart_port *port)
{
	unsigned int ien, iclr;

	iclr = serial_in(port, ARM_UART_ICLR);
	ien = serial_in(port, ARM_UART_IEN);

	iclr |= UART_IEN_TX_FIFO_EMPTY;
	ien &= ~UART_IEN_TX_FIFO_EMPTY;

	serial_out(port, ARM_UART_ICLR, iclr);
	serial_out(port, ARM_UART_IEN, ien);
}

static void serial_sprd_start_tx(struct uart_port *port)
{
	unsigned int ien;

	ien = serial_in(port, ARM_UART_IEN);
	if (!(ien & UART_IEN_TX_FIFO_EMPTY)) {
		ien |= UART_IEN_TX_FIFO_EMPTY;
		serial_out(port, ARM_UART_IEN, ien);
	}
}

static int serial_sprd_rx_dma_config(struct uart_port *port);
static void serial_sprd_stop_rx(struct uart_port *port)
{
	unsigned int ien, iclr;
	unsigned int ctrl1;
	struct sprd_uart_chip *chip_info =
	    (struct sprd_uart_chip *)port->private_data;

	if (chip_info->dma_enable) {
		/*disable the uart dma mode */
		ctrl1 = serial_in(port, ARM_UART_CTL1);
		ctrl1 &= ~UART_DMA_EN_BIT;
		serial_out(port, ARM_UART_CTL1, ctrl1);
	}

	iclr = serial_in(port, ARM_UART_ICLR);
	ien = serial_in(port, ARM_UART_IEN);

	ien &= ~(UART_IEN_RX_FIFO_FULL | UART_IEN_BREAK_DETECT);
	iclr |= UART_IEN_RX_FIFO_FULL | UART_IEN_BREAK_DETECT;

	serial_out(port, ARM_UART_IEN, ien);
	serial_out(port, ARM_UART_ICLR, iclr);
}

static void serial_sprd_enable_ms(struct uart_port *port)
{
}

static void serial_sprd_break_ctl(struct uart_port *port, int break_state)
{
}

static inline void serial_sprd_rx_chars(int irq, void *dev_id)
{
	struct uart_port *port = (struct uart_port *)dev_id;
	struct tty_port *tty = &port->state->port;
	unsigned int status, ch, flag, lsr, max_count = 2048;

	status = serial_in(port, ARM_UART_STS1);
	lsr = serial_in(port, ARM_UART_STS2);
	while ((status & 0x00ff) && max_count--) {
		ch = serial_in(port, ARM_UART_RXD);
		flag = TTY_NORMAL;
		port->icount.rx++;

		if (unlikely(lsr &
			     (UART_LSR_BI | UART_LSR_PE | UART_LSR_FE |
			      UART_LSR_OE))) {
			/*
			 *for statistics only
			 */
			if (lsr & UART_LSR_BI) {
				lsr &= ~(UART_LSR_FE | UART_LSR_PE);
				port->icount.brk++;
				/*
				 *we do the SysRQ and SAK checking here because otherwise the
				 *break may get masked by ignore_status_mask or read_status_mask
				 */
				if (uart_handle_break(port))
					goto ignore_char;
			} else if (lsr & UART_LSR_PE)
				port->icount.parity++;
			else if (lsr & UART_LSR_FE)
				port->icount.frame++;
			if (lsr & UART_LSR_OE)
				port->icount.overrun++;
			/*
			 *mask off conditions which should be ignored
			 */
			lsr &= port->read_status_mask;
			if (lsr & UART_LSR_BI)
				flag = TTY_BREAK;
			else if (lsr & UART_LSR_PE)
				flag = TTY_PARITY;
			else if (lsr & UART_LSR_FE)
				flag = TTY_FRAME;
		}
		if (uart_handle_sysrq_char(port, ch))
			goto ignore_char;
		serial_debug_save(port, &ch, 1);
		uart_insert_char(port, lsr, UART_LSR_OE, ch, flag);
ignore_char:
		status = serial_in(port, ARM_UART_STS1);
		lsr = serial_in(port, ARM_UART_STS2);
	}
	//tty->low_latency = 1;
	tty_flip_buffer_push(tty);
}

static inline void serial_sprd_tx_chars(int irq, void *dev_id)
{
	struct uart_port *port = dev_id;
	struct circ_buf *xmit = &port->state->xmit;
	int count;

	if (port->x_char) {
		serial_out(port, ARM_UART_TXD, port->x_char);
		port->icount.tx++;
		port->x_char = 0;
		return;
	}
	if (uart_circ_empty(xmit) || uart_tx_stopped(port)) {
		serial_sprd_stop_tx(port);
		return;
	}
	count = SP_TX_FIFO;
	do {
		serial_out(port, ARM_UART_TXD, xmit->buf[xmit->tail]);
		xmit->tail = (xmit->tail + 1) & (UART_XMIT_SIZE - 1);
		port->icount.tx++;
		if (uart_circ_empty(xmit))
			break;
	} while (--count > 0);

	if (uart_circ_chars_pending(xmit) < WAKEUP_CHARS) {
		uart_write_wakeup(port);
	}
	if (uart_circ_empty(xmit)) {
		serial_sprd_stop_tx(port);
	}

}

/*
 *this handles the interrupt from one port
 */
static irqreturn_t serial_sprd_interrupt_chars(int irq, void *dev_id)
{
	struct uart_port *port = (struct uart_port *)dev_id;
	struct sprd_uart_chip *chip_info =
	    (struct sprd_uart_chip *)port->private_data;
	u32 int_status = 0;

	int_status = serial_in(port, ARM_UART_STS2);

	serial_out(port, ARM_UART_ICLR, 0xffffffff);

	if (!(chip_info->dma_enable)) {
		if (int_status &
		    (UART_STS_RX_FIFO_FULL |
		     UART_STS_BREAK_DETECT | UART_STS_TIMEOUT)) {

			serial_sprd_rx_chars(irq, port);
		}
	} else {
		if (int_status & (UART_STS_BREAK_DETECT | UART_STS_TIMEOUT)) {

			serial_sprd_rx_chars(irq, port);
		}
	}

	if (int_status & UART_STS_TX_FIFO_EMPTY) {
		serial_sprd_tx_chars(irq, port);
	}

	return IRQ_HANDLED;
}

#define SPRD_EICINT_BASE	(SPRD_EIC_BASE+0x80)
/*
 *this handles the interrupt from rx0 wakeup
 */
static irqreturn_t wakeup_rx_interrupt(int irq, void *dev_id)
{
	u32 val;

	//SIC polarity 1
	val = __raw_readl(SPRD_EICINT_BASE + 0x10);
	if ((val & BIT(0)) == BIT(0))
		val &= ~BIT(0);
	else
		val |= BIT(0);
	__raw_writel(val, SPRD_EICINT_BASE + 0x10);

	//clear interrupt
	val = __raw_readl(SPRD_EICINT_BASE + 0x0C);
	val |= BIT(0);
	__raw_writel(val, SPRD_EICINT_BASE + 0x0C);

	// set wakeup symbol
	is_uart_rx_wakeup = true;

	return IRQ_HANDLED;
}

static void serial_sprd_uart_dma_rx_irqhandler(int dma_chn, void *data)
{
	struct uart_port *port;
	struct sprd_uart_chip *chip_info;
	unsigned char *recv_buf;
	u32 recv_size;

	port = (struct uart_port *)data;
	chip_info = (struct sprd_uart_chip *)port->private_data;

	spin_lock(&chip_info->uart_dma_lock);

	chip_info->dma_buf_write_offset += SP_RX_FIFO;
	chip_info->dma_rx_size += SP_RX_FIFO;

	recv_buf =
	    (unsigned char *)(chip_info->dma_buf_v) +
	    chip_info->dma_buf_read_offset;
	recv_size =
	    chip_info->dma_buf_write_offset - chip_info->dma_buf_read_offset;

	port->icount.rx += recv_size;
	tty_insert_flip_string(&port->state->port, recv_buf, recv_size);
	tty_flip_buffer_push(&port->state->port);

	chip_info->dma_buf_read_offset += recv_size;
	if (chip_info->dma_buf_read_offset == UART_DMA_BUF_SIZE) {
		chip_info->dma_buf_read_offset = 0x0;
	}
	if (chip_info->dma_buf_write_offset == UART_DMA_BUF_SIZE) {
		chip_info->dma_buf_write_offset = 0x0;
	}

	if (chip_info->dma_rx_size > (DMA_MAX_TRSC_LEN - (SP_RX_FIFO << 2))) {
		/*reset the rx dma chn */
		serial_sprd_rx_dma_config(port);
	}

	spin_unlock(&chip_info->uart_dma_lock);
}

/* FIXME: this pin config should be just defined int general pin mux table */
static void serial_sprd_pin_config(void)
{
#ifndef CONFIG_ARCH_SCX35
	value = __raw_readl(SPRD_GREG_BASE + 0x08);
	value |= 0x07 << 20;
	__raw_writel(value, SPRD_GREG_BASE + 0x08);
#endif
}

static int serial_sprd_rx_dma_config(struct uart_port *port)
{
	int ret;
	struct sprd_uart_chip *chip_info =
	    (struct sprd_uart_chip *)port->private_data;
	/*the sci_dma_cfg struct must in inti with {0} */
	struct sci_dma_cfg rx_dma_cfg;	// , tx_dma_cfg;

	chip_info = (struct sprd_uart_chip *)port->private_data;

	/*config the rx dma chn */
	if (chip_info && chip_info->dma_enable && chip_info->dma_rx_dev_id) {
		if (chip_info->dma_rx_chn == 0) {
			chip_info->dma_rx_chn =
			    sci_dma_request("uart", FULL_DMA_CHN);
			printk("alloc dma chn %d\n", chip_info->dma_rx_chn);
			printk("the dma buf addr is %x\n",
			       chip_info->dma_buf_p);
		} else {
			/*rset the dma chn */
			sci_dma_stop(chip_info->dma_rx_chn,
				     chip_info->dma_rx_dev_id);
		}
		/*fixme! */
		chip_info->dma_buf_read_offset = 0x0;
		chip_info->dma_buf_write_offset = 0x0;
		chip_info->dma_rx_size = 0x0;

		/*the struct sci_dma_cfg must be init with {0} */
		memset(&rx_dma_cfg, 0x0, sizeof(rx_dma_cfg));

		rx_dma_cfg.datawidth = BYTE_WIDTH;
		rx_dma_cfg.src_addr = chip_info->uart_phy_base + ARM_UART_RXD;
		rx_dma_cfg.des_addr = chip_info->dma_buf_p;
		rx_dma_cfg.src_step = 0x0;
		rx_dma_cfg.des_step = 0x1;
		rx_dma_cfg.fragmens_len = SP_RX_FIFO;
		rx_dma_cfg.block_len = SP_RX_FIFO;
		/*we will never transfer 256Mbytes data in one times */
		rx_dma_cfg.transcation_len = DMA_MAX_TRSC_LEN;
		rx_dma_cfg.req_mode = FRAG_REQ_MODE;
		rx_dma_cfg.wrap_to = rx_dma_cfg.des_addr;
		/*fixme, the wrap mode config */
		rx_dma_cfg.wrap_ptr =
		    chip_info->dma_buf_p + UART_DMA_BUF_SIZE - 1;

		ret =
		    sci_dma_config(chip_info->dma_rx_chn, &rx_dma_cfg, 1, NULL);
		/*fixme */
		ret =
		    sci_dma_register_irqhandle(chip_info->dma_rx_chn, FRAG_DONE,
					       serial_sprd_uart_dma_rx_irqhandler,
					       port);
		/*fixme */
		sci_dma_start(chip_info->dma_rx_chn, chip_info->dma_rx_dev_id);
	}

	if (chip_info && chip_info->dma_tx_dev_id) {
	}

	return 0;
}

static int serial_sprd_startup(struct uart_port *port)
{
	int ret = 0;
	unsigned int ien, ctrl1;
	struct sprd_uart_chip *chip_info =
	    (struct sprd_uart_chip *)port->private_data;

	/* FIXME: don't know who change u0cts pin in 88 */
	serial_sprd_pin_config();
	clk_enable(chip_info->clk);

	/* set fifo water mark,tx_int_mark=8,rx_int_mark=1 */
#if 0				/* ? */
	serial_out(port, ARM_UART_CTL2, 0x801);
#endif

	if (chip_info->dma_enable) {
		/*disable the uart dma mode */
		ctrl1 = serial_in(port, ARM_UART_CTL1);
		ctrl1 &= ~(UART_DMA_EN_BIT);
		serial_out(port, ARM_UART_CTL1, ctrl1);

		serial_sprd_rx_dma_config(port);
	}
	serial_out(port, ARM_UART_CTL2, ((SP_TX_FIFO << 8) | SP_RX_FIFO));
	/* clear rx fifo */
	while (serial_in(port, ARM_UART_STS1) & 0x00ff) {
		serial_in(port, ARM_UART_RXD);
	}
	/* clear tx fifo */
	while (serial_in(port, ARM_UART_STS1) & 0xff00) ;
	/* clear interrupt */
	serial_out(port, ARM_UART_IEN, 0x00);
	serial_out(port, ARM_UART_ICLR, 0xffffffff);
	/* allocate irq */
	ret =
	    request_irq(port->irq, serial_sprd_interrupt_chars, IRQF_DISABLED,
			"serial", port);
	if (ret) {
		printk(KERN_ERR "fail to request serial irq\n");
		free_irq(port->irq, port);
	}

	if (BT_RX_WAKE_UP == plat_data.wakeup_type) {
		int ret2 = 0;

		if (!port->line) {
			ret2 =
			    request_irq(IRQ_WAKEUP, wakeup_rx_interrupt,
					IRQF_SHARED, "wakeup_rx", port);
			if (ret2) {
				printk("fail to request wakeup irq\n");
				free_irq(IRQ_WAKEUP, NULL);
			}
		}
	}

	ctrl1 = serial_in(port, ARM_UART_CTL1);
	if (chip_info->dma_enable) {
		ctrl1 |= 0x3e00;
	} else {
		ctrl1 |= 0x3e00 | SP_RX_FIFO;
	}
	serial_out(port, ARM_UART_CTL1, ctrl1);

	spin_lock(&port->lock);
	/* enable interrupt */
	ien = serial_in(port, ARM_UART_IEN);
	if (chip_info->dma_enable) {
		if (chip_info->dma_rx_dev_id) {
			ien |=
			    UART_IEN_TX_FIFO_EMPTY |
			    UART_IEN_BREAK_DETECT | UART_IEN_TIMEOUT;
			serial_out(port, ARM_UART_IEN, ien);

			/*enable the uart dma mode */
			ctrl1 = serial_in(port, ARM_UART_CTL1);
			ctrl1 |= UART_DMA_EN_BIT;
			serial_out(port, ARM_UART_CTL1, ctrl1);
		}

		if (chip_info->dma_tx_dev_id) {
		}
	} else {
		ien |=
		    UART_IEN_RX_FIFO_FULL | UART_IEN_TX_FIFO_EMPTY |
		    UART_IEN_BREAK_DETECT | UART_IEN_TIMEOUT;
		serial_out(port, ARM_UART_IEN, ien);
	}

	spin_unlock(&port->lock);

	return 0;
}

static void serial_sprd_shutdown(struct uart_port *port)
{
	u32 ctrl1;
	struct sprd_uart_chip *chip_info =
	    (struct sprd_uart_chip *)port->private_data;

	if (chip_info->dma_enable) {
		/*disable the uart dma mode */
		ctrl1 = serial_in(port, ARM_UART_CTL1);
		ctrl1 &= ~UART_DMA_EN_BIT;
		serial_out(port, ARM_UART_CTL1, ctrl1);

		if (chip_info->dma_rx_chn) {
			sci_dma_free(chip_info->dma_rx_chn);
			chip_info->dma_rx_chn = 0x0;
		}

		if (chip_info->dma_tx_chn) {
			sci_dma_free(chip_info->dma_tx_chn);
			chip_info->dma_tx_chn = 0x0;
		}
	}
	serial_out(port, ARM_UART_IEN, 0x0);
	serial_out(port, ARM_UART_ICLR, 0xffffffff);
	clk_disable(chip_info->clk);
	free_irq(port->irq, port);
}

static void serial_sprd_set_termios(struct uart_port *port,
				    struct ktermios *termios,
				    struct ktermios *old)
{
	unsigned int baud, quot;
	unsigned int lcr, fc;
	/* ask the core to calculate the divisor for us */
	baud = uart_get_baud_rate(port, termios, old, 1200, 3000000);

	quot = (unsigned int)((port->uartclk + baud / 2) / baud);

	/* set data length */
	lcr = serial_in(port, ARM_UART_CTL0);
	lcr &= ~UART_DATA_BIT;
	switch (termios->c_cflag & CSIZE) {
	case CS5:
		lcr |= UART_DATA_5BIT;
		break;
	case CS6:
		lcr |= UART_DATA_6BIT;
		break;
	case CS7:
		lcr |= UART_DATA_7BIT;
		break;
	default:
	case CS8:
		lcr |= UART_DATA_8BIT;
		break;
	}
	/* calculate stop bits */
	lcr &= ~(UART_STOP_1BIT | UART_STOP_2BIT);
	if (termios->c_cflag & CSTOPB)
		lcr |= UART_STOP_2BIT;
	else
		lcr |= UART_STOP_1BIT;
	/* calculate parity */
	lcr &= ~UART_PARITY;
	if (termios->c_cflag & PARENB) {
		lcr |= UART_PARITY_EN;
		if (termios->c_cflag & PARODD)
			lcr |= UART_ODD_PAR;
		else
			lcr |= UART_EVEN_PAR;
	}
	/* change the port state. */
	/* update the per-port timeout */
	uart_update_timeout(port, termios->c_cflag, baud);

	port->read_status_mask = UART_LSR_OE;
	if (termios->c_iflag & INPCK)
		port->read_status_mask |= UART_LSR_FE | UART_LSR_PE;
	if (termios->c_iflag & (BRKINT | PARMRK))
		port->read_status_mask |= UART_LSR_BI;
	/* characters to ignore */
	port->ignore_status_mask = 0;
	if (termios->c_iflag & IGNPAR)
		port->ignore_status_mask |= UART_LSR_PE | UART_LSR_FE;
	if (termios->c_iflag & IGNBRK) {
		port->ignore_status_mask |= UART_LSR_BI;
		/* if we ignore parity and break indicators,ignore overruns too */
		if (termios->c_iflag & IGNPAR)
			port->ignore_status_mask |= UART_LSR_OE;
	}
	/* ignore all characters if CREAD is not set */
#if 0				/* ? */
	if ((termios->c_cflag & CREAD) == 0)
		port->ignore_status_mask |= UART_LSR_DR;
#endif
	/* flow control */
	fc = serial_in(port, ARM_UART_CTL1);
	fc &= ~(0x7F | RX_HW_FLOW_CTL_EN | TX_HW_FLOW_CTL_EN);
	if (termios->c_cflag & CRTSCTS) {
		fc |= RX_HW_FLOW_CTL_THRESHOLD;
		fc |= RX_HW_FLOW_CTL_EN;
		fc |= TX_HW_FLOW_CTL_EN;
	}
	/* clock divider bit0~bit15 */
	serial_out(port, ARM_UART_CLKD0, quot & 0xffff);
	/* clock divider bit16~bit20 */
	serial_out(port, ARM_UART_CLKD1, (quot & 0x1f0000) >> 16);
	serial_out(port, ARM_UART_CTL0, lcr);
	//fc |= 0x3e00 | SP_RX_FIFO;
	serial_out(port, ARM_UART_CTL1, fc);
}

static const char *serial_sprd_type(struct uart_port *port)
{
	return "SPX";
}

static void serial_sprd_release_port(struct uart_port *port)
{
}

static int serial_sprd_request_port(struct uart_port *port)
{
	return 0;
}

static void serial_sprd_config_port(struct uart_port *port, int flags)
{
	if (flags & UART_CONFIG_TYPE && serial_sprd_request_port(port) == 0)
		port->type = PORT_SPRD;
}

static int serial_sprd_verify_port(struct uart_port *port,
				   struct serial_struct *ser)
{
	if (unlikely(ser->type != PORT_SPRD))
		return -EINVAL;
	if (unlikely(port->irq != ser->irq))
		return -EINVAL;
	return 0;
}

static struct uart_ops serial_sprd_ops = {
	.tx_empty = serial_sprd_tx_empty,
	.get_mctrl = serial_sprd_get_mctrl,
	.set_mctrl = serial_sprd_set_mctrl,
	.stop_tx = serial_sprd_stop_tx,
	.start_tx = serial_sprd_start_tx,
	.stop_rx = serial_sprd_stop_rx,
	.enable_ms = serial_sprd_enable_ms,
	.break_ctl = serial_sprd_break_ctl,
	.startup = serial_sprd_startup,
	.shutdown = serial_sprd_shutdown,
	.set_termios = serial_sprd_set_termios,
	.type = serial_sprd_type,
	.release_port = serial_sprd_release_port,
	.request_port = serial_sprd_request_port,
	.config_port = serial_sprd_config_port,
	.verify_port = serial_sprd_verify_port,
};
static struct uart_port *serial_sprd_ports[UART_NR_MAX] = { 0 };

static struct {
	uint32_t ien;
	uint32_t ctrl0;
	uint32_t ctrl1;
	uint32_t ctrl2;
	uint32_t clkd0;
	uint32_t clkd1;
	uint32_t dspwait;
} uart_bak[UART_NR_MAX];

static struct clk *clk_startup(struct platform_device *pdev)
{
	struct clk *clk;
	struct clk *clk_parent;
	char clk_name[10];
	int ret;
	int clksrc;
	struct serial_data plat_local_data;

	sprintf(clk_name, "clk_uart%d", pdev->id);
	clk = clk_get(NULL, clk_name);
	if (IS_ERR(clk)) {
		printk("clock[%s]: failed to get clock by clk_get()!\n",
		       clk_name);
		return NULL;
	}

	plat_local_data = *(struct serial_data *)(pdev->dev.platform_data);
	clksrc = plat_local_data.clk;

	if (clksrc == 48000000) {
		clk_parent = clk_get(NULL, "clk_48m");
	} else {
		clk_parent = clk_get(NULL, "ext_26m");
	}
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get()!\n",
		       clk_name, "clk_48m");
		return NULL;
	}

	ret = clk_set_parent(clk, clk_parent);
	if (ret) {
		printk("clock[%s]: clk_set_parent() failed!\n", clk_name);
		return NULL;
	}
#if 0
	ret = clk_enable(clk);
	if (ret) {
		printk("clock[%s]: clk_enable() failed!\n", clk_name);
	}
#endif
	return clk;
}

static int serial_sprd_setup_port(struct platform_device *pdev,
				  struct resource *mem, struct resource *irq,
				  struct sprd_uart_chip *chip_info)
{
	struct serial_data plat_local_data;
	struct uart_port *up;
	up = kzalloc(sizeof(*up), GFP_KERNEL);
	if (up == NULL) {
		return -ENOMEM;
	}

	up->line = pdev->id;
	up->type = PORT_SPRD;
	up->iotype = SERIAL_IO_PORT;
	up->membase = (void *)mem->start;
	up->mapbase = mem->start;

	plat_local_data = *(struct serial_data *)(pdev->dev.platform_data);
	up->uartclk = plat_local_data.clk;

	up->irq = irq->start;
	up->fifosize = 128;
	up->ops = &serial_sprd_ops;
	up->flags = ASYNC_BOOT_AUTOCONF;

	if (chip_info->dma_enable) {
		chip_info->dma_buf_v =
		    dma_alloc_writecombine(NULL, UART_DMA_BUF_SIZE,
					   &chip_info->dma_buf_p, GFP_KERNEL);
		if (!chip_info->dma_buf_v) {
			kfree(up);
			return -ENOMEM;
		}

		spin_lock_init(&(chip_info->uart_dma_lock));

		up->private_data = chip_info;
	}
	up->private_data = chip_info;
	serial_sprd_ports[pdev->id] = up;

	/*fixme, need to check the result */
	chip_info->clk = clk_startup(pdev);
	return 0;
}

#ifdef CONFIG_SERIAL_SPRD_UART_CONSOLE
static inline void wait_for_xmitr(struct uart_port *port)
{
	unsigned int status, tmout = 10000;
	/* wait up to 10ms for the character(s) to be sent */
	do {
		status = serial_in(port, ARM_UART_STS1);
		if (--tmout == 0)
			break;
		udelay(1);
	} while (status & 0xff00);
}

static void serial_sprd_console_putchar(struct uart_port *port, int ch)
{
	wait_for_xmitr(port);
	serial_out(port, ARM_UART_TXD, ch);
}

static void serial_sprd_console_write(struct console *co, const char *s,
				      unsigned int count)
{
	struct uart_port *port = serial_sprd_ports[co->index];
	int ien;
	int locked = 1;
	if (oops_in_progress)
		locked = spin_trylock(&port->lock);
	else
		spin_lock(&port->lock);
	/*firstly,save the IEN register and disable the interrupts */
	ien = serial_in(port, ARM_UART_IEN);
	serial_out(port, ARM_UART_IEN, 0x0);

	uart_console_write(port, s, count, serial_sprd_console_putchar);
	/*finally,wait for  TXD FIFO to become empty and restore the IEN register */
	wait_for_xmitr(port);
	serial_out(port, ARM_UART_IEN, ien);
	if (locked)
		spin_unlock(&port->lock);
}

static int __init serial_sprd_console_setup(struct console *co, char *options)
{
	struct uart_port *port;
	struct sprd_uart_chip *chip_info;
	int baud = 115200;
	int bits = 8;
	int parity = 'n';
	int flow = 'n';

	if (unlikely(co->index >= UART_NR_MAX || co->index < 0))
		co->index = 0;

	port = serial_sprd_ports[co->index];
	if (port == NULL) {
		printk(KERN_INFO "srial port %d not yet initialized\n",
		       co->index);
		return -ENODEV;
	}
	chip_info = (struct sprd_uart_chip *)port->private_data;

	clk_enable(chip_info->clk);
	if (options)
		uart_parse_options(options, &baud, &parity, &bits, &flow);

	return uart_set_options(port, co, baud, parity, bits, flow);
}

static struct uart_driver serial_sprd_reg;
static struct console serial_sprd_console = {
	.name = "ttyS",
	.write = serial_sprd_console_write,
	.device = uart_console_device,
	.setup = serial_sprd_console_setup,
	.flags = CON_PRINTBUFFER,
	.index = -1,
	.data = &serial_sprd_reg,
};

#define SPRD_CONSOLE		&serial_sprd_console

#else /* !CONFIG_SERIAL_SPRD_UART_CONSOLE */
#define SPRD_CONSOLE		NULL
#endif

static struct uart_driver serial_sprd_reg = {
	.owner = THIS_MODULE,
	.driver_name = "serial_sprd",
	.dev_name = SP_TTY_NAME,
	.major = SP_TTY_MAJOR,
	.minor = SP_TTY_MINOR_START,
	.nr = UART_NR_MAX,
	.cons = SPRD_CONSOLE,
};

static int serial_sprd_probe(struct platform_device *pdev)
{
	int ret;
	struct resource *mem, *irq, *dma_res, *phy_addr;
	struct sprd_uart_chip *chip_info;

	if (unlikely(pdev->id < 0 || pdev->id >= UART_NR_MAX)) {
		dev_err(&pdev->dev, "does not support id %d\n", pdev->id);
		return -ENXIO;
	}

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (unlikely(!mem)) {
		dev_err(&pdev->dev, "not provide mem resource\n");
		return -ENODEV;
	}

	irq = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (unlikely(!irq)) {
		dev_err(&pdev->dev, "not provide irq resource\n");
		return -ENODEV;
	}
	/*fixme, need to check the result */
	chip_info = kzalloc(sizeof(*chip_info), GFP_KERNEL);

	/*if can't get the dma resource, use the normal mode */
	dma_res = platform_get_resource_byname(pdev, IORESOURCE_DMA,
					       "serial_dma_rx_id");
	if (dma_res) {
		phy_addr = platform_get_resource_byname(pdev, IORESOURCE_MEM,
							"serial_phy_addr");
		if (!phy_addr) {
			dev_err(&pdev->dev,
				"not provide uart %d phy addr resource\n",
				pdev->id);
			return -ENODEV;
		}

		chip_info->uart_phy_base = phy_addr->start;
		chip_info->dma_rx_dev_id = dma_res->start;

		dma_res = platform_get_resource_byname(pdev, IORESOURCE_DMA,
						       "serial_dma_tx_id");
		if (dma_res) {
			chip_info->dma_tx_dev_id = dma_res->start;
			chip_info->dma_enable = true;
		} else {
			chip_info->dma_enable = false;
		}
	} else {
		chip_info->dma_enable = false;
	}

	ret = serial_sprd_setup_port(pdev, mem, irq, chip_info);
	if (unlikely(ret != 0)) {
		dev_err(&pdev->dev, "setup port failed\n");
		return ret;
	}
	ret = uart_add_one_port(&serial_sprd_reg, serial_sprd_ports[pdev->id]);
	if (likely(ret == 0)) {
		platform_set_drvdata(pdev, serial_sprd_ports[pdev->id]);
	}
	if (!((void *)(pdev->dev.platform_data))) {
		dev_err(&pdev->dev, "serial driver get platform data failed\n");
		return -ENODEV;
	}
	plat_data = *(struct serial_data *)(pdev->dev.platform_data);
	printk("bt host wake up type is %d, clk is %d \n",
	       plat_data.wakeup_type, plat_data.clk);
	if (BT_RX_WAKE_UP == plat_data.wakeup_type) {
		wake_lock_init(&uart_rx_lock, WAKE_LOCK_SUSPEND,
			       "uart_rx_lock");
	}

	return ret;
}

static int serial_sprd_remove(struct platform_device *pdev)
{
	struct uart_port *up = platform_get_drvdata(pdev);
	struct sprd_uart_chip *chip_info;

	chip_info = (struct sprd_uart_chip *)up->private_data;
	if (chip_info->dma_enable) {
		if (chip_info->dma_buf_v) {
			dma_free_writecombine(NULL, UART_DMA_BUF_SIZE,
					      (void *)chip_info->dma_buf_v,
					      chip_info->dma_buf_p);
		}

		up->private_data = NULL;
	}
	kfree(chip_info);

	platform_set_drvdata(pdev, NULL);
	if (up) {
		uart_remove_one_port(&serial_sprd_reg, up);
		kfree(up);
		serial_sprd_ports[pdev->id] = NULL;
	}
	return 0;
}

static int serial_sprd_suspend(struct platform_device *dev, pm_message_t state)
{
	/* TODO */
	int id = dev->id;
	struct uart_port *port;

	port = serial_sprd_ports[id];
	uart_bak[id].ien = serial_in(port, ARM_UART_IEN);
	uart_bak[id].ctrl0 = serial_in(port, ARM_UART_CTL0);
	uart_bak[id].ctrl1 = serial_in(port, ARM_UART_CTL1);
	uart_bak[id].ctrl2 = serial_in(port, ARM_UART_CTL2);
	uart_bak[id].clkd0 = serial_in(port, ARM_UART_CLKD0);
	uart_bak[id].clkd1 = serial_in(port, ARM_UART_CLKD1);

	if (BT_RX_WAKE_UP == plat_data.wakeup_type) {
		is_uart_rx_wakeup = false;
	} else if (BT_RTS_HIGH_WHEN_SLEEP == plat_data.wakeup_type) {
		/*when the uart0 going to sleep,config the RTS pin of hardware flow
		   control as the AF3 to make the pin can be set to high */
		unsigned long fc = 0;
		struct uart_port *port = serial_sprd_ports[0];
		pinmap_set(REG_PIN_U0RTS,
			   (BITS_PIN_DS(3) | BITS_PIN_AF(3) | BIT_PIN_WPU |
			    BIT_PIN_SLP_WPU | BIT_PIN_SLP_OE));
		fc = serial_in(port, ARM_UART_CTL1);
		fc &= ~(RX_HW_FLOW_CTL_EN | TX_HW_FLOW_CTL_EN);
		serial_out(port, ARM_UART_CTL1, fc);
	} else {
		pr_debug("BT host wake up feature has not been supported\n");
	}

	return 0;
}

static int serial_sprd_resume(struct platform_device *dev)
{
	/* TODO */
	int id = dev->id;
	struct uart_port *port = serial_sprd_ports[id];

	port = serial_sprd_ports[id];
	serial_out(port, ARM_UART_CTL0, uart_bak[id].ctrl0);
	serial_out(port, ARM_UART_CTL1, uart_bak[id].ctrl1);
	serial_out(port, ARM_UART_CTL2, uart_bak[id].ctrl2);
	serial_out(port, ARM_UART_CLKD0, uart_bak[id].clkd0);
	serial_out(port, ARM_UART_CLKD1, uart_bak[id].clkd1);
	serial_out(port, ARM_UART_IEN, uart_bak[id].ien);

	if (BT_RX_WAKE_UP == plat_data.wakeup_type) {
		if (is_uart_rx_wakeup) {
			is_uart_rx_wakeup = false;
			wake_lock_timeout(&uart_rx_lock, HZ / 5);	// 0.2s
		}
	} else if (BT_RTS_HIGH_WHEN_SLEEP == plat_data.wakeup_type) {
		/*when the uart0 waking up,reconfig the RTS pin of hardware flow control work
		   in the hardware flow control mode to make the pin can be controlled by
		   hardware */
		unsigned long fc = 0;
		struct uart_port *port = serial_sprd_ports[0];
		fc = serial_in(port, ARM_UART_CTL1);
		fc |= (RX_HW_FLOW_CTL_EN | TX_HW_FLOW_CTL_EN);
		serial_out(port, ARM_UART_CTL1, fc);
		pinmap_set(REG_PIN_U0RTS,
			   (BITS_PIN_DS(1) | BITS_PIN_AF(0) | BIT_PIN_NUL |
			    BIT_PIN_SLP_NUL | BIT_PIN_SLP_Z));
	} else {
		pr_debug("BT host wake up feature has not been supported\n");
	}

	return 0;
}

static struct platform_driver serial_sprd_driver = {
	.probe = serial_sprd_probe,
	.remove = serial_sprd_remove,
	.suspend = serial_sprd_suspend,
	.resume = serial_sprd_resume,
	.driver = {
		   .name = "serial_sprd",
		   .owner = THIS_MODULE,
		   },
};

#if CONFIG_SERIAL_DEBUG
#include <linux/seq_file.h>
#include <linux/proc_fs.h>
#define SERIAL_DEBUG_BUF_SIZE (2*1024*1024)
static void *serial_debug_buf;
static volatile int serial_debug_buf_idx;
static int serial_debug_buf_count;
static void serial_debug_save(int idx, void *data, size_t len)
{
	/* if (idx == 1) */  {
		if (serial_debug_buf_idx + len < serial_debug_buf_count) {
			memcpy(serial_debug_buf + serial_debug_buf_idx, data,
			       len);
			serial_debug_buf_idx += len;
			// seq_write(serial_seq_m, data, len);
		} else {
			pr_err("serial debug buffer is full\n");
		}
	}
}

static int show_serial(struct seq_file *p, void *v)
{
	pr_info("read serial debug buffer %d\n", serial_debug_buf_idx);
	p->count = serial_debug_buf_idx;
	barrier();
	serial_debug_buf_idx = 0;
	pr_info("clear serial debug buffer\n");
	return 0;
}

static int single_release_serial(struct inode *inode, struct file *file)
{
	struct seq_file *m = file->private_data;
	m->buf = NULL;
	single_release(inode, file);
	serial_debug_buf_idx = 0;
	pr_info("clear serial debug buffer\n");
}

static int serial_open(struct inode *inode, struct file *file)
{
	struct seq_file *m;
	int res;

	res = single_open(file, show_serial, NULL);
	if (!res) {
		m = file->private_data;
		m->buf = serial_debug_buf;
		m->size = SERIAL_DEBUG_BUF_SIZE;
	}

	return res;
}

static const struct file_operations proc_serial_operations = {
	.open = serial_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release_serial,
};

static void serial_debug_init(void)
{
	int nr_pages = SERIAL_DEBUG_BUF_SIZE >> PAGE_SHIFT;
	struct page *pages;

	pages = alloc_pages(GFP_KERNEL, order_base_2(nr_pages));
	serial_debug_buf = page_address(pages);
	serial_debug_buf_count = nr_pages << PAGE_SHIFT;

	proc_create("serial_debug", 0, NULL, &proc_serial_operations);
}
#else
static void serial_debug_init(void)
{
};
#endif

static int __init serial_sprd_init(void)
{
	int ret = 0;

	ret = uart_register_driver(&serial_sprd_reg);
	if (unlikely(ret != 0))
		return ret;

	ret = platform_driver_register(&serial_sprd_driver);
	if (unlikely(ret != 0))
		uart_unregister_driver(&serial_sprd_reg);

	serial_debug_init();

	return ret;
}

static void __exit serial_sprd_exit(void)
{
	platform_driver_unregister(&serial_sprd_driver);
	uart_unregister_driver(&serial_sprd_reg);
}

module_init(serial_sprd_init);
module_exit(serial_sprd_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("sprd serial driver $Revision:1.0$");
