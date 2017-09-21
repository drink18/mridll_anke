#include "matrix.h"
void row_swap(float *array_in, int row1, int row2)
{
	int n;
	float temp;
	for (n = 0; n <3; n++)
	{
		 temp= array_in[3 * (row2 - 1) + n];
		 array_in[3 * (row2 - 1) + n] = array_in[3 * (row1 - 1) + n];
		 array_in[3 * (row1 - 1) + n] = temp;
	}
}
void col_swap(float *array_in, int col1, int col2)
{
	int n;
	float temp;
	for (n = 0; n < 3; n++)
	{
		temp = array_in[3 * (n - 1) + col1];
		array_in[3 * (n - 1) + col1] = array_in[3 * (n - 1) + col2];
		array_in[3 * (n - 1) + col2] = temp;
	}
}
void   row_minus(float *array_in,int  row)
{
int n;
for (n = 0; n <3;n++)
array_in[3 * (row - 1) + n] = -array_in[3 * (row - 1) + n];

}

void col_minus(float *array_in, int  col)
{
	int n;
	for (n = 0; n <3;n++)
		array_in[3 * (n - 1) + col] = -array_in[3 * (n - 1) + col];
	
}
void Matrix::MatrxiCalc(int *T, float *R, int *P, float *G)
{
	int n, m;
	if (T[0] == 0)
	{
		 col_swap(R, 1, 2);
		if (T[1] < 0)
		  col_minus(R, 2);
		if( T[3] < 0)
		   col_minus(R, 1);
	}
	else
	{ 
		if (T[0]<0)
			 col_minus(R, 1);
	
		if (T[4]<0)
			 col_minus(R, 2);
	}
		

	
	if(P[0] == 0)
	{ 
		 row_swap(R, 1, 2);
	if(P[1]<0)
		 row_minus(R, 1);
	if(P[3]<0)
		row_minus(R, 2);
	}
	else
	{
		if(P[0] < 0)
			row_minus(R, 1);

		if(P[4] < 0)
			row_minus(R, 2);
	}

	if(P[8]<0)
		 row_minus(R, 3);	
	for (n = 1; n <= 3; n++)
		for (m = 0; m <3;m++)
			R[3 * (n - 1) + m] = G[n-1]*R[3 * (n - 1) + m];
}
