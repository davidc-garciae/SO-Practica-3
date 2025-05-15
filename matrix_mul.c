// matrix_mul.c
// Práctica 3 SO: Multiplicación de matrices (secuencial y paralela)
// Lectura y escritura de matrices desde archivos de texto
// Integrantes: Cristian David Tamayo Espinosa / David Camilo García Echavarría

#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

// Lee una matriz de un archivo de texto
// Devuelve un puntero doble a la matriz y asigna filas y columnas
// El archivo debe tener una fila por línea, valores separados por espacio
float **leer_matriz(const char *filename, int *filas, int *columnas);

// Escribe una matriz en un archivo de texto
void escribir_matriz(const char *filename, float **matriz, int filas, int columnas);

// Libera la memoria de una matriz
void liberar_matriz(float **matriz, int filas);

// Multiplica dos matrices (A de tamaño filasA x colsA, B de tamaño colsA x colsB)
// Devuelve la matriz resultado (filasA x colsB)
float **multiplicar_matrices(float **A, int filasA, int colsA, float **B, int filasB, int colsB);

int main(int argc, char *argv[])
{
    // Leer número de procesos desde argumento
    int num_procesos = 4; // valor por defecto
    if (argc > 1)
    {
        num_procesos = atoi(argv[1]);
        if (num_procesos < 1)
            num_procesos = 1;
    }
    printf("Usando %d procesos para la multiplicación paralela.\n", num_procesos);

    // Leer matrices A y B
    int filasA, columnasA, filasB, columnasB;
    float **A = leer_matriz("A_small.txt", &filasA, &columnasA);
    float **B = leer_matriz("B_small.txt", &filasB, &columnasB);
    if (!A || !B)
    {
        fprintf(stderr, "Error leyendo matrices A o B\n");
        if (A)
            liberar_matriz(A, filasA);
        if (B)
            liberar_matriz(B, filasB);
        return 1;
    }
    if (columnasA != filasB)
    {
        fprintf(stderr, "Dimensiones incompatibles para multiplicación: A es %dx%d, B es %dx%d\n", filasA, columnasA, filasB, columnasB);
        liberar_matriz(A, filasA);
        liberar_matriz(B, filasB);
        return 1;
    }

    // --- Medición de tiempo para la versión paralela ---
    struct timespec t_ini, t_fin;
    clock_gettime(CLOCK_MONOTONIC, &t_ini);

    // --- Paso 1: Preparar memoria compartida para la matriz resultado C ---
    int shmid = shmget(IPC_PRIVATE, sizeof(float) * filasA * columnasB, IPC_CREAT | 0666);
    if (shmid < 0)
    {
        perror("shmget");
        liberar_matriz(A, filasA);
        liberar_matriz(B, filasB);
        return 1;
    }
    float *C_shm = (float *)shmat(shmid, NULL, 0);
    if (C_shm == (void *)-1)
    {
        perror("shmat");
        shmctl(shmid, IPC_RMID, NULL);
        liberar_matriz(A, filasA);
        liberar_matriz(B, filasB);
        return 1;
    }
    for (int i = 0; i < filasA * columnasB; i++)
        C_shm[i] = 0.0f;

    // --- Paso 2: Crear procesos hijos y dividir el trabajo ---
    int filas_por_proceso = filasA / num_procesos;
    int resto = filasA % num_procesos;
    pid_t *pids = (pid_t *)malloc(num_procesos * sizeof(pid_t));
    for (int p = 0; p < num_procesos; p++)
    {
        int start = p * filas_por_proceso + (p < resto ? p : resto);
        int end = start + filas_por_proceso + (p < resto ? 1 : 0);
        pid_t pid = fork();
        if (pid == 0)
        {
            // Proceso hijo: calcular filas [start, end)
            for (int i = start; i < end; i++)
            {
                for (int j = 0; j < columnasB; j++)
                {
                    float suma = 0.0f;
                    for (int k = 0; k < columnasA; k++)
                    {
                        suma += A[i][k] * B[k][j];
                    }
                    C_shm[i * columnasB + j] = suma;
                }
            }
            liberar_matriz(A, filasA);
            liberar_matriz(B, filasB);
            shmdt(C_shm);
            exit(0);
        }
        else if (pid > 0)
        {
            pids[p] = pid;
        }
        else
        {
            perror("fork");
            for (int q = 0; q < p; q++)
                waitpid(pids[q], NULL, 0);
            shmdt(C_shm);
            shmctl(shmid, IPC_RMID, NULL);
            liberar_matriz(A, filasA);
            liberar_matriz(B, filasB);
            free(pids);
            return 1;
        }
    }
    for (int p = 0; p < num_procesos; p++)
    {
        waitpid(pids[p], NULL, 0);
    }
    free(pids);

    // --- Medición de tiempo fin y cálculo ---
    clock_gettime(CLOCK_MONOTONIC, &t_fin);
    double tiempo_paralelo = (t_fin.tv_sec - t_ini.tv_sec) + (t_fin.tv_nsec - t_ini.tv_nsec) / 1e9;
    printf("Tiempo de ejecución (paralelo, %d procesos): %.6f segundos\n", num_procesos, tiempo_paralelo);

    // --- Guardar resultado en archivo ---
    float **C_ptrs = (float **)malloc(filasA * sizeof(float *));
    for (int i = 0; i < filasA; i++)
        C_ptrs[i] = &C_shm[i * columnasB];
    escribir_matriz("C_out_paralelo.txt", C_ptrs, filasA, columnasB);
    free(C_ptrs);

    // --- (Opcional) Medición de tiempo para la versión secuencial ---
    clock_gettime(CLOCK_MONOTONIC, &t_ini);
    float **C_seq = multiplicar_matrices(A, filasA, columnasA, B, filasB, columnasB);
    clock_gettime(CLOCK_MONOTONIC, &t_fin);
    double tiempo_secuencial = (t_fin.tv_sec - t_ini.tv_sec) + (t_fin.tv_nsec - t_ini.tv_nsec) / 1e9;
    printf("Tiempo de ejecución (secuencial): %.6f segundos\n", tiempo_secuencial);
    escribir_matriz("C_out_secuencial.txt", C_seq, filasA, columnasB);
    liberar_matriz(C_seq, filasA);

    // --- Mostrar speedup ---
    if (tiempo_secuencial > 0.0)
        printf("Speedup: %.2fx\n", tiempo_secuencial / tiempo_paralelo);

    // Liberar recursos
    shmdt(C_shm);
    shmctl(shmid, IPC_RMID, NULL);
    liberar_matriz(A, filasA);
    liberar_matriz(B, filasB);
    return 0;
}

float **leer_matriz(const char *filename, int *filas, int *columnas)
{
    FILE *f = fopen(filename, "r");
    if (!f)
        return NULL;
    // Primero, contar filas y columnas
    char buffer[4096];
    int cols = 0, rows = 0;
    while (fgets(buffer, sizeof(buffer), f))
    {
        if (rows == 0)
        {
            // Contar columnas en la primera fila
            char *p = buffer;
            while (*p)
            {
                while (*p == ' ' || *p == '\t')
                    p++;
                if ((*p >= '0' && *p <= '9') || *p == '-' || *p == '+')
                {
                    cols++;
                    while ((*p && *p != ' ' && *p != '\t' && *p != '\n'))
                        p++;
                }
                else if (*p)
                {
                    p++;
                }
            }
        }
        rows++;
    }
    rewind(f);
    float **matriz = (float **)malloc(rows * sizeof(float *));
    for (int i = 0; i < rows; i++)
    {
        matriz[i] = (float *)malloc(cols * sizeof(float));
        for (int j = 0; j < cols; j++)
        {
            fscanf(f, "%f", &matriz[i][j]);
        }
    }
    fclose(f);
    *filas = rows;
    *columnas = cols;
    return matriz;
}

void escribir_matriz(const char *filename, float **matriz, int filas, int columnas)
{
    FILE *f = fopen(filename, "w");
    if (!f)
        return;
    for (int i = 0; i < filas; i++)
    {
        for (int j = 0; j < columnas; j++)
        {
            fprintf(f, "%.15f%c", matriz[i][j], (j == columnas - 1) ? '\n' : ' ');
        }
    }
    fclose(f);
}

void liberar_matriz(float **matriz, int filas)
{
    for (int i = 0; i < filas; i++)
    {
        free(matriz[i]);
    }
    free(matriz);
}

float **multiplicar_matrices(float **A, int filasA, int colsA, float **B, int filasB, int colsB)
{
    float **C = (float **)malloc(filasA * sizeof(float *));
    for (int i = 0; i < filasA; i++)
    {
        C[i] = (float *)calloc(colsB, sizeof(float));
        for (int j = 0; j < colsB; j++)
        {
            for (int k = 0; k < colsA; k++)
            {
                C[i][j] += A[i][k] * B[k][j];
            }
        }
    }
    return C;
}
