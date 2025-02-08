//zephyrproject\.venv\Scripts\activate.bat
//cd %HOMEPATH%\zephyrproject\zephyr           west build -p always -b frdm_kl25z samples\basic\serialcerto

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/gpio.h>
#include <string.h>

//associa uma variavel com a componente uat na device tree
#define UART_DEVICE_NODE DT_CHOSEN(zephyr_shell_uart)

#define MSG_SIZE 13 //define o tamnho do vetor de mensagens

/* size of stack area used by each thread */
#define STACKSIZE 1024

/* scheduling priority used by each thread */
#define PRIORITY 1
#define periodo 10

K_MUTEX_DEFINE	(mut1); //define e inicializa o mutex 1

struct texto {
	uint8_t id;
	uint8_t dado1;
	uint8_t dado2;
	uint8_t dado3;
	uint8_t dado4;
	uint8_t dado5;
	uint8_t dado6;
	uint8_t dado7;
};

K_CONDVAR_DEFINE(condvar1);






/* queue to store up to 10 messages (aligned to 4-byte boundary) */
K_MSGQ_DEFINE(uart_msgq, MSG_SIZE, 10, 4);

K_MSGQ_DEFINE(msg_fifo, 8, 100, 4); // salva as posições de memoria em que as mensagens estao escritas

static const struct device *const uart_dev = DEVICE_DT_GET(UART_DEVICE_NODE); //coloca as informações da uart 
//no ponteiro uart_dev

/* receive buffer used in UART ISR callback */
static uint8_t rx_buf[MSG_SIZE] = {0b01010101,0b00010110,0b00000010,0b00001000}; //vetor rx buf
// 					   				  palavra     sync        stx        id
static int rx_buf_pos = 4; //contador de elemento do vetor

/*
 * Read characters from UART until line end is detected. Afterwards push the
 * data to the message queue.
 */
 uint8_t mensagem = 0;
 int cont0 = 0;

void serial_cb(const struct device *dev, void *user_data) //callback da serial
{// aceita como parametros o ponteiro dev, que contem as informaçoes da device tree sobre a uart
// e o ponteiro user data
	uint8_t c;
	

	/* read until FIFO empty */
	
	while (uart_fifo_read(uart_dev, &c, 1) == 1) {//enquanto existir informações na uart
		
		if ((c == '\n' || c == '\r' ) && rx_buf_pos > 0) {//se detectar enter ou mudança de linha
			/* terminate string */
			rx_buf[rx_buf_pos] = 0b00000011; // acrescenta o ETX
			rx_buf_pos++;
			rx_buf[rx_buf_pos] = '\0'; //volta o vetor do buf para 0
			rx_buf[3] = rx_buf[3] | cont0;
			/* if queue is full, message is silently dropped */
			
			k_msgq_put(&uart_msgq, &rx_buf, K_NO_WAIT);//sobe o vetor numa fila de mensagens
			rx_buf_pos--;
			rx_buf[rx_buf_pos] = 0; 
			struct texto mensagem_enviada  = {
			.id = rx_buf[3],
			.dado1 = rx_buf [4],
			.dado2 = rx_buf [5],
			.dado3 = rx_buf [6],
			.dado4 = rx_buf [7],
			.dado5 = rx_buf [8],
			.dado6 = rx_buf [9],
			.dado7 = rx_buf [10],
			};
			k_msgq_put(&msg_fifo, &mensagem_enviada, K_NO_WAIT);
			
			for (int r = 4; r<11;r++){
			rx_buf[r] = 0;
			}
			rx_buf[3] = 0b00001000;
			
			/* reset the buffer (it was copied to the msgq) */
			rx_buf_pos = 4;
			//printk("Carregando dados 1\n");
			mensagem++;
			cont0 = 0;
			
		} else if ((cont0 == 7) && (rx_buf_pos > 0)){
			rx_buf[rx_buf_pos] = 0b00000011; // acrescenta o ETX
			rx_buf_pos++;
			rx_buf[rx_buf_pos] = '\0'; //encerra o vetor
			rx_buf[3] = rx_buf[3] | 7;
			k_msgq_put(&uart_msgq, &rx_buf, K_NO_WAIT);//sobe o vetor numa fila de mensagens
			/* reset the buffer (it was copied to the msgq) */
			
			
			struct texto mensagem_enviada  = {
			.id = rx_buf[3],
			.dado1 = rx_buf [4],
			.dado2 = rx_buf [5],
			.dado3 = rx_buf [6],
			.dado4 = rx_buf [7],
			.dado5 = rx_buf [8],
			.dado6 = rx_buf [9],
			.dado7 = rx_buf [10],
			};
			rx_buf_pos = 4;
			//printk("Carregando dados 2\n");
			rx_buf[rx_buf_pos] = c;
			printk ("%c",rx_buf[rx_buf_pos]);
			rx_buf[3] = 0b00001000;
			k_msgq_put(&msg_fifo, &mensagem_enviada, K_NO_WAIT);
			mensagem++;
			cont0 = 1;
			rx_buf_pos++;
		}else if (rx_buf_pos < (sizeof(rx_buf) - 1)) {
				rx_buf[rx_buf_pos] = c;
				printk ("%c",rx_buf[rx_buf_pos]);
				rx_buf_pos++;
				cont0++;
		}
		
		c = 0;
	}
	

	

		//printk ("alberto  = %d",mensagem_enviada.dado1);
		/* else: characters beyond buffer size are dropped */

}

void leitor (){ // Essa função inicia o calback pa a função de leitura do teclado
	

	/* configure interrupt and callback to receive data */
	uart_irq_callback_user_data_set(uart_dev, serial_cb, NULL);// chama a função serial_cb quando o teclado é utilizado

	uart_irq_rx_enable(uart_dev); // inicia a uart 
	
}
//declaração de criação e inicialização de thread com leitor como função a ser percorrida
K_THREAD_DEFINE(leitura, STACKSIZE,  leitor, NULL, NULL, NULL, PRIORITY, 0, 0);

//INICIO DA TRANSMISSÃO

const struct device *stx = DEVICE_DT_GET(DT_NODELABEL(gpiob)); //coleta as informações da device tree sobre 
//a port B do gpio e salva no ponteiro stx

uint8_t i = 0;//contador de transmissão
uint8_t j = 0; //variavel de contagem de elemntos dentro do vetor
 // variavel auxiliar que faz a contagem de dados enviados por capsula
uint8_t cont1 = 0; // variavel auxiliar para a chamada do etx
uint8_t  d = 0;// variável em que colocamos o id e o número de dados
// vetor auxiliar utilizado para chegar ao valor de dados
int c = 0;// sincronizador da recepção
char tx_buf[MSG_SIZE];//vetor em que colocamos os valores na fila

uint8_t etx = 0b00000011;
bool transmitindo = 0;//mostra se está transmitindo
bool aux;//suporte de valor
uint8_t permissao = 0;// verificador do progresso da leitura
bool recebendo;// variável de estado que mostra se estamos recebendo algo
bool enviado;

void transmissao (struct k_timer *tempo){ //transmissão por bit bang
	if (transmitindo == 1){
		//printk("a");
		aux = (tx_buf [cont1] << i) & 0b10000000;
		gpio_pin_set(stx, 3, aux);
		//printk("i %d\n",i);
		i++;

		//printk("%d",aux);
		if (i == 8){
			i = 0;
			//printk(" aux = %d \n",tx_buf [cont1]);
			printk("cont1 %d\n",cont1);
			cont1++;
		}
	}else{
		gpio_pin_set(stx, 3, 0);
	}		
}
	

K_TIMER_DEFINE(tempo1, transmissao, NULL); //define e inicializa meu timer com o respectivo nome e função a ser chamada

void trans (){
	gpio_pin_configure(stx, 3, GPIO_OUTPUT); 
	k_timer_start(&tempo1, K_MSEC(periodo), K_MSEC(periodo)); // associa um tempo de expiração do meu timer 
	while (1){
		
		
		//printk(",");
		//printk("mensagem = %d", mensagem);
		if (mensagem > 0){
			if (k_mutex_lock(&mut1, K_FOREVER) == 0){
				printk(" trans peguei mutex\n");
			}else {
				printk(" trans nao peguei mutex\n");
			}
			
			//printk("b\n");
			if(transmitindo ==  0 && recebendo == 0){
				k_msgq_get(&uart_msgq, tx_buf, K_NO_WAIT);//faz pop na fila e coloca os valores no tx_buf
				while (tx_buf[j] != '\0'){
					//printk("tx_buf = %c\n",tx_buf[j]);
					j++;
				}	
				cont1 = 0;
				
				printk("\nj = %d\n",j);
				transmitindo = 1;
				
			}
			if (cont1 == j ){
				printk("parei na condvar\n");
				k_condvar_wait(&condvar1, &mut1, K_FOREVER);
				
				printk("voltei da condvar\n");
				if (enviado == 1){
					printk("c");
					transmitindo = 0;
					mensagem--;
					j = 0;
					
				}else{
					cont1 = 0;
				}
				k_mutex_unlock (&mut1);
				//printk("terminei uma mensagem\n");
			}
			k_msleep((80));
			
		}
		k_msleep((10));
	}
}
K_THREAD_DEFINE(transmis, STACKSIZE, trans , NULL, NULL, NULL,
		2, 0, 0);

//INICIO DA RECEPÇÂO

uint32_t reserva;
bool g;
uint8_t cont2;
int cont3 = 0;
int cont4 = 0;
 uint32_t recept_buf[100];

K_MUTEX_DEFINE (mut2);


void recepcao (){				
	//k_mutex_lock(&mut2, K_FOREVER);
	g = gpio_pin_get(stx, 2);
	reserva = ((reserva << 1) | g);//guardo os valores da recepcao e faço um shift
		//printk ("%d",g);
		
		//printk ("%d\n",reserva);
		if (recebendo == 1){
			if (cont2 == 31){
				recept_buf [cont3] = reserva;
				//printk("%d \n",recept_buf [cont3] );
				cont3++;
				//printk("cont3 = %d cont4 = %d\n",cont3,cont4);
				cont2 = 0;
				//printk(" %d ",reserva);
			}else{
				cont2++;
			}
		}else{
			if (reserva == -1 || reserva == 0){
				//printk("vazio  ");
			}else if (((reserva & 0b01000100010001000100010001000100) == ((reserva & 0b00100010001000100010001000100010) << 1 ))){
				recept_buf [cont3] = reserva;
				cont3++;			
			}
		}
}

K_TIMER_DEFINE(tempo2, recepcao, NULL); //define e inicializa meu timer com o respectivo nome e função a ser chamada

void mainrecepcao(){
	gpio_pin_configure(stx, 2, GPIO_INPUT);
	k_timer_start(&tempo2, K_USEC(2500), K_USEC(2500));
}

K_THREAD_DEFINE(recept, STACKSIZE, mainrecepcao , NULL, NULL, NULL,
		3, 0, 0);


//INICIO DA INTERPRETAÇÃO


int aux2;//registra o número de elementos do vetor
char int_buf[MSG_SIZE];
struct texto mensagem_comparacao;
int cont5 = 0;
uint8_t preguica = 0;
uint8_t ndados;
void maintraducao (){
	while(1){
		
		printk (".");
		//printk("teste\n");
		//printk("cont3 = %d cont4 = %d\n",cont3,cont4);
		if (cont3 > cont4 ){
			if (permissao == 0){
				if(recept_buf [cont4] == 0b000000000000011110000111111110000){
					permissao = 1;
					recebendo = 1;
					printk("sync\n");
					if (k_mutex_lock(&mut1, K_FOREVER) == 0){
						printk(" maint peguei mutex\n");
					}else {
						printk(" maint nao peguei mutex\n");
					}
				}
			}else{
				if(permissao == 1 && recept_buf [cont4] == 0b00000000000000000000000011110000){
				permissao = 2;
				printk("stx\n");
				}else if (permissao > 1){
					//printk("%d \n",recept_buf [cont4] );
					for (int s = 0; s < 8; s++){
						preguica = (preguica | ((recept_buf [cont4] & (1 << (s * 4))) >> (s * 3)));
						
					}
					//printk("preguica = %d  \n",preguica);
					if (permissao == 2){
						ndados = preguica & 7;
						printk("id = %d \n",preguica);
						printk("ndados = %d \n",ndados);
						int_buf [7] = preguica;
						preguica = 0;
						permissao = 3;	

					}else if (permissao == 3){
						printk("dado%d = %c \n",cont5,preguica);
						int_buf [cont5] = preguica; 
						//printk ("cont4 = %d \n",cont4);
						cont5++;
						preguica = 0;
						if (cont5 == ndados){
							permissao = 4;
							//cont3++;
						}
					}else if (permissao == 4 && recept_buf [cont4] == 0b0000000000000000000000011111111 ){
						printk("etx\n");
						permissao = 0;
						cont3 = 0;
						cont4 = 0;
						cont5 = 0;
						recebendo = 0;
						preguica = 0;
						struct texto mensagem_recebida  = {
						.id = int_buf[7],
						.dado1 = int_buf [0],
						.dado2 = int_buf [1],
						.dado3 = int_buf [2],
						.dado4 = int_buf [3],
						.dado5 = int_buf [4],
						.dado6 = int_buf [5],
						.dado7 = int_buf [6],
						};
						for (int y = 0; y < 8; y++){
							int_buf [y] = 0;
						}
						//printk("MEu id  = %d",mensagem_recebida.id & 11111000);
						if ((mensagem_recebida.id & 11111000) == 0b00001000){
							//printk("meu dado\n");
							k_msgq_get(&msg_fifo, &mensagem_comparacao, K_NO_WAIT);
							
							// printk("teste  = %d\n",mensagem_comparacao.id);
							// printk("teste  = %d\n",mensagem_comparacao.dado1);
							// printk("teste  = %d\n",mensagem_comparacao.dado2);
							// printk("teste  = %d\n",mensagem_comparacao.dado3);
							// printk("teste  = %d\n",mensagem_comparacao.dado4);
							// printk("teste  = %d\n",mensagem_comparacao.dado5);
							// printk("teste  = %d\n",mensagem_comparacao.dado6);
							// printk("teste  = %d\n",mensagem_comparacao.dado7);



							if (memcmp(&mensagem_comparacao, &mensagem_recebida, sizeof(struct texto)) == 0) {
      				 		 printf("As structs são iguais.\n");
							enviado = 1;
							printf("Sinalizei\n");
							k_condvar_signal(&condvar1);
    						
							
  							} else {
    						printf("As structs são diferentes.\n");
							enviado = 0;
    						}
						} else{
							recebendo = 0;
						} 
						k_mutex_unlock(&mut1);
					}else{
						printk("erro\n");
						permissao = 0;
						cont3 = 0;
						cont4 = 0;
						cont5 = 0;
						recebendo = 0;
						preguica = 0;
						
						for (int y = 0; y < 8; y++){
							int_buf [y] = 0;
						}
						k_mutex_unlock(&mut1);
					}
				}
				k_usleep(2500);
			}
			cont4++;
		}
		
		k_usleep(500);
	}
		
}
K_THREAD_DEFINE(trad, STACKSIZE, maintraducao , NULL, NULL, NULL,
		3, 0, 0);

// struct printk_data_t {
// 	void *fifo_reserved; /* 1st word reserved for use by fifo */
// 	uint32_t led;
// 	uint32_t cnt;
// };
