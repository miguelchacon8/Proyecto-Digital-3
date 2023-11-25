Universidad del Valle de Guatemala
Electrónica Digital 3
Proyecto final
Sistema SCADA simplificado
Autores: Luis Carranza, Miguel Chacón & Oscar Donis
Última actualización: 24/11/2023

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
El formato de las actualizaciones que envía el RTU es el siguiente:
#RTU #EVENTO HORA PUSH1 PUSH2 DIP1 DIP2 LED1 LED2 ADC
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

Conexiones físicas para el RTU
El RTU debe conectarse utilizando el pinout estándar GPIO, de modo que las conexiones de los pines son los siguientes:
Botones: 
GPIO 21 -> botón 1
GPIO 20 -> botón 2
Interruptores
GPIO 24 -> Dip 1
GPIO 25 -> Dip 2
Control de la alarma:
GPIO 22 -> Este iría conectado a la base del transistor o mosfet que genera el pulso para generar la alarma con la bocina. 
LEDS
GPIO 26 -> LED1
GPIO 19 -> LED2
Interrupciones para LEDS -> estos pines irían conectados a los pines GPIO16 y GPIO17 del ESP32 que sirven para activar/desactivar LEDs con IoT
GPIO 17 -> Interrupción LED1
GPIO 27 -> Interrupción LED2
Canal SPI para la lectura ADC:
SPICE0 se conecta a CS del MCP3002 (pin 1)
SPIMOSI -> Din del MCP3002 (pin 5)
SPIMISO -> Dout del MCP3002 (pin 6)
SPISCLK -> CLK del MCP3002 (pin 7)
- Importante considerar que el módulo ADC se debe conectar a 3.3V y Vss en sus pines 8 y 4 respectivamente.
- CH0 del MCP3002 se conecta al potenciómetro 
- Los LED se deben conectar con su respectiva resistencia a tierra entre 100 y 330 ohms
- Los pushbuttons se deben conectar en configuración pull-down para que cuando se presione envíe un 1 a la Raspberry
- Los DIP switches no importa su conexión ya que este se activa en el cambio. Se hicieron las pruebas con conexión pull-down pero no
es relevante siempre que el usuario entienda cuándo es 1 y 0.
Conexiones físicas para el ESP32
Para la comunicación con el RTU es ESP32 Debe de utilizar:
GPIO 16 -> LED1
GPIO 17 -> LED2
