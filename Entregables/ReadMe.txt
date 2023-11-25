Universidad del Valle de Guatemala
Electrónica Digital 3
Proyecto final
Sistema SCADA simplificado
Autores: Luis Carranza, Miguel Chacón & Oscar Donis

Descripción del proyecto:
El proyecto representa a un sistema SCADA simplificado, que consta de un historiador y distintos RTUs (según sea la necesidad) entre los cuales
se comparten información. Los RTUS son capaces de enviar actualizaciones periódicas al RTU sobre el estado de los botones, leds y lectura de ADC.
El RTU registra los siguientes eventos:
# evento
1 - Cambio en interruptor
2 - Se presionó un botón
3 - Se detectó una sobrecarga en la línea
4 - Se recibió un comando
5 - Evento IoT
6 - Actualización periódica de 2s
El historiador por su parte permite enviar comandos a través de un menú, donde se pueden verificar las actualizaciones, revisar el listado de 
RTUS conectados con su ip, generar un archivo log y encender/apagar LEDs de cada RTU. 

Cómo compilar los programas:
Se deben ingresar los siguientes comandos para compilar cada código 
Historiador.c ----> gcc Historiador.c -o Historiador -lpthread -lresolv
RTU.c ------------> gcc RTU.c -o RTU -lpthread -lwiringPi -lresolv
El programa de python no se necesita compilar y el del ESP32 solo se debe cargar al ESP32 que se utilizará luego de modificar la contraseña y 
el nombre de la red.

Cómo conectar el sistema:
1. Se deben compilar los programas
2. El primer programa a ejecutar es el Historiador.c y se puede hacer con el siguiente comando
./Historiador "#puerto" ------> en "#puerto" se coloca como argumento el puerto que se utilizará para la comunicación en la red local
3. Se debe ejecutar el programa de los RTU. Para ejecutarlo se utiliza el siguiente comando:
./RTU "ip del servidor" "#puerto" ---> "ip del servidor" se coloca la dirección IPv4 del servidor en la red local y el mismo puerto que para
el historiador. Se recomienda utilizar un número alto, como 2000
4. Alimentar el ESP32 y conectarse al servidor creado para realizar el encendido y apagado de LEDs desde el celular o un dispositivo móvil
5. Abrir la gráfica del ADC con el siguiente comando en la terminal
python Grafica_ADC.py 
6. Realizar las pruebas que se consideren necesarias del sistema

Notas: es importante considerar la nota descrita en el encabezado del código del Historiador en la que se indica lo que se debe realizar en 
caso de ejecutar el programa en un entorno Windows o bien Linux. También se debe considerar que si se desea conectar más de 10 RTUs se debe
cambiar el tamaño máximo de conexiones en el código del historiador. 
