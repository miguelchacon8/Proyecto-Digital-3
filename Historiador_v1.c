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

#define MSG_SIZE 60		// Tamaño (máximo) del mensaje. Puede ser mayor a 40.
unsigned int length;
struct sockaddr_in server, addr, addr_broadcast;
	int sockfd, n;

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

	//obteniendo la ip de mi pc y guardandola. 
    char hostname[256];
    char ip[256], ip1[20], ip2[20];
    struct hostent *host;
    struct in_addr **addr_list;

    if (gethostname(hostname, sizeof(hostname)) == 0) {
        host = gethostbyname(hostname);
        if (host != NULL) {
            addr_list = (struct in_addr **)host->h_addr_list;

            if (addr_list[0] != NULL) {
                strcpy(ip1, inet_ntoa(*addr_list[0]));
                printf("IP1: %s\n", ip1);
			}
			if (addr_list[1] != NULL) {
                strcpy(ip2, inet_ntoa(*addr_list[1]));
                printf("IP2: %s\n", ip2);
            } 
			else {
                printf("No se encontraron direcciones IP para el host.\n");
				fflush(stdout);
            }
        } else {
            printf("No se pudo obtener la información del host.\n");
			fflush(stdout);
        }
    } else {
        printf("No se pudo obtener el nombre del host.\n");
		fflush(stdout);
    }

	if(strncmp(ip1, "192.168.3", 9) == 0){
		strcpy(ip, ip1);
	}
	else if(strncmp(ip2, "192.168.3", 9) == 0){
		strcpy(ip, ip2);
	}


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
	printf("Mi dirección ip es: %s\nLa dirección de broadcast es %s*\n", my_ip, broadcast_ip);
	fflush(stdout);

	sockfd = socket(AF_INET, SOCK_DGRAM, 0); // Creates socket. Connectionless.
	if(sockfd < 0)
		error("Error: socket");

	// Estructura de datos para la comunicación del servidor con el cliente
	server.sin_family = AF_INET;				// symbol constant for Internet domain
	server.sin_port = htons(atoi(argv[1]));		// Número de puerto
	server.sin_addr.s_addr = htonl(INADDR_ANY);	// para recibir de cualquier interfaz de red
	
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

	while(1){
		memset(buffer, 0, MSG_SIZE);	// sets all values to zero
		printf("Ingrese un mensaje para enviar: ");
		fflush(stdout);
		fgets(buffer,MSG_SIZE-1,stdin); // MSG_SIZE-1 because a null character is added

		if(buffer[0] != '!'){
			// send message to anyone out there...
			n = sendto(sockfd, buffer, strlen(buffer), 0,
					  (const struct sockaddr *)&addr_broadcast, length);
			if(n < 0)
				error("Sendto");
            memset(buffer, 0, MSG_SIZE);

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
	char buffer[MSG_SIZE];
	while(1){
		memset(buffer, 0, MSG_SIZE);
		// receive message
		n = recvfrom(sockfd, buffer, MSG_SIZE, 0, (struct sockaddr *)&addr, &length);
		if(n < 0)
			error("recvfrom");
		printf("\nRecibí algo: %s\n", buffer);
		fflush(stdout);
		printf("Ingrese un mensaje para enviar: ");
		fflush(stdout);

	}

}