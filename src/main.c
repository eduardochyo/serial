
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/__assert.h>
#include <string.h>
#include <stdio.h>
#include <zephyr/drivers/uart.h>




/* size of stack area used by each thread */
#define STACKSIZE 1024

/* scheduling priority used by each thread */
#define PRIORITY 7

K_FIFO_DEFINE(printk_fifo);

struct data_item_t  {
	void *fifo_reserved; /* 1st word reserved for use by fifo */
	uint8_t palavra;
    uint8_t sync;
    uint8_t stx;
    uint8_t id;
    char v[7]; /*o número de dados d1, d2, d3... */
    uint8_t etx;
};

struct data_item_t tx_data;



/* change this to any other UART peripheral if desired */
#define UART_DEVICE_NODE DT_CHOSEN(zephyr_shell_uart)

#define MSG_SIZE 1

/* queue to store up to 10 messages (aligned to 4-byte boundary) */
K_MSGQ_DEFINE(uart_msgq, MSG_SIZE, 100, 4);

static const struct device *const uart_dev = DEVICE_DT_GET(UART_DEVICE_NODE);

/* receive buffer used in UART ISR callback */
static char rx_buf[MSG_SIZE];
static int rx_buf_pos;
char tx_buf[MSG_SIZE];

/*
 * Read characters from UART until line end is detected. Afterwards push the
 * data to the message queue.
 */
void serial_cb(const struct device *dev, void *user_data)
{
	uint8_t c;


	/* read until FIFO empty */
	while (uart_fifo_read(uart_dev, &c, 1) == 1) {
		if ((c == '\n' || c == '\r') && rx_buf_pos > 0) {
			/* terminate string */
			rx_buf[rx_buf_pos] = '\0';

			/* if queue is full, message is silently dropped */
			k_msgq_put(&uart_msgq, &rx_buf, K_NO_WAIT);

			/* reset the buffer (it was copied to the msgq) */
			rx_buf_pos = 0;
		} else if (rx_buf_pos < (sizeof(rx_buf) - 1)) {
			rx_buf[rx_buf_pos++] = c;
		}
		/* else: characters beyond buffer size are dropped */
	}
    int i;  
    tx_data.palavra = 0b01010101;
    tx_data.sync = 0b00010110;
    tx_data.stx = 0b00000010;
    tx_data.id = 0b00001000;
    tx_data.etx = 0b00000011;

    i = 0;
    while (k_msgq_get(&uart_msgq, &tx_buf, K_NO_WAIT) == 0) {
	    tx_data.v[i] = tx_buf;
        i++;
        if (i == 7){
	        k_fifo_put(&printk_fifo, &tx_data);
            i = 0;
            tx_data.id = (0b00001000 | 7);
        }
	}
    if (i != 0){
        k_fifo_put(&printk_fifo, &tx_data);
        tx_data.id = (0b00001000 | i);
        i = 0;
    }

}

void uart_out()
{
    int i;
    int n;
    char aux;     
    tx_data.palavra = 0b01010101;
    tx_data.sync = 0b00010110;
    tx_data.stx = 0b00000010;
    tx_data.id = 0b00001000;
    tx_data.etx = 0b00000011;

    printk("Digite o numero de caracteres");
    scanf("%d", &n);
    tx_data.id = (0b00001000 | n);
    printk("digite %d caracteres", n);
    for(i=0; i<7; i++ ){
        scanf("%c", &aux);
	    tx_data.v[i] = aux;
    }
	k_fifo_put(&printk_fifo, &tx_data);
    	
}



void uart_in(int id)
{
		if(id == 0){
		struct data_item_t *rx_data = k_fifo_get(&printk_fifo, K_FOREVER);
		printk("id 0 chamado\n");
		printk("Received data: %d id 0 \n", rx_data->palavra);
		//k_free(rx_data);
		}
}
void writer0 (void){
	while(1){
	uart_out();
	k_msleep(1000);
	}
}
/*void writer1 (void){
	while(1){
	uart_out(1);
	k_msleep(1000);
	}
}*/
void reader0 (void){
	while(1){
	uart_in(0);
	k_msleep(1000);
	}
	
}
/*void reader1 (void){
	while(1){
	uart_in(1);
	k_msleep(1000);
	}
}*/



K_THREAD_DEFINE(uart_in_0, STACKSIZE, reader0, NULL, NULL, NULL,
		4, 0, 0);
//K_THREAD_DEFINE(uart_in_1, STACKSIZE, reader1, NULL, NULL, NULL,
		//3, 0, 0);
//K_THREAD_DEFINE(uart_out_0, STACKSIZE, writer0, NULL, NULL, NULL,
//		1, 0, 0);
//K_THREAD_DEFINE(uart_out_1, STACKSIZE, writer1, NULL, NULL, NULL,
		//2, 0, 0);


