#include <stdio.h>
#include <string.h>
#include <omp.h>

#define N (2048)

float sum;
float a[N][N];
float b[N][N];
float c[N][N];

void matmul()
{
	int	i, j, k;

  #pragma omp parallel for private(i,j,k) shared(a,b,c)
	for (i = 0; i < N; i += 1) {
		for (j = 0; j < N; j += 1) {
			a[i][j] = 0;
			for (k = 0; k < N; k += 1) {
				a[i][j] += b[i][k] * c[j][k];
			}
		}
	}
}

void init()
{
	int	i, j;

  //#pragma omp parallel for private(i,j) shared(b,c)
	for (i = 0; i < N; i += 1) {
		for (j = 0; j < N; j += 1) {
			b[i][j] = 12 + i * j * 13;
			c[j][i] = -13 + i + j * 21;
		}
	}
}

void check()
{
	int	i, j;

  //#pragma omp parallel for private(i,j) shared(a,sum)
	for (i = 0; i < N; i += 1)
		for (j = 0; j < N; j += 1)
			sum += a[i][j];
	printf("sum = %lf\n", sum);
}

int main()
{
  printf("init\n");
	init();
  printf("matmul\n");
	matmul();
  printf("check\n");
	check();

	return 0;
}
