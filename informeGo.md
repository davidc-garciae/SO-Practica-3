# Informe Codigo en Go

## 1. Introducción

En esta práctica se implementó la multiplicación de matrices de gran tamaño en Go, con dos aproximaciones:

Secuencial: ejecución en un solo hilo.

Paralela: división de filas entre varias goroutines y uso de canales para comunicación (IPC).

El objetivo es comparar el rendimiento de ambas versiones y analizar el speedup obtenido al aumentar el número de goroutines.

## 2. Descripción de la solución

### 2.1. Lectura y escritura de matrices

Lectura línea a línea con bufio.Scanner.

Conversión a float64 y almacenamiento en [][]float64.

Escritura con bufio.Writer, formateando números.

### 2.2. Implementación secuencial

Tres bucles anidados en multiplySequential.

Medición de tiempo con time.Now() y time.Since().

### 2.3. Implementación paralela

Canal jobs para índices de fila.

Varias goroutines worker consumen tareas del canal jobs y envían los resultados a través del canal results.

Ensamble de filas en la matriz resultado.

### 2.4. Elección de IPC

Se usan canales (chan) de Go porque son seguros, sencillos y la forma idiomática de coordinación entre goroutines.

## 3. Resultados y análisis de rendimiento

Para las matrices (A.txt, B.txt), los tiempos fueron los siguientes:

| Goroutines | Tiempo secuencial (s) | Tiempo paralelo (s) | Speedup |
|------------|-----------------------|----------------------|---------|
| 1          | 0.000009              | 0.000212             | 0.04×   |
| 2          | 0.000014              | 0.000127             | 0.11×   |
| 4          | 0.000010              | 0.000171             | 0.06×   |
| 8          | 0.000008              | 0.002193             | 0.00×   |

Nota: Al usar matrices pequeñas el overhead de crear y sincronizar goroutines domina el tiempo, por eso el speedup es <1.

Generamos una grafica en python con los valores:

```python
import matplotlib.pyplot as plt

# Datos reales obtenidos
processes = [1, 2, 4, 8]
speedup   = [0.04, 0.11, 0.06, 0.00]

plt.plot(processes, speedup, marker='o')
plt.xlabel('Número de goroutines')
plt.ylabel('Speedup')
plt.title('Speedup vs Número de goroutines (A pequeña)')
plt.grid(True)
plt.savefig('output_go.png')
plt.show()
```

![image](https://github.com/user-attachments/assets/71f4ba85-9efc-499e-b0bf-09eff3ada1e4)


### Comparación de archivos de salida

Para verificar la corrección de la implementación, se compararon los archivos de salida generados por las versiones secuencial y paralela con el archivo de referencia (`C_small.txt`) usando el comando `diff`:

```sh
diff C.txt C_out_secuencial.txt
diff C_small.txt C_out_paralelo.txt
```

En ambos casos, se observó que los resultados dieron iguales en la mayor parte de los decimales, con alguna variacion en algun decimal final:

**Ejemplo de salida:**

```
1,12c1,12
< 4.693413018545468 5.561372870702934 ...
---
> 4.693413018545468 5.561372870702935 ...
```

### Conclusiones
-La implementación devuelve las  cifras con gran exactitud, incluso iguales decimales en muchos casos, por lo que el algoritmo está correctamente codificado.

-Con matrices pequeñas, el coste de lanzar y coordinar goroutines y canales supera la propia multiplicación, resultando en speedup menores que 1. Para notar ventaja, es esencial trabajar con matrices de gran dimensión.

-El modelo de concurrencia de Go (goroutines + canales) es muy simple y seguro, pero cada sincronización tiene un coste. Ajustar el número de goroutines al tamaño de la máquina (núcleos disponibles) y al volumen de datos es clave para maximizar el rendimiento.

-En problemas de gran escala (matrices grandes), separar las filas en varias goroutines puede reducir significativamente el tiempo total de cómputo, mostrando la utilidad del paralelismo en Go.

##  Referencias

- OSTEP: Processes API Chapter.
- Man pages de fork, shmget, shmat.
