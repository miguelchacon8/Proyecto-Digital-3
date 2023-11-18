/* adc_SPI_wiringPi.c
 Basado en código tomado de: https://projects.drogon.net/raspberry-pi/wiringpi/
 Adaptado y comentado por: Luis Alberto Rivera
 
 Programa para comunicar la Raspberry Pi con el integrado MCP3002, que realiza
 conversiones A/D. La comunicación se hace vía SPI.
 
 Nota 1: La Raspberry Pi tiene dos canales SPI.
 Nota 2: El chip MCP3002 tiene 2 canales ADC.
 Cuidado con los voltajes de entrada para el MCP3002. Deben estar entre 0 y VDD.
 Se sugiere usar VDD = 3.3 V de la Raspberry Pi.
 Recuerde conectar VSS del MCP3002 a la tierra de la RPi.
 
 Recuerde compilar usando -lwiringPi
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>			// Para los tipos enteros como uint8_t y uint16_t
#include <wiringPi.h>
#include <wiringPiSPI.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <semaphore.h>

#define SPI_CHANNEL	      0	// Canal SPI de la Raspberry Pi, 0 ó 1
#define SPI_SPEED 	115200	// Velocidad de la comunicación SPI (reloj, en HZ)
                            // Máxima de 3.6 MHz con VDD = 5V, 1.2 MHz con VDD = 2.7V
#define ADC_CHANNEL       0	// Canal A/D del MCP3002 a usar, 0 ó 1
#define BUTTON_PIN1 21
#define BUTTON_PIN2 20
#define DIP_PIN1 24
#define DIP_PIN2 23
#define ALARM 22
#define LED1IN 17
#define LED2IN 27
#define LED1 26
#define LED2 19
#define DEBOUNCE_DELAY 500

#define MSG_SIZE 50 	// Tamaño (máximo) del mensaje. 

uint16_t get_ADC(int channel);	// prototipo
void Bocina(void *ptr);
void buttonPressed(void);
void Actualizacion(void *ptr);
void receive(void *ptr);
void error(const char *);
void ADC(void *ptr);
void men(void);
void LEDinterrupt(void);

int UTR = 1;
int flag_ADC = 0;
int flag_boton1 = 0;
int flag_boton2 = 0;
int flag_dip1 = 0;
int flag_dip2 = 0;
int flag_led1 = 0;
int flag_led2 = 0;
int num_evento = 0;
float voltaje = 0;
char hora[13];
uint16_t ADCvalue;
FILE *datos;
char lectura[4];
int i;
char mensaje[60];

int sockfd, n;
unsigned int length;
struct sockaddr_in server, from, addr_broadcast;
struct hostent *hp;
char buffer[MSG_SIZE];
int channel = 0, speed = 115200; 
unsigned char d1;
char tok[2][1];

sem_t semaforo;

void ADC(void *ptr){
    while(1){
        ADCvalue = get_ADC(ADC_CHANNEL);
        sprintf(lectura, "%d\n", ADCvalue);
        fputs(lectura, datos);
        usleep(3000);
	    sem_wait(&semaforo);
        voltaje = ADCvalue*(3.3/1024);
        if(ADCvalue >= 775 || ADCvalue <= 155){
            if(flag_ADC != 1){
                flag_ADC = 1;
                num_evento = 3;
                men();
                printf("%s -> cambio el ADC\n", mensaje);
                n = sendto(sockfd, mensaje, strlen(mensaje), 0, (const struct sockaddr *)&server, length);
	            if(n < 0)
		            error("Sendto");
                fflush(stdout);
            }
        }
        else {
            if(flag_ADC != 0){
                flag_ADC = 0;
                num_evento = 3;
                men();
                printf("%s -> cambio el ADC\n", mensaje);
                n = sendto(sockfd, mensaje, strlen(mensaje), 0, (const struct sockaddr *)&server, length);
	            if(n < 0)
		            error("Sendto");
                fflush(stdout);
            }
        }
	sem_post(&semaforo);
    }
}

int main(int argc, char *argv[])
{  
    int boolval = 1;

    sem_init(&semaforo, 0, 1);
    if(argc != 3)
	{
		printf("usage %s hostname port\n", argv[0]);
		exit(1);
	}

	sockfd = socket(AF_INET, SOCK_DGRAM, 0); // Creates socket. Connectionless.
	if(sockfd < 0)
		error("socket");

	server.sin_family = AF_INET;	// symbol constant for Internet domain
	hp = gethostbyname(argv[1]);	// converts hostname input (e.g. 192.168.1.101)
	if(hp == 0)
		error("Unknown host");
    server.sin_addr.s_addr = htonl(INADDR_ANY);
	//memcpy((char *)&server.sin_addr, (char *)hp->h_addr, hp->h_length);
    if(setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &boolval, sizeof(boolval))<0)
        error("error setting");
     
	server.sin_port = htons(atoi(argv[2]));	// port field
    addr_broadcast.sin_family = AF_INET;	// symbol constant for Internet domain   
    addr_broadcast.sin_port = htons(atoi(argv[2]));	// port field
    addr_broadcast.sin_addr.s_addr = inet_addr("10.0.0.255");
    
	length = sizeof(struct sockaddr_in);	// size of structure
    if(bind(sockfd, (struct sockaddr *)&server, length)<0)
        error("Error binding socket");
    datos = fopen("datos.txt", "w");
	// Configura el SPI en la RPi
    
    wiringPiSetupGpio();
    pinMode(ALARM, OUTPUT);
    pinMode(BUTTON_PIN1, INPUT); // Set button pin as input
    pinMode(BUTTON_PIN2, INPUT); // Set button pin as input
    pinMode(DIP_PIN1, INPUT); // Set button pin as input
    pinMode(DIP_PIN2, INPUT); // Set button pin as input
    wiringPiISR(BUTTON_PIN1, INT_EDGE_RISING, &buttonPressed); // Set up interrupt for button press
    wiringPiISR(BUTTON_PIN2, INT_EDGE_RISING, &buttonPressed);
    wiringPiISR(DIP_PIN1, INT_EDGE_BOTH, &buttonPressed);
    wiringPiISR(DIP_PIN2, INT_EDGE_BOTH, &buttonPressed);
    pinMode(LED1, OUTPUT);
    pinMode(LED2, OUTPUT);
    pinMode(LED1IN, INPUT);
    pinMode(LED2IN, INPUT);
    wiringPiISR(LED1IN, INT_EDGE_BOTH, &LEDinterrupt);
    wiringPiISR(LED2IN, INT_EDGE_BOTH, &LEDinterrupt);
    // Obtener la hora
    struct timeval tv;
    gettimeofday(&tv, NULL);

    time_t current_time = tv.tv_sec;
    struct tm *tm_info = localtime(&current_time);

    int hour = tm_info->tm_hour;        // Hours (0-23)
    int minute = tm_info->tm_min;       // Minutes (0-59)
    int second = tm_info->tm_sec;       // Seconds (0-59)

    // Extract the fractional part of the second from the timeval structure
    int fraction = tv.tv_usec / 1000; // Convert microseconds to milliseconds
    sprintf(hora, "%02d:%02d:%02d.%03d", hour, minute, second, fraction);
    fflush(stdout);
    
    //Creación de un hilo para la bocina
    pthread_t thread2, thread3, thread4, threadrecv;
    pthread_create(&thread2, NULL, (void*)&Bocina, NULL);
    pthread_create(&thread3, NULL, (void*)&Actualizacion, NULL);
    pthread_create(&thread4, NULL, (void*)&ADC, NULL);
    pthread_create(&threadrecv, NULL, (void*)&receive, NULL);
    
    n = sendto(sockfd, "# 10.0.0.6", strlen(mensaje), 0, (const struct sockaddr *)&addr_broadcast, length);
	while(1){
        gettimeofday(&tv, NULL);

        time_t current_time = tv.tv_sec;
        struct tm *tm_info = localtime(&current_time);

        int hour = tm_info->tm_hour;        // Hours (0-23)
        int minute = tm_info->tm_min;       // Minutes (0-59)
        int second = tm_info->tm_sec;       // Seconds (0-59)

        // Extract the fractional part of the second from the timeval structure
        int fraction = tv.tv_usec / 1000; // Convert microseconds to milliseconds
        sprintf(hora, "%02d:%02d:%02d.%03d", hour, minute, second, fraction);
        fflush(stdout);
	}
    //fclose(datos);
  return 0;   
}

// Como se describe en las secciones 5 y 6 del manual del MCP3002, necesitamos
// enviar dos bytes para iniciar la conversión. El primer byte incluye un "start bit",
// y bits que indican el modo y el canal a usar. El segundo byte no importa (pero se
// debe enviar). Al ser enviados los dos bytes de la RPi al MCP3002, dos bytes se
// regresan, los cuales contienen el valor convertido. Pueden leer sobre comunivoid buttonPressed(void);cación
// SPI para más detalles.
// La comunicación podría hacerse "a mano". Habría que mapear registros, configurar
// los puertos GPIO adecuados, enviar datos usando funciones como ioctl(), etc. Sin
// embargo, la utilidad SPI de wiringPi nos facilita el trabajo.
// Pueden implementar la comunicación "a mano" si se sienten aventureros...

// Entrada: ADC_chan -- 0 ó 1
// Salida: un entero "unsigned" de 16 bit  con el valor de la conversión. Dado que la
//         resolución del ADC es de 10 bits, el valor retornado estará entre 0 y 1023.
// Asume modo "Single Ended" (no "Pseudo-Differential Mode").
uint16_t get_ADC(int ADC_chan)
{
	uint8_t spiData[2];	// La comunicación usa dos bytes
	uint16_t resultado;
	
	// Asegurarse que el canal sea válido. Si lo que viene no es válido, usar canal 0.
	if((ADC_chan < 0) || (ADC_chan > 1))
		ADC_chan = 0;

	// Construimos el byte de configuración: 0, start bit, modo, canal, MSBF: 01MC1000
	spiData[0] = 0b01101000 | (ADC_chan << 4);  // M = 1 ==> "single ended"
												// C: canal: 0 ó 1
	spiData[1] = 0;	// "Don't care", este valor no importa.
	
	// La siguiente función realiza la transacción de escritura/lectura sobre el bus SPI
	// seleccionado. Los datos que estaban en el buffer spiData se sobreescriben por
	// los datos que vienen por SPI.
	wiringPiSPIDataRW(SPI_CHANNEL, spiData, 2);	// 2 bytes
	
	// spiData[0] y spiData[1] tienen el resultado (2 bits y 8 bits, respectivamente)
	resultado = (spiData[0] << 8) | spiData[1];
	
	return(resultado);
}

void receive(void *ptr){
    int n1;
    char buffer1[MSG_SIZE];
    while(1){
        memset(buffer1, 0, MSG_SIZE);
        n1 = recvfrom(sockfd, buffer1, MSG_SIZE, 0, (struct sockaddr *)&from, &length);
        //printf("%s\n", buffer1);
	    if(n < 0)
		    error("Sendto");
        if(strcmp(buffer1, "10.0.0.6 LED1") == 0){
            if(flag_led1 == 0){
                digitalWrite(LED1, HIGH);
                flag_led1 = 1;
            }else if(flag_led1 == 1){
                digitalWrite(LED1, LOW);
                flag_led1 = 0;                
            }
		}else if (strcmp(buffer1, "10.0.0.6 LED2") == 0){
            if(flag_led2 == 0){
                digitalWrite(LED2, HIGH);
                flag_led2 = 1;
            }else if(flag_led2 == 1){
                digitalWrite(LED2, LOW);
                flag_led2 = 0;                
            }
        }
    }
}

void LEDinterrupt(void){
        if(digitalRead(LED1IN) == 1){
            digitalWrite(LED1, HIGH);
            flag_led1 = 1;
        }else if(digitalRead(LED1IN) == 0){
            digitalWrite(LED1, LOW);
            flag_led1 = 0;
        } if(digitalRead(LED2IN) == 1){
            digitalWrite(LED2, HIGH);
            flag_led2 = 1;
        }else if(digitalRead(LED2IN) == 0){
            digitalWrite(LED2, LOW);
            flag_led2 = 0;
        }
        sem_wait(&semaforo);
        num_evento = 5;
        men();
        printf("%s\n", mensaje);
        n = sendto(sockfd, mensaje, strlen(mensaje), 0, (const struct sockaddr *)&server, length);
        sem_post(&semaforo);        
}

void buttonPressed(void) {
    static unsigned long lastDebounceTime1 = 0;
    static unsigned long lastDebounceTime2 = 0;
    unsigned long currentTime = millis();

    if (digitalRead(BUTTON_PIN1) == 1 && currentTime - lastDebounceTime1 > DEBOUNCE_DELAY){
        flag_boton1 = 1;
        lastDebounceTime1 = currentTime;
    }
    else if (digitalRead(BUTTON_PIN2) == 1 && currentTime - lastDebounceTime2 > DEBOUNCE_DELAY){
        flag_boton2 = 1;
        lastDebounceTime2 = currentTime;
    }
    else if (digitalRead(DIP_PIN1) == 1 && currentTime - lastDebounceTime2 > DEBOUNCE_DELAY){
        flag_dip1 = 1;
        lastDebounceTime2 = currentTime;
    }
    else if (digitalRead(DIP_PIN2) == 1 && currentTime - lastDebounceTime2 > DEBOUNCE_DELAY){
        flag_dip2 = 1;
        lastDebounceTime2 = currentTime;
    }
    if(flag_boton1 == 1 || flag_boton2 == 1 || flag_dip1== 1 || flag_dip2== 1){
        fflush(stdout);
        sem_wait(&semaforo);
        num_evento = 2;
        men();
        printf("%s\n", mensaje);
        n = sendto(sockfd, mensaje, strlen(mensaje), 0, (const struct sockaddr *)&server, length);
        sem_post(&semaforo);
        if(n < 0)
            error("Sendto");
        fflush(stdout);
        delay(700); // Wait for 700ms to debounce
        flag_boton1 = 0;
        flag_boton2 = 0;
    }
}

// Hilo para el control de la bocina
void Bocina(void *ptr){
    // Mientras el programa principal se ejecuta, el hilo también
    while(1){
        while(flag_ADC != 0){         //Si el valor ya no es r sale del loop y no hace nada el hilo
            digitalWrite(ALARM, HIGH);  //Generar un pulso con frecuencia de aproximadamente 440Hz -> T = 2.273ms
            usleep(2273);
            digitalWrite(ALARM, LOW);
            usleep(2273);
        }
        digitalWrite(ALARM, LOW);
    }
    // Si sale del loop es porque el programa terminó
    pthread_exit(0);
}

void Actualizacion(void *ptr){
    while(1){
        sem_wait(&semaforo);
        num_evento = 6;
        men();
        printf("%s\n", mensaje);
        n = sendto(sockfd, mensaje, strlen(mensaje), 0, (const struct sockaddr *)&server, length);
	    sem_post(&semaforo);
        if(n < 0)
		    error("Sendto");
        fflush(stdout);
        sleep(2);
    }
}

void men(void){
    sprintf(mensaje, "%d %d %s %d %d %d %d %d %d %.2f", UTR, num_evento, hora, flag_boton1, flag_boton2, flag_dip1, flag_dip2, flag_led1, flag_led2, voltaje);
}

void error(const char *msg)
{
	perror(msg);
	exit(0);
}
