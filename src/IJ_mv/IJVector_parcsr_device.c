/******************************************************************************
 * Copyright (c) 1998 Lawrence Livermore National Security, LLC and other
 * HYPRE Project Developers. See the top-level COPYRIGHT file for details.
 *
 * SPDX-License-Identifier: (Apache-2.0 OR MIT)
 ******************************************************************************/

/******************************************************************************
 *
 * IJVector_ParCSR interface
 *
 *****************************************************************************/

#include "_hypre_onedpl.hpp"
#include "_hypre_IJ_mv.h"
#include "_hypre_utilities.hpp"

#if defined(HYPRE_USING_CUDA) || defined(HYPRE_USING_HIP) || defined(HYPRE_USING_SYCL)

#if defined(HYPRE_USING_SYCL)
template<typename T1, typename T2>
struct hypre_IJVectorAssembleFunctor
{
   typedef std::tuple<T1, T2> Tuple;

   Tuple operator()(const Tuple& x, const Tuple& y )
   {
      return std::make_tuple( hypre_max(std::get<0>(x), std::get<0>(y)),
                                        std::get<1>(x) + std::get<1>(y) );
   }
};
#else
template<typename T1, typename T2>
struct hypre_IJVectorAssembleFunctor : public
   thrust::binary_function< thrust::tuple<T1, T2>, thrust::tuple<T1, T2>, thrust::tuple<T1, T2> >
{
   typedef thrust::tuple<T1, T2> Tuple;

   __device__ Tuple operator()(const Tuple& x, const Tuple& y )
   {
      return thrust::make_tuple( hypre_max(thrust::get<0>(x), thrust::get<0>(y)),
                                 thrust::get<1>(x) + thrust::get<1>(y) );
   }
};
#endif

HYPRE_Int hypre_IJVectorAssembleSortAndReduce3(HYPRE_Int N0, HYPRE_BigInt *I0, char *X0,
                                               HYPRE_Complex *A0, HYPRE_Int *N1);

HYPRE_Int hypre_IJVectorAssembleSortAndReduce1(HYPRE_Int N0, HYPRE_BigInt *I0, char *X0,
                                               HYPRE_Complex *A0, HYPRE_Int *N1, HYPRE_BigInt **I1, char **X1, HYPRE_Complex **A1 );

/* y[map[i]-offset] = x[i] or y[map[i]] += x[i] depending on SorA,
 * same index cannot appear more than once in map */
__global__ void
hypreCUDAKernel_IJVectorAssemblePar(
#if defined(HYPRE_USING_SYCL)
   sycl::nd_item<1>& item,
#endif
   HYPRE_Int n,
   HYPRE_Complex *x,
   HYPRE_BigInt *map,
   HYPRE_BigInt offset,
   char *SorA,
   HYPRE_Complex *y)
{
#if defined(HYPRE_USING_SYCL)
   HYPRE_Int i = static_cast<HYPRE_Int>(item.get_global_linear_id());
#else
   HYPRE_Int i = hypre_cuda_get_grid_thread_id<1, 1>();
#endif

   if (i >= n)
   {
      return;
   }

   if (SorA[i])
   {
      y[map[i] - offset] = x[i];
   }
   else
   {
      y[map[i] - offset] += x[i];
   }
}

/*
 */
HYPRE_Int
hypre_IJVectorSetAddValuesParDevice(hypre_IJVector       *vector,
                                    HYPRE_Int             num_values,
                                    const HYPRE_BigInt   *indices,
                                    const HYPRE_Complex  *values,
                                    const char           *action)
{
   HYPRE_BigInt *IJpartitioning = hypre_IJVectorPartitioning(vector);
   HYPRE_BigInt  vec_start, vec_stop;
   vec_start = IJpartitioning[0];
   vec_stop  = IJpartitioning[1] - 1;
   HYPRE_Int nrows = vec_stop - vec_start + 1;
   const char SorA = action[0] == 's' ? 1 : 0;

   if (num_values <= 0)
   {
      return hypre_error_flag;
   }

   /* this is a special use to set/add local values */
   if (!indices)
   {
      hypre_ParVector *par_vector = (hypre_ParVector*) hypre_IJVectorObject(vector);
      hypre_Vector    *local_vector = hypre_ParVectorLocalVector(par_vector);
      HYPRE_Int        num_values2 = hypre_min( hypre_VectorSize(local_vector), num_values );
      HYPRE_BigInt    *indices2 = hypre_TAlloc(HYPRE_BigInt, num_values2, HYPRE_MEMORY_DEVICE);
#if defined(HYPRE_USING_SYCL)
      hypreSycl_sequence(indices2, indices2 + num_values2, vec_start);
#else
      HYPRE_THRUST_CALL(sequence, indices2, indices2 + num_values2, vec_start);
#endif

      hypre_IJVectorSetAddValuesParDevice(vector, num_values2, indices2, values, action);

      hypre_TFree(indices2, HYPRE_MEMORY_DEVICE);

      return hypre_error_flag;
   }

   hypre_AuxParVector *aux_vector = (hypre_AuxParVector*) hypre_IJVectorTranslator(vector);

   if (!aux_vector)
   {
      hypre_AuxParVectorCreate(&aux_vector);
      hypre_AuxParVectorInitialize_v2(aux_vector, HYPRE_MEMORY_DEVICE);
      hypre_IJVectorTranslator(vector) = aux_vector;
   }

   HYPRE_Int      stack_elmts_max      = hypre_AuxParVectorMaxStackElmts(aux_vector);
   HYPRE_Int      stack_elmts_current  = hypre_AuxParVectorCurrentStackElmts(aux_vector);
   HYPRE_Int      stack_elmts_required = stack_elmts_current + num_values;
   HYPRE_BigInt  *stack_i              = hypre_AuxParVectorStackI(aux_vector);
   HYPRE_Complex *stack_data           = hypre_AuxParVectorStackData(aux_vector);
   char          *stack_sora           = hypre_AuxParVectorStackSorA(aux_vector);

   if ( stack_elmts_max < stack_elmts_required )
   {
      HYPRE_Int stack_elmts_max_new = nrows * hypre_AuxParVectorInitAllocFactor(aux_vector);
      if (hypre_AuxParVectorUsrOffProcElmts(aux_vector) >= 0)
      {
         stack_elmts_max_new += hypre_AuxParVectorUsrOffProcElmts(aux_vector);
      }
      stack_elmts_max_new = hypre_max(stack_elmts_max * hypre_AuxParVectorGrowFactor(aux_vector),
                                      stack_elmts_max_new);
      stack_elmts_max_new = hypre_max(stack_elmts_required, stack_elmts_max_new);

      hypre_AuxParVectorStackI(aux_vector)    = stack_i    =
                                                   hypre_TReAlloc_v2(stack_i,    HYPRE_BigInt,  stack_elmts_max, HYPRE_BigInt,  stack_elmts_max_new,
                                                                     HYPRE_MEMORY_DEVICE);
      hypre_AuxParVectorStackData(aux_vector) = stack_data =
                                                   hypre_TReAlloc_v2(stack_data, HYPRE_Complex, stack_elmts_max, HYPRE_Complex, stack_elmts_max_new,
                                                                     HYPRE_MEMORY_DEVICE);
      hypre_AuxParVectorStackSorA(aux_vector) = stack_sora =
                                                   hypre_TReAlloc_v2(stack_sora,          char, stack_elmts_max,          char, stack_elmts_max_new,
                                                                     HYPRE_MEMORY_DEVICE);

      hypre_AuxParVectorMaxStackElmts(aux_vector) = stack_elmts_max_new;
   }

   hypreDevice_CharFilln(stack_sora + stack_elmts_current, num_values, SorA);

   hypre_TMemcpy(stack_i    + stack_elmts_current, indices, HYPRE_BigInt,  num_values,
                 HYPRE_MEMORY_DEVICE, HYPRE_MEMORY_DEVICE);
   hypre_TMemcpy(stack_data + stack_elmts_current, values,  HYPRE_Complex, num_values,
                 HYPRE_MEMORY_DEVICE, HYPRE_MEMORY_DEVICE);

   hypre_AuxParVectorCurrentStackElmts(aux_vector) += num_values;

   return hypre_error_flag;
}

/******************************************************************************
 *
 *
 *****************************************************************************/

HYPRE_Int
hypre_IJVectorAssembleParDevice(hypre_IJVector *vector)
{
   MPI_Comm comm = hypre_IJVectorComm(vector);
   HYPRE_BigInt *IJpartitioning = hypre_IJVectorPartitioning(vector);
   HYPRE_BigInt  vec_start, vec_stop;
   vec_start = IJpartitioning[0];
   vec_stop  = IJpartitioning[1] - 1;
   hypre_ParVector *par_vector = (hypre_ParVector*) hypre_IJVectorObject(vector);
   hypre_AuxParVector *aux_vector = (hypre_AuxParVector*) hypre_IJVectorTranslator(vector);

   if (!aux_vector)
   {
      return hypre_error_flag;
   }

   if (!par_vector)
   {
      return hypre_error_flag;
   }

   HYPRE_Int      nelms      = hypre_AuxParVectorCurrentStackElmts(aux_vector);
   HYPRE_BigInt  *stack_i    = hypre_AuxParVectorStackI(aux_vector);
   HYPRE_Complex *stack_data = hypre_AuxParVectorStackData(aux_vector);
   char          *stack_sora = hypre_AuxParVectorStackSorA(aux_vector);

   in_range<HYPRE_BigInt> pred(vec_start, vec_stop);
#if defined(HYPRE_USING_SYCL)
   HYPRE_Int nelms_on = HYPRE_ONEDPL_CALL(std::count_if, stack_i, stack_i + nelms, pred);
#else
   HYPRE_Int nelms_on = HYPRE_THRUST_CALL(count_if, stack_i, stack_i + nelms, pred);
#endif
   HYPRE_Int nelms_off = nelms - nelms_on;
   HYPRE_Int nelms_off_max;
   hypre_MPI_Allreduce(&nelms_off, &nelms_off_max, 1, HYPRE_MPI_INT, hypre_MPI_MAX, comm);

   /* communicate for aux off-proc and add to remote aux on-proc */
   if (nelms_off_max)
   {
      HYPRE_Int      new_nnz       = 0;
      HYPRE_BigInt  *off_proc_i    = NULL;
      HYPRE_Complex *off_proc_data = NULL;

      if (nelms_off)
      {
         /* copy off-proc entries out of stack and remove from stack */
         off_proc_i          = hypre_TAlloc(HYPRE_BigInt,  nelms_off, HYPRE_MEMORY_DEVICE);
         off_proc_data       = hypre_TAlloc(HYPRE_Complex, nelms_off, HYPRE_MEMORY_DEVICE);
         char *off_proc_sora = hypre_TAlloc(char,          nelms_off, HYPRE_MEMORY_DEVICE);
         char *is_on_proc    = hypre_TAlloc(char,          nelms,     HYPRE_MEMORY_DEVICE);

#if defined(HYPRE_USING_SYCL)
         HYPRE_ONEDPL_CALL(std::transform, stack_i, stack_i + nelms, is_on_proc, pred);
         auto zip_in = oneapi::dpl::make_zip_iterator(stack_i, stack_data, stack_sora);
         auto zip_out = oneapi::dpl::make_zip_iterator(off_proc_i, off_proc_data, off_proc_sora);
         auto new_end1 = hypreSycl_copy_if( zip_in,  /* first */
                                            zip_in + nelms, /* last */
                                            is_on_proc, /* stencil */
                                            zip_out, /* result */
      [] (const auto & x) {return x;} );

         hypre_assert(std::get<0>(new_end1.base()) - off_proc_i == nelms_off);

         /* remove off-proc entries from stack */
         auto new_end2 = hypreSycl_remove_if( zip_in, /* first */
                                            zip_in + nelms, /* last */
                                            is_on_proc, /* stencil */
      [] (const auto & x) {return x;} );

         hypre_assert(std::get<0>(new_end2.base()) - stack_i == nelms_on);
#else
         HYPRE_THRUST_CALL(transform, stack_i, stack_i + nelms, is_on_proc, pred);

         auto new_end1 = HYPRE_THRUST_CALL(
                            copy_if,
                            thrust::make_zip_iterator(thrust::make_tuple(stack_i,         stack_data,
                                                                         stack_sora        )),  /* first */
                            thrust::make_zip_iterator(thrust::make_tuple(stack_i + nelms, stack_data + nelms,
                                                                         stack_sora + nelms)),  /* last */
                            is_on_proc,                                                                                              /* stencil */
                            thrust::make_zip_iterator(thrust::make_tuple(off_proc_i,      off_proc_data,
                                                                         off_proc_sora)),       /* result */
                            thrust::not1(thrust::identity<char>()) );

         hypre_assert(thrust::get<0>(new_end1.get_iterator_tuple()) - off_proc_i == nelms_off);

         /* remove off-proc entries from stack */
         auto new_end2 = HYPRE_THRUST_CALL(
                            remove_if,
                            thrust::make_zip_iterator(thrust::make_tuple(stack_i,         stack_data,
                                                                         stack_sora        )),  /* first */
                            thrust::make_zip_iterator(thrust::make_tuple(stack_i + nelms, stack_data + nelms,
                                                                         stack_sora + nelms)),  /* last */
                            is_on_proc,                                                                                              /* stencil */
                            thrust::not1(thrust::identity<char>()) );

         hypre_assert(thrust::get<0>(new_end2.get_iterator_tuple()) - stack_i == nelms_on);
#endif

         hypre_AuxParVectorCurrentStackElmts(aux_vector) = nelms_on;

         hypre_TFree(is_on_proc, HYPRE_MEMORY_DEVICE);

         /* sort and reduce */
         hypre_IJVectorAssembleSortAndReduce3(nelms_off, off_proc_i, off_proc_sora, off_proc_data, &new_nnz);

         hypre_TFree(off_proc_sora, HYPRE_MEMORY_DEVICE);
      }

      /* send off_proc_i/data to remote processes and the receivers call addtovalues */
      hypre_IJVectorAssembleOffProcValsPar(vector, -1, new_nnz, HYPRE_MEMORY_DEVICE, off_proc_i,
                                           off_proc_data);

      hypre_TFree(off_proc_i,    HYPRE_MEMORY_DEVICE);
      hypre_TFree(off_proc_data, HYPRE_MEMORY_DEVICE);
   }

   /* Note: the stack might have been changed in hypre_IJVectorAssembleOffProcValsPar,
    * so must get the size and the pointers again */
   nelms      = hypre_AuxParVectorCurrentStackElmts(aux_vector);
   stack_i    = hypre_AuxParVectorStackI(aux_vector);
   stack_data = hypre_AuxParVectorStackData(aux_vector);
   stack_sora = hypre_AuxParVectorStackSorA(aux_vector);

#ifdef HYPRE_DEBUG
   /* the stack should only have on-proc elements now */
#if define(HYPRE_USING_SYCL)
   HYPRE_Int tmp = HYPRE_ONEDPL_CALL(std::count_if, stack_i, stack_i + nelms, pred);
#else
   HYPRE_Int tmp = HYPRE_THRUST_CALL(count_if, stack_i, stack_i + nelms, pred);
#endif
   hypre_assert(nelms == tmp);
#endif

   if (nelms)
   {
      HYPRE_Int      new_nnz;
      HYPRE_BigInt  *new_i;
      HYPRE_Complex *new_data;
      char          *new_sora;

      /* sort and reduce */
      hypre_IJVectorAssembleSortAndReduce1(nelms, stack_i, stack_sora, stack_data, &new_nnz, &new_i,
                                           &new_sora, &new_data);

      /* set/add to local vector */
      dim3 bDim = hypre_GetDefaultDeviceBlockDimension();
      dim3 gDim = hypre_GetDefaultDeviceGridDimension(new_nnz, "thread", bDim);
      HYPRE_GPU_LAUNCH( hypreCUDAKernel_IJVectorAssemblePar, gDim, bDim, new_nnz, new_data, new_i,
                        vec_start, new_sora,
                        hypre_VectorData(hypre_ParVectorLocalVector(par_vector)) );

      hypre_TFree(new_i,    HYPRE_MEMORY_DEVICE);
      hypre_TFree(new_data, HYPRE_MEMORY_DEVICE);
      hypre_TFree(new_sora, HYPRE_MEMORY_DEVICE);
   }

   hypre_AuxParVectorDestroy(aux_vector);
   hypre_IJVectorTranslator(vector) = NULL;

   return hypre_error_flag;
}

/* helper routine used in hypre_IJVectorAssembleParCSRDevice:
 * 1. sort (X0, A0) with key I0
 * 2. for each segment in I0, zero out in A0 all before the last `set'
 * 3. reduce A0 [with sum] and reduce X0 [with max]
 * N0: input size; N1: size after reduction (<= N0)
 * Note: (I1, X1, A1) are not resized to N1 but have size N0
 */
HYPRE_Int
hypre_IJVectorAssembleSortAndReduce1(HYPRE_Int       N0,
                                     HYPRE_BigInt   *I0,
                                     char           *X0,
                                     HYPRE_Complex  *A0,
                                     HYPRE_Int      *N1,
                                     HYPRE_BigInt  **I1,
                                     char          **X1,
                                     HYPRE_Complex **A1 )
{
#if defined(HYPRE_USING_SYCL)
   /* WM: double check this */
   auto zipped_begin = oneapi::dpl::make_zip_iterator(I0, X0, A0);
   HYPRE_ONEDPL_CALL( std::stable_sort,
                      zipped_begin, zipped_begin + N0,
                      std::less< std::tuple<HYPRE_BigInt, char, HYPRE_Complex> >() );
#else
   HYPRE_THRUST_CALL( stable_sort_by_key,
                      I0,
                      I0 + N0,
                      thrust::make_zip_iterator(thrust::make_tuple(X0, A0)) );
#endif

   HYPRE_BigInt  *I = hypre_TAlloc(HYPRE_BigInt,  N0, HYPRE_MEMORY_DEVICE);
   char          *X = hypre_TAlloc(char,          N0, HYPRE_MEMORY_DEVICE);
   HYPRE_Complex *A = hypre_TAlloc(HYPRE_Complex, N0, HYPRE_MEMORY_DEVICE);

   /* output X: 0: keep, 1: zero-out */
#if defined(HYPRE_USING_SYCL)
   /* WM: todo - HERE!!! */
#else
   HYPRE_THRUST_CALL(
      exclusive_scan_by_key,
      make_reverse_iterator(thrust::device_pointer_cast<HYPRE_BigInt>(I0) + N0), /* key begin */
      make_reverse_iterator(thrust::device_pointer_cast<HYPRE_BigInt>(I0)),      /* key end */
      make_reverse_iterator(thrust::device_pointer_cast<char>(X0) + N0),         /* input value begin */
      make_reverse_iterator(thrust::device_pointer_cast<char>(X) + N0),          /* output value begin */
      char(0),                                                                   /* init */
      thrust::equal_to<HYPRE_BigInt>(),
      thrust::maximum<char>() );

   HYPRE_THRUST_CALL(replace_if, A0, A0 + N0, X, thrust::identity<char>(), 0.0);

   auto new_end = HYPRE_THRUST_CALL(
                     reduce_by_key,
                     I0,                                                              /* keys_first */
                     I0 + N0,                                                         /* keys_last */
                     thrust::make_zip_iterator(thrust::make_tuple(X0,      A0     )), /* values_first */
                     I,                                                               /* keys_output */
                     thrust::make_zip_iterator(thrust::make_tuple(X,       A      )), /* values_output */
                     thrust::equal_to<HYPRE_BigInt>(),                                /* binary_pred */
                     hypre_IJVectorAssembleFunctor<char, HYPRE_Complex>()             /* binary_op */);
#endif

   *N1 = new_end.first - I;
   *I1 = I;
   *X1 = X;
   *A1 = A;

   return hypre_error_flag;
}

HYPRE_Int
hypre_IJVectorAssembleSortAndReduce3(HYPRE_Int  N0, HYPRE_BigInt  *I0, char *X0, HYPRE_Complex  *A0,
                                     HYPRE_Int *N1)
{
#if defined(HYPRE_USING_SYCL)
   HYPRE_ONEDPL_CALL( std::stable_sort_by_key,
                      I0,
                      I0 + N0,
                      std::make_zip_iterator(X0, A0) );
#else
   HYPRE_THRUST_CALL( stable_sort_by_key,
                      I0,
                      I0 + N0,
                      thrust::make_zip_iterator(thrust::make_tuple(X0, A0)) );
#endif

   HYPRE_BigInt  *I = hypre_TAlloc(HYPRE_BigInt,  N0, HYPRE_MEMORY_DEVICE);
   HYPRE_Complex *A = hypre_TAlloc(HYPRE_Complex, N0, HYPRE_MEMORY_DEVICE);

   /* output in X0: 0: keep, 1: zero-out */
#if defined(HYPRE_USING_SYCL)
   /* WM: double check this... also, is there a better workaround here for lack of reverse iterator in dpl? */
   HYPRE_Int *reverse_perm = hypre_TAlloc(HYPRE_Int, N0, HYPRE_MEMORY_DEVICE);
   HYPRE_ONEDPL_CALL( std::transform,
                      oneapi::dpl::counting_iterator<DiffType>(0),
                      oneapi::dpl::counting_iterator<DiffType>(N0),
                      first,
                      [N0] (auto i) { return N0 - i - 1; });
   HYPRE_ONEDPL_CALL(
      oneapi::dpl::inclusive_scan_by_segment,
      oneapi::dpl::make_permutation_iterator(I0, reverse_perm), /* key begin */
      oneapi::dpl::make_permutation_iterator(I0, reverse_perm) + N0, /* key end */
      oneapi::dpl::make_permutation_iterator(X0, reverse_perm), /* input value begin */
      oneapi::dpl::make_permutation_iterator(X0, reverse_perm), /* input value end */
      std::equal_to<HYPRE_BigInt>(),
      std::maximum<char>() );
   hypre_TFree(reverse_perm, HYPRE_MEMORY_DEVICE);

   /* WM: todo - HERE!!! */
   /* HYPRE_THRUST_CALL(replace_if, A0, A0 + N0, X0, thrust::identity<char>(), 0.0); */

   auto new_end = oneapi::dpl::reduce_by_segment(
                                 oneapi::dpl::execution::make_device_policy<class devutils>(*hypre_HandleComputeStream( hypre_handle())),
                                 I0,      /* keys_first */
                                 I0 + N0, /* keys_last */
                                 A0,      /* values_first */
                                 I,       /* keys_output */
                                 A        /* values_output */);
#else
   HYPRE_THRUST_CALL(
      inclusive_scan_by_key,
      make_reverse_iterator(thrust::device_pointer_cast<HYPRE_BigInt>(I0) + N0), /* key begin */
      make_reverse_iterator(thrust::device_pointer_cast<HYPRE_BigInt>(I0)),    /* key end */
      make_reverse_iterator(thrust::device_pointer_cast<char>(X0) + N0),       /* input value begin */
      make_reverse_iterator(thrust::device_pointer_cast<char>(X0) + N0),       /* output value begin */
      thrust::equal_to<HYPRE_BigInt>(),
      thrust::maximum<char>() );

   HYPRE_THRUST_CALL(replace_if, A0, A0 + N0, X0, thrust::identity<char>(), 0.0);

   auto new_end = HYPRE_THRUST_CALL(
                     reduce_by_key,
                     I0,      /* keys_first */
                     I0 + N0, /* keys_last */
                     A0,      /* values_first */
                     I,       /* keys_output */
                     A        /* values_output */);
#endif

   HYPRE_Int Nt = new_end.second - A;

   hypre_assert(Nt <= N0);

   /* remove numerical zeros */
#if defined(HYPRE_USING_SYCL)
   auto new_end2 = hypreSycl_copy_if( std::make_zip_iterator(I, A),
                                      std::make_zip_iterator(I, A) + Nt,
                                      A,
                                      std::make_zip_iterator(I0, A0)),
   [] (const auto & x) {return x;} );

   *N1 = std::get<0>(new_end2.base()) - I0;
#else
   auto new_end2 = HYPRE_THRUST_CALL( copy_if,
                                      thrust::make_zip_iterator(thrust::make_tuple(I, A)),
                                      thrust::make_zip_iterator(thrust::make_tuple(I, A)) + Nt,
                                      A,
                                      thrust::make_zip_iterator(thrust::make_tuple(I0, A0)),
                                      thrust::identity<HYPRE_Complex>() );

   *N1 = thrust::get<0>(new_end2.get_iterator_tuple()) - I0;
#endif


   hypre_assert(*N1 <= Nt);

   hypre_TFree(I, HYPRE_MEMORY_DEVICE);
   hypre_TFree(A, HYPRE_MEMORY_DEVICE);

   return hypre_error_flag;
}

#endif
