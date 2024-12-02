
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/__assert.h>
#include <string.h>
#include <console/console.h>
#include <stdio.h>


/* size of stack area used by each thread */
#define STACKSIZE 1024

/* scheduling priority used by each thread */
#define PRIORITY 7

K_FIFO_DEFINE(printk_fifo);

    struct data_item_t  {
	void *fifo_reserved; /* 1st word reserved for use by fifo */
	char U = U;
    char sync = 0b00010110;
    char stx = 0b00000010;
    char id/nx = 0b00001000;
    char v[7]; /*o número de dados d1, d2, d3... */
    char etx = 0b00000011;
	};

struct data_item_t tx_data;

void uart_out(int id)
{
    int i;
    int n;
    char aux; 

    printk("Dgite o número de caracter's");
    scanf("%d", &n);
    tx_data.id/nx = 0b00001000 | n;
    printk("digite %d caracteres", n);
    for(i=0; i++; i<7){
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
		printk("Received data: %d id 0 \n", rx_data->valor);
		//k_free(rx_data);
		}
}
void writer0 (void){
	while(1){
	uart_out(0);
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
K_THREAD_DEFINE(uart_out_0, STACKSIZE, writer0, NULL, NULL, NULL,
		1, 0, 0);
//K_THREAD_DEFINE(uart_out_1, STACKSIZE, writer1, NULL, NULL, NULL,
		//2, 0, 0);
