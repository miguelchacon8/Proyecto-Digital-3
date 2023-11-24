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
#define BUTTON_PIN1 21
#define BUTTON_PIN2 20
#define DIP_PIN1 24
#define DIP_PIN2 25
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
void Dip1(void);
void Dip2(void);
void Actualizacion(void *ptr);
void receive(void *ptr);
void error(const char *);
void ADC(void *ptr);
void men(void);
void LEDinterrupt(void);
void Obtener_ip(void);


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
uint16_t ADCvalue = 0;
FILE *datos;
char lectura[4];
int i;
char mensaje[60];
unsigned int length;
int client_socket, n;
struct sockaddr_in server_addr;
struct hostent *hp;
char buffer[MSG_SIZE];
int channel = 0, speed = 115200; 
unsigned char d1;
char tok[2][1];
char ip[INET_ADDRSTRLEN];

sem_t semaforo;

int main(int argc, char *argv[])
{  
    sem_init(&semaforo, 0, 1);
    if(argc != 3)
	{
		printf("usage %s hostname port\n", argv[0]);
		exit(1);
	}

	if ((client_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Error al crear el socket");
        exit(EXIT_FAILURE);
    }

	server_addr.sin_family = AF_INET;	// symbol constant for Internet domain
	server_addr.sin_port = htons(atoi(argv[2]));	// port field
	if (inet_pton(AF_INET, argv[1], &server_addr.sin_addr) <= 0) {
        perror("Dirección no válida/soportada");
        exit(EXIT_FAILURE);
    }
	// Conectar al servidor
    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Conexión fallida");
        exit(EXIT_FAILURE);
    }
    
	//-----------------------------------------------------------
    
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
    wiringPiISR(DIP_PIN1, INT_EDGE_BOTH, &Dip1);
    wiringPiISR(DIP_PIN2, INT_EDGE_BOTH, &Dip2);
    pinMode(LED1, OUTPUT);
    pinMode(LED2, OUTPUT);
//     pinMode(LED1IN, INPUT);
    pinMode(LED2IN, INPUT);
    wiringPiISR(LED1IN, INT_EDGE_BOTH, &LEDinterrupt);
    wiringPiISR(LED2IN, INT_EDGE_BOTH, &LEDinterrupt);
    
    if(wiringPiSPISetup(SPI_CHANNEL, SPI_SPEED)<0){
        printf("WiringPiSPISetup falló \n");
        return(-1);
    }
    //Obtener la hora
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
    
    //Creación de un hilo para la bocina
    pthread_t thread2, thread3, thread4, threadrecv;
    pthread_create(&thread2, NULL, (void*)&Bocina, NULL);
    pthread_create(&thread3, NULL, (void*)&Actualizacion, NULL);
    pthread_create(&thread4, NULL, (void*)&ADC, NULL);
    pthread_create(&threadrecv, NULL, (void*)&receive, NULL);
    Obtener_ip();
    char my_ip[18];
    sprintf(my_ip, "# %s", ip);
    n = send(client_socket, my_ip, strlen(my_ip), 0);
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
    close(client_socket);
    fclose(datos);
  return 0;   
}

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

void ADC(void *ptr){
    while(1){
        ADCvalue = get_ADC(ADC_CHANNEL);
        sprintf(lectura, "%d\n", ADCvalue);
       
        fputs(lectura, datos);
        usleep(1000);
	    sem_wait(&semaforo);
        voltaje = ADCvalue*(3.3/1024);
        if(ADCvalue >= 775 || ADCvalue <= 155){
            if(flag_ADC != 1){
                flag_ADC = 1;
                num_evento = 3;
                men();
                printf("%s -> cambio el ADC\n", mensaje);
                n = send(client_socket, mensaje, strlen(mensaje), 0);
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
                n = send(client_socket, mensaje, strlen(mensaje), 0);
	            if(n < 0)
		            error("Sendto");
                fflush(stdout);
            }
        }
	sem_post(&semaforo);
    }
}

void Actualizacion(void *ptr){
    while(1){
        sem_wait(&semaforo);
        num_evento = 6;
        men();
        printf("%s\n", mensaje);
        n = send(client_socket, mensaje, strlen(mensaje), 0);
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
void receive(void *ptr){
    int n1;
    char buffer1[MSG_SIZE], led1[21], led2[21], *token, buffer_temp[MSG_SIZE];
    while(1){
        sprintf(led1, "%s LED1", ip);
        sprintf(led2, "%s LED2", ip);
        memset(buffer1, 0, MSG_SIZE);
        n1 = recv(client_socket, buffer1, MSG_SIZE, 0);
        
        if (n1 <= 0) {
            printf("Cliente desconectado\n");
            break;
        }
        buffer1[n1] = '\0';
        //printf("%s\n", buffer1);
        
        sem_wait(&semaforo);
        if(strncmp(buffer1, "RTU", 3) == 0){
            strcpy(buffer_temp, buffer1);
            token = strtok(buffer_temp, " ");
            token = strtok(NULL, ".");
            UTR = atoi(token);
        }
        
	    if(n < 0)
		    error("Sendto");
        if(strcmp(buffer1, led1) == 0){
            if(flag_led1 == 0){
                digitalWrite(LED1, HIGH);
                flag_led1 = 1;
            }else if(flag_led1 == 1){
                digitalWrite(LED1, LOW);
                flag_led1 = 0;                
            }
		}else if (strcmp(buffer1, led2) == 0){
            if(flag_led2 == 0){
                digitalWrite(LED2, HIGH);
                flag_led2 = 1;
            }else if(flag_led2 == 1){
                digitalWrite(LED2, LOW);
                flag_led2 = 0;                
            }
        }
        num_evento = 4;
        men();
        printf("%s\n", mensaje);
        n = send(client_socket, mensaje, strlen(mensaje), 0);
        sem_post(&semaforo);
        fflush(stdout);
        
    }
    close(client_socket);
    pthread_exit(NULL);
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
        fflush(stdout);
        n = send(client_socket, mensaje, strlen(mensaje), 0);
        sem_post(&semaforo);        
}

void Dip1(void) {
    static unsigned long lastDebounceTime1 = 0;
    static unsigned long lastDebounceTime2 = 0;
    unsigned long currentTime = millis();

    if (digitalRead(DIP_PIN1) == 1 && currentTime - lastDebounceTime2 > DEBOUNCE_DELAY){
        flag_dip1 = 1;
        lastDebounceTime2 = currentTime;
    }
    if (digitalRead(DIP_PIN1) == 0 && currentTime - lastDebounceTime2 > DEBOUNCE_DELAY){
        flag_dip1 = 0;
        lastDebounceTime2 = currentTime;
    }
    if(flag_dip1== 1 || flag_dip1== 0){
        fflush(stdout);
        sem_wait(&semaforo);
        num_evento = 1;   
        men();
        printf("%s\n", mensaje);
        fflush(stdout);
        n = send(client_socket, mensaje, strlen(mensaje), 0);
        sem_post(&semaforo);
        if(n < 0)
            error("Sendto");
        fflush(stdout);
        delay(700); // Wait for 700ms to debounce
    }
}

void Dip2(void) {
    static unsigned long lastDebounceTime1 = 0;
    static unsigned long lastDebounceTime2 = 0;
    unsigned long currentTime = millis();

    if (digitalRead(DIP_PIN2) == 1 && currentTime - lastDebounceTime2 > DEBOUNCE_DELAY){
        flag_dip2 = 1;
        lastDebounceTime2 = currentTime;
    }
    if (digitalRead(DIP_PIN2) == 0 && currentTime - lastDebounceTime2 > DEBOUNCE_DELAY){
        flag_dip2 = 0;
        lastDebounceTime2 = currentTime;
    }
    if(flag_dip2== 1 || flag_dip2== 0 ){
        fflush(stdout);
        sem_wait(&semaforo);
        num_evento = 1;   
        men();
        printf("%s\n", mensaje);
        fflush(stdout);
        n = send(client_socket, mensaje, strlen(mensaje), 0);
        sem_post(&semaforo);
        if(n < 0)
            error("Sendto");
        fflush(stdout);
        delay(700); // Wait for 700ms to debounce
    }
}

void buttonPressed(void) {
    static unsigned long lastDebounceTime1 = 0;
    static unsigned long lastDebounceTime2 = 0;
    unsigned long currentTime = millis();

    if (digitalRead(BUTTON_PIN1) == 1 && currentTime - lastDebounceTime1 > DEBOUNCE_DELAY){
        flag_boton1 = 1;
        lastDebounceTime1 = currentTime;
    }
    if (digitalRead(BUTTON_PIN2) == 1 && currentTime - lastDebounceTime2 > DEBOUNCE_DELAY){
        flag_boton2 = 1;
        lastDebounceTime2 = currentTime;
    }
    if(flag_boton1 == 1 || flag_boton2 == 1){
        fflush(stdout);
        sem_wait(&semaforo);
        num_evento = 2;   
        men();
        printf("%s\n", mensaje);
        fflush(stdout);
        n = send(client_socket, mensaje, strlen(mensaje), 0);
        sem_post(&semaforo);
        if(n < 0)
            error("Sendto");
        fflush(stdout);
        delay(700); // Wait for 700ms to debounce
        flag_boton1 = 0;
        flag_boton2 = 0;
    }
}

//Hilo para el control de la bocina
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


void Obtener_ip(void){
	struct ifaddrs *ifaddr, *ifa;
    int family;
	if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        exit(EXIT_FAILURE);
    }

    //Iterar a través de la lista de interfaces
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL) {
            continue;
        }

        family = ifa->ifa_addr->sa_family;

        // Si la interfaz es de tipo AF_INET (IPv4) o AF_INET6 (IPv6)
        if (family == AF_INET) {
            // Puedes cambiar "wlan0" por el nombre de la interfaz deseada
			// En este caso se utiliza el GUID del adaptador de la computadora o bien "wlan0" si es en una Raspberry
            if (strcmp(ifa->ifa_name, ADAPTADOR_RED) == 0) {
                if (family == AF_INET) {
                    // IPv4
                    struct sockaddr_in *addr = (struct sockaddr_in *)ifa->ifa_addr;
                    inet_ntop(AF_INET, &addr->sin_addr, ip, INET_ADDRSTRLEN);
                    printf("Dirección IP de wlan0 (IPv4): %s\n", ip);
                    fflush(stdout);
                }
            }
        }
    }
	freeifaddrs(ifaddr);
}

void error(const char *msg)
{
	perror(msg);
	exit(0);
}
