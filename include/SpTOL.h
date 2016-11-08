/*
    This file is part of SpTOL.

    SpTOL is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as
    published by the Free Software Foundation, either version 3 of
    the License, or (at your option) any later version.

    SpTOL is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with SpTOL.
    If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef SPTOL_H_INCLUDED
#define SPTOL_H_INCLUDED

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include <omp.h>


#ifdef __cplusplus
extern "C" {
#endif

/**
 * Define sptScalar as 64-bit float
 * (or 32-bit float, if an old GPU is not good at 64-bit arithmetic
 */
// typedef double sptScalar;
typedef float sptScalar;

/**
 * Dense dynamic array of scalars
 */
typedef struct {
    size_t    len;   /// length
    size_t    cap;   /// capacity
    sptScalar *data; /// data
} sptVector;

/**
 * Dense dynamic array of size_t's
 */
typedef struct {
    size_t len;   /// length
    size_t cap;   /// capacity
    size_t *data; /// data
} sptSizeVector;

/**
 * Dense matrix type
 */
typedef struct {
    size_t    nrows;   /// # rows
    size_t    ncols;   /// # columns
    size_t    cap;     /// # of allocated rows
    size_t    stride;  /// ncols rounded up to 8
    sptScalar *values; /// values, length cap*stride
} sptMatrix;

/**
 * Sparse matrix type, COO format
 */
typedef struct {
    size_t        nrows;  /// # rows
    size_t        ncols;  /// # colums
    size_t        nnz;    /// # non-zeros
    sptSizeVector rowind; /// row indices, length nnz
    sptSizeVector colind; /// column indices, length nnz
    sptVector     values; /// non-zero values, length nnz
} sptSparseMatrix;

/**
 * Sparse tensor type
 */
typedef struct {
    size_t        nmodes;  /// # modes
    size_t        sortkey; /// the least significant mode during sort
    size_t        *ndims;  /// size of each mode, length nmodes
    size_t        nnz;     /// # non-zeros
    sptSizeVector *inds;   /// indices of each element, length [nmodes][nnz]
    sptVector     values;  /// non-zero values, length nnz
} sptSparseTensor;

/**
 * Semi-sparse tensor type
 * The chosen mode is dense, while other modes are sparse.
 * Can be considered as "sparse tensor of dense fiber".
 * The "fiber" here can be defined as a vector of elements that have indices
 * only different in the last mode.
 */
typedef struct {
    size_t        nmodes; /// # Modes, must >= 2
    size_t        *ndims; /// size of each mode, length nmodes
    size_t        mode;   /// the mode where data is stored in dense format
    size_t        nnz;    /// # non-zero fibers
    sptSizeVector *inds;  /// indices of each dense fiber, length [nmodes][nnz], the mode-th value is ignored
    size_t        stride; /// ndims[mode] rounded up to 8
    sptMatrix     values; /// dense fibers, size nnz*ndims[mode]
} sptSemiSparseTensor;

/**
 * An opaque data type to store a specific time point, using either CPU or GPU clock.
 */
typedef struct sptTagTimer *sptTimer;

typedef enum {
    SPTERR_NO_ERROR       = 0,
    SPTERR_UNKNOWN        = 1,
    SPTERR_SHAPE_MISMATCH = 2,
    SPTERR_VALUE_ERROR    = 3,
    SPTERR_ZERO_DIVISION  = 4,
    SPTERR_OS_ERROR       = 0x10000,
    SPTERR_CUDA_ERROR     = 0x20000,
} SptError;

int sptGetLastError(const char **module, const char **file, unsigned *line, const char **reason);
void sptClearLastError(void);

/* Helper function for pure C module */
int sptCudaSetDevice(int device);
int sptCudaGetLastError(void);

/* Timer functions, using either CPU or GPU timer */
int sptNewTimer(sptTimer *timer, int use_cuda);
int sptStartTimer(sptTimer timer);
int sptStopTimer(sptTimer timer);
double sptElapsedTime(const sptTimer timer);
double sptPrintElapsedTime(const sptTimer timer, const char *name);
int sptFreeTimer(sptTimer timer);

/* Dense vector, aka variable length array */
int sptNewVector(sptVector *vec, size_t len, size_t cap);
int sptConstantVector(sptVector * const vec, sptScalar const val);
int sptCopyVector(sptVector *dest, const sptVector *src);
int sptAppendVector(sptVector *vec, sptScalar value);
int sptAppendVectorWithVector(sptVector *vec, const sptVector *append_vec);
int sptResizeVector(sptVector *vec, size_t size);
void sptFreeVector(sptVector *vec);
int sptDumpVector(sptVector *vec, FILE *fp);

/* Dense vector, with size_t type */
int sptNewSizeVector(sptSizeVector *vec, size_t len, size_t cap);
int sptConstantSizeVector(sptSizeVector * const vec, size_t const num);
int sptCopySizeVector(sptSizeVector *dest, const sptSizeVector *src);
int sptAppendSizeVector(sptSizeVector *vec, size_t value);
int sptAppendSizeVectorWithVector(sptSizeVector *vec, const sptSizeVector *append_vec);
int sptResizeSizeVector(sptSizeVector *vec, size_t value);
void sptFreeSizeVector(sptSizeVector *vec);
int sptDumpSizeVector(sptSizeVector *vec, FILE *fp);

/* Dense matrix */
int sptNewMatrix(sptMatrix *mtx, size_t nrows, size_t ncols);
int sptRandomizeMatrix(sptMatrix *mtx, size_t nrows, size_t ncols);
int sptConstantMatrix(sptMatrix * const mtx, sptScalar const val);
int sptCopyMatrix(sptMatrix *dest, const sptMatrix *src);
void sptFreeMatrix(sptMatrix *mtx);
int sptAppendMatrix(sptMatrix *mtx, const sptScalar values[]);
int sptResizeMatrix(sptMatrix *mtx, size_t newsize);
int sptSparseTensorToMatrix(sptMatrix *dest, const sptSparseTensor *src);
int sptDumpMatrix(sptMatrix *mtx, FILE *fp);

/* Sparse matrix */
int sptNewSparseMatrix(sptSparseMatrix *mtx, size_t nrows, size_t ncols);
int sptCopySparseMatrix(sptSparseMatrix *dest, const sptSparseMatrix *src);
void sptFreeSparseMatrix(sptSparseMatrix *mtx);

/* Sparse tensor */
int sptNewSparseTensor(sptSparseTensor *tsr, size_t nmodes, const size_t ndims[]);
int sptCopySparseTensor(sptSparseTensor *dest, const sptSparseTensor *src);
void sptFreeSparseTensor(sptSparseTensor *tsr);
int sptLoadSparseTensor(sptSparseTensor *tsr, size_t start_index, FILE *fp);
int sptDumpSparseTensor(const sptSparseTensor *tsr, size_t start_index, FILE *fp);
void sptSparseTensorSortIndex(sptSparseTensor *tsr);
void sptSparseTensorSortIndexAtMode(sptSparseTensor *tsr, size_t mode);
/**
 * epsilon is a small positive value, every -epsilon < x < x would be considered as zero
 */
int sptSemiSparseTensorToSparseTensor(sptSparseTensor *dest, const sptSemiSparseTensor *src, sptScalar epsilon);

int sptNewSemiSparseTensor(sptSemiSparseTensor *tsr, size_t nmodes, size_t mode, const size_t ndims[]);
int sptCopySemiSparseTensor(sptSemiSparseTensor *dest, const sptSemiSparseTensor *src);
void sptFreeSemiSparseTensor(sptSemiSparseTensor *tsr);
int sptSparseTensorToSemiSparseTensor(sptSemiSparseTensor *dest, const sptSparseTensor *src, size_t mode);
int sptSemiSparseTensorSortIndex(sptSemiSparseTensor *tsr);
/**
 * Set indices of a semi-sparse according to a reference sparse
 * Call sptSparseTensorSortIndexAtMode on ref first
 */
int sptSemiSparseTensorSetIndices(sptSemiSparseTensor *dest, sptSizeVector *fiberidx, sptSparseTensor *ref);

/* Sparse tensor unary operations */
int sptSparseTensorMulScalar(sptSparseTensor *X, sptScalar a);
int sptSparseTensorDivScalar(sptSparseTensor *X, sptScalar a);
/* Sparse tensor binary operations */
int sptSparseTensorAdd(sptSparseTensor *Z, const sptSparseTensor *X, const sptSparseTensor *Y);
int sptSparseTensorSub(sptSparseTensor *Z, const sptSparseTensor *X, const sptSparseTensor *Y);
int sptSparseTensorDotMul(sptSparseTensor *Z, const sptSparseTensor *X, const sptSparseTensor *Y);
int sptSparseTensorDotMulEq(sptSparseTensor *Z, const sptSparseTensor *X, const sptSparseTensor *Y);
int sptOmpSparseTensorDotMulEq(sptSparseTensor *Z, const sptSparseTensor *X, const sptSparseTensor *Y);
int sptCudaSparseTensorDotMulEq(sptSparseTensor *Z, const sptSparseTensor *X, const sptSparseTensor *Y);
int sptSparseTensorDotDiv(sptSparseTensor *Z, const sptSparseTensor *X, const sptSparseTensor *Y);

/**
 * Sparse tensor times a dense matrix (TTM)
 * Input: sparse tensor X[I][J][K], dense matrix U[I][R}, mode n={0, 1, 2}
 * Output: sparse tensor Y[I][J][R] (e.g. n=2)
 */
int sptSparseTensorMulMatrix(sptSemiSparseTensor *Y, sptSparseTensor *X, const sptMatrix *U, size_t mode);
int sptOmpSparseTensorMulMatrix(sptSemiSparseTensor *Y, sptSparseTensor *X, const sptMatrix *U, size_t mode);
int sptCudaSparseTensorMulMatrix(sptSemiSparseTensor *Y, sptSparseTensor *X, const sptMatrix *U, size_t mode);
int sptCudaSparseTensorMulMatrixNew(sptSemiSparseTensor *Y, sptSparseTensor *X, const sptMatrix *U, size_t mode);

/**
 * Kronecker product
 */
int sptSparseTensorKroneckerMul(sptSparseTensor *Y, const sptSparseTensor *A, const sptSparseTensor *B);

/**
 * Khatri-Rao product
 */
int sptSparseTensorKhatriRaoMul(sptSparseTensor *Y, const sptSparseTensor *A, const sptSparseTensor *B);

/* OMP functions */
int sptSparseTensorAddOMP(sptSparseTensor *Y, sptSparseTensor *X, size_t const nthreads);
int sptSparseTensorSubOMP(sptSparseTensor *Y, sptSparseTensor *X, size_t const nthreads);

#ifdef __cplusplus
}
#endif

#endif
