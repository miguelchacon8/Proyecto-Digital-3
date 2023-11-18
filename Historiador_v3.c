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

#define ADAPTADOR_RED "{8B93B931-7325-41A7-A313-D29DEEE73F83}"
#define MSG_SIZE 60		// Tamaño (máximo) del mensaje. Puede ser mayor a 40.
unsigned int length;
struct sockaddr_in server, addr, addr_broadcast;
int sockfd, n;
int rtus = 0; 
char RTUS[10][16];
void Recibir(void *ptr);
void error(const char *msg);


int main(int argc, char *argv[])
{
	
	char buffer[MSG_SIZE];	// to store received messages or messages to be sent.
	int boolval = 1;		// for a socket option

	if(argc != 2){
		printf("usage: %s port\n", argv[0]);
		exit(1);
	}


	struct ifaddrs *ifaddr, *ifa;
    int family;
    char ip[INET_ADDRSTRLEN];

	sockfd = socket(AF_INET, SOCK_DGRAM, 0); // Creates socket. Connectionless.
	if(sockfd < 0)
		error("Error: socket");

	// Estructura de datos para la comunicación del servidor con el cliente
	server.sin_family = AF_INET;				// symbol constant for Internet domain
	server.sin_port = htons(atoi(argv[1]));		// Número de puerto
	server.sin_addr.s_addr = htonl(INADDR_ANY);	// para recibir de cualquier interfaz de red
	

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
                    //printf("Dirección IP de wlan0 (IPv4): %s\n", ip);
                    
                }
            }
        }
    }
	freeifaddrs(ifaddr);


	char *token, my_ip[16], broadcast_ip[16];
	strcpy(my_ip, ip);
	token = strtok(ip, ". ");
	sprintf(broadcast_ip, "%s", token);
	for(int i = 2; i <= 3; i++){
		token = strtok(NULL, ". ");
		strcat(broadcast_ip, ".");
		strcat(broadcast_ip, token);
	}
	strcat(broadcast_ip, ".255");
	printf("Mi dirección ip es: %s\nLa dirección de broadcast es %s\n", my_ip, broadcast_ip);
	fflush(stdout);

	
	// Información de la estructura para la comunicación de broadcast con el resto de servidores
	addr_broadcast.sin_family = AF_INET;
	addr_broadcast.sin_port = htons(atoi(argv[1]));
	addr_broadcast.sin_addr.s_addr = inet_addr(broadcast_ip);	// Guarda la dirección de broadcast obtenida anteriormente

	length = sizeof(struct sockaddr_in);		// size of structure

	// binds the socket to the address of the host and the port number
	if(bind(sockfd, (struct sockaddr *)&server, length) < 0)
		error("Error binding socket.");
	// Cambiar los permisos del socket para permitir broadcast
	if(setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &boolval, sizeof(boolval)) < 0)
   		error("Error setting socket options\n");

	pthread_t lectura;
	if((pthread_create(&lectura, NULL, (void *)&Recibir, NULL))<0){
		perror("No se pudo crear el hilo de lectura");
		exit(-1);
	}

	while(buffer[0] != '!'){ 
		memset(buffer, 0, MSG_SIZE);	// sets all values to zero
		printf("-----------MENÚ -------------\n");
		fflush(stdout);
		printf("Ingrese el número de la acción que desea realizar: \n1 - Actualizar el listado con los dispositivos conectados\n");
		fflush(stdout);
		printf("2 - Revisar el historial de las últimas 30 actualizaciones/alertas\n3 - Revisar la lista de RTU's conectados\n");
		fflush(stdout);
		int i, instruc = 4;
		for(i = 0; i < rtus; i++){
			char led_rtu[10];
			sprintf(led_rtu, "LED1, RTU%d\n", i+1);
			printf("%d - Encender %s", instruc, led_rtu);
			sprintf(led_rtu, "LED2, RTU%d\n", i+1);
			printf("%d - Encender %s", instruc+1, led_rtu);
			fflush(stdout);
			instruc = instruc+2;
		}

		fgets(buffer,MSG_SIZE-1,stdin); // MSG_SIZE-1 because a null character is added
		// if(strncmp(buffer, "ENC", 3) == 0){
		// 	if(buffer[0] != '!'){
		// 		// send message to anyone out there...
		// 		n = sendto(sockfd, buffer, strlen(buffer), 0,
		// 				(const struct sockaddr *)&addr_broadcast, length);
		// 		if(n < 0)
		// 			error("Sendto");
		// 		memset(buffer, 0, MSG_SIZE);

		// 	}
		// }
		// // Función solo para pruebas ------------------------------------------------------------
		// else if(strncmp(buffer, "#", 1) == 0){
		// 	if(buffer[0] != '!'){
		// 		// send message to anyone out there...
		// 		n = sendto(sockfd, buffer, strlen(buffer), 0,
		// 				(const struct sockaddr *)&addr_broadcast, length);
		// 		if(n < 0)
		// 			error("Sendto");
		// 		memset(buffer, 0, MSG_SIZE);
		// 		}
		// }
		// // ---------------------------------------------------------------------------------------
		if(strncmp(buffer, "2", 1) == 0){
			printf("------HISTORIAL DE ACTUALIZACIONES------\n");
			fflush(stdout);
		}
		else if(strncmp(buffer, "3", 1) == 0){
			int i;
			if (rtus == 0)
				printf("No hay RTUS conectados\n");
			for(i = 0; i < rtus; i++){
				printf("RTU%d %s\n", i+1, RTUS[i]);
				fflush(stdout);
			}
		}
		else{
			int cmp;
			if(cmp = atoi(buffer)){
				int i, rtu=1;
				for(i = 4; i<100; i+=2){
					rtu++;
					//addr = (char *)(RTUS[cmp-i]);
					
					struct sockaddr_in addr1;
					addr1.sin_family = AF_INET;
					addr1.sin_port = htons(atoi(argv[1])); // Reemplaza "puerto_deseado" con el puerto correcto
					inet_pton(AF_INET, RTUS[rtu], &addr1.sin_addr); // Reemplaza "dirección_ip_deseada" con la IP correcta
					socklen_t length1 = sizeof(addr1);
					if((cmp-i) == 0){
						n = sendto(sockfd, "Encender LED1", 13, 0,(const struct sockaddr *)&addr1, length1);
						if(n < 0)
							error("Sendto");
					}
					else if((cmp-i) == 1){
						n = sendto(sockfd, "Encender LED2", 13, 0,(const struct sockaddr *)&addr1, length1);
						if(n < 0)
							error("Sendto");
					}
				}
			}
			else if(strncmp(buffer, "1", 1) != 0){
				printf("No ingresó un comando válido\n");
				fflush(stdout);
			}
		}

	} 

	close(sockfd);	// close socket.
	
	return 0;
}

void error(const char *msg){
	perror(msg);
	exit(0);
}

void Recibir(void *ptr){
	int n;
	char buffer[60];
	int ult_ip = 0;
	while(1){
		memset(buffer, 0, MSG_SIZE);
		// receive message
		n = recvfrom(sockfd, buffer, MSG_SIZE, 0, (struct sockaddr *)&addr, &length);
        //printf("%s\n", buffer);
		char ip_recibida[16];
		//char final_ip[3];
		char *token;
		int cmp;
		if(n < 0)
        	error("recvfrom");
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
			//strcpy(final_ip, token);
			//cmp = atoi(final_ip);
			sleep(3);
			sprintf(buffer, "RTU %d", rtus);
			n = sendto(sockfd, buffer, strlen(buffer), 0,
					  (const struct sockaddr *)&addr, length);
			if(n < 0)
				error("Sendto");
			printf("\nSe conectó un dispositivo con dirección ip: %s\n", RTUS[rtus-1]);
			fflush(stdout);
		}

	}

}

