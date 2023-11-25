# Universidad del Valle de Guatemala
# Electrónica Digital 3
# Proyecto Final - Sistema SCADA Simplificado
# Gráfica de datos del ADC en tiempo real 
# Integrantes de grupo: Oscar Donis, Miguel Chacón y Luis Carranza
# Última actualización: 24/11/2023

import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation

# Función para leer los datos del archivo
def read_data():
    with open("datos.txt", "r") as file:            # Abre el archivo en modo lectura
        data = [int(line.strip()) for line in file] # Lee cada línea del archivo y la convierte a entero
    return data                                     # Retorna la lista de datos

# Función de animación para graficar en tiempo real
def animate(i):
    data = read_data()  # Lee los datos del archivo
    plt.cla() # Limpia el eje actual antes de dibujar
    plt.plot(data, label="Números") # Grafica los datos
    plt.xlabel("Iteración")
    plt.ylabel("Valor")
    plt.legend()

# Configura la animación
ani = FuncAnimation(plt.gcf(), animate, interval=10) # Llama a la función animate cada segundo (10 ms)
plt.show() # Muestra la animación
