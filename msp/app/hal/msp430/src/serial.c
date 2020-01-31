/*
 * MSP430 serial driver
 *
 * Copyright 2018 Phoenix Systems
 * Copyright 2007 Pawel Pisarczyk
 * Author: Krystian Wasik
 *
 * %LICENSE%
 */

#include <serial.h>
#include <hal.h>

#include <string.h>

#define USCI_PORT_SEL                   P3SEL
#define RXD                             BIT4
#define TXD                             BIT3

#define SERIAL_BUFFER_SIZE              (128)

typedef struct {
    unsigned empty;
    unsigned first;
    unsigned last;
    unsigned overflow;
    uint8_t data[SERIAL_BUFFER_SIZE];
} ring_buffer_t;

static ring_buffer_t serial_rx_buffer;
static uint8_t serial_tx_buffer[SERIAL_BUFFER_SIZE];

static volatile int serial_tx_busy = 0;

static void ring_buffer_init(ring_buffer_t *s);
static void ring_buffer_push(ring_buffer_t *s, uint8_t in);
static int ring_buffer_pop(ring_buffer_t *s, uint8_t *out);

int serial_init(serial_baudrate_t baudrate)
{
    ring_buffer_init(&serial_rx_buffer);

    UCA0CTL1 = UCSWRST;
    UCA0CTL0 = 0;
    UCA0CTL1 |= UCSSEL__SMCLK;

    switch (baudrate) {
    case serial_baudrate__9600:
        UCA0BRW = 416;
        UCA0MCTL = UCBRS_2 + UCBRF_0;
        break;
    case serial_baudrate__19200:
        UCA0BRW = 208;
        UCA0MCTL = UCBRS_6 + UCBRF_0;
        break;
    case serial_baudrate__38400:
        UCA0BRW = 138;
        UCA0MCTL = UCBRS_3 + UCBRF_0;
        break;
    case serial_baudrate__57600:
        UCA0BRW = 69;
        UCA0MCTL = UCBRS_7 + UCBRF_0;
        break;
    case serial_baudrate__115200:
        UCA0BRW = 34;
        UCA0MCTL = UCBRS_4 + UCBRF_0;
        break;
    }

    USCI_PORT_SEL |= RXD | TXD;
    UCA0CTL1 &= ~UCSWRST;
    UCA0IE = UCRXIE; /* Enable RX interrupts */

    return 0;
}

int serial_is_tx_busy(void)
{
    return serial_tx_busy;
}

#define DMA0TSEL_MASK       (0x001f)
#define DMA1TSEL_MASK       (0x1f00)

int serial_write(const uint8_t *data, uint16_t len)
{
    if (serial_tx_busy)
        return 0;

    if (len > sizeof(serial_tx_buffer))
        len = sizeof(serial_tx_buffer);

    serial_tx_busy = 1;

    memcpy(serial_tx_buffer, data, len);

    /* Configure DMA0 */
    DMACTL0 &= ~(DMA0TSEL_MASK);
    DMACTL0 |= DMA0TSEL__USCIA0TX;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"
    DMA0SA = (uint32_t)(serial_tx_buffer + 1);
    DMA0DA = (uint32_t)&UCA0TXBUF;
#pragma GCC diagnostic pop

    DMA0SZ = len - 1;
    DMA0CTL = DMASRCBYTE | DMADSTBYTE | DMASRCINCR_3 | DMADT_0 | DMAIE | DMAEN | DMAREQ;

    /* Start first transfer */
    UCA0TXBUF = *serial_tx_buffer;

    return len;
}

int serial_read(uint8_t *data, uint16_t len)
{
    uint8_t *orig_data = data;

    uint8_t byte;
    while (len--) {
        if (ring_buffer_pop(&serial_rx_buffer, &byte) < 0) {
            break;
        } else {
            *data++ = byte;
        }
    }

    return data - orig_data;
}

int serial_deinit(void)
{
    DMA0CTL &= ~DMAEN;
    UCA0CTL1 = UCSWRST;
    UCA0CTL1 &= ~UCMST;

    return 0;
}

void serial_tx_dma_handler(void)
{
    serial_tx_busy = 0;
    DMA0CTL &= ~DMAEN;
}

void serial_rx_handler(void)
{
    uint8_t byte = UCA0RXBUF;

    ring_buffer_push(&serial_rx_buffer, byte);
}

static void ring_buffer_init( ring_buffer_t *s)
{
    if (!s)
        return;

    __istate_t istate = __enter_critical();

    s->empty = 1;
    s->first = 0;
    s->last = 0;
    s->overflow = 0;

    __leave_critical(istate);
}

static void ring_buffer_push(ring_buffer_t *s, uint8_t in)
{
    if (!s)
        return;

    __istate_t istate = __enter_critical();

    if (s->empty) {
        s->empty = 0;
    } else {
        s->last = (s->last + 1)%SERIAL_BUFFER_SIZE;

        /* Check for overflow */
        if (s->last == s->first) {
            s->overflow = 1;
            s->first = (s->first + 1)%SERIAL_BUFFER_SIZE;
        }
    }

    s->data[s->last] = in;

    __leave_critical(istate);
}

static int ring_buffer_pop(ring_buffer_t *s, uint8_t *out)
{
    if (!s || s->empty)
        return -1;

    __istate_t istate = __enter_critical();

    if (out) *out = s->data[s->first];

    if (s->first == s->last) {
        s->empty = 1;
    } else {
        s->first = (s->first + 1)%SERIAL_BUFFER_SIZE;
    }

    __leave_critical(istate);

    return 0;
}

#define DMAIV__CHANNEL0              (0x2)

void __attribute__((interrupt(DMA_VECTOR))) __interrupt_vector_dma(void)
{
    uint16_t _dmaiv = DMAIV; /* Read DMAIV once to clear interrupt flag */

    if(_dmaiv == DMAIV__CHANNEL0)
        serial_tx_dma_handler();
}

#define UCAxIV__UCRXIFG             (0x2)
#define UCAxIV__UCTXIFG             (0x4)

void __attribute__((interrupt(USCI_A0_VECTOR))) __interrupt_vector_usci_a1(void)
{
    uint16_t _uca0iv = UCA0IV;

    if (_uca0iv == UCAxIV__UCRXIFG)
        serial_rx_handler();
}
