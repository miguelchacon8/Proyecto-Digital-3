/*	Universidad del Valle de Guatemala
	Electrónica Digital 3
	Proyecto Final - Sistema SCADA simplificado
	Código del Historiador
	Integrantes de grupo: Luis Carranza, Miguel Chacón y Oscar Donis
	Última actualización: 24/11/2023
	Instrucciones: Para compilar el programa debe ingresar el comando:
	gcc Historiador.c - o Historiador -lpthread -lresolv 

	IMPORTANTE: Para correr el archivo en Windows se debe cambiar el GUID del adaptador de red de la computadora en la línea 37
	En la Raspberry se puede descomentar la parte de la línea 27 a la 34 y comentar la línea 37
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <net/if.h>
#include <ifaddrs.h>

// Esta sección del código permite definir variables específicas para la dirección ip seg Windows o para Raspberry Pi
// #ifdef _WIN32 // Si se está compilando en Windows
//     // Definir variables específicas para Windows
//     #define ADAPTADOR_RED "{8B93B931-7325-41A7-A313-D29DEEE73F83}"
// #else // Si se está compilando en otro sistema (por ejemplo, Raspberry Pi)
//     // Definir variables específicas para Raspberry Pi
// 	#define ADAPTADOR_RED "wlan0"
// #endif

#define MAX_CLIENT 10	// Número máximo de clientes
#define ADAPTADOR_RED "{8B93B931-7325-41A7-A313-D29DEEE73F83}"	// GUID del adaptador de red de la computadora utilizada para el historiador
#define MSG_SIZE 60		// Tamaño (máximo) del mensaje. Puede ser mayor a 40.
#define MAX_LENGTH 60	// Tamaño máximo de la cadena de caracteres del arreglo
#define MAX_MESSAGES 30	// Número máximo de mensajes que se pueden guardar en el arreglo

// Variables globales ----------------------------------------------------------------------------------------------------------
struct sockaddr_in server_addr, client_addr;	// Estructuras de datos para la comunicación del servidor con el cliente
int addrlen = sizeof(server_addr);				// Tamaño de la estructura de datos del servidor
int server_socket, client_socket[MAX_CLIENT], n;// Sockets para el servidor y el cliente
int rtus = 0;							// Número de RTUS conectados
char RTUS[10][17];						// Arreglo para guardar las direcciones ip de los RTUS conectados
char ip[INET_ADDRSTRLEN];				// Variable para guardar la dirección ip del servidor (historiador)
char Arreglo[MAX_MESSAGES][MAX_LENGTH];	// Arreglo para guardar los mensajes recibidos de las actualizaciones

// Variables para el arreglo circular para desplegar y guardar los mensajes
int start = 0;						
int end = 0;						
int count = 0;

// Prototipos de funciones
void Recibir_clientes(void *ptr);
void Menu(void *ptr);
void error(const char *msg);
void Obtener_ip(void);
void guardar_mensaje(const char *message);
void Mostrar_actualizacion(void);
void log_file(void);

// Estructura para almacenar los datos de cada cliente
struct ClientData {
    int client_socket;
    int client_id;
};

// Función principal -----------------------------------------------------------------------------------------------------------
int main(int argc, char *argv[])
{
	if(argc != 2){
		printf("usage: %s port\n", argv[0]);
		exit(1);
	}
	// Establecer el socket del servidor
	if((server_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0){ 
		error("Error: socket");
		exit(EXIT_FAILURE);
	}
	// Estructura de datos para la comunicación del servidor con el cliente
	server_addr.sin_family = AF_INET;				// IPv4
	server_addr.sin_port = htons(atoi(argv[1]));	// Puerto del servidor
	server_addr.sin_addr.s_addr = INADDR_ANY;		// Para recibir de cualquier interfaz de red
	// Obtener la dirección ip del servidor (historiador)
    Obtener_ip();

	// Enlazar el socket del servidor con la estructura de datos
	if(bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0){
		error("Error binding socket.");
		exit(EXIT_FAILURE);
		}
	// Escuchar por conexiones entrantes
	if(listen(server_socket, 2) == -1){
		perror("Error al escuchar");
		exit(EXIT_FAILURE);
	}
	// Creación del hilo para el menú
	pthread_t menu;
	if((pthread_create(&menu, NULL, (void *)&Menu, NULL))<0){
		perror("No se pudo crear el hilo del menu");
		exit(-1);
	}
	// Creación de los hilos para los clientes
	pthread_t threads_clients[MAX_CLIENT];
	int client_id = 0;	// Identificador del cliente
	while (client_id < MAX_CLIENT) {
		// Aceptar conexión del cliente
		client_socket[client_id] = accept(server_socket, (struct sockaddr *)&client_addr, (socklen_t *)&addrlen);
        // Guardar los datos del cliente en una estructura
        struct ClientData *client_data = (struct ClientData *)malloc(sizeof(struct ClientData));
        client_data->client_socket = client_socket[client_id];
		client_data->client_id = client_id;

        // Crear un hilo para manejar la conexión con el cliente
        if((pthread_create(&threads_clients[client_id], NULL, (void *)Recibir_clientes, (void *)client_data)) < 0){
			perror("No se pudo crear el hilo del menu");
			exit(-1);
		}
        client_id++;
    }	
	// Esperar a que todos los hilos terminen
    for (int i = 0; i < MAX_CLIENT; ++i) {
        pthread_join(threads_clients[i], NULL);
    }
	pthread_join(menu, NULL);

	// Cerrar el socket del servidor
	close(server_socket);	
	// Cerrar los sockets de los clientes
	for (int i = 0; i < MAX_CLIENT; ++i) {
        close(client_socket[i]);
    }
	return 0;
}

// Función para mensajes de error---------------------------------------------------------------------------------------------------------------
void error(const char *msg){
	perror(msg);
	exit(0);
}

// Función para obtener la dirección ip del servidor (historiador)-----------------------------------------------------------------------------
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
        // Si la interfaz es de tipo AF_INET (IPv4) 
        if (family == AF_INET) {
            if (strcmp(ifa->ifa_name, ADAPTADOR_RED) == 0) {	// Si la interfaz es la del adaptador de red del historiador
                if (family == AF_INET) {
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

// Función para el hilo del menú----------------------------------------------------------------------------------------------------------------------
void Menu(void *ptr){
	char buffer[MSG_SIZE];	// Buffer para recibir los mensajes que deberá enviar al cliente
	int cmp;				// Variable para comparar el comando ingresado por el usuario
	while(buffer[0] != '!'){
		memset(buffer, 0, MSG_SIZE);	// Limpiar el buffer
		printf("\n------------------- MENÚ -------------------\n");
		fflush(stdout);
		printf("Ingrese el número de la acción que desea realizar: \n1 - Actualizar el listado con los dispositivos conectados\n");
		fflush(stdout);
		printf("2 - Revisar el historial de las últimas 30 actualizaciones/alertas\n3 - Revisar la lista de RTU's conectados\n");
		fflush(stdout);
		printf("4 - Crear el archivo log.txt\n");
		fflush(stdout);
		// Imprimir las opciones para encender o apagar los leds de cada RTU
		int i, instruc = 5;
		for(i = 0; i < rtus; i++){
			char led_rtu[10];
			sprintf(led_rtu, "LED1, RTU%d\n", i+1);
			printf("%d - Encender/apagar %s", instruc, led_rtu);
			sprintf(led_rtu, "LED2, RTU%d\n", i+1);
			printf("%d - Encender/apagar %s", instruc+1, led_rtu);
			fflush(stdout);
			instruc = instruc+2;
		}

		fgets(buffer,MSG_SIZE-1,stdin); // Leer el comando ingresado por el usuario

		// Si ingresó el comando 2 muestra el historial de actualizaciones
		if(strncmp(buffer, "2", 1) == 0){
			printf("\n-------- HISTORIAL DE ACTUALIZACIONES --------\n");
			fflush(stdout);
			Mostrar_actualizacion();
			printf("\n\n");
			fflush(stdout);
		}
		// Si ingresó el comando 3 muestra la lista de RTUS conectados
		else if(strncmp(buffer, "3", 1) == 0){
			int i;
			if (rtus == 0){
				printf("No hay RTUS conectados\n");
				fflush(stdout);
			}
			else{
				printf("\n\n----------- RTUS conectados -------------\n");
				fflush(stdout);
				for(i = 0; i < rtus; i++){
				printf("RTU%d %s\n", i+1, RTUS[i]);	// Imprime las direcciones guardadas en el arreglo RTUS
				fflush(stdout);
				}
				printf("\n\n");
				fflush(stdout);
			}
			
		}
		// Si ingresó un 1 solo ignora y actualiza
		else if(strcmp(buffer, "1\n") == 0){
				// solo actualiza
			}
		// Si ingresó un 4 crea el archivo log.txt
		else if(strcmp(buffer, "4\n") == 0){
			log_file();
			printf("\nSe creó el archivo log.txt\n");
			fflush(stdout);
		}
		// Si ingresó un 5 o mayor se enciende o apaga el led correspondiente
		else if(cmp = atoi(buffer)){
			if(cmp <= (rtus*2+4)){	// Si el comando ingresado es una instrucción mayor a 5 y menor o igual al número de RTUS conectados*2
				int i, rtu = 0;
				for(i = 5; i<=(rtus*2+4); i+=2){	// Recorre las instrucciones para encender o apagar los leds de cada RTU
					printf("%d\n", rtu);
					fflush(stdout);
					char ip_destino[30];
					if((cmp-i) == 0){				// Si es una instrucción para encender o apagar el LED1
						fflush(stdout);
						sprintf(ip_destino, "%s LED1", RTUS[rtu]);	// Agrega la dirección ip del RTU y el comando a enviar
						printf("\nEncendiendo/apagando LED\n");
						fflush(stdout);
						n = send(client_socket[rtu], ip_destino, 21, 0);	// Envia el comando al RTU
					}
					else if((cmp-i) == 1){			// Si es una instrucción para encender o apagar el LED2
						sprintf(ip_destino, "%s LED2", RTUS[rtu]);	// Agrega la dirección ip del RTU y el comando a enviar
						printf("\nEncendiendo/apagando LED\n");
						fflush(stdout);
						n = send(client_socket[rtu], ip_destino, 21, 0);	// Envia el comando al RTU
					}
					rtu++;
				}
			}
			else{
				printf("No ingresó un comando válido\n");
				fflush(stdout);
			}
			
		}
		// Si no es ninguno, el comando no es válido
		else{
			printf("No ingresó un comando válido\n");
			fflush(stdout);
			}
	}
}
// Función para recibir mensajes de los clientes--------------------------------------------------------------------------------------------------
void Recibir_clientes(void *ptr) {
    struct ClientData *client_data = (struct ClientData *)ptr;	// Guarda la estructura con la información del cliente recibida
    char buffer[MSG_SIZE];	// Buffer para recibir los mensajes del cliente
    int bytes_received;		// Mensajes recibidos

    while (1) {
		memset(buffer, 0, MSG_SIZE);	// Limpiar el buffer
        bytes_received = recv(client_data->client_socket, buffer, MSG_SIZE, 0);	// Recibir los mensajes del cliente
        buffer[bytes_received] = '\0';	// Asegurar que la cadena esté terminada
        if (bytes_received <= 0) {		// Si se recibió un mensaje vacío o el cliente se desconectó
            printf("Cliente %d desconectado\n", (client_data->client_id + 1));
            break;
        }
		char ip_recibida[16];	// Guardar la ip recibida por el socket del cliente
		char *token;
		if(strncmp(buffer, "#", 1) == 0){
			token = strtok(buffer, " .");	// Separar el # de la ip
			for(int i = 1; i <= 4; i++){
				if(i==1){
					token = strtok(NULL, ".");
					sprintf(ip_recibida, "%s", token);	
				}
				else{
					token = strtok(NULL, ".\n");
					strcat(ip_recibida, ".");			// Concatena las primeras 3 partes de la ip en la variable broadcast_ip
					strcat(ip_recibida, token);
				}
			}
			strcpy(RTUS[rtus], ip_recibida);			// Guarda la dirección ip del RTU en el arreglo RTUS
			rtus++;										// Aumenta la cantidad de RTUS conectados

			sprintf(buffer, "RTU %d.", rtus);			// Agrega el número de RTU al mensaje a enviar
			n = send(client_data->client_socket, buffer, strlen(buffer), 0);
			char ip_impresa[16];
			strcpy(ip_impresa, RTUS[rtus-1]);
			printf("\nSe conectó un dispositivo con dirección ip: %s\n", ip_impresa);	// Imprime la dirección ip del RTU
			fflush(stdout);
		}
		else{
			guardar_mensaje(buffer);	// Si no es una ip, guarda el mensaje de actualización en el arreglo
		}
	}
    close(client_data->client_socket);	// Cerrar el socket del cliente
    free(client_data);					// Liberar la memoria
    pthread_exit(NULL);
}
// Función para guardar los mensajes de actualización en el arreglo-----------------------------------------------------------------------------------
void guardar_mensaje(const char *message) {
    strncpy(Arreglo[end], message, MAX_LENGTH - 1);
    Arreglo[end][MAX_LENGTH - 1] = '\0';// Asegurar que la cadena esté terminada
    end = (end + 1) % MAX_MESSAGES; 	// Avanzar la posición final circularmente
    if (count < MAX_MESSAGES) {
        count++;
    } else {
        start = (start + 1) % MAX_MESSAGES; // Avanzar la posición inicial si ya se alcanzó el límite
    }
}
// Función para mostrar las actualizaciones en la terminal----------------------------------------------------------------------------------
void Mostrar_actualizacion() {
    int i;
    int index = start;
	printf("U E Hora   P1 P2 D1 D2 L1 L2 ADC\n");
	fflush(stdout);
    for (i = 0; i < count; i++) {
        printf("%s\n", Arreglo[index]);		// Imprime el mensaje guardado en el arreglo
		fflush(stdout);
        index = (index + 1) % MAX_MESSAGES; // Avanzar la posición inicial circularmente
    }
}
// Función para crear el archivo log.txt--------------------------------------------------------------------------------------------------
void log_file() {
	FILE *datos;
	datos = fopen("log.txt", "w");	// Crea el archivo log.txt
    int i;							// Mismo procedimiento que el utilizado para mostrar la actualización
    int index = start;
	char act[MSG_SIZE];
	fputs("U E Hora   P1 P2 D1 D2 L1 L2 ADC\n", datos);
	//fflush(stdout);
    for (i = 0; i < count; i++) {
		sprintf(act, "%s\n", Arreglo[index]);	// Guarda el mensaje en la variable act con un "enter"
		fputs(act, datos);						// Guarda el mensaje en el archivo log.txt		
        index = (index + 1) % MAX_MESSAGES; 	// Avanzar la posición inicial circularmente
    }
	fclose(datos);	// Cierra el archivo log.txt
}

// Fin del código --------------------------------------------------------------------------------------------------------------
