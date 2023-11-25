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

#ifdef _WIN32 // Si se está compilando en Windows
    // Definir variables específicas para Windows
    #define ADAPTADOR_RED "{8B93B931-7325-41A7-A313-D29DEEE73F83}"
#else // Si se está compilando en otro sistema (por ejemplo, Raspberry Pi)
    // Definir variables específicas para Raspberry Pi
	#define ADAPTADOR_RED "wlan0"
#endif

//#define ADAPTADOR_RED "{8B93B931-7325-41A7-A313-D29DEEE73F83}"
#define SPI_CHANNEL 0	// Canal SPI de la Raspberry Pi, 0 ó 1
#define SPI_SPEED 	1500000	// Velocidad de la comunicación SPI (reloj, en HZ)
                            // Máxima de 3.6 MHz con VDD = 5V, 1.2 MHz con VDD = 2.7V
#define ADC_CHANNEL       0	// Canal A/D del MCP3002 a usar, 0 ó 1
#define BUTTON_PIN1 21      // Pin del botón 1
#define BUTTON_PIN2 20      // Pin del botón 2
#define DIP_PIN1 24         // Pin del dip switch 1
#define DIP_PIN2 25         // Pin del dip switch 2
#define ALARM 22            // Pin de la bocina
#define LED1IN 17           // Pin dispositivo IoT para la led 1
#define LED2IN 27           // Pin dispositivo IoT para la led 2
#define LED1 26             // Pin de la led 1
#define LED2 19             // Pin de la led 2
#define DEBOUNCE_DELAY 500  // Tiempo de debounce para los botones

#define MSG_SIZE 50 	// Tamaño (máximo) del mensaje. 

uint16_t get_ADC(int channel);	// prototipos de funciones
void Bocina(void *ptr);
void buttonPressed(void);
void Dip1(void);
void Dip2(void);
void Actualizacion(void *ptr);
void receive(void *ptr);
void error(const char *);
void ADC(void *ptr);
void men(void);
void LEDinterrupt(void);
void Obtener_ip(void);


int UTR = 1;            // Identificador único del RTU
int flag_ADC = 0;       // Bandera para el ADC
int flag_boton1 = 0;    // Bandera para el botón 1
int flag_boton2 = 0;    // Bandera para el botón 2
int flag_dip1 = 0;      // Bandera para el dip switch 1
int flag_dip2 = 0;      // Bandera para el dip switch 2
int flag_led1 = 0;      // Bandera para la led 1    
int flag_led2 = 0;      // Bandera para la led 2
int num_evento = 0;     // Número de evento
float voltaje = 0;      // Valor del voltaje
char hora[13];          // Hora del sistema
uint16_t ADCvalue = 0;  // Valor del ADC
FILE *datos;            // Archivo para guardar los datos
char lectura[4];        // Lectura del ADC
int i;                  // Variable para el for
char mensaje[60];       // Mensaje a enviar
unsigned int length;    // Tamaño del mensaje
int client_socket, n;   // Socket del cliente
struct sockaddr_in server_addr; // Dirección del servidor
struct hostent *hp;     // Host
char buffer[MSG_SIZE];  // Buffer para el mensaje
int channel = 0, speed = 115200;  // Canal y velocidad de la comunicación serial
unsigned char d1;       // Dato 1
char tok[2][1];         // Token para separar el mensaje
char ip[INET_ADDRSTRLEN];   // Dirección IP

sem_t semaforo;         // Semaforo para la sección crítica

int main(int argc, char *argv[])    // argc: ip del historiador; argv: port
{  
    sem_init(&semaforo, 0, 1);      // Inicializar el semaforo
    if(argc != 3)                   // Verificar que se hayan ingresado los argumentos
	{
		printf("usage %s hostname port\n", argv[0]);    // Si no se ingresaron los argumentos, imprimir el uso correcto
		exit(1);    
	}

	if ((client_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {  // Crear el socket
        perror("Error al crear el socket");             // Si hay error, imprimirlo
        exit(EXIT_FAILURE);
    }

	server_addr.sin_family = AF_INET;	// symbol constant for Internet domain
	server_addr.sin_port = htons(atoi(argv[2]));	// port field
	if (inet_pton(AF_INET, argv[1], &server_addr.sin_addr) <= 0) {  // Convertir la dirección IP
        perror("Dirección no válida/soportada");
        exit(EXIT_FAILURE);
    }
	// Conectar al servidor
    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {   // Conectar al servidor
        perror("Conexión fallida");
        exit(EXIT_FAILURE);
    }
    
	//-----------------------------------------------------------
    
    datos = fopen("datos.txt", "w");    // Abrir el archivo para guardar los datos
	// Configura el SPI en la RPi
    
    wiringPiSetupGpio();                // Inicializar wiringPi
    pinMode(ALARM, OUTPUT);             // Configurar el pin de la bocina como salida
    pinMode(BUTTON_PIN1, INPUT); // Configurar el pin del botón 1 como entrada
    pinMode(BUTTON_PIN2, INPUT); // Configurar el pin del botón 2 como entrada
    pinMode(DIP_PIN1, INPUT);    // Configurar el pin del dip switch 1 como entrada
    pinMode(DIP_PIN2, INPUT);    // Configurar el pin del dip switch 2 como entrada
    wiringPiISR(BUTTON_PIN1, INT_EDGE_RISING, &buttonPressed);  // Interrupción para el botón 1
    wiringPiISR(BUTTON_PIN2, INT_EDGE_RISING, &buttonPressed);  // Interrupción para el botón 2
    wiringPiISR(DIP_PIN1, INT_EDGE_BOTH, &Dip1);            // Interrupción para el dip switch 1
    wiringPiISR(DIP_PIN2, INT_EDGE_BOTH, &Dip2);            // Interrupción para el dip switch 2
    pinMode(LED1, OUTPUT);        // Configurar el pin de la led 1 como salida
    pinMode(LED2, OUTPUT);        // Configurar el pin de la led 2 como salida
    pinMode(LED1IN, INPUT);       // Configurar el pin del dispositivo IoT para la led 1 como entrada
    pinMode(LED2IN, INPUT);       // Configurar el pin del dispositivo IoT para la led 2 como entrada
    wiringPiISR(LED1IN, INT_EDGE_BOTH, &LEDinterrupt);  // Interrupción para el dispositivo IoT de la led 1
    wiringPiISR(LED2IN, INT_EDGE_BOTH, &LEDinterrupt);  // Interrupción para el dispositivo IoT de la led 2
    
    if(wiringPiSPISetup(SPI_CHANNEL, SPI_SPEED)<0){     // Configurar el SPI
        printf("WiringPiSPISetup falló \n");
        return(-1);
    }
    //Obtener la hora
    struct timeval tv;                  // Estructura para obtener la hora
    gettimeofday(&tv, NULL);            // Obtener la hora

    time_t current_time = tv.tv_sec;    // Obtener la hora actual
    struct tm *tm_info = localtime(&current_time);  // Obtener la hora local

    int hour = tm_info->tm_hour;        // Hours (0-23)
    int minute = tm_info->tm_min;       // Minutes (0-59)
    int second = tm_info->tm_sec;       // Seconds (0-59)

    // Extract the fractional part of the second from the timeval structure
    int fraction = tv.tv_usec / 1000; // Convert microseconds to milliseconds
    sprintf(hora, "%02d:%02d:%02d.%03d", hour, minute, second, fraction);   // Guardar la hora en la variable hora
    
    //Creación de un hilo para la bocina
    pthread_t thread2, thread3, thread4, threadrecv;                // Declaración de los hilos
    pthread_create(&thread2, NULL, (void*)&Bocina, NULL);           // Creación del hilo para la bocina
    pthread_create(&thread3, NULL, (void*)&Actualizacion, NULL);    // Creación del hilo para la actualización
    pthread_create(&thread4, NULL, (void*)&ADC, NULL);              // Creación del hilo para el ADC
    pthread_create(&threadrecv, NULL, (void*)&receive, NULL);       // Creación del hilo para recibir mensajes
    Obtener_ip();                                // Obtener la dirección IP
    char my_ip[18];                             // Variable para guardar la dirección IP
    sprintf(my_ip, "# %s", ip);            // Guardar la dirección IP en la variable my_ip
    n = send(client_socket, my_ip, strlen(my_ip), 0);   // Enviar la dirección IP al historiador
	while(1){
        gettimeofday(&tv, NULL);        // Obtener la hora

        time_t current_time = tv.tv_sec;    // Obtener la hora actual
        struct tm *tm_info = localtime(&current_time);  // Obtener la hora local

        int hour = tm_info->tm_hour;        // Hours (0-23)
        int minute = tm_info->tm_min;       // Minutes (0-59)
        int second = tm_info->tm_sec;       // Seconds (0-59)

        // Extract the fractional part of the second from the timeval structure
        int fraction = tv.tv_usec / 1000; // Convert microseconds to milliseconds
        sprintf(hora, "%02d:%02d:%02d.%03d", hour, minute, second, fraction);   // Guardar la hora en la variable hora
        fflush(stdout); // Limpiar el buffer de salida
	}
    close(client_socket);   // Cerrar el socket
    fclose(datos);        // Cerrar el archivo
  return 0;   
}

uint16_t get_ADC(int ADC_chan)  // Función para obtener el valor del ADC
{
	uint8_t spiData[2];	// La comunicación usa dos bytes
	uint16_t resultado; // 10 bits para el valor del ADC
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
	
	return(resultado);  // Devolver el resultado
}

void ADC(void *ptr){    // Hilo para el ADC
    while(1){       
        ADCvalue = get_ADC(ADC_CHANNEL);    // Obtener el valor del ADC
        sprintf(lectura, "%d\n", ADCvalue); // Guardar el valor del ADC en la variable lectura
       
        fputs(lectura, datos);            // Guardar el valor del ADC en el archivo
        usleep(1000);                  // Esperar 1ms
	    sem_wait(&semaforo);        // Entrar a la sección crítica
        voltaje = ADCvalue*(3.3/1024);  // Calcular el voltaje
        if(ADCvalue >= 775 || ADCvalue <= 155){ // Si el valor del ADC es mayor o igual a 775 o menor o igual a 155
            if(flag_ADC != 1){      // Si la bandera es diferente de 1
                flag_ADC = 1;       // Cambiar la bandera a 1
                num_evento = 3;     // Número de evento para ADC
                men();          // Crear el mensaje
                printf("%s -> cambio el ADC\n", mensaje);   // Imprimir el mensaje
                n = send(client_socket, mensaje, strlen(mensaje), 0);   // Enviar el mensaje al historiador
	            if(n < 0)   // Si hay error al enviar el mensaje
		            error("Sendto");    // Imprimir el error
                fflush(stdout); // Limpiar el buffer de salida
            }
        }
        else {
            if(flag_ADC != 0){    // Si la bandera es diferente de 0
                flag_ADC = 0;       // Cambiar la bandera a 0
                num_evento = 3;     // Número de evento para ADC
                men();              // Crear el mensaje
                printf("%s -> cambio el ADC\n", mensaje);       // Imprimir el mensaje
                n = send(client_socket, mensaje, strlen(mensaje), 0);   // Enviar el mensaje al historiador
	            if(n < 0)   // Si hay error al enviar el mensaje
		            error("Sendto");    // Imprimir el error
                fflush(stdout); // Limpiar el buffer de salida
            }
        }
	sem_post(&semaforo);    // Salir de la sección crítica
    }
}

void Actualizacion(void *ptr){  // Hilo para la actualización
    while(1){
        sem_wait(&semaforo);    // Entrar a la sección crítica
        num_evento = 6;         // Número de evento para la actualización
        men();                  // Crear el mensaje
        printf("%s\n", mensaje);    // Imprimir el mensaje
        n = send(client_socket, mensaje, strlen(mensaje), 0);   // Enviar el mensaje al historiador
	    sem_post(&semaforo);    // Salir de la sección crítica
        if(n < 0)   // Si hay error al enviar el mensaje
		    error("Sendto");    // Imprimir el error
        fflush(stdout); // Limpiar el buffer de salida
        sleep(2);   // Esperar 2 segundos
    }
}

void men(void){ // Función para crear el mensaje
    sprintf(mensaje, "%d %d %s %d %d %d %d %d %d %.2f", UTR, num_evento, hora, flag_boton1, flag_boton2, flag_dip1, flag_dip2, flag_led1, flag_led2, voltaje);  // Crear el mensaje con número de UTR, número de evento, todas las banderas y el voltaje
}
void receive(void *ptr){    // Hilo para recibir mensajes
    int n1;            // Variable para el número de bytes recibidos
    char buffer1[MSG_SIZE], led1[21], led2[21], *token, buffer_temp[MSG_SIZE];  // Buffer para el mensaje, buffer para el mensaje de las leds, token para separar el mensaje y buffer temporal para separar el mensaje
    while(1){
        sprintf(led1, "%s LED1", ip);   // Crear el mensaje para la led 1
        sprintf(led2, "%s LED2", ip);   // Crear el mensaje para la led 2
        memset(buffer1, 0, MSG_SIZE);   // Limpiar el buffer
        n1 = recv(client_socket, buffer1, MSG_SIZE, 0);  // Recibir el mensaje
        
        if (n1 <= 0) {  // Si el número de bytes recibidos es menor o igual a 0
            printf("Cliente desconectado\n");   // Imprimir que el cliente se desconectó
            break;  // Salir del loop
        }
        buffer1[n1] = '\0'; // Agregar el caracter nulo al final del mensaje
        //printf("%s\n", buffer1);
        
        sem_wait(&semaforo);    // Entrar a la sección crítica
        if(strncmp(buffer1, "RTU", 3) == 0){    // Si el mensaje es para el RTU
            strcpy(buffer_temp, buffer1);   // Copiar el mensaje en el buffer temporal
            token = strtok(buffer_temp, " ");   // Separar el mensaje
            token = strtok(NULL, ".");  // Separar el mensaje
            UTR = atoi(token);  // Guardar el número de UTR
        }
        
	    if(n < 0)   // Si hay error al enviar el mensaje
		    error("Sendto");    // Imprimir el error
        if(strcmp(buffer1, led1) == 0){ // Si el mensaje es para la led 1
            if(flag_led1 == 0){ // Si la bandera es 0
                digitalWrite(LED1, HIGH);   // Encender la led 1
                flag_led1 = 1;  // Cambiar la bandera a 1
            }else if(flag_led1 == 1){   // Si la bandera es 1
                digitalWrite(LED1, LOW);    // Apagar la led 1
                flag_led1 = 0;  // Cambiar la bandera a 0
            }
		}else if (strcmp(buffer1, led2) == 0){  // Si el mensaje es para la led 2
            if(flag_led2 == 0){ // Si la bandera es 0
                digitalWrite(LED2, HIGH);   // Encender la led 2
                flag_led2 = 1;  // Cambiar la bandera a 1
            }else if(flag_led2 == 1){   // Si la bandera es 1
                digitalWrite(LED2, LOW);    // Apagar la led 2
                flag_led2 = 0;  // Cambiar la bandera a 0
            }
        }
        num_evento = 4; // Número de evento para la led
        men();  // Crear el mensaje
        printf("%s\n", mensaje);    // Imprimir el mensaje
        n = send(client_socket, mensaje, strlen(mensaje), 0);   // Enviar el mensaje al historiador
        sem_post(&semaforo);    // Salir de la sección crítica
        fflush(stdout); // Limpiar el buffer de salida
        
    }
    close(client_socket);   // Cerrar el socket
    pthread_exit(NULL); // Salir del hilo
}

void LEDinterrupt(void){    // Función para la interrupción de las leds
        if(digitalRead(LED1IN) == 1){   // Si el pin del dispositivo IoT de la led 1 es 1
            digitalWrite(LED1, HIGH);   // Encender la led 1
            flag_led1 = 1;  // Cambiar la bandera a 1
        }else if(digitalRead(LED1IN) == 0){ // Si el pin del dispositivo IoT de la led 1 es 0
            digitalWrite(LED1, LOW);    // Apagar la led 1
            flag_led1 = 0;  // Cambiar la bandera a 0
        } if(digitalRead(LED2IN) == 1){ // Si el pin del dispositivo IoT de la led 2 es 1
            digitalWrite(LED2, HIGH);   // Encender la led 2
            flag_led2 = 1;  // Cambiar la bandera a 1
        }else if(digitalRead(LED2IN) == 0){ // Si el pin del dispositivo IoT de la led 2 es 0
            digitalWrite(LED2, LOW);    // Apagar la led 2
            flag_led2 = 0;  // Cambiar la bandera a 0
        }
        sem_wait(&semaforo);    // Entrar a la sección crítica
        num_evento = 5; // Número de evento para la led
        men();  // Crear el mensaje
        printf("%s\n", mensaje);    // Imprimir el mensaje
        fflush(stdout); // Limpiar el buffer de salida
        n = send(client_socket, mensaje, strlen(mensaje), 0);   // Enviar el mensaje al historiador
        sem_post(&semaforo);      // Salir de la sección crítica
}

void Dip1(void) {   // Función para el dip switch 1
    static unsigned long lastDebounceTime1 = 0; // Variables para el tiempo de debounce
    static unsigned long lastDebounceTime2 = 0; 
    unsigned long currentTime = millis();   // Obtener el tiempo actual

    if (digitalRead(DIP_PIN1) == 1 && currentTime - lastDebounceTime2 > DEBOUNCE_DELAY){    // Si el pin del dip switch 1 es 1 y el tiempo actual menos el tiempo de debounce es mayor al tiempo de debounce
        flag_dip1 = 1;  // Cambiar la bandera a 1
        lastDebounceTime2 = currentTime;    // Guardar el tiempo actual
    }
    if (digitalRead(DIP_PIN1) == 0 && currentTime - lastDebounceTime2 > DEBOUNCE_DELAY){    // Si el pin del dip switch 1 es 0 y el tiempo actual menos el tiempo de debounce es mayor al tiempo de debounce
        flag_dip1 = 0;  // Cambiar la bandera a 0
        lastDebounceTime2 = currentTime;    // Guardar el tiempo actual
    }
    if(flag_dip1== 1 || flag_dip1== 0){ // Si la bandera es 1 o 0
        fflush(stdout); // Limpiar el buffer de salida
        sem_wait(&semaforo);    // Entrar a la sección crítica
        num_evento = 1;     // Número de evento para el dip switch
        men();        // Crear el mensaje
        printf("%s\n", mensaje);    // Imprimir el mensaje
        fflush(stdout); // Limpiar el buffer de salida
        n = send(client_socket, mensaje, strlen(mensaje), 0);   // Enviar el mensaje al historiador
        sem_post(&semaforo);    // Salir de la sección crítica
        if(n < 0)   // Si hay error al enviar el mensaje
            error("Sendto");    // Imprimir el error
        fflush(stdout); // Limpiar el buffer de salida
        delay(700); // Wait for 700ms to debounce
    }
}

void Dip2(void) {   // Función para el dip switch 2
    static unsigned long lastDebounceTime1 = 0; // Variables para el tiempo de debounce
    static unsigned long lastDebounceTime2 = 0;
    unsigned long currentTime = millis();   // Obtener el tiempo actual

    if (digitalRead(DIP_PIN2) == 1 && currentTime - lastDebounceTime2 > DEBOUNCE_DELAY){    // Si el pin del dip switch 2 es 1 y el tiempo actual menos el tiempo de debounce es mayor al tiempo de debounce
        flag_dip2 = 1;  // Cambiar la bandera a 1
        lastDebounceTime2 = currentTime;    // Guardar el tiempo actual
    }
    if (digitalRead(DIP_PIN2) == 0 && currentTime - lastDebounceTime2 > DEBOUNCE_DELAY){    // Si el pin del dip switch 2 es 0 y el tiempo actual menos el tiempo de debounce es mayor al tiempo de debounce
        flag_dip2 = 0;  // Cambiar la bandera a 0
        lastDebounceTime2 = currentTime;    // Guardar el tiempo actual
    }
    if(flag_dip2== 1 || flag_dip2== 0 ){    // Si la bandera es 1 o 0
        fflush(stdout); // Limpiar el buffer de salida
        sem_wait(&semaforo);    // Entrar a la sección crítica
        num_evento = 1;         // Número de evento para el dip switch
        men();        // Crear el mensaje
        printf("%s\n", mensaje);    // Imprimir el mensaje
        fflush(stdout); // Limpiar el buffer de salida
        n = send(client_socket, mensaje, strlen(mensaje), 0);   // Enviar el mensaje al historiador
        sem_post(&semaforo);    // Salir de la sección crítica
        if(n < 0)   // Si hay error al enviar el mensaje
            error("Sendto");    // Imprimir el error
        fflush(stdout); // Limpiar el buffer de salida
        delay(700); // Wait for 700ms to debounce
    }
}

void buttonPressed(void) {  // Función para el botón
    static unsigned long lastDebounceTime1 = 0; // Variables para el tiempo de debounce
    static unsigned long lastDebounceTime2 = 0; 
    unsigned long currentTime = millis();   // Obtener el tiempo actual

    if (digitalRead(BUTTON_PIN1) == 1 && currentTime - lastDebounceTime1 > DEBOUNCE_DELAY){   // Si el pin del botón 1 es 1 y el tiempo actual menos el tiempo de debounce es mayor al tiempo de debounce
        flag_boton1 = 1;    // Cambiar la bandera a 1
        lastDebounceTime1 = currentTime;    // Guardar el tiempo actual
    }
    if (digitalRead(BUTTON_PIN2) == 1 && currentTime - lastDebounceTime2 > DEBOUNCE_DELAY){  // Si el pin del botón 2 es 1 y el tiempo actual menos el tiempo de debounce es mayor al tiempo de debounce
        flag_boton2 = 1;    // Cambiar la bandera a 1
        lastDebounceTime2 = currentTime;    // Guardar el tiempo actual
    }
    if(flag_boton1 == 1 || flag_boton2 == 1){   // Si la bandera del boton 1 o la bandera del boton 2 es 1
        fflush(stdout); // Limpiar el buffer de salida
        sem_wait(&semaforo);    // Entrar a la sección crítica
        num_evento = 2;     // Número de evento para el botón
        men();      // Crear el mensaje
        printf("%s\n", mensaje);    // Imprimir el mensaje
        fflush(stdout); // Limpiar el buffer de salida
        n = send(client_socket, mensaje, strlen(mensaje), 0);   // Enviar el mensaje al historiador
        sem_post(&semaforo);    // Salir de la sección crítica
        if(n < 0)   // Si hay error al enviar el mensaje
            error("Sendto");    // Imprimir el error
        fflush(stdout); // Limpiar el buffer de salida
        delay(700); // Wait for 700ms to debounce
        flag_boton1 = 0;    // Cambiar la bandera del botón 1 a 0
        flag_boton2 = 0;    // Cambiar la bandera del botón 2 a 0
    }
}

//Hilo para el control de la bocina
void Bocina(void *ptr){
    // Mientras el programa principal se ejecuta, el hilo también
    while(1){
        while(flag_ADC != 0){         //Si el valor ya no es r sale del loop y no hace nada el hilo
            digitalWrite(ALARM, HIGH);  //Generar un pulso con frecuencia de aproximadamente 440Hz -> T = 2.273ms
            usleep(2273);            //Esperar 2.273ms
            digitalWrite(ALARM, LOW);   //Apagar la bocina
            usleep(2273);         //Esperar 2.273ms
        }
        digitalWrite(ALARM, LOW);   //Apagar la bocina
    }
    // Si sale del loop es porque el programa terminó
    pthread_exit(0);    // Salir del hilo
}


void Obtener_ip(void){  // Función para obtener la dirección IP
	struct ifaddrs *ifaddr, *ifa;   // Estructuras para obtener la dirección IP
    int family; // Familia de la dirección IP
	if (getifaddrs(&ifaddr) == -1) {    // Obtener la dirección IP
        perror("getifaddrs");   // Si hay error, imprimirlo
        exit(EXIT_FAILURE); // Salir del programa
    }

    //Iterar a través de la lista de interfaces
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {  // Iterar a través de la lista de interfaces
        if (ifa->ifa_addr == NULL) {    // Si la dirección IP es nula
            continue;   // Continuar con el siguiente elemento
        }

        family = ifa->ifa_addr->sa_family;  // Obtener la familia de la dirección IP

        // Si la interfaz es de tipo AF_INET (IPv4) o AF_INET6 (IPv6)
        if (family == AF_INET) {    // Si la interfaz es de tipo AF_INET (IPv4)
            // Puedes cambiar "wlan0" por el nombre de la interfaz deseada
			// En este caso se utiliza el GUID del adaptador de la computadora o bien "wlan0" si es en una Raspberry
            if (strcmp(ifa->ifa_name, ADAPTADOR_RED) == 0) {    // Si el nombre de la interfaz es igual al nombre de la interfaz de la computadora
                if (family == AF_INET) {    
                    // IPv4
                    struct sockaddr_in *addr = (struct sockaddr_in *)ifa->ifa_addr; // Obtener la dirección IP
                    inet_ntop(AF_INET, &addr->sin_addr, ip, INET_ADDRSTRLEN);   // Convertir la dirección IP a una cadena de caracteres
                    printf("Dirección IP de wlan0 (IPv4): %s\n", ip);   // Imprimir la dirección IP
                    fflush(stdout); // Limpiar el buffer de salida
                }
            }
        }
    }
	freeifaddrs(ifaddr);    // Liberar la memoria
}

void error(const char *msg) // Función para imprimir el error
{
	perror(msg);    // Imprimir el error
	exit(0);    // Salir del programa
}
