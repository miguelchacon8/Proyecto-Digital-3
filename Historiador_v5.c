/*	Universidad del Valle de Guatemala
	Electrónica Digital 3
	Proyecto Final - Sistema SCADA simplificado
	Código del Historiador
	Integrantes de grupo: Luis Carranza, Miguel Chacón y Oscar Donis
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

// #ifdef _WIN32 // Si se está compilando en Windows
//     // Definir variables específicas para Windows
//     #define ADAPTADOR_RED "{8B93B931-7325-41A7-A313-D29DEEE73F83}"
// #else // Si se está compilando en otro sistema (por ejemplo, Raspberry Pi)
//     // Definir variables específicas para Raspberry Pi
// 	#define ADAPTADOR_RED "wlan0"
// #endif

#define MAX_CLIENT 10
#define ADAPTADOR_RED "{8B93B931-7325-41A7-A313-D29DEEE73F83}"
#define MSG_SIZE 60		// Tamaño (máximo) del mensaje. Puede ser mayor a 40.
#define MAX_LENGTH 60
#define MAX_MESSAGES 30
unsigned int length;
struct sockaddr_in server_addr, client_addr;
int addrlen = sizeof(server_addr);
int server_socket, client_socket[MAX_CLIENT], n;
int rtus = 0;
char RTUS[10][17];
char ip[INET_ADDRSTRLEN];
char Arreglo[MAX_MESSAGES][MAX_LENGTH];
int start = 0;
int end = 0;
int count = 0;

void Recibir_clientes(void *ptr);
void Menu(void *ptr);
void error(const char *msg);
void Obtener_ip(void);
void guardar_mensaje(const char *message);
void Mostrar_actualizacion(void);

struct ClientData {
    int client_socket;
    int client_id;
};

int main(int argc, char *argv[])
{
	if(argc != 2){
		printf("usage: %s port\n", argv[0]);
		exit(1);
	}

	if((server_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0){ // Creates socket. Connectionless.
		error("Error: socket");
		exit(EXIT_FAILURE);
	}
	// Estructura de datos para la comunicación del servidor con el cliente
	server_addr.sin_family = AF_INET;				// symbol constant for Internet domain
	server_addr.sin_port = htons(atoi(argv[1]));		// Número de puerto
	server_addr.sin_addr.s_addr = INADDR_ANY;	// para recibir de cualquier interfaz de red

    Obtener_ip();

	// binds the socket to the address of the host and the port number
	if(bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0){
		error("Error binding socket.");
		exit(EXIT_FAILURE);
		}
	if(listen(server_socket, 2) == -1){
		perror("Error al escuchar");
		exit(EXIT_FAILURE);
	}

	pthread_t menu;

	if((pthread_create(&menu, NULL, (void *)&Menu, NULL))<0){
		perror("No se pudo crear el hilo del menu");
		exit(-1);
	}

	pthread_t threads_clients[MAX_CLIENT];

	int client_id = 0;
	while (client_id < MAX_CLIENT) {
		client_socket[client_id] = accept(server_socket, (struct sockaddr *)&client_addr, (socklen_t *)&addrlen);

        // Aceptar conexión del cliente
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

	close(server_socket);	// close socket.
	for (int i = 0; i < MAX_CLIENT; ++i) {
        close(client_socket[i]);
    }

	return 0;
}

void error(const char *msg){
	perror(msg);
	exit(0);
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

void Menu(void *ptr){
	char buffer[MSG_SIZE];	// to store received messages or messages to be sent.
	int cmp;
	while(buffer[0] != '!'){
		memset(buffer, 0, MSG_SIZE);	// sets all values to zero
		printf("\n------------------- MENÚ -------------------\n");
		fflush(stdout);
		printf("Ingrese el número de la acción que desea realizar: \n1 - Actualizar el listado con los dispositivos conectados\n");
		fflush(stdout);
		printf("2 - Revisar el historial de las últimas 30 actualizaciones/alertas\n3 - Revisar la lista de RTU's conectados\n");
		fflush(stdout);
		int i, instruc = 4;
		for(i = 0; i < rtus; i++){
			char led_rtu[10];
			sprintf(led_rtu, "LED1, RTU%d\n", i+1);
			printf("%d - Encender/apagar %s", instruc, led_rtu);
			sprintf(led_rtu, "LED2, RTU%d\n", i+1);
			printf("%d - Encender/apagar %s", instruc+1, led_rtu);
			fflush(stdout);
			instruc = instruc+2;
		}

		fgets(buffer,MSG_SIZE-1,stdin); // MSG_SIZE-1 because a null character is added
		if(strncmp(buffer, "2", 1) == 0){
			printf("\n-------- HISTORIAL DE ACTUALIZACIONES --------\n");
			fflush(stdout);
			Mostrar_actualizacion();
			printf("\n\n");
			fflush(stdout);
		}
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
				printf("RTU%d %s\n", i+1, RTUS[i]);
				fflush(stdout);
				}
				printf("\n\n");
				fflush(stdout);
			}
			
		}
		else if(strcmp(buffer, "1\n") == 0){
				// solo actualiza
			}
		else if(cmp = atoi(buffer)){
			
			if(cmp <= (rtus*2+3)){
				int i, rtu = 0;
				for(i = 4; i<=(rtus*2+3); i+=2){
					
					printf("%d\n", rtu);
					fflush(stdout);
					char ip_destino[30];


					if((cmp-i) == 0){
						fflush(stdout);
						sprintf(ip_destino, "%s LED1", RTUS[rtu]);
						printf("\nEncendiendo/apagando LED\n");
						fflush(stdout);
						n = send(client_socket[rtu], ip_destino, 21, 0);
						// if(n < 0)
						// 	error("Sendto");
					}
					else if((cmp-i) == 1){
						sprintf(ip_destino, "%s LED2", RTUS[rtu]);
						printf("\nEncendiendo/apagando LED\n");
						fflush(stdout);
						n = send(client_socket[rtu], ip_destino, 21, 0);
						// if(n < 0)
						// 	error("Sendto");
					}
					rtu++;
				}
			}
			else{
				printf("No ingresó un comando válido\n");
				fflush(stdout);
			}
			
		}
		else{
			printf("No ingresó un comando válido\n");
			fflush(stdout);
			}

	}

}
void Recibir_clientes(void *ptr) {
    struct ClientData *client_data = (struct ClientData *)ptr;
    char buffer[MSG_SIZE];
    int bytes_received;

    while (1) {
        // Recibir mensaje del cliente
		memset(buffer, 0, MSG_SIZE);
        bytes_received = recv(client_data->client_socket, buffer, MSG_SIZE, 0);
        buffer[bytes_received] = '\0';
        if (bytes_received <= 0) {
            printf("Cliente %d desconectado\n", (client_data->client_id + 1));
            break;
        }
		//printf("Cliente %d: %s\n", client_data->client_id, buffer);
		char ip_recibida[16];
		//char final_ip[3];
		char *token;
		int cmp;
		// if(n < 0)
        // 	error("recvfrom");
		if(strncmp(buffer, "#", 1) == 0){
			token = strtok(buffer, " .");
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
			strcpy(RTUS[rtus], ip_recibida);
			rtus++;

			sprintf(buffer, "RTU %d.", rtus);
			n = send(client_data->client_socket, buffer, strlen(buffer), 0);
			char ip_impresa[16];
			strcpy(ip_impresa, RTUS[rtus-1]);
			printf("\nSe conectó un dispositivo con dirección ip: %s\n", ip_impresa);
			fflush(stdout);

		}
		else{
			guardar_mensaje(buffer);
		}
	}
    close(client_data->client_socket);
    free(client_data);
    pthread_exit(NULL);
}

void guardar_mensaje(const char *message) {
    strncpy(Arreglo[end], message, MAX_LENGTH - 1);
    Arreglo[end][MAX_LENGTH - 1] = '\0'; // Asegurar que la cadena esté terminada
    end = (end + 1) % MAX_MESSAGES; // Avanzar la posición final circularmente
    if (count < MAX_MESSAGES) {
        count++;
    } else {
        start = (start + 1) % MAX_MESSAGES; // Avanzar la posición inicial si ya se alcanzó el límite
    }
}

void Mostrar_actualizacion() {
    int i;
    int index = start;
	printf("U E Hora   P1 P2 D1 D2 L1 L2 ADC\n");
	fflush(stdout);
    for (i = 0; i < count; i++) {
        printf("%s\n", Arreglo[index]);
		fflush(stdout);
        index = (index + 1) % MAX_MESSAGES; // Avanzar la posición inicial circularmente
    }
}