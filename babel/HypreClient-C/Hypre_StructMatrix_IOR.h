/*
 * File:          Hypre_StructMatrix_IOR.h
 * Symbol:        Hypre.StructMatrix-v0.1.6
 * Symbol Type:   class
 * Babel Version: 0.8.0
 * SIDL Created:  20030121 14:39:13 PST
 * Generated:     20030121 14:39:15 PST
 * Description:   Intermediate Object Representation for Hypre.StructMatrix
 * 
 * WARNING: Automatically generated; changes will be lost
 * 
 * babel-version = 0.8.0
 * source-line   = 425
 * source-url    = file:/home/painter/linear_solvers/babel/Interfaces.idl
 */

#ifndef included_Hypre_StructMatrix_IOR_h
#define included_Hypre_StructMatrix_IOR_h

#ifndef included_SIDL_header_h
#include "SIDL_header.h"
#endif
#ifndef included_Hypre_Operator_IOR_h
#include "Hypre_Operator_IOR.h"
#endif
#ifndef included_Hypre_ProblemDefinition_IOR_h
#include "Hypre_ProblemDefinition_IOR.h"
#endif
#ifndef included_Hypre_StructuredGridBuildMatrix_IOR_h
#include "Hypre_StructuredGridBuildMatrix_IOR.h"
#endif
#ifndef included_SIDL_BaseClass_IOR_h
#include "SIDL_BaseClass_IOR.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Symbol "Hypre.StructMatrix" (version 0.1.6)
 * 
 * A single class that implements both a build interface and an operator
 * interface. It returns itself for <code>GetConstructedObject</code>.
 */

struct Hypre_StructMatrix__array;
struct Hypre_StructMatrix__object;

extern struct Hypre_StructMatrix__object*
Hypre_StructMatrix__new(void);

extern struct Hypre_StructMatrix__object*
Hypre_StructMatrix__remote(const char *url);

extern void Hypre_StructMatrix__init(
  struct Hypre_StructMatrix__object* self);
extern void Hypre_StructMatrix__fini(
  struct Hypre_StructMatrix__object* self);
extern void Hypre_StructMatrix__IOR_version(int32_t *major, int32_t *minor);

/*
 * Forward references for external classes and interfaces.
 */

struct Hypre_StructGrid__array;
struct Hypre_StructGrid__object;
struct Hypre_StructStencil__array;
struct Hypre_StructStencil__object;
struct Hypre_Vector__array;
struct Hypre_Vector__object;
struct SIDL_BaseInterface__array;
struct SIDL_BaseInterface__object;
struct SIDL_ClassInfo__array;
struct SIDL_ClassInfo__object;

/*
 * Declare the method entry point vector.
 */

struct Hypre_StructMatrix__epv {
  /* Implicit builtin methods */
  void* (*f__cast)(
    struct Hypre_StructMatrix__object* self,
    const char* name);
  void (*f__delete)(
    struct Hypre_StructMatrix__object* self);
  void (*f__ctor)(
    struct Hypre_StructMatrix__object* self);
  void (*f__dtor)(
    struct Hypre_StructMatrix__object* self);
  /* Methods introduced in SIDL.BaseInterface-v0.8.1 */
  void (*f_addRef)(
    struct Hypre_StructMatrix__object* self);
  void (*f_deleteRef)(
    struct Hypre_StructMatrix__object* self);
  SIDL_bool (*f_isSame)(
    struct Hypre_StructMatrix__object* self,
    struct SIDL_BaseInterface__object* iobj);
  struct SIDL_BaseInterface__object* (*f_queryInt)(
    struct Hypre_StructMatrix__object* self,
    const char* name);
  SIDL_bool (*f_isType)(
    struct Hypre_StructMatrix__object* self,
    const char* name);
  /* Methods introduced in SIDL.BaseClass-v0.8.1 */
  struct SIDL_ClassInfo__object* (*f_getClassInfo)(
    struct Hypre_StructMatrix__object* self);
  /* Methods introduced in SIDL.BaseInterface-v0.8.1 */
  /* Methods introduced in Hypre.Operator-v0.1.6 */
  int32_t (*f_SetCommunicator)(
    struct Hypre_StructMatrix__object* self,
    void* comm);
  int32_t (*f_GetDoubleValue)(
    struct Hypre_StructMatrix__object* self,
    const char* name,
    double* value);
  int32_t (*f_GetIntValue)(
    struct Hypre_StructMatrix__object* self,
    const char* name,
    int32_t* value);
  int32_t (*f_SetDoubleParameter)(
    struct Hypre_StructMatrix__object* self,
    const char* name,
    double value);
  int32_t (*f_SetIntParameter)(
    struct Hypre_StructMatrix__object* self,
    const char* name,
    int32_t value);
  int32_t (*f_SetStringParameter)(
    struct Hypre_StructMatrix__object* self,
    const char* name,
    const char* value);
  int32_t (*f_SetIntArrayParameter)(
    struct Hypre_StructMatrix__object* self,
    const char* name,
    struct SIDL_int__array* value);
  int32_t (*f_SetDoubleArrayParameter)(
    struct Hypre_StructMatrix__object* self,
    const char* name,
    struct SIDL_double__array* value);
  int32_t (*f_Setup)(
    struct Hypre_StructMatrix__object* self,
    struct Hypre_Vector__object* b,
    struct Hypre_Vector__object* x);
  int32_t (*f_Apply)(
    struct Hypre_StructMatrix__object* self,
    struct Hypre_Vector__object* b,
    struct Hypre_Vector__object** x);
  /* Methods introduced in SIDL.BaseInterface-v0.8.1 */
  /* Methods introduced in Hypre.ProblemDefinition-v0.1.6 */
  int32_t (*f_Initialize)(
    struct Hypre_StructMatrix__object* self);
  int32_t (*f_Assemble)(
    struct Hypre_StructMatrix__object* self);
  int32_t (*f_GetObject)(
    struct Hypre_StructMatrix__object* self,
    struct SIDL_BaseInterface__object** A);
  /* Methods introduced in Hypre.StructuredGridBuildMatrix-v0.1.6 */
  int32_t (*f_SetGrid)(
    struct Hypre_StructMatrix__object* self,
    struct Hypre_StructGrid__object* grid);
  int32_t (*f_SetStencil)(
    struct Hypre_StructMatrix__object* self,
    struct Hypre_StructStencil__object* stencil);
  int32_t (*f_SetValues)(
    struct Hypre_StructMatrix__object* self,
    struct SIDL_int__array* index,
    int32_t num_stencil_indices,
    struct SIDL_int__array* stencil_indices,
    struct SIDL_double__array* values);
  int32_t (*f_SetBoxValues)(
    struct Hypre_StructMatrix__object* self,
    struct SIDL_int__array* ilower,
    struct SIDL_int__array* iupper,
    int32_t num_stencil_indices,
    struct SIDL_int__array* stencil_indices,
    struct SIDL_double__array* values);
  int32_t (*f_SetNumGhost)(
    struct Hypre_StructMatrix__object* self,
    struct SIDL_int__array* num_ghost);
  int32_t (*f_SetSymmetric)(
    struct Hypre_StructMatrix__object* self,
    int32_t symmetric);
  /* Methods introduced in Hypre.StructMatrix-v0.1.6 */
};

/*
 * Define the class object structure.
 */

struct Hypre_StructMatrix__object {
  struct SIDL_BaseClass__object                  d_sidl_baseclass;
  struct Hypre_Operator__object                  d_hypre_operator;
  struct Hypre_ProblemDefinition__object         d_hypre_problemdefinition;
  struct Hypre_StructuredGridBuildMatrix__object 
    d_hypre_structuredgridbuildmatrix;
  struct Hypre_StructMatrix__epv*                d_epv;
  void*                                          d_data;
};

struct Hypre_StructMatrix__external {
  struct Hypre_StructMatrix__object*
  (*createObject)(void);

  struct Hypre_StructMatrix__object*
  (*createRemote)(const char *url);

};

/*
 * This function returns a pointer to a static structure of
 * pointers to function entry points.  Its purpose is to provide
 * one-stop shopping for loading DLLs.
 * loading DLLs
 */

const struct Hypre_StructMatrix__external*
Hypre_StructMatrix__externals(void);

#ifdef __cplusplus
}
#endif
#endif
