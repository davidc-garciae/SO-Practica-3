// matrix_mul.go
// Práctica de multiplicación de matrices en Go
// Implementa dos versiones: secuencial y paralela (usando goroutines y canales como mecanismo de IPC)
// Autores: Cristian David Tamayo Espinosa / David Camilo García Echavarría
 					

package main

import (
	"bufio"
	"flag"
	"fmt"
	"os"
	"strconv"
	"strings"
	"sync"
	"time"
)

// readMatrix lee una matriz de un archivo de texto
func readMatrix(path string) ([][]float64, error) {
	file, err := os.Open(path)
	if err != nil {
		return nil, err
	}
	defer file.Close()

	scanner := bufio.NewScanner(file)
	var mat [][]float64
	for scanner.Scan() {
		line := strings.TrimSpace(scanner.Text())
		if line == "" {
			continue
		}
		parts := strings.Fields(line)
		row := make([]float64, len(parts))
		for i, p := range parts {
			v, err := strconv.ParseFloat(p, 64)
			if err != nil {
				return nil, err
			}
			row[i] = v
		}
		mat = append(mat, row)
	}
	if err := scanner.Err(); err != nil {
		return nil, err
	}
	return mat, nil
}

// writeMatrix escribe una matriz en un archivo de texto
func writeMatrix(path string, mat [][]float64) error {
	file, err := os.Create(path)
	if err != nil {
		return err
	}
	defer file.Close()

	bw := bufio.NewWriter(file)
	for _, row := range mat {
		for j, v := range row {
			// Se formatea con 6 decimales
			if j > 0 {
				bw.WriteString(" ")
			}
			bw.WriteString(fmt.Sprintf("%.15f", v))
		}
		bw.WriteString("\n")
	}
	return bw.Flush()
}

// multiplySequential realiza la multiplicación clásica A x B
func multiplySequential(A, B [][]float64) [][]float64 {
	N := len(A)
	M := len(B)
	P := len(B[0])
	C := make([][]float64, N)
	for i := 0; i < N; i++ {
		C[i] = make([]float64, P)
		for j := 0; j < P; j++ {
			var sum float64
			for k := 0; k < M; k++ {
				sum += A[i][k] * B[k][j]
			}
			C[i][j] = sum
		}
	}
	return C
}

// worker paralelo: lee índices de fila y calcula esas filas de C
func worker(A, B [][]float64, jobs <-chan int, results chan<- struct {
	row  int
	data []float64
}, wg *sync.WaitGroup) {
	defer wg.Done()
	P := len(B[0])
	M := len(B)
	for i := range jobs {
		// calcular la fila i
		r := make([]float64, P)
		for j := 0; j < P; j++ {
			var sum float64
			for k := 0; k < M; k++ {
				sum += A[i][k] * B[k][j]
			}
			r[j] = sum
		}
		results <- struct {
			row  int
			data []float64
		}{row: i, data: r}
	}
}

// multiplyParallel divide el trabajo entre 'workers' goroutines y usa canales
func multiplyParallel(A, B [][]float64, workers int) [][]float64 {
	N := len(A)
	P := len(B[0])
	// Matriz resultado inicializada vacía
	C := make([][]float64, N)
	for i := 0; i < N; i++ {
		C[i] = make([]float64, P)
	}

	jobs := make(chan int, N)
	results := make(chan struct {
		row  int
		data []float64
	}, N)
	var wg sync.WaitGroup

	// Lanzar goroutines
	for w := 0; w < workers; w++ {
		wg.Add(1)
		go worker(A, B, jobs, results, &wg)
	}

	// Enviar trabajos
	for i := 0; i < N; i++ {
		jobs <- i
	}
	close(jobs)

	// Esperar a que terminen los workers en segundo plano
	go func() {
		wg.Wait()
		close(results)
	}()

	// Recoger resultados
	for res := range results {
		C[res.row] = res.data
	}
	return C
}

func main() {
	// Flags de ejecución
	numProc := flag.Int("p", 4, "Número de goroutines para versión paralela")
	fileA := flag.String("a", "A.txt", "Archivo de entrada matriz A")
	fileB := flag.String("b", "B.txt", "Archivo de entrada matriz B")
	flag.Parse()

	// Leer matrices
	A, err := readMatrix(*fileA)
	if err != nil {
		fmt.Fprintf(os.Stderr, "Error leyendo A: %v\n", err)
		os.Exit(1)
	}
	B, err := readMatrix(*fileB)
	if err != nil {
		fmt.Fprintf(os.Stderr, "Error leyendo B: %v\n", err)
		os.Exit(1)
	}
	// Verificar dimensiones
	if len(A) == 0 || len(B) == 0 || len(A[0]) != len(B) {
		fmt.Fprintln(os.Stderr, "Dimensiones incompatibles para multiplicar")
		os.Exit(1)
	}

	// Versión secuencial
	start := time.Now()
	Cseq := multiplySequential(A, B)
	selapsed := time.Since(start)
	fmt.Printf("Tiempo secuencial: %.6f segundos\n", selapsed.Seconds())
	// Guardar resultado secuencial
	writeMatrix("C_out_secuencial.txt", Cseq)

	// Versión paralela
	start = time.Now()
	Cpar := multiplyParallel(A, B, *numProc)
	pelapsed := time.Since(start)
	fmt.Printf("Tiempo paralelo (%d goroutines): %.6f segundos\n", *numProc, pelapsed.Seconds())
	writeMatrix("C_out_paralelo.txt", Cpar)

	// Speedup
	if pelapsed > 0 {
		fmt.Printf("Speedup: %.2fx\n", selapsed.Seconds()/pelapsed.Seconds())
	}
}
