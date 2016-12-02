/*
    This file is part of ParTI!.

    ParTI! is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as
    published by the Free Software Foundation, either version 3 of
    the License, or (at your option) any later version.

    ParTI! is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with ParTI!.
    If not, see <http://www.gnu.org/licenses/>.
*/

#include <ParTI.h>
#include <assert.h>
#include <math.h>
#include <cblas.h>
#include <lapacke.h>
#include "sptensor.h"
#include <omp.h>


double OmpCpdAlsStep(
  sptSparseTensor const * const spten,
  size_t const rank,
  size_t const niters,
  double const tol,
  sptMatrix ** mats,
  sptScalar * const lambda)
{
  size_t const nmodes = spten->nmodes;
  size_t const nnz = spten->nnz;
  size_t const stride = mats[0]->stride;
  int nthreads, tid;

  #pragma omp parallel 
  {
    nthreads = omp_get_num_threads();
    tid = omp_get_thread_num();
    // printf("T%d of %d threads\n", tid, nthreads);
  }

  sptMatrix * tmp_mat = mats[nmodes];
  sptMatrix ** ata = (sptMatrix **)malloc((nmodes+1) * sizeof(*ata));
  for(size_t m=0; m < nmodes+1; ++m) {
    ata[m] = (sptMatrix *)malloc(sizeof(sptMatrix));
  }

  #pragma omp parallel for
  for(size_t m=0; m < nmodes; ++m) {
    sptScalar * mats_values = mats[m]->values;
    sptNewMatrix(ata[m], rank, rank);
     /* The same storage order with mats. */
    /* sptMatrixTransposeMultiply(mats[m], ata[m]); */
    cblas_sgemm(CblasRowMajor, CblasTrans, CblasNoTrans, rank, rank, mats[m]->nrows, 1.0, 
      mats_values, mats[m]->stride, mats_values, mats[m]->stride, 0.0, ata[m]->values, ata[m]->stride);
  }
  sptNewMatrix(ata[nmodes], rank, rank);
  // printf("Initial ata:\n");
  // for(size_t m=0; m < nmodes+1; ++m)
  //   sptDumpMatrix(ata[m], stdout);

  double oldfit = 0;
  double fit = 0;

  // timer_reset(&g_timers[TIMER_ATA]);
  // Timer itertime;
  // Timer * modetime = (Timer*)malloc(nmodes*sizeof(Timer));
  
  /* For MttkrpHyperTensor with size rank. */
  sptVector scratch;
  sptNewVector (&scratch, nnz * stride, nnz * stride);
  size_t nmats = nmodes - 1;
  sptSizeVector mats_order;
  sptNewSizeVector(&mats_order, nmats, nmats);

  for(size_t it=0; it < niters; ++it) {
    // timer_fstart(&itertime);
    for(size_t m=0; m < nmodes; ++m) {
      // printf("\nmode %lu \n", m);

      assert(spten->ndims[m] == mats[m]->nrows);
      tmp_mat->nrows = mats[m]->nrows;

      /* Factor Matrices order */
      size_t j = 0;
      for (int i=nmodes-1; i>=0; --i) {
        if (i != m) {
          mats_order.data[j] = i;
          ++ j;
        }
      }
      assert (j == nmats);
      // sptDumpSizeVector(&mats_order, stdout);

      assert (sptOmpMTTKRP(spten, mats, &mats_order, m, &scratch) == 0);
      // printf("sptMTTKRP:\n");
      // sptDumpMatrix(mats[nmodes], stdout);

      sptOmpMatrixDotMulSeq(m, nmodes, ata);
      // printf("sptMatrixDotMulSeq:\n");
      // sptDumpMatrix(ata[nmodes], stdout);

      /* mat_syminv(ata[nmodes]); */
      int * ipiv = (int*)malloc(rank * sizeof(int));
      sptMatrix * unitMat = (sptMatrix*)malloc(sizeof(sptMatrix));
      sptIdentityMatrix(unitMat, rank, rank);
      LAPACKE_sgesv(LAPACK_ROW_MAJOR, rank, rank, ata[nmodes]->values, ata[nmodes]->stride, ipiv, unitMat->values, unitMat->stride);
      free(ipiv);
      sptFreeMatrix(unitMat);
      free(unitMat);
      // printf("Inverse ata[nmodes]:\n");
      // sptDumpMatrix(ata[nmodes], stdout);

      memset(mats[m]->values, 0, mats[m]->nrows * mats[m]->stride * sizeof(sptScalar));
      /* sptMatrixMultiply(tmp_mat, ata[nmodes], mats[m]); */
      cblas_sgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans, 
        tmp_mat->nrows, rank, mats[m]->ncols, 
        1.0, tmp_mat->values, tmp_mat->stride, 
        ata[nmodes]->values, ata[nmodes]->stride, 
        0.0, mats[m]->values, mats[m]->stride);
      // printf("Update mats[m]:\n");
      // sptDumpMatrix(mats[m], stdout);

      /* sptMatrix2Norm(mats[m], lambda); */
      sptOmpMatrix2Norm(mats[m], lambda);
      // printf("Normalize mats[m]:\n");
      // sptDumpMatrix(mats[m], stdout);
      // printf("lambda:\n");
      // for(size_t i=0; i<rank; ++i)
      //   printf("%lf  ", lambda[i]);
      // printf("\n\n");

      /* sptMatrixTransposeMultiply(mats[m], ata[m]); */
      cblas_sgemm(CblasRowMajor, CblasTrans, CblasNoTrans, rank, rank, mats[m]->nrows, 1.0, 
      mats[m]->values, mats[m]->stride, mats[m]->values, mats[m]->stride, 0.0, ata[m]->values, ata[m]->stride);
      // printf("Update ata[m]:\n");
      // sptDumpMatrix(ata[m], stdout);

      // timer_stop(&modetime[m]);
    }

    // PrintDenseValueVector(lambda, rank, "lambda", "debug.txt");
    // fit = KruskalTensorFit(spten, lambda, mats, tmp_mat, ata);
    // timer_stop(&itertime);

    printf("  its = %3lu  fit = %0.5f  delta = %+0.4e\n",
        it+1, fit, fit - oldfit);
    // for(IndexType m=0; m < nmodes; ++m) {
    //   printf("     mode = %1"PF_INDEX" (%0.3fs)\n", m+1,
    //       modetime[m].seconds);
    // }
    // if(it > 0 && fabs(fit - oldfit) < tol) {
    //   break;
    // }
    // oldfit = fit;
  }

  // GetFinalLambda(rank, nmodes, mats, lambda);

  for(size_t m=0; m < nmodes+1; ++m) {
    sptFreeMatrix(ata[m]);
  }
  free(ata);
  sptFreeVector(&scratch);
  sptFreeSizeVector(&mats_order);
  // free(modetime);

  return fit;
}


int sptOmpCpdAls(
  sptSparseTensor const * const spten,
  size_t const rank,
  size_t const niters,
  double const tol,
  sptKruskalTensor * ktensor)
{
  size_t nmodes = spten->nmodes;

  /* Initialize factor matrices */
  size_t max_dim = sptMaxSizeArray(spten->ndims, nmodes);
  sptMatrix ** mats = (sptMatrix **)malloc((nmodes+1) * sizeof(*mats));
  for(size_t m=0; m < nmodes+1; ++m) {
    mats[m] = (sptMatrix *)malloc(sizeof(sptMatrix));
  }
  for(size_t m=0; m < nmodes; ++m) {
    assert(sptNewMatrix(mats[m], spten->ndims[m], rank) == 0);
    assert(sptConstantMatrix(mats[m], 1) == 0);
    // assert(sptRandomizeMatrix(mats[m], spten->ndims[m], rank) == 0);
  }
  sptNewMatrix(mats[nmodes], max_dim, rank);

  // printf("Initial mats:\n");
  // for(size_t m=0; m < nmodes+1; ++m)
  //   sptDumpMatrix(mats[m], stdout);

  sptScalar * lambda = (sptScalar *) malloc(rank * sizeof(sptScalar));

  sptTimer timer;
  sptNewTimer(&timer, 0);
  sptStartTimer(timer);

  ktensor->fit = OmpCpdAlsStep(spten, rank, niters, tol, mats, lambda);

  sptStopTimer(timer);
  sptPrintElapsedTime(timer, "CPU  SpTns CPD-ALS");
  sptFreeTimer(timer);

  ktensor->rank = rank;
  ktensor->nmodes = nmodes;
  ktensor->lambda = lambda;
  ktensor->factors = mats;

  sptFreeMatrix(mats[nmodes]);
  
  return 0;
}


