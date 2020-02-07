/*============================================================================
 * Routines to handle specific settings related to a cs_equation_t structure
 *============================================================================*/

/*
  This file is part of Code_Saturne, a general-purpose CFD tool.

  Copyright (C) 1998-2020 EDF S.A.

  This program is free software; you can redistribute it and/or modify it under
  the terms of the GNU General Public License as published by the Free Software
  Foundation; either version 2 of the License, or (at your option) any later
  version.

  This program is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
  FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
  details.

  You should have received a copy of the GNU General Public License along with
  this program; if not, write to the Free Software Foundation, Inc., 51 Franklin
  Street, Fifth Floor, Boston, MA 02110-1301, USA.
*/

/*----------------------------------------------------------------------------*/

#include "cs_defs.h"

/*----------------------------------------------------------------------------
 * Standard C library headers
 *----------------------------------------------------------------------------*/

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#if defined(HAVE_PETSC)
#include <petscversion.h>
#endif

/*----------------------------------------------------------------------------
 *  Local headers
 *----------------------------------------------------------------------------*/

#include <bft_error.h>
#include <bft_mem.h>

#include "cs_boundary_zone.h"
#include "cs_cdo_bc.h"
#include "cs_fp_exception.h"
#include "cs_hodge.h"
#include "cs_log.h"
#include "cs_mesh_location.h"
#include "cs_multigrid.h"
#include "cs_sles.h"
#include "cs_source_term.h"
#include "cs_volume_zone.h"

#if defined(HAVE_PETSC)
#include "cs_sles_petsc.h"
#endif

/*----------------------------------------------------------------------------
 * Header for the current file
 *----------------------------------------------------------------------------*/

#include "cs_equation_param.h"

/*----------------------------------------------------------------------------*/

BEGIN_C_DECLS

/*============================================================================
 * Type definitions
 *============================================================================*/

/*============================================================================
 * Local private variables
 *============================================================================*/

static const cs_real_t  _weak_pena_bc_coef_by_default = 100.;
static const cs_real_t  _strong_pena_bc_coef_by_default = 1e12;

static const char _err_empty_eqp[] =
  N_(" Stop setting an empty cs_equation_param_t structure.\n"
     " Please check your settings.\n");

/*============================================================================
 * Private function prototypes
 *============================================================================*/

#if defined(HAVE_PETSC)
/*----------------------------------------------------------------------------*/
/*!
 * \brief Predefined settings for GAMG as a preconditioner
 */
/*----------------------------------------------------------------------------*/

static inline void
_petsc_pcmg_hook(void)
{
#if PETSC_VERSION_GE(3,7,0)
  PetscOptionsSetValue(NULL, "-mg_levels_ksp_type", "richardson");
  PetscOptionsSetValue(NULL, "-mg_levels_pc_type", "sor");
  PetscOptionsSetValue(NULL, "-mg_levels_ksp_max_it", "1");
#else
  PetscOptionsSetValue("-mg_levels_ksp_type", "richardson");
  PetscOptionsSetValue("-mg_levels_pc_type", "sor");
  PetscOptionsSetValue("-mg_levels_ksp_max_it", "1");
#endif
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Predefined settings for GAMG as a preconditioner
 */
/*----------------------------------------------------------------------------*/

static inline void
_petsc_pcgamg_hook(void)
{
  _petsc_pcmg_hook();

#if PETSC_VERSION_GE(3,7,0)
  PetscOptionsSetValue(NULL, "-pc_gamg_threshold", "0.02");
  PetscOptionsSetValue(NULL, "-pc_gamg_reuse_interpolation", "TRUE");
  PetscOptionsSetValue(NULL, "-pc_gamg_square_graph", "4");
#else
  PetscOptionsSetValue("-pc_gamg_threshold", "0.02");
  PetscOptionsSetValue("-pc_gamg_reuse_interpolation", "TRUE");
  PetscOptionsSetValue("-pc_gamg_square_graph", "4");
#endif
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Predefined settings for BoomerAMG in HYPRE as a preconditioner
 */
/*----------------------------------------------------------------------------*/

static inline void
_petsc_pchypre_hook(void)
{
#if PETSC_VERSION_GE(3,7,0)
  PetscOptionsSetValue(NULL,"-pc_hypre_type","boomeramg");
  PetscOptionsSetValue(NULL,"-pc_hypre_boomeramg_coarsen_type","HMIS");
  PetscOptionsSetValue(NULL,"-pc_hypre_boomeramg_interp_type","ext+i-cc");
  PetscOptionsSetValue(NULL,"-pc_hypre_boomeramg_agg_nl","2");
  PetscOptionsSetValue(NULL,"-pc_hypre_boomeramg_P_max","4");
  PetscOptionsSetValue(NULL,"-pc_hypre_boomeramg_strong_threshold","0.5");
  PetscOptionsSetValue(NULL,"-pc_hypre_boomeramg_no_CF","");
#else
  PetscOptionsSetValue("-pc_hypre_type","boomeramg");
  PetscOptionsSetValue("-pc_hypre_boomeramg_coarsen_type","HMIS");
  PetscOptionsSetValue("-pc_hypre_boomeramg_interp_type","ext+i-cc");
  PetscOptionsSetValue("-pc_hypre_boomeramg_agg_nl","2");
  PetscOptionsSetValue("-pc_hypre_boomeramg_P_max","4");
  PetscOptionsSetValue("-pc_hypre_boomeramg_strong_threshold","0.5");
  PetscOptionsSetValue("-pc_hypre_boomeramg_no_CF","");
#endif
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Set command line options for PC according to the kind of
 *        preconditionner
 *
 * \param[in]      slesp    set of parameters for the linear algebra
 * \param[in]      eqname   name of the equation to handle
 * \param[in, out] pc       PETSc preconditioner structure
 */
/*----------------------------------------------------------------------------*/

static void
_petsc_set_pc_type(cs_param_sles_t    slesp,
                   const char        *eqname,
                   PC                 pc)
{
  if (slesp.solver == CS_PARAM_ITSOL_MUMPS ||
      slesp.solver == CS_PARAM_ITSOL_MUMPS_LDLT)
    return; /* Direct solver: Nothing to do at this stage */

  switch (slesp.precond) {

  case CS_PARAM_PRECOND_NONE:
    PCSetType(pc, PCNONE);
    break;

  case CS_PARAM_PRECOND_DIAG:
    PCSetType(pc, PCJACOBI);
    break;

  case CS_PARAM_PRECOND_BJACOB_ILU0:
  case CS_PARAM_PRECOND_BJACOB_SGS:
    PCSetType(pc, PCBJACOBI);
    break;

  case CS_PARAM_PRECOND_SSOR:
    PCSetType(pc, PCSOR);
    PCSORSetSymmetric(pc, SOR_SYMMETRIC_SWEEP);
    break;

  case CS_PARAM_PRECOND_ICC0:
#if defined(PETSC_HAVE_HYPRE)
    if (slesp.solver_class == CS_PARAM_SLES_CLASS_HYPRE) {
      PCSetType(pc, PCHYPRE);
      PCHYPRESetType(pc, "euclid");
    }
    else {
      PCSetType(pc, PCICC);
      PCFactorSetLevels(pc, 0);
    }
#else
    PCSetType(pc, PCICC);
    PCFactorSetLevels(pc, 0);
#endif
    break;

  case CS_PARAM_PRECOND_ILU0:
#if defined(PETSC_HAVE_HYPRE)
    if (slesp.solver_class == CS_PARAM_SLES_CLASS_HYPRE) {
      PCSetType(pc, PCHYPRE);
      PCHYPRESetType(pc, "euclid");
    }
    else {
      PCSetType(pc, PCBJACOBI);
    }
#else
    PCSetType(pc, PCBJACOBI);
#endif
    break;

  case CS_PARAM_PRECOND_AS:
    PCSetType(pc, PCASM);
    break;

  case CS_PARAM_PRECOND_AMG:
    {
      switch (slesp.amg_type) {

      case CS_PARAM_AMG_PETSC_GAMG:
        PCSetType(pc, PCGAMG);
        PCGAMGSetType(pc, PCGAMGAGG);
        PCGAMGSetNSmooths(pc, 1);
        break;

      case CS_PARAM_AMG_PETSC_PCMG:
        PCSetType(pc, PCMG);
        break;

      case CS_PARAM_AMG_HYPRE_BOOMER:
#if defined(PETSC_HAVE_HYPRE)
        PCSetType(pc, PCHYPRE);
        PCHYPRESetType(pc, "boomeramg");
#else
        cs_base_warn(__FILE__, __LINE__);
        cs_log_printf(CS_LOG_DEFAULT,
                      "%s: Eq. %s: Switch to MG since BoomerAMG is not"
                      " available.\n",
                      __func__, eqname);
#endif
        break;

      default:
        bft_error(__FILE__, __LINE__, 0,
                  " %s: Eq. %s: Invalid AMG type for the PETSc library.",
                  __func__, eqname);
        break;

      } /* End of switch on the AMG type */

    } /* AMG as preconditioner */
    break;

  default:
    bft_error(__FILE__, __LINE__, 0,
              " %s: Eq. %s: Preconditioner not interfaced with PETSc.",
              __func__, eqname);
  }

}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Set command line options for PC according to the kind of
 *        preconditionner
 *
 * \param[in]    slesp     set of parameters for the linear algebra solver
 */
/*----------------------------------------------------------------------------*/

static void
_petsc_set_pc_options_from_command_line(cs_param_sles_t    slesp)
{

  switch (slesp.precond) {

#if defined(PETSC_HAVE_HYPRE)
  case CS_PARAM_PRECOND_ILU0:
  case CS_PARAM_PRECOND_ICC0:
    if (slesp.solver_class == CS_PARAM_SLES_CLASS_HYPRE) {
#if PETSC_VERSION_GE(3,7,0)
      PetscOptionsSetValue(NULL, "-pc_euclid_level", "-help");
#else
      PetscOptionsSetValue("-pc_euclid_level", "0");
#endif
    }
    break;
#endif  /* PETSc with HYPRE */

  case CS_PARAM_PRECOND_AMG:
    {
      switch (slesp.amg_type) {

      case CS_PARAM_AMG_PETSC_GAMG:
        _petsc_pcgamg_hook();
        break;

      case CS_PARAM_AMG_PETSC_PCMG:
        _petsc_pcmg_hook();
        break;

      case CS_PARAM_AMG_HYPRE_BOOMER:
#if defined(PETSC_HAVE_HYPRE)
        _petsc_pchypre_hook();
#else
        _petsc_pcmg_hook();
#endif
        break;

      default:
        break; /* Nothing else to do at this stage */

      } /* End of switch on the AMG type */

    } /* AMG as preconditioner */
    break;

  default:
    break; /* Nothing else to do at this stage */

  }

}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Set PETSc solver
 *
 * \param[in]      slesp    pointer to SLES parameters
 * \param[in, out] a        pointer to PETSc matrix
 * \param[in, out] ksp      pointer to PETSc KSP context
 */
/*----------------------------------------------------------------------------*/

static void
_petsc_set_krylov_solver(cs_param_sles_t   slesp,
                         Mat               a,
                         KSP               ksp)
{
  /* 1) Set the type of normalization for the residual */
  switch (slesp.resnorm_type) {

  case CS_PARAM_RESNORM_NORM2_RHS: /* Try to have "true" norm */
    KSPSetNormType(ksp, KSP_NORM_UNPRECONDITIONED);
    break;
  case CS_PARAM_RESNORM_NONE:
    KSPSetNormType(ksp, KSP_NORM_NONE);
    break;
  default:
    KSPSetNormType(ksp, KSP_NORM_UNPRECONDITIONED);
    break;

  }

  /* 2) Set the krylov solver */
  switch (slesp.solver) {

  case CS_PARAM_ITSOL_NONE:
    KSPSetType(ksp, KSPPREONLY);
    break;

  case CS_PARAM_ITSOL_BICG:      /* Improved Bi-CG stab */
    KSPSetType(ksp, KSPIBCGS);
    /* No choice otherwise PETSc yields an error */
    KSPSetNormType(ksp, KSP_NORM_UNPRECONDITIONED);
    break;

  case CS_PARAM_ITSOL_BICGSTAB2: /* Preconditioned BiCGstab2 */
    KSPSetType(ksp, KSPBCGSL);
    break;

  case CS_PARAM_ITSOL_CG:        /* Preconditioned Conjugate Gradient */
    if (slesp.precond == CS_PARAM_PRECOND_AMG ||
        slesp.precond == CS_PARAM_PRECOND_AMG_BLOCK)
      KSPSetType(ksp, KSPFCG);
    else
      KSPSetType(ksp, KSPCG);
    break;

  case CS_PARAM_ITSOL_FCG:       /* Flexible Conjuguate Gradient */
    KSPSetType(ksp, KSPFCG);
    break;

  case CS_PARAM_ITSOL_FGMRES:     /* Preconditioned flexible GMRES */
    KSPSetType(ksp, KSPFGMRES);
    break;

  case CS_PARAM_ITSOL_GMRES:     /* Preconditioned GMRES */
    KSPSetType(ksp, KSPLGMRES);
    break;

  case CS_PARAM_ITSOL_MINRES:    /* Minimal residual */
    KSPSetType(ksp, KSPMINRES);
    break;

  case CS_PARAM_ITSOL_MUMPS:     /* Direct solver (factorization) */
  case CS_PARAM_ITSOL_MUMPS_LDLT:
#if defined(PETSC_HAVE_MUMPS)
    KSPSetType(ksp, KSPPREONLY);
#else
    bft_error(__FILE__, __LINE__, 0,
              " %s: MUMPS not interfaced with this installation of PETSc.",
              __func__);
#endif
    break;

  default:
    bft_error(__FILE__, __LINE__, 0,
              " %s: Iterative solver not interfaced with PETSc.", __func__);
  }

  /* 3) Additional settings arising from command lines */
  switch (slesp.solver) {

  case CS_PARAM_ITSOL_GMRES: /* Preconditioned GMRES */
#if PETSC_VERSION_GE(3,7,0)
    PetscOptionsSetValue(NULL, "-ksp_gmres_modifiedgramschmidt", "1");
#else
    PetscOptionsSetValue("-ksp_gmres_modifiedgramschmidt", "1");
#endif
    break;

  default:
    break; /* Nothing to do. Settings performed with another mechanism */

  }

  /* Apply modifications to the KSP structure given with command lines.
   * This setting stands for a first setting and may be overwritten with
   * parameters stored in the structure cs_param_sles_t
   *
   * Automatic monitoring
   *  PetscOptionsSetValue(NULL, "-ksp_monitor", "");
   *
   */

  KSPSetFromOptions(ksp);

  /* Apply settings from the cs_param_sles_t structure */
  switch (slesp.solver) {

  case CS_PARAM_ITSOL_GMRES: /* Preconditioned GMRES */
    {
      const int  n_max_restart = 40;

      KSPGMRESSetRestart(ksp, n_max_restart);
    }
    break;

#if defined(PETSC_HAVE_MUMPS)
  case CS_PARAM_ITSOL_MUMPS:
    {
      PC  pc;
      KSPGetPC(ksp, &pc);
      PCSetType(pc, PCLU);
      PCFactorSetMatSolverType(pc, MATSOLVERMUMPS);
    }
    break;

  case CS_PARAM_ITSOL_MUMPS_LDLT:
    {
      PC  pc;
      KSPGetPC(ksp, &pc);
      MatSetOption(a, MAT_SPD, PETSC_TRUE); /* set MUMPS id%SYM=1 */
      PCSetType(pc, PCCHOLESKY);

      PCFactorSetMatSolverType(pc, MATSOLVERMUMPS);
      PCFactorSetUpMatSolverType(pc); /* call MatGetFactor() to create F */
    }
    break;
#endif

  default:
    break; /* Nothing else to do */
  }

  /* Set KSP tolerances */
  PetscReal rtol, abstol, dtol;
  PetscInt  maxit;
  KSPGetTolerances(ksp, &rtol, &abstol, &dtol, &maxit);
  KSPSetTolerances(ksp,
                   slesp.eps,          /* relative convergence tolerance */
                   abstol,             /* absolute convergence tolerance */
                   dtol,               /* divergence tolerance */
                   slesp.n_max_iter);  /* max number of iterations */

}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Set PETSc solver and preconditioner
 *
 * \param[in, out] context  pointer to optional (untyped) value or structure
 * \param[in, out] a        pointer to PETSc Matrix context
 * \param[in, out] ksp      pointer to PETSc KSP context
 */
/*----------------------------------------------------------------------------*/

static void
_petsc_setup_hook(void   *context,
                  Mat     a,
                  KSP     ksp)
{
  cs_equation_param_t  *eqp = (cs_equation_param_t *)context;
  cs_param_sles_t  slesp = eqp->sles_param;

  cs_fp_exception_disable_trap(); /* Avoid trouble with a too restrictive
                                     SIGFPE detection */

  /* 1) Set the solver */
  _petsc_set_krylov_solver(slesp, a, ksp);

  /* Sanity checks */
  if (cs_glob_n_ranks > 1 && slesp.solver_class == CS_PARAM_SLES_CLASS_PETSC) {

    if (slesp.precond == CS_PARAM_PRECOND_ILU0 ||
        slesp.precond == CS_PARAM_PRECOND_ICC0) {
#if defined(PETSC_HAVE_HYPRE)
      slesp.solver_class = CS_PARAM_SLES_CLASS_HYPRE;
#else
      slesp.precond = CS_PARAM_PRECOND_BJACOB_ILU0;
      cs_base_warn(__FILE__, __LINE__);
      cs_log_printf(CS_LOG_DEFAULT,
                    " %s: Eq. %s: Modify the requested preconditioner to"
                    " enable a parallel computation with PETSC.\n"
                    " Switch to a block jacobi preconditioner.\n"
                    " Please check your settings.", __func__, eqp->name);
#endif
    }
    else if (slesp.precond == CS_PARAM_PRECOND_SSOR) {

      slesp.precond = CS_PARAM_PRECOND_BJACOB_SGS;
      cs_base_warn(__FILE__, __LINE__);
      cs_log_printf(CS_LOG_DEFAULT,
                    " %s: Eq. %s: Modify the requested preconditioner to"
                    " enable a parallel computation with PETSC.\n"
                    " Switch to a block jacobi preconditioner.\n"
                    " Please check your settings.", __func__, eqp->name);

    }

  } /* Advanced check for parallel run */

  /* 2) Set the preconditioner */
  PC  pc;
  KSPGetPC(ksp, &pc);
  _petsc_set_pc_type(slesp, eqp->name, pc);

  /* 3) Set PC options from command line */
  _petsc_set_pc_options_from_command_line(slesp);

  /* Apply modifications to the PC structure given with command lines.
   * This setting stands for a first setting and may be overwritten with
   * parameters stored in the structure cs_param_sles_t
   * To get the last word use cs_user_sles_petsc_hook()  */
  PCSetFromOptions(pc);

  /* 4) User function for additional settings */
  cs_user_sles_petsc_hook((void *)eqp, a, ksp);

  /* Dump the setup related to PETSc in a specific file */
  if (!slesp.setup_done) {

    KSPSetUp(ksp);

    cs_sles_petsc_log_setup(ksp);
    eqp->sles_param.setup_done = true;
  }

  cs_fp_exception_restore_trap(); /* Avoid trouble with a too restrictive
                                     SIGFPE detection */
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Common settings for block preconditioning (when a system is split
 *         according to the vector component x,y,z)
 *
 * \param[in, out] context  pointer to optional (untyped) value or structure
 * \param[in, out] a        pointer to PETSc Matrix context
 * \param[in, out] ksp      pointer to PETSc KSP context
 */
/*----------------------------------------------------------------------------*/

static void
_petsc_common_block_hook(cs_param_sles_t    slesp,
                         KSP                ksp,
                         PC                 pc,
                         KSP              **xyz_subksp)
{
  /* Set KSP tolerances */
  PetscReal rtol, abstol, dtol;
  PetscInt  maxit;
  KSPGetTolerances(ksp, &rtol, &abstol, &dtol, &maxit);
  KSPSetTolerances(ksp,
                   slesp.eps,         /* relative convergence tolerance */
                   abstol,            /* absolute convergence tolerance */
                   dtol,              /* divergence tolerance */
                   slesp.n_max_iter); /* max number of iterations */

  switch (slesp.resnorm_type) {

  case CS_PARAM_RESNORM_NORM2_RHS: /* Try to have "true" norm */
    KSPSetNormType(ksp, KSP_NORM_UNPRECONDITIONED);
    break;
  case CS_PARAM_RESNORM_NONE:
    KSPSetNormType(ksp, KSP_NORM_NONE);
    break;
  default:
    KSPSetNormType(ksp, KSP_NORM_UNPRECONDITIONED);
    break;

  }

  PCSetType(pc, PCFIELDSPLIT);
  PCFieldSplitSetType(pc, PC_COMPOSITE_ADDITIVE);

  /* Apply modifications to the KSP structure */
  PetscInt  id, n_split;

  PCFieldSplitSetBlockSize(pc, 3);
  id = 0;
  PCFieldSplitSetFields(pc, "x", 1, &id, &id);
  id = 1;
  PCFieldSplitSetFields(pc, "y", 1, &id, &id);
  id = 2;
  PCFieldSplitSetFields(pc, "z", 1, &id, &id);

  PCFieldSplitGetSubKSP(pc, &n_split, xyz_subksp);
  assert(n_split == 3);

}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Function pointer: setup hook for setting PETSc solver and
 *         preconditioner.
 *         Case of multiplicative AMG block preconditioner for a CG with GAMG
 *         as AMG type
 *
 * \param[in, out] context  pointer to optional (untyped) value or structure
 * \param[in, out] a        pointer to PETSc Matrix context
 * \param[in, out] ksp      pointer to PETSc KSP context
 */
/*----------------------------------------------------------------------------*/

static void
_petsc_amg_block_gamg_hook(void     *context,
                           Mat       a,
                           KSP       ksp)
{
  PC  pc;
  KSP  *xyz_subksp;

  cs_equation_param_t  *eqp = (cs_equation_param_t *)context;
  cs_param_sles_t  slesp = eqp->sles_param;

  assert(eqp->dim == 3);        /* This designed for this case */

  cs_fp_exception_disable_trap(); /* Avoid trouble with a too restrictive
                                     SIGFPE detection */

  /* Set the solver */
  _petsc_set_krylov_solver(slesp, a, ksp);

  /* Common settings to block preconditionner */
  KSPGetPC(ksp, &pc);
  _petsc_common_block_hook(slesp,
                           ksp,
                           pc,
                           &xyz_subksp);

  /* Predefined settings when using AMG as a preconditioner */
  _petsc_pcgamg_hook();

  PC  _pc;
  for (PetscInt id = 0; id < 3; id++) {

    KSP  _ksp = xyz_subksp[id];
    KSPSetType(_ksp, KSPPREONLY);
    KSPGetPC(_ksp, &_pc);
    PCSetType(_pc, PCGAMG);

  }

  /* User function for additional settings */
  cs_user_sles_petsc_hook(context, a, ksp);

  PCSetFromOptions(pc);
  KSPSetFromOptions(ksp);
  KSPSetUp(ksp);

  /* Dump the setup related to PETSc in a specific file */
  if (!slesp.setup_done) {
    cs_sles_petsc_log_setup(ksp);
    slesp.setup_done = true;
  }

  PetscFree(xyz_subksp);

  cs_fp_exception_restore_trap(); /* Avoid trouble with a too restrictive
                                     SIGFPE detection */
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Function pointer: setup hook for setting PETSc solver and
 *         preconditioner.
 *         Case of multiplicative AMG block preconditioner for a CG with boomer
 *         as AMG type
 *
 * \param[in, out] context  pointer to optional (untyped) value or structure
 * \param[in, out] a        pointer to PETSc Matrix context
 * \param[in, out] ksp      pointer to PETSc KSP context
 */
/*----------------------------------------------------------------------------*/

static void
_petsc_amg_block_boomer_hook(void     *context,
                             Mat       a,
                             KSP       ksp)
{
  PC  pc;
  KSP  *xyz_subksp;

  cs_equation_param_t  *eqp = (cs_equation_param_t *)context;
  cs_param_sles_t  slesp = eqp->sles_param;

  assert(eqp->dim == 3);        /* This designed for this case */

  cs_fp_exception_disable_trap(); /* Avoid trouble with a too restrictive
                                     SIGFPE detection */

  /* Set the solver */
  _petsc_set_krylov_solver(slesp, a, ksp);

  /* Common settings to block preconditionner */
  KSPGetPC(ksp, &pc);
  _petsc_common_block_hook(slesp,
                           ksp,
                           pc,
                           &xyz_subksp);

  /* Predefined settings when using AMG as a preconditioner */
  _petsc_pchypre_hook();

  PC  _pc;
  for (PetscInt id = 0; id < 3; id++) {

    KSP  _ksp = xyz_subksp[id];
    KSPSetType(_ksp, KSPPREONLY);
    KSPGetPC(_ksp, &_pc);
    PCSetType(_pc, PCHYPRE);
    PCHYPRESetType(_pc, "boomeramg");

  }

  /* User function for additional settings */
  cs_user_sles_petsc_hook(context, a, ksp);

  PCSetFromOptions(pc);
  KSPSetFromOptions(ksp);
  KSPSetUp(ksp);

  /* Dump the setup related to PETSc in a specific file */
  if (!slesp.setup_done) {
    cs_sles_petsc_log_setup(ksp);
    slesp.setup_done = true;
  }

  PetscFree(xyz_subksp);

  cs_fp_exception_restore_trap(); /* Avoid trouble with a too restrictive
                                     SIGFPE detection */
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Function pointer: setup hook for setting PETSc solver and
 *         preconditioner.
 *         Case of block Jacobi preconditioner
 *
 * \param[in, out] context  pointer to optional (untyped) value or structure
 * \param[in, out] a        pointer to PETSc Matrix context
 * \param[in, out] ksp      pointer to PETSc KSP context
 */
/*----------------------------------------------------------------------------*/

static void
_petsc_block_jacobi_hook(void     *context,
                         Mat       a,
                         KSP       ksp)
{
  PC  pc;
  KSP  *xyz_subksp;
  cs_equation_param_t  *eqp = (cs_equation_param_t *)context;
  cs_param_sles_t  slesp = eqp->sles_param;

  assert(eqp->dim == 3);        /* This designed for this case */

  cs_fp_exception_disable_trap(); /* Avoid trouble with a too restrictive
                                     SIGFPE detection */

  /* Set the solver (tolerance and max_it too) */
  _petsc_set_krylov_solver(slesp, a, ksp);

  /* Common settings to block preconditionner */
  KSPGetPC(ksp, &pc);
  _petsc_common_block_hook(slesp,
                           ksp,
                           pc,
                           &xyz_subksp);

  KSPSetUp(ksp);

  /* Predefined settings when using block-ILU as a preconditioner */
  PC  _pc;
  for (PetscInt id = 0; id < 3; id++) {

    KSP  _ksp = xyz_subksp[id];
    KSPSetType(_ksp, KSPPREONLY);
    KSPGetPC(_ksp, &_pc);
    if (slesp.solver_class == CS_PARAM_SLES_CLASS_HYPRE) {
      PCSetType(_pc, PCHYPRE);
      PCHYPRESetType(_pc, "euclid"); /* ILU(1) by default */
    }
    else {

      PC  _subpc;
      KSP *_subksp;

      PCSetType(_pc, PCBJACOBI); /* Default for the block is an ILU(0) */
      KSPSetUp(_ksp);
      PCBJacobiGetSubKSP(_pc, NULL, NULL, &_subksp);
      KSPSetType(_subksp[0], KSPPREONLY);
      KSPGetPC(_subksp[0], &_subpc);

      if (slesp.precond == CS_PARAM_PRECOND_BJACOB_SGS)
        PCSetType(_subpc, PCEISENSTAT);
      else if (slesp.precond == CS_PARAM_PRECOND_BJACOB_ILU0) {
        PCFactorSetLevels(_pc, 0);
        PCFactorSetReuseOrdering(_pc, PETSC_TRUE);
        PCFactorSetMatOrderingType(_pc, MATORDERING1WD);
      }
      else
        bft_error(__FILE__, __LINE__, 0,
                  " %s: Invalid preconditioner.",__func__);

    }

  }

  PCSetFromOptions(pc);
  PCSetUp(pc);

  /* User function for additional settings */
  cs_user_sles_petsc_hook(context, a, ksp);

  KSPSetFromOptions(ksp);

  /* Dump the setup related to PETSc in a specific file */
  if (!slesp.setup_done) {
    KSPSetUp(ksp);
    cs_sles_petsc_log_setup(ksp);
    slesp.setup_done = true;
  }

  PetscFree(xyz_subksp);

  cs_fp_exception_restore_trap(); /* Avoid trouble with a too restrictive
                                     SIGFPE detection */
}
#endif /* defined(HAVE_PETSC) */

/*----------------------------------------------------------------------------*/
/*!
 * \brief Set parameters for initializing SLES structures used for the
 *        resolution of the linear system.
 *        Case of saturne's own solvers.
 *
 * \param[in, out]  eqp      pointer to a \ref cs_equation_param_t structure
 */
/*----------------------------------------------------------------------------*/

static void
_set_saturne_sles(cs_equation_param_t   *eqp)
{
  cs_param_sles_t  slesp = eqp->sles_param;

  /* 1- Define the preconditioner */
  /*    ========================= */

  int  poly_degree;
  cs_sles_pc_t *pc = NULL;

  switch (slesp.precond) {

  case CS_PARAM_PRECOND_DIAG:
    poly_degree = 0;
    break;

  case CS_PARAM_PRECOND_POLY1:
    poly_degree = 1;
    break;

  case CS_PARAM_PRECOND_POLY2:
    poly_degree = 2;
    break;

  case CS_PARAM_PRECOND_AMG:
    poly_degree = -1;
    switch (slesp.amg_type) {

    case CS_PARAM_AMG_HOUSE_V:
      pc = cs_multigrid_pc_create(CS_MULTIGRID_V_CYCLE);
      break;
    case CS_PARAM_AMG_HOUSE_K:
      if (slesp.solver == CS_PARAM_ITSOL_CG)
        slesp.solver = CS_PARAM_ITSOL_FCG;
      pc = cs_multigrid_pc_create(CS_MULTIGRID_K_CYCLE);
      break;

    default:
      bft_error(__FILE__, __LINE__, 0,
                " %s: Eq: %s; Invalid AMG type with Code_Saturne solvers.",
                __func__, eqp->name);
      break;

    } /* Switch on the type of AMG */
    break; /* AMG as preconditioner */

  case CS_PARAM_PRECOND_GKB_CG:
  case CS_PARAM_PRECOND_GKB_GMRES:
    poly_degree = -1;
    break;

  case CS_PARAM_PRECOND_NONE:
  default:
    poly_degree = -1;       /* None or other */

  } /* Switch on the preconditioner type */

  /* 2- Define the iterative solver */
  /*    =========================== */

  cs_sles_it_t  *it = NULL;
  cs_multigrid_t  *mg = NULL;

  switch (slesp.solver) {

  case CS_PARAM_ITSOL_AMG:
    {
      switch (slesp.amg_type) {

      case CS_PARAM_AMG_HOUSE_V:
        mg = cs_multigrid_define(slesp.field_id,
                                 NULL,
                                 CS_MULTIGRID_V_CYCLE);

        /* Advanced setup (default is specified inside the brackets)
         * for AMG as solver */
        cs_multigrid_set_solver_options
          (mg,
           CS_SLES_JACOBI,   /* descent smoother type (CS_SLES_PCG) */
           CS_SLES_JACOBI,   /* ascent smoother type (CS_SLES_PCG) */
           CS_SLES_PCG,      /* coarse solver type (CS_SLES_PCG) */
           slesp.n_max_iter, /* n max cycles (100) */
           5,                /* n max iter for descent (10) */
           5,                /* n max iter for ascent (10) */
           1000,             /* n max iter coarse solver (10000) */
           0,                /* polynomial precond. degree descent (0) */
           0,                /* polynomial precond. degree ascent (0) */
           -1,               /* polynomial precond. degree coarse (0) */
           1.0,    /* precision multiplier descent (< 0 forces max iters) */
           1.0,    /* precision multiplier ascent (< 0 forces max iters) */
           1);     /* requested precision multiplier coarse (default 1) */
        break;

      case CS_PARAM_AMG_HOUSE_K:
        mg = cs_multigrid_define(slesp.field_id,
                                 NULL,
                                 CS_MULTIGRID_K_CYCLE);

        cs_multigrid_set_solver_options
          (mg,
           CS_SLES_P_SYM_GAUSS_SEIDEL, /* descent smoother */
           CS_SLES_P_SYM_GAUSS_SEIDEL, /* ascent smoother */
           CS_SLES_PCG,                /* coarse smoother */
           slesp.n_max_iter,           /* n_max_cycles */
           1,                          /* n_max_iter_descent, */
           1,                          /* n_max_iter_ascent */
           100,                        /* n_max_iter_coarse */
           0,                          /* poly_degree_descent */
           0,                          /* poly_degree_ascent */
           0,                          /* poly_degree_coarse */
           -1.0,                       /* precision_mult_descent */
           -1.0,                       /* precision_mult_ascent */
           1);                         /* precision_mult_coarse */
        break;

      default:
        bft_error(__FILE__, __LINE__, 0,
                  " %s; eq: %s -- Invalid AMG type with Code_Saturne"
                  " solvers.", __func__, eqp->name);
        break;

      } /* End of switch on the AMG type */

    }
    break; /* AMG as solver */

  case CS_PARAM_ITSOL_BICG:
    it = cs_sles_it_define(slesp.field_id,
                           NULL,
                           CS_SLES_BICGSTAB,
                           poly_degree,
                           slesp.n_max_iter);
    break;

  case CS_PARAM_ITSOL_BICGSTAB2:
    it = cs_sles_it_define(slesp.field_id,
                           NULL,
                           CS_SLES_BICGSTAB2,
                           poly_degree,
                           slesp.n_max_iter);
    break;

  case CS_PARAM_ITSOL_CG:
    it = cs_sles_it_define(slesp.field_id,
                           NULL,
                           CS_SLES_PCG,
                           poly_degree,
                           slesp.n_max_iter);
    break;

  case CS_PARAM_ITSOL_CR3:
    it = cs_sles_it_define(slesp.field_id,
                           NULL,
                           CS_SLES_PCR3,
                           poly_degree,
                           slesp.n_max_iter);
    break;

  case CS_PARAM_ITSOL_FCG:
    it = cs_sles_it_define(slesp.field_id,
                           NULL,
                           CS_SLES_IPCG,
                           poly_degree,
                           slesp.n_max_iter);
    break;

  case CS_PARAM_ITSOL_GAUSS_SEIDEL:
    it = cs_sles_it_define(slesp.field_id,
                           NULL,
                           CS_SLES_P_GAUSS_SEIDEL,
                           -1, /* Not useful to apply a preconditioner */
                           slesp.n_max_iter);
    break;

  case CS_PARAM_ITSOL_GKB_CG:
    it = cs_sles_it_define(slesp.field_id,
                           NULL,
                           CS_SLES_IPCG, /* Flexible CG */
                           poly_degree,
                           slesp.n_max_iter);
    break;

  case CS_PARAM_ITSOL_GKB_GMRES:
    it = cs_sles_it_define(slesp.field_id,
                           NULL,
                           CS_SLES_GMRES, /* Should a flexible GMRES */
                           poly_degree,
                           slesp.n_max_iter);
    break;

  case CS_PARAM_ITSOL_GMRES:
    it = cs_sles_it_define(slesp.field_id,
                           NULL,
                           CS_SLES_GMRES,
                           poly_degree,
                           slesp.n_max_iter);
    break;

  case CS_PARAM_ITSOL_JACOBI:
    it = cs_sles_it_define(slesp.field_id,
                           NULL,
                           CS_SLES_JACOBI,
                           -1, /* Not useful to apply a preconditioner */
                           slesp.n_max_iter);
    break;

  case CS_PARAM_ITSOL_SYM_GAUSS_SEIDEL:
    it = cs_sles_it_define(slesp.field_id,
                           NULL,
                           CS_SLES_P_SYM_GAUSS_SEIDEL,
                           -1, /* Not useful to apply a preconditioner */
                           slesp.n_max_iter);
    break;

  default:
    bft_error(__FILE__, __LINE__, 0,
              " %s: Invalid iterative solver for solving equation %s.\n"
              " Please modify your settings.", __func__, eqp->name);
    break;

  } /* end of switch */

  /* Update the preconditioner settings if needed */
  if (slesp.precond == CS_PARAM_PRECOND_AMG) {

    assert(pc != NULL && it != NULL);

    mg = cs_sles_pc_get_context(pc);
    cs_sles_it_transfer_pc(it, &pc);

    /* Change the default settings for CDO/HHO when used as preconditioner */
    cs_multigrid_set_solver_options
      (mg,
       CS_SLES_PCG,       /* descent smoother */
       CS_SLES_PCG,       /* ascent smoother */
       CS_SLES_PCG,       /* coarse solver */
       slesp.n_max_iter,  /* n_max_cycles */
       4,                 /* n_max_iter_descent, */
       4,                 /* n_max_iter_ascent */
       200,               /* n_max_iter_coarse */
       0,                 /* poly_degree_descent */
       0,                 /* poly_degree_ascent */
       0,                 /* poly_degree_coarse */
       -1.0,              /* precision_mult_descent */
       -1.0,              /* precision_mult_ascent */
       1.0);              /* precision_mult_coarse */

    /* If this is a K-cycle multigrid. Change the default aggregation
       algorithm */
    if (slesp.amg_type == CS_PARAM_AMG_HOUSE_K)
      cs_multigrid_set_coarsening_options(mg,
                                          8,   /* aggregation_limit*/
                                          CS_GRID_COARSENING_SPD_MX,
                                          10,  /* n_max_levels */
                                          50,  /* min_g_cells */
                                          0.,  /* P0P1 relaxation */
                                          0);  /* postprocess */

  } /* AMG as preconditioner */

  /* Define the level of verbosity for SLES structure */
  if (slesp.verbosity > 3) {

    cs_sles_t  *sles = cs_sles_find_or_add(slesp.field_id, NULL);
    cs_sles_it_t  *sles_it = (cs_sles_it_t *)cs_sles_get_context(sles);

    cs_sles_it_set_plot_options(sles_it, eqp->name,
                                true);    /* use_iteration instead of
                                             wall clock time */

  }

}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Set a parameter attached to a keyname in a \ref cs_equation_param_t
 *         structure
 *
 * \param[in]       label    label to identify the error message
 * \param[in, out]  eqp      pointer to a \ref cs_equation_param_t structure
 * \param[in]       key      key related to the member of eq to set
 * \param[in]       keyval   accessor to the value to set
 */
/*----------------------------------------------------------------------------*/

static void
_set_key(const char            *label,
         cs_equation_param_t   *eqp,
         cs_equation_key_t      key,
         const char            *keyval)
{
  const char  emsg[] = " %s: %s equation --> Invalid key value %s for"
    " keyword %s.\n";

  switch(key) {

  case CS_EQKEY_ADV_FORMULATION:
    if (strcmp(keyval, "conservative") == 0)
      eqp->adv_formulation = CS_PARAM_ADVECTION_FORM_CONSERV;
    else if (strcmp(keyval, "non_conservative") == 0)
      eqp->adv_formulation = CS_PARAM_ADVECTION_FORM_NONCONS;
    else if (strcmp(keyval, "skew_symmetric") == 0)
      eqp->adv_formulation = CS_PARAM_ADVECTION_FORM_SKEWSYM;
    else {
      const char *_val = keyval;
      bft_error(__FILE__, __LINE__, 0,
                emsg, __func__, label, _val, "CS_EQKEY_ADV_FORMULATION");
    }
    break;

  case CS_EQKEY_ADV_SCHEME:
    if (strcmp(keyval, "upwind") == 0)
      eqp->adv_scheme = CS_PARAM_ADVECTION_SCHEME_UPWIND;
    else if (strcmp(keyval, "samarskii") == 0)
      eqp->adv_scheme = CS_PARAM_ADVECTION_SCHEME_SAMARSKII;
    else if (strcmp(keyval, "sg") == 0)
      eqp->adv_scheme = CS_PARAM_ADVECTION_SCHEME_SG;
    else if (strcmp(keyval, "centered") == 0)
      eqp->adv_scheme = CS_PARAM_ADVECTION_SCHEME_CENTERED;
    else if (strcmp(keyval, "mix_centered_upwind") == 0)
      eqp->adv_scheme = CS_PARAM_ADVECTION_SCHEME_MIX_CENTERED_UPWIND;
    else if (strcmp(keyval, "cip") == 0) {
      eqp->adv_scheme = CS_PARAM_ADVECTION_SCHEME_CIP;
      /* Automatically switch to a non-conservative formulation */
      eqp->adv_formulation = CS_PARAM_ADVECTION_FORM_NONCONS;
    }
    else if (strcmp(keyval, "cip_cw") == 0) {
      eqp->adv_scheme = CS_PARAM_ADVECTION_SCHEME_CIP_CW;
      /* Automatically switch to a non-conservative formulation */
      eqp->adv_formulation = CS_PARAM_ADVECTION_FORM_NONCONS;
    }
    else {
      const char *_val = keyval;
      bft_error(__FILE__, __LINE__, 0,
                emsg, __func__, label, _val, "CS_EQKEY_ADV_SCHEME");
    }
    break;

  case CS_EQKEY_ADV_UPWIND_PORTION:
    eqp->upwind_portion = atof(keyval);
    break;

  case CS_EQKEY_AMG_TYPE:
    if (strcmp(keyval, "none") == 0 || strcmp(keyval, "") == 0)
      eqp->sles_param.amg_type = CS_PARAM_AMG_NONE;
    else if (strcmp(keyval, "v_cycle") == 0) {
      eqp->sles_param.amg_type = CS_PARAM_AMG_HOUSE_V;
      eqp->sles_param.solver_class = CS_PARAM_SLES_CLASS_CS;
    }
    else if (strcmp(keyval, "k_cycle") == 0) {
      eqp->sles_param.amg_type = CS_PARAM_AMG_HOUSE_K;
      eqp->sles_param.solver_class = CS_PARAM_SLES_CLASS_CS;
    }
    else if (strcmp(keyval, "boomer") == 0) {
      eqp->sles_param.amg_type = CS_PARAM_AMG_HYPRE_BOOMER;
      eqp->sles_param.solver_class = CS_PARAM_SLES_CLASS_HYPRE;
    }
    else if (strcmp(keyval, "gamg") == 0) {
      eqp->sles_param.amg_type = CS_PARAM_AMG_PETSC_GAMG;
      eqp->sles_param.solver_class = CS_PARAM_SLES_CLASS_PETSC;
    }
    else if (strcmp(keyval, "pcmg") == 0) {
      eqp->sles_param.amg_type = CS_PARAM_AMG_PETSC_PCMG;
      eqp->sles_param.solver_class = CS_PARAM_SLES_CLASS_PETSC;
    }
    else {
      const char *_val = keyval;
      bft_error(__FILE__, __LINE__, 0,
                emsg, __func__, label, _val, "CS_EQKEY_AMG_TYPE");
    }
    break;

  case CS_EQKEY_BC_ENFORCEMENT:
    if (strcmp(keyval, "algebraic") == 0)
      eqp->default_enforcement = CS_PARAM_BC_ENFORCE_ALGEBRAIC;
    else if (strcmp(keyval, "penalization") == 0)
      eqp->default_enforcement = CS_PARAM_BC_ENFORCE_PENALIZED;
    else if (strcmp(keyval, "weak_sym") == 0)
      eqp->default_enforcement = CS_PARAM_BC_ENFORCE_WEAK_SYM;
    else if (strcmp(keyval, "weak") == 0)
      eqp->default_enforcement = CS_PARAM_BC_ENFORCE_WEAK_NITSCHE;
    else {
      const char *_val = keyval;
      bft_error(__FILE__, __LINE__, 0,
                emsg, __func__, label, _val, "CS_EQKEY_BC_ENFORCEMENT");
    }
    break;

  case CS_EQKEY_BC_QUADRATURE:
    {
      cs_quadrature_type_t  qtype = CS_QUADRATURE_NONE;

      if (strcmp(keyval, "bary") == 0)
        qtype = CS_QUADRATURE_BARY;
      else if (strcmp(keyval, "bary_subdiv") == 0)
        qtype = CS_QUADRATURE_BARY_SUBDIV;
      else if (strcmp(keyval, "higher") == 0)
        qtype = CS_QUADRATURE_HIGHER;
      else if (strcmp(keyval, "highest") == 0)
        qtype = CS_QUADRATURE_HIGHEST;
      else {
        const char *_val = keyval;
        bft_error(__FILE__, __LINE__, 0,
                  emsg, __func__, label, _val, "CS_EQKEY_BC_QUADRATURE");
      }

      for (int i = 0; i < eqp->n_bc_defs; i++)
        cs_xdef_set_quadrature(eqp->bc_defs[i], qtype);

    }
    break;

  case CS_EQKEY_BC_STRONG_PENA_COEFF:
    eqp->strong_pena_bc_coeff = atof(keyval);
    if (eqp->strong_pena_bc_coeff < 1.)
      bft_error(__FILE__, __LINE__, 0,
                " %s: Invalid value of the penalization coefficient %5.3e\n"
                " This should be positive and large.",
                __func__, eqp->strong_pena_bc_coeff);
    break;

  case CS_EQKEY_BC_WEAK_PENA_COEFF:
    eqp->weak_pena_bc_coeff = atof(keyval);
    if (eqp->weak_pena_bc_coeff < 0.)
      bft_error(__FILE__, __LINE__, 0,
                " %s: Invalid value of the penalization coefficient %5.3e\n"
                " This should be positive.",
                __func__, eqp->weak_pena_bc_coeff);
    break;

  case CS_EQKEY_DO_LUMPING:
    if (strcmp(keyval, "true") == 0 || strcmp(keyval, "1") == 0)
      eqp->do_lumping = true;
    else
      eqp->do_lumping = false;  /* Should be the default behavior */
    break;

  case CS_EQKEY_DOF_REDUCTION:
    if (strcmp(keyval, "derham") == 0)
      eqp->dof_reduction = CS_PARAM_REDUCTION_DERHAM;
    else if (strcmp(keyval, "average") == 0)
      eqp->dof_reduction = CS_PARAM_REDUCTION_AVERAGE;
    else {
      const char *_val = keyval;
      bft_error(__FILE__, __LINE__, 0,
                emsg, __func__, label, _val, "CS_EQKEY_DOF_REDUCTION");
    }
    break;

  case CS_EQKEY_EXTRA_OP:
    if (strcmp(keyval, "balance") == 0)
      eqp->process_flag |= CS_EQUATION_POST_BALANCE;
    else if (strcmp(keyval, "peclet") == 0)
      eqp->process_flag |= CS_EQUATION_POST_PECLET;
    else if (strcmp(keyval, "upwind_coef") == 0)
      eqp->process_flag |= CS_EQUATION_POST_UPWIND_COEF;
    else if (strcmp(keyval, "normal_flux") == 0)
      eqp->process_flag |= CS_EQUATION_POST_NORMAL_FLUX;
    else {
      const char *_val = keyval;
      bft_error(__FILE__, __LINE__, 0,
                emsg, __func__, label, _val, "CS_EQKEY_EXTRA_OP");
    }
    break;

  case CS_EQKEY_HODGE_DIFF_ALGO:
    if (strcmp(keyval,"cost") == 0 || strcmp(keyval,"ocs") == 0)
      eqp->diffusion_hodge.algo = CS_PARAM_HODGE_ALGO_COST;
    else if (strcmp(keyval, "ocs2") == 0)
      eqp->diffusion_hodge.algo = CS_PARAM_HODGE_ALGO_OCS2;
    else if (strcmp(keyval, "bubble") == 0)
      eqp->diffusion_hodge.algo = CS_PARAM_HODGE_ALGO_BUBBLE;
    else if (strcmp(keyval, "voronoi") == 0)
      eqp->diffusion_hodge.algo = CS_PARAM_HODGE_ALGO_VORONOI;
    else if (strcmp(keyval, "wbs") == 0)
      eqp->diffusion_hodge.algo = CS_PARAM_HODGE_ALGO_WBS;
    else if (strcmp(keyval, "auto") == 0)
      eqp->diffusion_hodge.algo = CS_PARAM_HODGE_ALGO_AUTO;
    else {
      const char *_val = keyval;
      bft_error(__FILE__, __LINE__, 0,
                emsg, __func__, label, _val, "CS_EQKEY_HODGE_DIFF_ALGO");
    }
    break;

  case CS_EQKEY_HODGE_DIFF_COEF:
    if (strcmp(keyval, "dga") == 0)
      eqp->diffusion_hodge.coef = 1./3.;
    else if (strcmp(keyval, "sushi") == 0)
      eqp->diffusion_hodge.coef = 1./sqrt(3.);
    else if (strcmp(keyval, "gcr") == 0)
      eqp->diffusion_hodge.coef = 1.0;
    else if (strcmp(keyval, "frac23") == 0 || strcmp(keyval, "2/3") == 0)
      eqp->diffusion_hodge.coef = 2./3.;
    else
      eqp->diffusion_hodge.coef = atof(keyval);
    break;

  case CS_EQKEY_HODGE_TIME_ALGO:
    if (strcmp(keyval, "voronoi") == 0)
      eqp->time_hodge.algo = CS_PARAM_HODGE_ALGO_VORONOI;
    else if (strcmp(keyval, "wbs") == 0)
      eqp->time_hodge.algo = CS_PARAM_HODGE_ALGO_WBS;
    else {
      const char *_val = keyval;
      bft_error(__FILE__, __LINE__, 0,
                emsg, __func__, label, _val, "CS_EQKEY_HODGE_TIME_ALGO");
    }
    break;

  case CS_EQKEY_HODGE_REAC_ALGO:
    if (strcmp(keyval, "voronoi") == 0)
      eqp->reaction_hodge.algo = CS_PARAM_HODGE_ALGO_VORONOI;
    else if (strcmp(keyval, "wbs") == 0)
      eqp->reaction_hodge.algo = CS_PARAM_HODGE_ALGO_WBS;
    else {
      const char *_val = keyval;
      bft_error(__FILE__, __LINE__, 0,
                emsg, __func__, label, _val, "CS_EQKEY_HODGE_REAC_ALGO");
    }
    break;

  case CS_EQKEY_ITSOL:
    if (strcmp(keyval, "amg") == 0)
      eqp->sles_param.solver = CS_PARAM_ITSOL_AMG;
    else if (strcmp(keyval, "bicg") == 0)
      eqp->sles_param.solver = CS_PARAM_ITSOL_BICG;
    else if (strcmp(keyval, "bicgstab2") == 0)
      eqp->sles_param.solver = CS_PARAM_ITSOL_BICGSTAB2;
    else if (strcmp(keyval, "cg") == 0)
      eqp->sles_param.solver = CS_PARAM_ITSOL_CG;
    else if (strcmp(keyval, "cr3") == 0)
      eqp->sles_param.solver = CS_PARAM_ITSOL_CR3;
    else if (strcmp(keyval, "fcg") == 0)
      eqp->sles_param.solver = CS_PARAM_ITSOL_FCG;
    else if (strcmp(keyval, "gauss_seidel") == 0 ||
             strcmp(keyval, "gs") == 0)
      eqp->sles_param.solver = CS_PARAM_ITSOL_GAUSS_SEIDEL;
    else if (strcmp(keyval, "gmres") == 0)
      eqp->sles_param.solver = CS_PARAM_ITSOL_GMRES;
    else if (strcmp(keyval, "fgmres") == 0)
      eqp->sles_param.solver = CS_PARAM_ITSOL_FGMRES;
    else if (strcmp(keyval, "jacobi") == 0)
      eqp->sles_param.solver = CS_PARAM_ITSOL_JACOBI;
    else if (strcmp(keyval, "minres") == 0)
      eqp->sles_param.solver = CS_PARAM_ITSOL_MINRES;
    else if (strcmp(keyval, "mumps") == 0) {
      eqp->sles_param.solver = CS_PARAM_ITSOL_MUMPS;
      eqp->sles_param.precond = CS_PARAM_PRECOND_NONE;
      eqp->sles_param.solver_class = CS_PARAM_SLES_CLASS_PETSC;
    }
    else if (strcmp(keyval, "mumps_ldlt") == 0) {
      eqp->sles_param.solver = CS_PARAM_ITSOL_MUMPS_LDLT;
      eqp->sles_param.precond = CS_PARAM_PRECOND_NONE;
      eqp->sles_param.solver_class = CS_PARAM_SLES_CLASS_PETSC;
    }
    else if (strcmp(keyval, "none") == 0)
      eqp->sles_param.solver = CS_PARAM_ITSOL_NONE;
    else {
      const char *_val = keyval;
      bft_error(__FILE__, __LINE__, 0,
                emsg, __func__, label, _val, "CS_EQKEY_ITSOL");
    }
    break;

  case CS_EQKEY_ITSOL_MAX_ITER:
    eqp->sles_param.n_max_iter = atoi(keyval);
    break;

  case CS_EQKEY_ITSOL_EPS:
    eqp->sles_param.eps = atof(keyval);
    break;

  case CS_EQKEY_ITSOL_RESNORM_TYPE:
    if (strcmp(keyval, "none") == 0 || strcmp(keyval, "false") == 0)
      eqp->sles_param.resnorm_type = CS_PARAM_RESNORM_NONE;
    else if (strcmp(keyval, "rhs") == 0)
      eqp->sles_param.resnorm_type = CS_PARAM_RESNORM_NORM2_RHS;
    else if (strcmp(keyval, "weighted_rhs") == 0 ||
             strcmp(keyval, "weighted") == 0)
      eqp->sles_param.resnorm_type = CS_PARAM_RESNORM_WEIGHTED_RHS;
    else if (strcmp(keyval, "filtered_rhs") == 0 ||
             strcmp(keyval, "filtered") == 0)
      eqp->sles_param.resnorm_type = CS_PARAM_RESNORM_FILTERED_RHS;
    else {
      const char *_val = keyval;
      bft_error(__FILE__, __LINE__, 0,
                emsg, __func__, label, _val, "CS_EQKEY_ITSOL_RESNORM_TYPE");
    }
    break;

  case CS_EQKEY_OMP_ASSEMBLY_STRATEGY:
    if (strcmp(keyval, "critical") == 0)
      eqp->omp_assembly_choice = CS_PARAM_ASSEMBLE_OMP_CRITICAL;
    else if (strcmp(keyval, "atomic") == 0)
      eqp->sles_param.precond = CS_PARAM_ASSEMBLE_OMP_ATOMIC;
    else {
      const char *_val = keyval;
      bft_error(__FILE__, __LINE__, 0,
                emsg, __func__, label, _val, "CS_EQKEY_OMP_ASSEMBLY_STRATEGY");
    }
    break;

  case CS_EQKEY_PRECOND:
    if (strcmp(keyval, "none") == 0) {
      eqp->sles_param.precond = CS_PARAM_PRECOND_NONE;
      eqp->sles_param.amg_type = CS_PARAM_AMG_NONE;
    }
    else if (strcmp(keyval, "jacobi") == 0)
      eqp->sles_param.precond = CS_PARAM_PRECOND_DIAG;
    else if (strcmp(keyval, "block_jacobi") == 0 ||
             strcmp(keyval, "block_jacobi_ilu0") == 0 ||
             strcmp(keyval, "jacobi_block") == 0)
      eqp->sles_param.precond = CS_PARAM_PRECOND_BJACOB_ILU0;
    else if (strcmp(keyval, "block_jacobi_sgs") == 0 ||
             strcmp(keyval, "block_jacobi_ssor") == 0)
      eqp->sles_param.precond = CS_PARAM_PRECOND_BJACOB_SGS;
    else if (strcmp(keyval, "poly1") == 0)
      eqp->sles_param.precond = CS_PARAM_PRECOND_POLY1;
    else if (strcmp(keyval, "poly2") == 0)
      eqp->sles_param.precond = CS_PARAM_PRECOND_POLY2;
    else if (strcmp(keyval, "ssor") == 0)
      eqp->sles_param.precond = CS_PARAM_PRECOND_SSOR;
    else if (strcmp(keyval, "ilu0") == 0)
      eqp->sles_param.precond = CS_PARAM_PRECOND_ILU0;
    else if (strcmp(keyval, "icc0") == 0)
      eqp->sles_param.precond = CS_PARAM_PRECOND_ICC0;
    else if (strcmp(keyval, "amg") == 0) {

      eqp->sles_param.precond = CS_PARAM_PRECOND_AMG;

      /* Set the default choice */
      if (eqp->sles_param.solver_class == CS_PARAM_SLES_CLASS_CS)
        eqp->sles_param.amg_type = CS_PARAM_AMG_HOUSE_K;
      else if (eqp->sles_param.solver_class == CS_PARAM_SLES_CLASS_PETSC)
        eqp->sles_param.amg_type = CS_PARAM_AMG_PETSC_GAMG;
#if defined(PETSC_HAVE_HYPRE)
      /* Up to now HYPRE is available only through the PETSc interface */
      else if (eqp->sles_param.solver_class == CS_PARAM_SLES_CLASS_HYPRE)
        eqp->sles_param.amg_type = CS_PARAM_AMG_HYPRE_BOOMER;
#endif
      else
        bft_error(__FILE__, __LINE__, 0,
                  "%s: Invalid choice of AMG type. Please modify your settings",
                  __func__);

    }
    else if (strcmp(keyval, "amg_block") == 0 ||
             strcmp(keyval, "block_amg") == 0) {
      if (eqp->dim == 1) {  /* Switch to a classical AMG preconditioner */

        eqp->sles_param.precond = CS_PARAM_PRECOND_AMG;

        /* Set the default choice */
        if (eqp->sles_param.solver_class == CS_PARAM_SLES_CLASS_CS)
          eqp->sles_param.amg_type = CS_PARAM_AMG_HOUSE_K;
        if (eqp->sles_param.solver_class == CS_PARAM_SLES_CLASS_PETSC)
          eqp->sles_param.amg_type = CS_PARAM_AMG_PETSC_GAMG;

      }
      else {

        eqp->sles_param.precond = CS_PARAM_PRECOND_AMG_BLOCK;

        /* Set the default choice */
#if defined(PETSC_HAVE_HYPRE)
        eqp->sles_param.amg_type = CS_PARAM_AMG_HYPRE_BOOMER;
        eqp->sles_param.solver_class = CS_PARAM_SLES_CLASS_HYPRE;
#else
        eqp->sles_param.amg_type = CS_PARAM_AMG_PETSC_GAMG;
        eqp->sles_param.solver_class = CS_PARAM_SLES_CLASS_PETSC;
#endif

      }
    }
    else if (strcmp(keyval, "as") == 0)
      eqp->sles_param.precond = CS_PARAM_PRECOND_AS;
    else {
      const char *_val = keyval;
      bft_error(__FILE__, __LINE__, 0,
                emsg, __func__, label, _val, "CS_EQKEY_PRECOND");
    }
    break;

  case CS_EQKEY_SLES_VERBOSITY: /* "verbosity" for SLES structures */
    eqp->sles_param.verbosity = atoi(keyval);
    break;

  case CS_EQKEY_SOLVER_FAMILY:
    if (strcmp(keyval, "cs") == 0)
      eqp->sles_param.solver_class = CS_PARAM_SLES_CLASS_CS;
    else if (strcmp(keyval, "petsc") == 0)
      eqp->sles_param.solver_class = CS_PARAM_SLES_CLASS_PETSC;
    else if (strcmp(keyval, "hypre") == 0)
      eqp->sles_param.solver_class = CS_PARAM_SLES_CLASS_HYPRE;
    else {
      const char *_val = keyval;
      bft_error(__FILE__, __LINE__, 0,
                emsg, __func__, label, _val, "CS_EQKEY_SOLVER_FAMILY");
    }
    break;

  case CS_EQKEY_SPACE_SCHEME:
    if (strcmp(keyval, "cdo_vb") == 0) {
      eqp->space_scheme = CS_SPACE_SCHEME_CDOVB;
      eqp->space_poly_degree = 0;
      eqp->time_hodge.type = CS_PARAM_HODGE_TYPE_VPCD;
      eqp->diffusion_hodge.type = CS_PARAM_HODGE_TYPE_EPFD;
      eqp->diffusion_hodge.algo = CS_PARAM_HODGE_ALGO_BUBBLE;
      eqp->diffusion_hodge.coef = 2*cs_math_1ov3;
      eqp->reaction_hodge.type = CS_PARAM_HODGE_TYPE_VPCD;
      eqp->reaction_hodge.algo = CS_PARAM_HODGE_ALGO_WBS;
    }
    else if (strcmp(keyval, "cdo_vcb") == 0) {
      eqp->space_scheme = CS_SPACE_SCHEME_CDOVCB;
      eqp->space_poly_degree = 0;
      eqp->time_hodge.type = CS_PARAM_HODGE_TYPE_VPCD;
      eqp->diffusion_hodge.algo = CS_PARAM_HODGE_ALGO_WBS;
      eqp->diffusion_hodge.type = CS_PARAM_HODGE_TYPE_VC;
      eqp->reaction_hodge.type = CS_PARAM_HODGE_TYPE_VPCD;
      eqp->reaction_hodge.algo = CS_PARAM_HODGE_ALGO_WBS;
    }
    else if (strcmp(keyval, "cdo_fb") == 0) {
      eqp->space_scheme = CS_SPACE_SCHEME_CDOFB;
      eqp->space_poly_degree = 0;
      eqp->time_hodge.type = CS_PARAM_HODGE_TYPE_CPVD;
      eqp->time_hodge.algo = CS_PARAM_HODGE_ALGO_VORONOI;
      eqp->reaction_hodge.algo = CS_PARAM_HODGE_ALGO_VORONOI;
      eqp->diffusion_hodge.type = CS_PARAM_HODGE_TYPE_EDFP;
    }
    else if (strcmp(keyval, "cdo_eb") == 0) {
      eqp->space_scheme = CS_SPACE_SCHEME_CDOEB;
      eqp->space_poly_degree = 0;
      eqp->time_hodge.type = CS_PARAM_HODGE_TYPE_EPFD;
      eqp->diffusion_hodge.type = CS_PARAM_HODGE_TYPE_FPED;
      eqp->reaction_hodge.type = CS_PARAM_HODGE_TYPE_EPFD;
    }

    /* Only diffusion is implemented for HHO schemes up to now */
    else if (strcmp(keyval, "hho_p0") == 0) {
      eqp->space_scheme = CS_SPACE_SCHEME_HHO_P0;
      eqp->space_poly_degree = 0;
      eqp->time_hodge.type = CS_PARAM_HODGE_TYPE_CPVD;
      eqp->diffusion_hodge.type = CS_PARAM_HODGE_TYPE_EDFP;
    }
    else if (strcmp(keyval, "hho_p1") == 0) {
      eqp->space_scheme = CS_SPACE_SCHEME_HHO_P1;
      eqp->space_poly_degree = 1;
      eqp->time_hodge.type = CS_PARAM_HODGE_TYPE_CPVD;
      eqp->diffusion_hodge.type = CS_PARAM_HODGE_TYPE_EDFP;
    }
    else if (strcmp(keyval, "hho_p2") == 0) {
      eqp->space_scheme = CS_SPACE_SCHEME_HHO_P2;
      eqp->space_poly_degree = 2;
      eqp->time_hodge.type = CS_PARAM_HODGE_TYPE_CPVD;
      eqp->diffusion_hodge.type = CS_PARAM_HODGE_TYPE_EDFP;
    }
    else {
      const char *_val = keyval;
      bft_error(__FILE__, __LINE__, 0,
                emsg, __func__, label, _val, "CS_EQKEY_SPACE_SCHEME");
    }
    break;

  case CS_EQKEY_TIME_SCHEME:

    if (strcmp(keyval, "no") == 0 || strcmp(keyval, "steady") == 0) {
      eqp->time_scheme = CS_TIME_SCHEME_STEADY;
    }
    else if (strcmp(keyval, "euler_implicit") == 0) {
      eqp->time_scheme = CS_TIME_SCHEME_EULER_IMPLICIT;
      eqp->theta = 1.;
    }
    else if (strcmp(keyval, "euler_explicit") == 0) {
      eqp->time_scheme = CS_TIME_SCHEME_EULER_EXPLICIT;
      eqp->theta = 0.;
    }
    else if (strcmp(keyval, "crank_nicolson") == 0) {
      eqp->time_scheme = CS_TIME_SCHEME_CRANKNICO;
      eqp->theta = 0.5;
    }
    else if (strcmp(keyval, "theta_scheme") == 0)
      eqp->time_scheme = CS_TIME_SCHEME_THETA;
    else {
      const char *_val = keyval;
      bft_error(__FILE__, __LINE__, 0,
                emsg, __func__, label, _val, "CS_EQKEY_TIME_SCHEME");
    }
    break;

  case CS_EQKEY_TIME_THETA:
    eqp->theta = atof(keyval);
    break;

  case CS_EQKEY_VERBOSITY: /* "verbosity" */
    eqp->verbosity = atoi(keyval);
    break;

  default:
    bft_error(__FILE__, __LINE__, 0,
              _(" %s: Invalid key for setting the equation %s."),
              __func__, label);

  } /* Switch on keys */

}

/*============================================================================
 * Public function prototypes
 *============================================================================*/

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Create a \ref cs_equation_param_t structure with the given
 *         parameters. The remaining parameters are set with default values;
 *
 * \param[in] name          name of the equation
 * \param[in] type          type of equation
 * \param[in] dim           dim of the variable associated to this equation
 * \param[in] default_bc    type of boundary condition set by default
 *
 * \return a pointer to a new allocated \ref cs_equation_param_t structure
 */
/*----------------------------------------------------------------------------*/

cs_equation_param_t *
cs_equation_create_param(const char            *name,
                         cs_equation_type_t     type,
                         int                    dim,
                         cs_param_bc_type_t     default_bc)
{
  cs_equation_param_t  *eqp = NULL;

  BFT_MALLOC(eqp, 1, cs_equation_param_t);

  /* Store the name of the equation */
  int  len = strlen(name)+1;
  BFT_MALLOC(eqp->name, len, char);
  strncpy(eqp->name, name, len);

  /* Set additional members */
  eqp->type = type;
  eqp->dim = dim;

  /* Other default settings */
  eqp->verbosity = 2;
  eqp->flag = 0;
  eqp->process_flag = 0;

  /* Vertex-based schemes imply specific discrete Hodge operators for
     diffusion, time and reaction terms.
     Default initialization is made in accordance with this choice */
  eqp->space_scheme = CS_SPACE_SCHEME_CDOVB;
  eqp->dof_reduction = CS_PARAM_REDUCTION_DERHAM;
  eqp->space_poly_degree = 0;

  /* Boundary conditions structure.
     One assigns a boundary condition by default */
  eqp->default_bc = default_bc;
  eqp->n_bc_defs = 0;
  eqp->bc_defs = NULL;
  eqp->default_enforcement = CS_PARAM_BC_ENFORCE_ALGEBRAIC;
  eqp->strong_pena_bc_coeff = _strong_pena_bc_coef_by_default;
  eqp->weak_pena_bc_coeff = _weak_pena_bc_coef_by_default;

  /* Initial condition (zero value by default) */
  eqp->n_ic_defs = 0;
  eqp->ic_defs = NULL;

  /* Description of the time discretization (default values) */
  eqp->time_property = NULL;
  eqp->time_scheme = CS_TIME_SCHEME_EULER_IMPLICIT;
  eqp->theta = 1.0;
  eqp->do_lumping = false;
  eqp->time_hodge = (cs_param_hodge_t) {
    .is_unity = true,
    .is_iso = true,
    .inv_pty = false,
    .algo = CS_PARAM_HODGE_ALGO_VORONOI,
    .type = CS_PARAM_HODGE_TYPE_VPCD,
    .coef = 1.,
  };

  /* Description of the discetization of the diffusion term */
  eqp->diffusion_property = NULL;
  eqp->diffusion_hodge = (cs_param_hodge_t) {
    .is_unity = false,
    .is_iso = true,
    .inv_pty = false,
    .algo = CS_PARAM_HODGE_ALGO_COST,
    .type = CS_PARAM_HODGE_TYPE_EPFD,
    .coef = 1./3.,
  };

  /* Description of the discetization of the curl-curl term */
  eqp->curlcurl_property = NULL;
  eqp->curlcurl_hodge = (cs_param_hodge_t) {
    .is_unity = false,
    .is_iso = true,
    .inv_pty = false,
    .algo = CS_PARAM_HODGE_ALGO_COST,
    .type = CS_PARAM_HODGE_TYPE_FPED,
    .coef = 1./3.,
  };

  /* Description of the discetization of the grad-div term */
  eqp->graddiv_property = NULL;
  eqp->graddiv_hodge = (cs_param_hodge_t) {
    .is_unity = true,
    .is_iso = true,
    .inv_pty = false,
    .algo = CS_PARAM_HODGE_ALGO_VORONOI,
    .type = CS_PARAM_HODGE_TYPE_EPFD,
    .coef = 1./3.,
  };

  /* Advection term */
  eqp->adv_field = NULL;
  eqp->adv_formulation = CS_PARAM_ADVECTION_FORM_CONSERV;
  eqp->adv_scheme = CS_PARAM_ADVECTION_SCHEME_UPWIND;
  eqp->upwind_portion = 0.15;

  /* Description of the discretization of the reaction term.
     No reaction term by default */
  eqp->n_reaction_terms = 0;
  eqp->reaction_properties = NULL;
  eqp->reaction_hodge = (cs_param_hodge_t) {
    .is_unity = false,
    .is_iso = true,
    .inv_pty = false,
    .algo = CS_PARAM_HODGE_ALGO_WBS,
    .type = CS_PARAM_HODGE_TYPE_VPCD,
  };

  /* Source term (always in the right-hand side)
     No source term by default */
  eqp->n_source_terms = 0;
  eqp->source_terms = NULL;

  /* Members of the structure to handle the enforcement of (internal) DoFs */
  eqp->enforcement_type = 0;
  BFT_MALLOC(eqp->enforcement_ref_value, eqp->dim, cs_real_t);
  for (int i = 0; i < eqp->dim; i++)
    eqp->enforcement_ref_value[i] = 0.;

  eqp->n_enforced_cells = 0;
  eqp->enforced_cell_ids = NULL;
  eqp->enforced_cell_values = NULL;

  eqp->n_enforced_dofs = 0;
  eqp->enforced_dof_ids = NULL;
  eqp->enforced_dof_values = NULL;

  /* Settings for driving the linear algebra */
  eqp->sles_param = (cs_param_sles_t) {

    .verbosity = 0,                         /* SLES verbosity */
    .field_id = -1,                         /* associated field id */
    .solver_class = CS_PARAM_SLES_CLASS_CS, /* in-house solverq */
    .precond = CS_PARAM_PRECOND_DIAG,       /* preconditioner */
    .solver = CS_PARAM_ITSOL_GMRES,         /* iterative solver */
    .amg_type = CS_PARAM_AMG_NONE,          /* no predefined AMG type */
    .n_max_iter = 10000,                    /* max. number of iterations */
    .eps = 1e-8,                   /* stopping criterion on the accuracy */
    .resnorm_type = CS_PARAM_RESNORM_NONE,

  };

  /* Settings for the OpenMP strategy */
  eqp->omp_assembly_choice = CS_PARAM_ASSEMBLE_OMP_CRITICAL;

  return eqp;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Copy the settings from one \ref cs_equation_param_t structure to
 *         another one
 *
 * \param[in]      ref   pointer to the reference \ref cs_equation_param_t
 * \param[in, out] dst   pointer to the \ref cs_equation_param_t to update
 */
/*----------------------------------------------------------------------------*/

void
cs_equation_param_update_from(const cs_equation_param_t   *ref,
                              cs_equation_param_t         *dst)
{
  /* Generic members */
  dst->type = ref->type;
  dst->dim = ref->dim;
  dst->verbosity = ref->verbosity;
  dst->process_flag = ref->process_flag;
  dst->flag = ref->flag;
  dst->space_scheme = ref->space_scheme;
  dst->dof_reduction = ref->dof_reduction;
  dst->space_poly_degree = ref->space_poly_degree;

  /* Boundary conditions structure */
  dst->default_bc = ref->default_bc;
  dst->default_enforcement = ref->default_enforcement;
  dst->strong_pena_bc_coeff = ref->strong_pena_bc_coeff;
  dst->n_bc_defs = ref->n_bc_defs;
  BFT_MALLOC(dst->bc_defs, dst->n_bc_defs, cs_xdef_t *);
  for (int i = 0; i < ref->n_bc_defs; i++)
    dst->bc_defs[i] = cs_xdef_copy(ref->bc_defs[i]);

  /* Description of the time discretization */
  dst->time_scheme = ref->time_scheme;
  dst->theta = ref->theta;
  dst->do_lumping = ref->do_lumping;
  dst->time_property = ref->time_property;

  cs_hodge_copy_parameters(&(ref->time_hodge), &(dst->time_hodge));

  /* Initial condition (zero value by default) */
  dst->n_ic_defs = ref->n_ic_defs;
  BFT_MALLOC(dst->ic_defs, dst->n_ic_defs, cs_xdef_t *);
  for (int i = 0; i < ref->n_ic_defs; i++)
    dst->ic_defs[i] = cs_xdef_copy(ref->ic_defs[i]);

  /* Diffusion term */
  dst->diffusion_property = ref->diffusion_property;

  cs_hodge_copy_parameters(&(ref->diffusion_hodge), &(dst->diffusion_hodge));

  /* Curl-curl term */
  dst->curlcurl_property = ref->curlcurl_property;

  cs_hodge_copy_parameters(&(ref->curlcurl_hodge), &(dst->curlcurl_hodge));

  /* Grad-div term */
  dst->graddiv_property = ref->graddiv_property;

  cs_hodge_copy_parameters(&(ref->graddiv_hodge), &(dst->graddiv_hodge));

  /* Advection term */
  dst->adv_formulation = ref->adv_formulation;
  dst->adv_scheme = ref->adv_scheme;
  dst->upwind_portion = ref->upwind_portion;
  dst->adv_field = ref->adv_field;

  /* Reaction term */
  dst->n_reaction_terms = ref->n_reaction_terms;
  BFT_MALLOC(dst->reaction_properties, dst->n_reaction_terms, cs_property_t *);
  for (int i = 0; i < ref->n_reaction_terms; i++)
    dst->reaction_properties[i] = ref->reaction_properties[i];

  cs_hodge_copy_parameters(&(ref->reaction_hodge), &(dst->reaction_hodge));

  /* Source term */
  dst->n_source_terms = ref->n_source_terms;
  BFT_MALLOC(dst->source_terms, dst->n_source_terms, cs_xdef_t *);
  for (int i = 0; i < dst->n_source_terms; i++)
    dst->source_terms[i] = cs_xdef_copy(ref->source_terms[i]);

  /* No enforcement of internal DoFs */
  dst->enforcement_type = ref->enforcement_type;
  BFT_MALLOC(dst->enforcement_ref_value, dst->dim, cs_real_t);
  memcpy(dst->enforcement_ref_value, ref->enforcement_ref_value,
         dst->dim*sizeof(cs_real_t));

  dst->n_enforced_cells = ref->n_enforced_cells;
  if (ref->n_enforced_cells > 0) {

    BFT_MALLOC(dst->enforced_cell_ids, dst->n_enforced_cells, cs_lnum_t);
    memcpy(dst->enforced_cell_ids, ref->enforced_cell_ids,
           dst->n_enforced_cells*sizeof(cs_lnum_t));

    dst->enforced_cell_values= NULL;
    if (ref->enforced_cell_values != NULL) {
      cs_lnum_t  size = dst->n_enforced_cells*dst->dim;
      BFT_MALLOC(dst->enforced_cell_values, size, cs_real_t);
      memcpy(dst->enforced_dof_values, ref->enforced_dof_values,
             size*sizeof(cs_real_t));
    }

  }

  dst->n_enforced_dofs = ref->n_enforced_dofs;
  if (ref->n_enforced_dofs > 0) {

    BFT_MALLOC(dst->enforced_dof_ids, dst->n_enforced_dofs, cs_lnum_t);
    memcpy(dst->enforced_dof_ids, ref->enforced_dof_ids,
           dst->n_enforced_dofs*sizeof(cs_lnum_t));

    dst->enforced_dof_values = NULL;
    if (ref->enforced_dof_values != NULL) {
      cs_lnum_t  size = dst->n_enforced_dofs*dst->dim;
      BFT_MALLOC(dst->enforced_dof_values, size, cs_real_t);
      memcpy(dst->enforced_dof_values, ref->enforced_dof_values,
             size*sizeof(cs_real_t));
    }

  }

  /* Settings for driving the linear algebra.
     Field id is not copied at this stage. */
  dst->sles_param.verbosity = ref->sles_param.verbosity;
  dst->sles_param.solver_class = ref->sles_param.solver_class;
  dst->sles_param.precond = ref->sles_param.precond;
  dst->sles_param.solver = ref->sles_param.solver;
  dst->sles_param.amg_type = ref->sles_param.amg_type;
  dst->sles_param.n_max_iter = ref->sles_param.n_max_iter;
  dst->sles_param.eps = ref->sles_param.eps;
  dst->sles_param.resnorm_type = ref->sles_param.resnorm_type;

  /* Settings for performance */
  dst->omp_assembly_choice = ref->omp_assembly_choice;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Free a \ref cs_equation_param_t
 *
 * \param[in, out] eqp          pointer to a \ref cs_equation_param_t
 *
 * \return a NULL pointer
 */
/*----------------------------------------------------------------------------*/

cs_equation_param_t *
cs_equation_free_param(cs_equation_param_t     *eqp)
{
  if (eqp == NULL)
    return NULL;

  /* Information related to the definition of the boundary conditions */
  if (eqp->n_bc_defs > 0) {

    for (int i = 0; i < eqp->n_bc_defs; i++)
      eqp->bc_defs[i] = cs_xdef_free(eqp->bc_defs[i]);
    BFT_FREE(eqp->bc_defs);

  }

  /* Information related to the definition of reaction terms */
  if (eqp->n_reaction_terms > 0) {

    BFT_FREE(eqp->reaction_properties);

    /* Remark: properties are freed when the global cs_domain_t structure is
       freed thanks to a call to cs_property_free() */
  }

  /* Information related to the definition of source terms */
  if (eqp->n_source_terms > 0) {

    for (int i = 0; i < eqp->n_source_terms; i++)
      eqp->source_terms[i] = cs_xdef_free(eqp->source_terms[i]);
    BFT_FREE(eqp->source_terms);

  }

  /* Information related to the enforcement of internal DoFs */
  BFT_FREE(eqp->enforcement_ref_value);

  if (eqp->n_enforced_cells > 0) {
    eqp->n_enforced_cells = 0;
    BFT_FREE(eqp->enforced_cell_ids);
    BFT_FREE(eqp->enforced_cell_values);
  }

  if (eqp->n_enforced_dofs > 0) {
    eqp->n_enforced_dofs = 0;
    BFT_FREE(eqp->enforced_dof_ids);
    BFT_FREE(eqp->enforced_dof_values);
  }

  /* Information related to the definition of initial conditions */
  if (eqp->n_ic_defs > 0) {

    for (int i = 0; i < eqp->n_ic_defs; i++)
      eqp->ic_defs[i] = cs_xdef_free(eqp->ic_defs[i]);
    BFT_FREE(eqp->ic_defs);

  }

  BFT_FREE(eqp->name);
  BFT_FREE(eqp);

  return NULL;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Set a parameter attached to a keyname in a \ref cs_equation_param_t
 *         structure
 *
 * \param[in, out]  eqp      pointer to a \ref cs_equation_param_t structure
 * \param[in]       key      key related to the member of eq to set
 * \param[in]       keyval   accessor to the value to set
 */
/*----------------------------------------------------------------------------*/

void
cs_equation_set_param(cs_equation_param_t   *eqp,
                      cs_equation_key_t      key,
                      const char            *keyval)
{
  /* Sanity checks */
  if (eqp == NULL)
    bft_error(__FILE__, __LINE__, 0, "%s: %s\n", __func__, _err_empty_eqp);
  if (keyval == NULL)
    bft_error(__FILE__, __LINE__, 0, "%s: Eq: %s: Key value is empty",
              __func__, eqp->name);
  if (eqp->flag & CS_EQUATION_LOCKED)
    bft_error(__FILE__, __LINE__, 0,
              _(" %s: Equation %s is not modifiable anymore.\n"
                " Please check your settings."), eqp->name, __func__);

  /* Conversion of the string to lower case */
  char val[CS_BASE_STRING_LEN];
  for (size_t i = 0; i < strlen(keyval); i++)
    val[i] = tolower(keyval[i]);
  val[strlen(keyval)] = '\0';

  /* Set the couple (key,keyval) */
  _set_key(eqp->name, eqp, key, val);
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Set parameters for initializing SLES structures used for the
 *        resolution of the linear system.
 *        Settings are related to this equation.
 *
 * \param[in, out]  eqp       pointer to a \ref cs_equation_param_t struct.
 */
/*----------------------------------------------------------------------------*/

void
cs_equation_param_set_sles(cs_equation_param_t      *eqp)
{
  cs_param_sles_t  slesp = eqp->sles_param;

  switch (slesp.solver_class) {

  case CS_PARAM_SLES_CLASS_CS: /* Code_Saturne's own solvers */
    _set_saturne_sles(eqp);
    break;

  case CS_PARAM_SLES_CLASS_PETSC: /* PETSc solvers */
  case CS_PARAM_SLES_CLASS_HYPRE: /* HYPRE solvers through PETSc */
#if defined(HAVE_PETSC)

    cs_sles_petsc_init();

    if (slesp.precond == CS_PARAM_PRECOND_AMG_BLOCK) {

      if (slesp.amg_type == CS_PARAM_AMG_PETSC_GAMG) {
        cs_sles_petsc_define(slesp.field_id,
                             NULL,
                             MATMPIAIJ,
                             _petsc_amg_block_gamg_hook,
                             (void *)eqp);
      }
      else if (slesp.amg_type == CS_PARAM_AMG_HYPRE_BOOMER) {
#if defined(PETSC_HAVE_HYPRE)
        cs_sles_petsc_define(slesp.field_id,
                             NULL,
                             MATMPIAIJ,
                             _petsc_amg_block_boomer_hook,
                             (void *)eqp);
#else
        bft_error(__FILE__, __LINE__, 0,
                  " %s: Boomer is not available. Switch to another AMG.",
                  __func__);
#endif
      }
      else
        bft_error(__FILE__, __LINE__, 0,
                  "%s: Invalid amg type for an AMG by block.", __func__);

    }
    else if ((slesp.precond == CS_PARAM_PRECOND_BJACOB_ILU0 ||
              slesp.precond == CS_PARAM_PRECOND_BJACOB_SGS) && eqp->dim > 1)
      cs_sles_petsc_define(slesp.field_id,
                           NULL,
                           MATMPIAIJ,
                           _petsc_block_jacobi_hook,
                           (void *)eqp);
    else
      cs_sles_petsc_define(slesp.field_id,
                           NULL,
                           MATMPIAIJ,
                           _petsc_setup_hook,
                           (void *)eqp);

#else
    bft_error(__FILE__, __LINE__, 0,
              _(" %s: PETSC algorithms used to solve %s are not linked.\n"
                " Please install Code_Saturne with PETSc."),
              __func__, eqp->name);

#endif /* HAVE_PETSC */
    break;

  default:
    bft_error(__FILE__, __LINE__, 0,
              _(" %s: Algorithm requested to solve %s is not implemented yet.\n"
                " Please modify your settings."), __func__, eqp->name);
    break;

  } /* End of switch on class of solvers */

  /* Define the level of verbosity for SLES structure */
  if (slesp.verbosity > 1) {

    cs_sles_t  *sles = cs_sles_find_or_add(slesp.field_id, NULL);

    /* Set verbosity */
    cs_sles_set_verbosity(sles, slesp.verbosity);

  }

}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Last modification of the cs_equation_param_t structure before
 *         launching the computation
 *
 * \param[in, out]  eqp      pointer to a \ref cs_equation_param_t structure
 */
/*----------------------------------------------------------------------------*/

void
cs_equation_param_last_stage(cs_equation_param_t   *eqp)
{
  if (eqp == NULL)
    bft_error(__FILE__, __LINE__, 0, "%s: %s\n", __func__, _err_empty_eqp);

  if (eqp->flag & CS_EQUATION_LOCKED)
    bft_error(__FILE__, __LINE__, 0,
              _(" %s: Equation %s is not modifiable anymore.\n"
                " Please check your settings."), eqp->name, __func__);

  if (eqp->do_lumping) { /* Activate a set of options */

    eqp->reaction_hodge.algo = CS_PARAM_HODGE_ALGO_VORONOI;
    eqp->time_hodge.algo = CS_PARAM_HODGE_ALGO_VORONOI;

    /* A simple barycentric quadrature rule is applied to all source terms */
    for (int i = 0; i < eqp->n_source_terms; i++)
      cs_xdef_set_quadrature(eqp->source_terms[i], CS_QUADRATURE_BARY);

  }

}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Summary of a \ref cs_equation_param_t structure
 *
 * \param[in]  eqp      pointer to a \ref cs_equation_param_t structure
 */
/*----------------------------------------------------------------------------*/

void
cs_equation_summary_param(const cs_equation_param_t   *eqp)
{
  if (eqp == NULL)
    return;

  const char *eqname = eqp->name;

  char  prefix[256];
  assert(strlen(eqname) < 200); /* Check that prefix is large enough */

  /* High-level settings */
  cs_log_printf(CS_LOG_SETUP, "\n### %s: High-level settings\n", eqname);
  cs_log_printf(CS_LOG_SETUP, "  * %s | Type: ", eqname);
  switch (eqp->type) {
  case CS_EQUATION_TYPE_GROUNDWATER:
    cs_log_printf(CS_LOG_SETUP, "Associated to groundwater flows\n");
    break;
  case CS_EQUATION_TYPE_MAXWELL:
    cs_log_printf(CS_LOG_SETUP, "Associated to the Maxwell module\n");
    break;
  case CS_EQUATION_TYPE_THERMAL:
    cs_log_printf(CS_LOG_SETUP, "Associated to the thermal module\n");
    break;
  case CS_EQUATION_TYPE_NAVSTO:
    cs_log_printf(CS_LOG_SETUP, "Associated to the Navier-Stokes system\n");
    break;
  case CS_EQUATION_TYPE_PREDEFINED:
    cs_log_printf(CS_LOG_SETUP, "Predefined\n");
    break;
  case CS_EQUATION_TYPE_USER:
    cs_log_printf(CS_LOG_SETUP, "User-defined\n");
    break;

  default:
    bft_error(__FILE__, __LINE__, 0,
              " Eq. %s has no type.\n Please check your settings.", eqname);
  }

  bool  unsteady = (eqp->flag & CS_EQUATION_UNSTEADY) ? true : false;
  bool  convection = (eqp->flag & CS_EQUATION_CONVECTION) ? true : false;
  bool  diffusion = (eqp->flag & CS_EQUATION_DIFFUSION) ? true : false;
  bool  curlcurl = (eqp->flag & CS_EQUATION_CURLCURL) ? true : false;
  bool  graddiv = (eqp->flag & CS_EQUATION_GRADDIV) ? true : false;
  bool  reaction = (eqp->flag & CS_EQUATION_REACTION) ? true : false;
  bool  source_term = (eqp->n_source_terms > 0) ? true : false;
  bool  force_values = (eqp->flag & CS_EQUATION_FORCE_VALUES) ? true : false;

  cs_log_printf(CS_LOG_SETUP,
                "  * %s | Terms: unsteady:%s, convection:%s, diffusion:%s\n",
                eqname, cs_base_strtf(unsteady), cs_base_strtf(convection),
                cs_base_strtf(diffusion));
  cs_log_printf(CS_LOG_SETUP,
                "  * %s | Terms: curl-curl:%s, grad-div:%s\n",
                eqname, cs_base_strtf(curlcurl), cs_base_strtf(graddiv));
  cs_log_printf(CS_LOG_SETUP,
                "  * %s | Terms: reaction:%s, source term:%s,"
                " force internal values: %s\n",
                eqname,cs_base_strtf(reaction), cs_base_strtf(source_term),
                cs_base_strtf(force_values));

  if (eqp->space_scheme < CS_SPACE_N_SCHEMES)
    cs_log_printf(CS_LOG_SETUP, "  * %s | Space scheme:       %s\n",
                  eqname, cs_param_get_space_scheme_name(eqp->space_scheme));
  else
    bft_error(__FILE__, __LINE__, 0,
              " Undefined space scheme for eq. %s", eqname);

  cs_log_printf(CS_LOG_SETUP, "  * %s | Space poly degree:  %d\n",
                eqname, eqp->space_poly_degree);
  cs_log_printf(CS_LOG_SETUP, "  * %s | Verbosity:          %d\n",
                eqname, eqp->verbosity);

  if (cs_glob_n_threads > 1) {
    if (eqp->omp_assembly_choice == CS_PARAM_ASSEMBLE_OMP_CRITICAL)
      cs_log_printf(CS_LOG_SETUP, "  * %s | OpenMP.Assembly.Choice:  %s\n",
                    eqname, "critical");
    else if (eqp->omp_assembly_choice == CS_PARAM_ASSEMBLE_OMP_ATOMIC)
      cs_log_printf(CS_LOG_SETUP, "  * %s | OpenMP.Assembly.Choice:  %s\n",
                    eqname, "atomic");
  }

  /* Boundary conditions */
  cs_log_printf(CS_LOG_SETUP, "\n### %s: Boundary condition settings\n",
                eqname);
  cs_log_printf(CS_LOG_SETUP,
                "  * %s | Boundary conditions | Default: %s\n",
                eqname, cs_param_get_bc_name(eqp->default_bc));
  cs_log_printf(CS_LOG_SETUP,
                "  * %s | Boundary conditions | Enforcement: %s\n",
                eqname,
                cs_param_get_bc_enforcement_name(eqp->default_enforcement));
  if (eqp->default_enforcement == CS_PARAM_BC_ENFORCE_PENALIZED)
    cs_log_printf(CS_LOG_SETUP,
                  "  * %s | Boundary conditions | Penalization coefficient:"
                  " %5.3e\n", eqname, eqp->strong_pena_bc_coeff);
  else if (eqp->default_enforcement == CS_PARAM_BC_ENFORCE_WEAK_NITSCHE ||
           eqp->default_enforcement == CS_PARAM_BC_ENFORCE_WEAK_SYM)
    cs_log_printf(CS_LOG_SETUP,
                  "  * %s | Boundary conditions | Penalization coefficient:"
                  " %5.3e\n", eqname, eqp->weak_pena_bc_coeff);

  cs_log_printf(CS_LOG_SETUP, "  * %s | Boundary conditions |"
                " Number of definitions: %d\n", eqname, eqp->n_bc_defs);

  if (eqp->verbosity > 0) {
    char  desc[128];
    for (int id = 0; id < eqp->n_bc_defs; id++) {
      const cs_xdef_t  *d = eqp->bc_defs[id];

      cs_cdo_bc_get_desc(d->meta, desc);
      sprintf(prefix, "        Definition %4d", id);
      cs_log_printf(CS_LOG_SETUP, "\n%s | Type: %s\n", prefix, desc);
      cs_xdef_log(prefix, d);
    }
  }

  if (unsteady) {

    cs_log_printf(CS_LOG_SETUP, "\n### %s: Time settings\n", eqname);
    cs_log_printf(CS_LOG_SETUP,
                  "  * %s | Initial conditions | Number of definitions: %d",
                  eqname, eqp->n_ic_defs);
    if (eqp->n_ic_defs > 0)
      cs_log_printf(CS_LOG_SETUP, "\n\n");
    for (int i = 0; i < eqp->n_ic_defs; i++) {
      sprintf(prefix, "        Definition %4d", i);
      cs_xdef_log(prefix, eqp->ic_defs[i]);
    }

    const char  *time_scheme = cs_param_get_time_scheme_name(eqp->time_scheme);
    if (time_scheme != NULL) {
      cs_log_printf(CS_LOG_SETUP, "\n  * %s | Time scheme: %s",
                    eqname, time_scheme);
      if (eqp->time_scheme == CS_TIME_SCHEME_THETA)
        cs_log_printf(CS_LOG_SETUP, " with value %f\n", eqp->theta);
      else
        cs_log_printf(CS_LOG_SETUP, "\n");
    }
    else
      bft_error(__FILE__, __LINE__, 0, " Invalid time scheme.");

    cs_log_printf(CS_LOG_SETUP, "  * %s | Mass.Lumping: %s\n",
                  eqname, cs_base_strtf(eqp->do_lumping));
    cs_log_printf(CS_LOG_SETUP, "  * %s | Time property: %s\n\n",
                  eqname, cs_property_get_name(eqp->time_property));

    sprintf(prefix, "        Time Hodge op. ");
    cs_param_hodge_log(prefix, eqp->time_hodge);

  } /* Unsteady term */

  if (diffusion) {

    cs_log_printf(CS_LOG_SETUP, "\n### %s: Diffusion term settings\n", eqname);
    cs_log_printf(CS_LOG_SETUP, "  * %s | Diffusion property: %s\n\n",
                  eqname, cs_property_get_name(eqp->diffusion_property));

    sprintf(prefix, "        Diffusion Hodge op. ");
    cs_param_hodge_log(prefix, eqp->diffusion_hodge);

  } /* Diffusion term */

  if (curlcurl) {

    cs_log_printf(CS_LOG_SETUP, "\n### %s: Curl-Curl term settings\n", eqname);
    cs_log_printf(CS_LOG_SETUP, "  * %s | Curl-Curl property: %s\n\n",
                  eqname, cs_property_get_name(eqp->curlcurl_property));

    sprintf(prefix, "        Curl-curl Hodge op. ");
    cs_param_hodge_log(prefix, eqp->curlcurl_hodge);

  } /* Curl-curl term */

  if (graddiv) {

    cs_log_printf(CS_LOG_SETUP, "\n### %s: Grad-Div term settings\n", eqname);
    cs_log_printf(CS_LOG_SETUP, "  * %s | Grad-Div property: %s\n\n",
                  eqname, cs_property_get_name(eqp->graddiv_property));

    sprintf(prefix, "        Grad-Div Hodge op. ");
    cs_param_hodge_log(prefix, eqp->graddiv_hodge);

  } /* Curl-curl term */

  if (convection) {

    cs_log_printf(CS_LOG_SETUP, "\n### %s: Advection term settings\n", eqname);
    cs_log_printf(CS_LOG_SETUP, "  * %s | Advection.Field: %s\n",
                  eqname, cs_advection_field_get_name(eqp->adv_field));

    cs_log_printf(CS_LOG_SETUP, "  * %s | Advection.Formulation:", eqname);
    switch(eqp->adv_formulation) {

    case CS_PARAM_ADVECTION_FORM_CONSERV:
      cs_log_printf(CS_LOG_SETUP, " Conservative\n");
      break;
    case CS_PARAM_ADVECTION_FORM_NONCONS:
      cs_log_printf(CS_LOG_SETUP, " Non-conservative\n");
      break;
    case CS_PARAM_ADVECTION_FORM_SKEWSYM:
      cs_log_printf(CS_LOG_SETUP, " Skew-symmetric\n");
      break;

    default:
      bft_error(__FILE__, __LINE__, 0, " Invalid operator type for advection.");

    } /* Switch on formulation */

    cs_log_printf(CS_LOG_SETUP, "  * %s | Advection.Scheme:", eqname);
    switch(eqp->adv_scheme) {
    case CS_PARAM_ADVECTION_SCHEME_CENTERED:
      cs_log_printf(CS_LOG_SETUP, " centered\n");
      break;
    case CS_PARAM_ADVECTION_SCHEME_CIP:
      cs_log_printf(CS_LOG_SETUP, " continuous interior penalty\n");
      break;
    case CS_PARAM_ADVECTION_SCHEME_CIP_CW:
      cs_log_printf(CS_LOG_SETUP, " continuous interior penalty (CellWise)\n");
      break;
    case CS_PARAM_ADVECTION_SCHEME_MIX_CENTERED_UPWIND:
      cs_log_printf(CS_LOG_SETUP, " centered-upwind (%3.2f %% of upwind)\n",
                    100*eqp->upwind_portion);
      break;
    case CS_PARAM_ADVECTION_SCHEME_SAMARSKII:
      cs_log_printf(CS_LOG_SETUP,
                    " upwind weighted with Samarskii function\n");
      break;
    case CS_PARAM_ADVECTION_SCHEME_SG:
      cs_log_printf(CS_LOG_SETUP,
                    " upwind weighted with Scharfetter-Gummel function\n");
      break;
    case CS_PARAM_ADVECTION_SCHEME_UPWIND:
      cs_log_printf(CS_LOG_SETUP, " upwind\n");
      break;
    default:
      bft_error(__FILE__, __LINE__, 0, " Invalid scheme for advection.");
    }

  } /* Advection term */

  if (reaction) {

    cs_log_printf(CS_LOG_SETUP, "\n### %s: Reaction settings\n", eqname);
    cs_log_printf(CS_LOG_SETUP, "  * %s | Reaction | Number of terms: %d\n",
                  eqname, eqp->n_reaction_terms);

    sprintf(prefix, "        Reaction Hodge op. ");
    cs_param_hodge_log(prefix, eqp->reaction_hodge);

  } /* Reaction terms */

  if (source_term) {

    cs_log_printf(CS_LOG_SETUP, "\n### %s: Source term settings\n", eqname);
    cs_log_printf(CS_LOG_SETUP, "  * %s | Source terms | Number of terms: %d\n",
                  eqname, eqp->n_source_terms);

    for (int s_id = 0; s_id < eqp->n_source_terms; s_id++) {
      sprintf(prefix, "        Definition %4d", s_id);
      cs_xdef_log(prefix, eqp->source_terms[s_id]);
    }

  } /* Source terms */

  /* Iterative solver information */
  const cs_param_sles_t   slesp = eqp->sles_param;

  cs_log_printf(CS_LOG_SETUP, "\n### %s: Linear algebra settings\n\n",
                eqname);
  cs_log_printf(CS_LOG_SETUP, "        SLES | Family:");
  if (slesp.solver_class == CS_PARAM_SLES_CLASS_CS)
    cs_log_printf(CS_LOG_SETUP, "             Code_Saturne\n");
  else if (slesp.solver_class == CS_PARAM_SLES_CLASS_PETSC)
    cs_log_printf(CS_LOG_SETUP, "             PETSc\n");
  else if (slesp.solver_class == CS_PARAM_SLES_CLASS_HYPRE)
    cs_log_printf(CS_LOG_SETUP, "             HYPRE\n");

  cs_log_printf(CS_LOG_SETUP, "        SLES | Verbosity:          %d\n",
                slesp.verbosity);
  cs_log_printf(CS_LOG_SETUP, "        SLES | Field id:           %d\n",
                slesp.field_id);
  cs_log_printf(CS_LOG_SETUP, "        SLES | Solver.MaxIter:     %d\n",
                slesp.n_max_iter);

  cs_log_printf(CS_LOG_SETUP, "        SLES | Solver.Name:        %s\n",
                cs_param_get_solver_name(slesp.solver));
  if (slesp.solver == CS_PARAM_ITSOL_AMG)
    cs_log_printf(CS_LOG_SETUP, "        SLES | AMG.Type:           %s\n",
                  cs_param_get_amg_type_name(slesp.amg_type));
  cs_log_printf(CS_LOG_SETUP, "        SLES | Solver.Precond:     %s\n",
                cs_param_get_precond_name(slesp.precond));
  if (slesp.precond == CS_PARAM_PRECOND_AMG)
    cs_log_printf(CS_LOG_SETUP, "        SLES | AMG.Type:           %s\n",
                  cs_param_get_amg_type_name(slesp.amg_type));

  cs_log_printf(CS_LOG_SETUP, "        SLES | Solver.Eps:        % -10.6e\n",
                slesp.eps);

  switch (slesp.resnorm_type) {
  case CS_PARAM_RESNORM_NORM2_RHS:
    cs_log_printf(CS_LOG_SETUP, "        SLES | Solver.Normalized:  %s\n",
                  "Euclidean norm of the RHS");
    break;
  case CS_PARAM_RESNORM_WEIGHTED_RHS:
    cs_log_printf(CS_LOG_SETUP, "        SLES | Solver.Normalized:  %s\n",
                  "Weighted Euclidean norm of the RHS");
    break;
  case CS_PARAM_RESNORM_FILTERED_RHS:
    cs_log_printf(CS_LOG_SETUP, "        SLES | Solver.Normalized:  %s\n",
                  "Filtered Euclidean norm of the RHS");
    break;
  case CS_PARAM_RESNORM_NONE:
  default:
    cs_log_printf(CS_LOG_SETUP, "        SLES | Solver.Normalized:  %s\n",
                  "None");
    break;
  }
  cs_log_printf(CS_LOG_SETUP, "\n");
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Define the initial condition for the unknown related to this equation
 *         This definition can be done on a specified mesh location.
 *         By default, the unknown is set to zero everywhere.
 *         Here a constant value is set to all the entities belonging to the
 *         given mesh location
 *
 * \param[in, out]  eqp       pointer to a cs_equation_param_t structure
 * \param[in]       z_name    name of the associated zone (if NULL or
 *                            "" all cells are considered)
 * \param[in]       val       pointer to the value
 *
 * \return a pointer to the new \ref cs_xdef_t structure
 */
/*----------------------------------------------------------------------------*/

cs_xdef_t *
cs_equation_add_ic_by_value(cs_equation_param_t    *eqp,
                            const char             *z_name,
                            cs_real_t              *val)
{
  if (eqp == NULL)
    bft_error(__FILE__, __LINE__, 0, "%s: %s\n", __func__, _err_empty_eqp);

  /* Add a new cs_xdef_t structure */
  int  z_id = cs_get_vol_zone_id(z_name);

  cs_flag_t  meta_flag = 0;
  if (z_id == 0)
    meta_flag |= CS_FLAG_FULL_LOC;

  cs_xdef_t  *d = cs_xdef_volume_create(CS_XDEF_BY_VALUE,
                                        eqp->dim,
                                        z_id,
                                        CS_FLAG_STATE_UNIFORM, // state flag
                                        meta_flag,
                                        val);

  int  new_id = eqp->n_ic_defs;
  eqp->n_ic_defs += 1;
  BFT_REALLOC(eqp->ic_defs, eqp->n_ic_defs, cs_xdef_t *);
  eqp->ic_defs[new_id] = d;

  return d;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Define the initial condition for the unknown related to this equation
 *         This definition can be done on a specified mesh location.
 *         By default, the unknown is set to zero everywhere.
 *         Here the value related to all the entities belonging to the
 *         given mesh location is such that the integral over these cells
 *         returns the requested quantity
 *
 * \param[in, out]  eqp       pointer to a cs_equation_param_t structure
 * \param[in]       z_name    name of the associated zone (if NULL or
 *                            "" all cells are considered)
 * \param[in]       quantity  quantity to distribute over the mesh location
 *
 * \return a pointer to the new \ref cs_xdef_t structure
 */
/*----------------------------------------------------------------------------*/

cs_xdef_t *
cs_equation_add_ic_by_qov(cs_equation_param_t    *eqp,
                          const char             *z_name,
                          double                  quantity)
{
  if (eqp == NULL)
    bft_error(__FILE__, __LINE__, 0, "%s: %s\n", __func__, _err_empty_eqp);

  /* Add a new cs_xdef_t structure */
  int z_id = cs_get_vol_zone_id(z_name);

  cs_flag_t  meta_flag = 0;
  if (z_id == 0)
    meta_flag |= CS_FLAG_FULL_LOC;

  cs_xdef_t  *d = cs_xdef_volume_create(CS_XDEF_BY_QOV,
                                        eqp->dim,
                                        z_id,
                                        0, // state flag
                                        meta_flag,
                                        &quantity);

  int  new_id = eqp->n_ic_defs;
  eqp->n_ic_defs += 1;
  BFT_REALLOC(eqp->ic_defs, eqp->n_ic_defs, cs_xdef_t *);
  eqp->ic_defs[new_id] = d;

  return d;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Define the initial condition for the unknown related to this
 *         equation. This definition can be done on a specified mesh location.
 *         By default, the unknown is set to zero everywhere.
 *         Here the initial value is set according to an analytical function
 *
 * \param[in, out] eqp       pointer to a cs_equation_param_t structure
 * \param[in]      z_name    name of the associated zone (if NULL or "" if
 *                           all cells are considered)
 * \param[in]      analytic  pointer to an analytic function
 * \param[in]      input     NULL or pointer to a structure cast on-the-fly
 *
 * \return a pointer to the new \ref cs_xdef_t structure
 */
/*----------------------------------------------------------------------------*/

cs_xdef_t *
cs_equation_add_ic_by_analytic(cs_equation_param_t    *eqp,
                               const char             *z_name,
                               cs_analytic_func_t     *analytic,
                               void                   *input)
{
  if (eqp == NULL)
    bft_error(__FILE__, __LINE__, 0, _err_empty_eqp);

  /* Add a new cs_xdef_t structure */
  int z_id = cs_get_vol_zone_id(z_name);

  cs_flag_t  meta_flag = 0;
  if (z_id == 0)
    meta_flag |= CS_FLAG_FULL_LOC;

  cs_xdef_analytic_input_t  anai = {.func = analytic,
                                    .input = input };

  cs_xdef_t  *d = cs_xdef_volume_create(CS_XDEF_BY_ANALYTIC_FUNCTION,
                                        eqp->dim, z_id,
                                        0, // state flag
                                        meta_flag,
                                        &anai);

  int  new_id = eqp->n_ic_defs;
  eqp->n_ic_defs += 1;
  BFT_REALLOC(eqp->ic_defs, eqp->n_ic_defs, cs_xdef_t *);
  eqp->ic_defs[new_id] = d;

  return d;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Set a boundary condition from an existing \ref cs_xdef_t structure
 *         The lifecycle of the cs_xdef_t structure is now managed by the
 *         current \ref cs_equation_param_t structure.
 *
 * \param[in, out] eqp    pointer to a cs_equation_param_t structure
 * \param[in]      xdef   pointer to the \ref cs_xdef_t structure to transfer
*/
/*----------------------------------------------------------------------------*/

void
cs_equation_add_xdef_bc(cs_equation_param_t        *eqp,
                        cs_xdef_t                  *xdef)
{
  if (eqp == NULL)
    bft_error(__FILE__, __LINE__, 0, "%s: %s\n", __func__, _err_empty_eqp);

  int  new_id = eqp->n_bc_defs;
  eqp->n_bc_defs += 1;
  BFT_REALLOC(eqp->bc_defs, eqp->n_bc_defs, cs_xdef_t *);
  eqp->bc_defs[new_id] = xdef;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Define and initialize a new structure to set a boundary condition
 *         related to the given equation structure
 *         z_name corresponds to the name of a pre-existing cs_zone_t
 *
 * \param[in, out]  eqp       pointer to a cs_equation_param_t structure
 * \param[in]       bc_type   type of boundary condition to add
 * \param[in]       z_name    name of the related boundary zone
 * \param[in]       values    pointer to a array storing the values
 *
 * \return a pointer to the new \ref cs_xdef_t structure
 */
/*----------------------------------------------------------------------------*/

cs_xdef_t *
cs_equation_add_bc_by_value(cs_equation_param_t         *eqp,
                            const cs_param_bc_type_t     bc_type,
                            const char                  *z_name,
                            cs_real_t                   *values)
{
  if (eqp == NULL)
    bft_error(__FILE__, __LINE__, 0, "%s: %s\n", __func__, _err_empty_eqp);

  /* Add a new cs_xdef_t structure */
  int  dim = eqp->dim;
  if (bc_type == CS_PARAM_BC_NEUMANN||
      bc_type == CS_PARAM_BC_HMG_NEUMANN)
    dim *= 3;  /* vector if scalar eq, tensor if vector eq. */

  if (bc_type == CS_PARAM_BC_ROBIN) {
    /* FluxNormal + alpha * (u - u_0) = beta => Set (alpha, u_0, beta) */
    if (eqp->dim == 1)
      dim = 3;
    else
      bft_error(__FILE__, __LINE__, 0,
                "%s: This situation is not handled yet.\n", __func__);
  }

  cs_flag_t  bc_flag = cs_cdo_bc_get_flag(bc_type);

  cs_xdef_t  *d = cs_xdef_boundary_create(CS_XDEF_BY_VALUE,
                                          dim,
                                          cs_get_bdy_zone_id(z_name),
                                          CS_FLAG_STATE_UNIFORM, // state flag
                                          bc_flag, // meta
                                          (void *)values);

  int  new_id = eqp->n_bc_defs;
  eqp->n_bc_defs += 1;
  BFT_REALLOC(eqp->bc_defs, eqp->n_bc_defs, cs_xdef_t *);
  eqp->bc_defs[new_id] = d;

  return d;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Define and initialize a new structure to set a boundary condition
 *         related to the given equation structure
 *         z_name corresponds to the name of a pre-existing cs_zone_t
 *
 * \param[in, out]  eqp       pointer to a cs_equation_param_t structure
 * \param[in]       bc_type   type of boundary condition to add
 * \param[in]       z_name    name of the related boundary zone
 * \param[in]       loc       information to know where are located values
 * \param[in]       array     pointer to an array
 * \param[in]       is_owner  transfer the lifecycle to the cs_xdef_t structure
 *                            (true or false)
 * \param[in]       index     optional pointer to the array index
 *
 * \return a pointer to the new allocated \ref cs_xdef_t structure
 */
/*----------------------------------------------------------------------------*/

cs_xdef_t *
cs_equation_add_bc_by_array(cs_equation_param_t        *eqp,
                            const cs_param_bc_type_t    bc_type,
                            const char                 *z_name,
                            cs_flag_t                   loc,
                            cs_real_t                  *array,
                            bool                        is_owner,
                            cs_lnum_t                  *index)
{
  if (eqp == NULL)
    bft_error(__FILE__, __LINE__, 0, "%s: %s\n", __func__, _err_empty_eqp);

  assert(cs_flag_test(loc, cs_flag_primal_face) ||
         cs_flag_test(loc, cs_flag_primal_vtx)  ||
         cs_flag_test(loc, cs_flag_primal_edge)); /* for circulation */

  /* Add a new cs_xdef_t structure */
  cs_xdef_array_input_t  input = {.stride = eqp->dim,
                                  .loc = loc,
                                  .values = array,
                                  .index = index,
                                  .is_owner = is_owner};

  cs_flag_t  state_flag = 0;
  if (loc == cs_flag_primal_face)
    state_flag = CS_FLAG_STATE_FACEWISE;

  int dim = eqp->dim;
  if (bc_type == CS_PARAM_BC_NEUMANN||
      bc_type == CS_PARAM_BC_HMG_NEUMANN)
    dim *= 3;  /* vector if scalar eq, tensor if vector eq. */

  if (bc_type == CS_PARAM_BC_ROBIN) {
    /* FluxNormal = alpha * (u_0 - u) + beta => Set (alpha, beta, u_0) */
    if (eqp->dim == 1)
      dim = 3;
    else
      bft_error(__FILE__, __LINE__, 0,
                "%s: This situation is not handled yet.\n", __func__);
  }

  cs_xdef_t  *d = cs_xdef_boundary_create(CS_XDEF_BY_ARRAY,
                                          dim,
                                          cs_get_bdy_zone_id(z_name),
                                          state_flag,
                                          cs_cdo_bc_get_flag(bc_type), // meta
                                          (void *)&input);

  int  new_id = eqp->n_bc_defs;
  eqp->n_bc_defs += 1;
  BFT_REALLOC(eqp->bc_defs, eqp->n_bc_defs, cs_xdef_t *);
  eqp->bc_defs[new_id] = d;

  return d;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Define and initialize a new structure to set a boundary condition
 *         related to the given equation param structure
 *         ml_name corresponds to the name of a pre-existing cs_mesh_location_t
 *
 * \param[in, out] eqp       pointer to a cs_equation_param_t structure
 * \param[in]      bc_type   type of boundary condition to add
 * \param[in]      z_name    name of the associated zone (if NULL or "" if
 *                           all cells are considered)
 * \param[in]      analytic  pointer to an analytic function defining the value
 * \param[in]      input     NULL or pointer to a structure cast on-the-fly
 *
 * \return a pointer to the new \ref cs_xdef_t structure
*/
/*----------------------------------------------------------------------------*/

cs_xdef_t *
cs_equation_add_bc_by_analytic(cs_equation_param_t        *eqp,
                               const cs_param_bc_type_t    bc_type,
                               const char                 *z_name,
                               cs_analytic_func_t         *analytic,
                               void                       *input)
{
  if (eqp == NULL)
    bft_error(__FILE__, __LINE__, 0, "%s: %s\n", __func__, _err_empty_eqp);

  /* Add a new cs_xdef_t structure */
  cs_xdef_analytic_input_t  anai = {.func = analytic,
                                    .input = input };

  int dim = eqp->dim;
  if (bc_type == CS_PARAM_BC_NEUMANN||
      bc_type == CS_PARAM_BC_HMG_NEUMANN)
    dim *= 3;  /* vector if scalar eq, tensor if vector eq. */

  if (bc_type == CS_PARAM_BC_CIRCULATION) {
    /* This is a vector-valued equation but the DoF is scalar-valued since
     * it is a circulation associated to each edge */
    if (eqp->dim == 3)
      dim = 1;
    else
      bft_error(__FILE__, __LINE__, 0,
                "%s: This situation is not handled.\n", __func__);
  }

  if (bc_type == CS_PARAM_BC_ROBIN) {
    /* FluxNormal = alpha * (u_0 - u) + beta => Set (alpha, beta, u_0) */
    if (eqp->dim == 1)
      dim = 3;
    else
      bft_error(__FILE__, __LINE__, 0,
                "%s: This situation is not handled yet.\n", __func__);
  }

  cs_xdef_t  *d = cs_xdef_boundary_create(CS_XDEF_BY_ANALYTIC_FUNCTION,
                                          dim,
                                          cs_get_bdy_zone_id(z_name),
                                          0, // state
                                          cs_cdo_bc_get_flag(bc_type), // meta
                                          &anai);

  int  new_id = eqp->n_bc_defs;
  eqp->n_bc_defs += 1;
  BFT_REALLOC(eqp->bc_defs, eqp->n_bc_defs, cs_xdef_t *);
  eqp->bc_defs[new_id] = d;

  return d;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Define and initialize a new structure to set a sliding boundary
 *         condition related to the given equation structure
 *         z_name corresponds to the name of a pre-existing cs_zone_t
 *
 * \param[in, out] eqp       pointer to a cs_equation_param_t structure
 * \param[in]      z_name    name of the related boundary zone
 */
/*----------------------------------------------------------------------------*/

void
cs_equation_add_sliding_condition(cs_equation_param_t     *eqp,
                                  const char              *z_name)
{
  if (eqp == NULL)
    bft_error(__FILE__, __LINE__, 0, "%s: %s\n", __func__, _err_empty_eqp);
  if (eqp->dim < 3)
    bft_error(__FILE__, __LINE__, 0, "%s: Invalid dimension of equation\n",
              __func__);

  /* Add two definitions: one for the normal component and one for the
     tangential component */
  BFT_REALLOC(eqp->bc_defs, eqp->n_bc_defs + 1, cs_xdef_t *);

  cs_xdef_t  *d = NULL;
  cs_real_t  val = 0;

  /* Add the homogeneous Dirichlet on the normal component */
  d = cs_xdef_boundary_create(CS_XDEF_BY_VALUE,
                              1,
                              cs_get_bdy_zone_id(z_name),
                              CS_FLAG_STATE_UNIFORM,  /* state flag */
                              CS_CDO_BC_SLIDING,      /* meta */
                              (void *)&val);

  eqp->bc_defs[eqp->n_bc_defs] = d;
  eqp->n_bc_defs += 1;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Associate a new term related to the Laplacian operator for the
 *         equation associated to the given \ref cs_equation_param_t structure
 *         Laplacian means div-grad (either for vector-valued or scalar-valued
 *         equations)
 *
 * \param[in, out] eqp        pointer to a cs_equation_param_t structure
 * \param[in]      property   pointer to a cs_property_t structure
 */
/*----------------------------------------------------------------------------*/

void
cs_equation_add_diffusion(cs_equation_param_t   *eqp,
                          cs_property_t         *property)
{
  if (eqp == NULL)
    bft_error(__FILE__, __LINE__, 0, "%s: %s\n", __func__, _err_empty_eqp);
  if (property == NULL)
    bft_error(__FILE__, __LINE__, 0,
              "%s: Eq. %s: Stop adding an empty property.",
              __func__, eqp->name);

  eqp->flag |= CS_EQUATION_DIFFUSION;
  eqp->diffusion_property = property;

  cs_property_type_t  type = cs_property_get_type(eqp->diffusion_property);
  if (type & CS_PROPERTY_ISO)
    eqp->diffusion_hodge.is_iso = true;
  else
    eqp->diffusion_hodge.is_iso = false;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Associate a new term related to the curl-curl operator for the
 *         equation associated to the given \ref cs_equation_param_t structure
 *
 * \param[in, out] eqp        pointer to a cs_equation_param_t structure
 * \param[in]      property   pointer to a cs_property_t structure
 */
/*----------------------------------------------------------------------------*/

void
cs_equation_add_curlcurl(cs_equation_param_t   *eqp,
                         cs_property_t         *property)
{
  if (eqp == NULL)
    bft_error(__FILE__, __LINE__, 0, "%s: %s\n", __func__, _err_empty_eqp);
  if (property == NULL)
    bft_error(__FILE__, __LINE__, 0,
              "%s: Eq. %s: Stop adding an empty property.",
              __func__, eqp->name);

  eqp->flag |= CS_EQUATION_CURLCURL;
  eqp->curlcurl_property = property;

  cs_property_type_t  type = cs_property_get_type(eqp->curlcurl_property);
  if (type & CS_PROPERTY_ISO)
    eqp->curlcurl_hodge.is_iso = true;
  else
    eqp->curlcurl_hodge.is_iso = false;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Associate a new term related to the grad-div operator for the
 *         equation associated to the given \ref cs_equation_param_t structure
 *
 * \param[in, out] eqp        pointer to a cs_equation_param_t structure
 * \param[in]      property   pointer to a cs_property_t structure
 */
/*----------------------------------------------------------------------------*/

void
cs_equation_add_graddiv(cs_equation_param_t   *eqp,
                        cs_property_t         *property)
{
  if (eqp == NULL)
    bft_error(__FILE__, __LINE__, 0, "%s: %s\n", __func__, _err_empty_eqp);
  if (property == NULL)
    bft_error(__FILE__, __LINE__, 0,
              "%s: Eq. %s: Stop adding an empty property.",
              __func__, eqp->name);

  eqp->flag |= CS_EQUATION_GRADDIV;
  eqp->graddiv_property = property;

  cs_property_type_t  type = cs_property_get_type(eqp->graddiv_property);
  if (type & CS_PROPERTY_ISO)
    eqp->graddiv_hodge.is_iso = true;
  else
    eqp->graddiv_hodge.is_iso = false;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Associate a new term related to the time derivative operator for
 *         the equation associated to the given \ref cs_equation_param_t
 *         structure
 *
 * \param[in, out] eqp        pointer to a cs_equation_param_t structure
 * \param[in]      property   pointer to a cs_property_t structure
 */
/*----------------------------------------------------------------------------*/

void
cs_equation_add_time(cs_equation_param_t   *eqp,
                     cs_property_t         *property)
{
  if (eqp == NULL)
    bft_error(__FILE__, __LINE__, 0, "%s: %s\n", __func__, _err_empty_eqp);
  if (property == NULL)
    bft_error(__FILE__, __LINE__, 0,
              "%s: Eq. %s: Stop adding an empty property.",
              __func__, eqp->name);

  eqp->flag |= CS_EQUATION_UNSTEADY;
  eqp->time_property = property;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Associate a new term related to the advection operator for the
 *         equation associated to the given \ref cs_equation_param_t structure
 *
 * \param[in, out] eqp        pointer to a cs_equation_param_t structure
 * \param[in]      adv_field  pointer to a cs_adv_field_t structure
 */
/*----------------------------------------------------------------------------*/

void
cs_equation_add_advection(cs_equation_param_t   *eqp,
                          cs_adv_field_t        *adv_field)
{
  if (eqp == NULL)
    bft_error(__FILE__, __LINE__, 0, "%s: %s\n", __func__, _err_empty_eqp);
  if (adv_field == NULL)
    bft_error(__FILE__, __LINE__, 0,
              "%s: Eq: %s: Stop adding an empty advection field.",
              __func__, eqp->name);

  eqp->flag |= CS_EQUATION_CONVECTION;
  eqp->adv_field = adv_field;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Associate a new term related to the reaction operator for the
 *         equation associated to the given \ref cs_equation_param_t structure
 *
 * \param[in, out] eqp        pointer to a cs_equation_param_t structure
 * \param[in]      property   pointer to a cs_property_t structure
 *
 * \return the id related to the reaction term
 */
/*----------------------------------------------------------------------------*/

int
cs_equation_add_reaction(cs_equation_param_t   *eqp,
                         cs_property_t         *property)
{
  if (eqp == NULL)
    bft_error(__FILE__, __LINE__, 0, "%s: %s\n", __func__, _err_empty_eqp);
  if (property == NULL)
    bft_error(__FILE__, __LINE__, 0,
              "%s: Eq. %s: Stop adding an empty property.",
              __func__, eqp->name);

  /* Only this kind of reaction term is available up to now.
     Add a new reaction term */
  int  new_id = eqp->n_reaction_terms;
  eqp->n_reaction_terms += 1;
  BFT_REALLOC(eqp->reaction_properties, eqp->n_reaction_terms, cs_property_t *);
  eqp->reaction_properties[new_id] = property;

  /* Flag the equation with "reaction" */
  eqp->flag |= CS_EQUATION_REACTION;

  return new_id;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Add a new source term by initializing a cs_xdef_t structure.
 *         Case of a definition by a constant value
 *
 * \param[in, out] eqp       pointer to a cs_equation_param_t structure
 * \param[in]      z_name    name of the associated zone (if NULL or
 *                            "" all cells are considered)
 * \param[in]      val       pointer to the value
 *
 * \return a pointer to the new \ref cs_xdef_t structure
 */
/*----------------------------------------------------------------------------*/

cs_xdef_t *
cs_equation_add_source_term_by_val(cs_equation_param_t    *eqp,
                                   const char             *z_name,
                                   cs_real_t              *val)
{
  if (eqp == NULL)
    bft_error(__FILE__, __LINE__, 0, "%s: %s\n", __func__, _err_empty_eqp);

  /* Add a new cs_xdef_t structure */
  int z_id = cs_get_vol_zone_id(z_name);

  /* Define a flag according to the kind of space discretization */
  cs_flag_t  state_flag = CS_FLAG_STATE_DENSITY | CS_FLAG_STATE_UNIFORM;
  cs_flag_t  meta_flag = cs_source_term_set_default_flag(eqp->space_scheme);

  if (z_id == 0)
    meta_flag |= CS_FLAG_FULL_LOC;

  cs_xdef_t  *d = cs_xdef_volume_create(CS_XDEF_BY_VALUE,
                                        eqp->dim,
                                        z_id,
                                        state_flag,
                                        meta_flag,
                                        (void *)val);

  int  new_id = eqp->n_source_terms;
  eqp->n_source_terms += 1;
  BFT_REALLOC(eqp->source_terms, eqp->n_source_terms, cs_xdef_t *);
  eqp->source_terms[new_id] = d;

  return d;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Add a new source term by initializing a cs_xdef_t structure.
 *         Case of a definition by an analytical function
 *
 * \param[in, out] eqp       pointer to a cs_equation_param_t structure
 * \param[in]      z_name    name of the associated zone (if NULL or "" if
 *                           all cells are considered)
 * \param[in]      func      pointer to an analytical function
 * \param[in]      input     NULL or pointer to a structure cast on-the-fly
 *
 * \return a pointer to the new \ref cs_xdef_t structure
 */
/*----------------------------------------------------------------------------*/

cs_xdef_t *
cs_equation_add_source_term_by_analytic(cs_equation_param_t    *eqp,
                                        const char             *z_name,
                                        cs_analytic_func_t     *func,
                                        void                   *input)
{
  if (eqp == NULL)
    bft_error(__FILE__, __LINE__, 0, "%s: %s\n", __func__, _err_empty_eqp);

  /* Add a new cs_xdef_t structure */
  int z_id = cs_get_vol_zone_id(z_name);

  /* Define a flag according to the kind of space discretization */
  cs_flag_t  state_flag = CS_FLAG_STATE_DENSITY;
  cs_flag_t  meta_flag = cs_source_term_set_default_flag(eqp->space_scheme);

  if (z_id == 0)
    meta_flag |= CS_FLAG_FULL_LOC;

  cs_xdef_analytic_input_t  ana_input = {.func = func, .input = input };
  cs_xdef_t  *d = cs_xdef_volume_create(CS_XDEF_BY_ANALYTIC_FUNCTION,
                                        eqp->dim,
                                        z_id,
                                        state_flag,
                                        meta_flag,
                                        &ana_input);

  /* Default setting for quadrature is different in this case */
  cs_xdef_set_quadrature(d, CS_QUADRATURE_BARY_SUBDIV);

  int  new_id = eqp->n_source_terms;
  eqp->n_source_terms += 1;
  BFT_REALLOC(eqp->source_terms, eqp->n_source_terms, cs_xdef_t *);
  eqp->source_terms[new_id] = d;

  return d;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Add a new source term by initializing a cs_xdef_t structure.
 *         Case of a definition by a DoF function
 *
 * \param[in, out] eqp       pointer to a cs_equation_param_t structure
 * \param[in]      z_name    name of the associated zone (if NULL or "" if
 *                           all cells are considered)
 * \param[in]      loc_flag  location of element ids given as parameter
 * \param[in]      func      pointer to a DoF function
 * \param[in]      input     NULL or pointer to a structure cast on-the-fly
 *
 * \return a pointer to the new \ref cs_xdef_t structure
 */
/*----------------------------------------------------------------------------*/

cs_xdef_t *
cs_equation_add_source_term_by_dof_func(cs_equation_param_t    *eqp,
                                        const char             *z_name,
                                        cs_flag_t               loc_flag,
                                        cs_dof_func_t          *func,
                                        void                   *input)
{
  if (eqp == NULL)
    bft_error(__FILE__, __LINE__, 0, "%s: %s\n", __func__, _err_empty_eqp);

  /* Add a new cs_xdef_t structure */
  int z_id = cs_get_vol_zone_id(z_name);

  /* Define a flag according to the kind of space discretization */
  cs_flag_t  state_flag = CS_FLAG_STATE_DENSITY;
  cs_flag_t  meta_flag = cs_source_term_set_default_flag(eqp->space_scheme);

  if (z_id == 0)
    meta_flag |= CS_FLAG_FULL_LOC;

  cs_xdef_dof_input_t  context = { .func = func,
                                   .input = input,
                                   .loc = loc_flag };

  cs_xdef_t  *d = cs_xdef_volume_create(CS_XDEF_BY_DOF_FUNCTION,
                                        eqp->dim,
                                        z_id,
                                        state_flag,
                                        meta_flag,
                                        &context);

  /* Default setting for quadrature is different in this case */
  cs_xdef_set_quadrature(d, CS_QUADRATURE_BARY_SUBDIV);

  int  new_id = eqp->n_source_terms;
  eqp->n_source_terms += 1;
  BFT_REALLOC(eqp->source_terms, eqp->n_source_terms, cs_xdef_t *);
  eqp->source_terms[new_id] = d;

  return d;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Add a new source term by initializing a cs_xdef_t structure.
 *         Case of a definition by an array.
 *
 * \param[in, out] eqp       pointer to a cs_equation_param_t structure
 * \param[in]      z_name    name of the associated zone (if NULL or "" if
 *                           all cells are considered)
 * \param[in]      loc       information to know where are located values
 * \param[in]      array     pointer to an array
 * \param[in]      is_owner  transfer the lifecycle to the cs_xdef_t structure
 *                           (true or false)
 * \param[in]      index     optional pointer to the array index
 *
 * \return a pointer to the new \ref cs_xdef_t structure
 */
/*----------------------------------------------------------------------------*/

cs_xdef_t *
cs_equation_add_source_term_by_array(cs_equation_param_t    *eqp,
                                     const char             *z_name,
                                     cs_flag_t               loc,
                                     cs_real_t              *array,
                                     bool                    is_owner,
                                     cs_lnum_t              *index)
{
  if (eqp == NULL)
    bft_error(__FILE__, __LINE__, 0, "%s: %s\n", __func__, _err_empty_eqp);

  /* Add a new cs_xdef_t structure */
  int z_id = cs_get_vol_zone_id(z_name);

  /* Define a flag according to the kind of space discretization */
  cs_flag_t  state_flag = CS_FLAG_STATE_DENSITY;
  if (cs_flag_test(loc, cs_flag_primal_cell) == true)
    state_flag |= CS_FLAG_STATE_CELLWISE;

  cs_flag_t  meta_flag = cs_source_term_set_default_flag(eqp->space_scheme);

  if (z_id == 0)
    meta_flag |= CS_FLAG_FULL_LOC;

  cs_xdef_array_input_t  input = {.stride = eqp->dim,
                                  .loc = loc,
                                  .values = array,
                                  .is_owner = is_owner,
                                  .index = index };

  cs_xdef_t  *d = cs_xdef_volume_create(CS_XDEF_BY_ARRAY,
                                        eqp->dim,
                                        z_id,
                                        state_flag,
                                        meta_flag,
                                        (void *)&input);

  int  new_id = eqp->n_source_terms;
  eqp->n_source_terms += 1;
  BFT_REALLOC(eqp->source_terms, eqp->n_source_terms, cs_xdef_t *);
  eqp->source_terms[new_id] = d;

  return d;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Add an enforcement of the value of degrees of freedom located at
 *         mesh vertices.
 *         The spatial discretization scheme for the given equation has to be
 *         CDO-Vertex based or CDO-Vertex+Cell-based schemes.
 *
 *         One assumes that values are interlaced if eqp->dim > 1
 *         ref_value or elt_values has to be defined. If both parameters are
 *         defined, one keeps the definition in elt_values
 *
 * \param[in, out] eqp         pointer to a cs_equation_param_t structure
 * \param[in]      n_elts      number of vertices to enforce
 * \param[in]      elt_ids     list of vertices
 * \param[in]      ref_value   ignored if NULL
 * \param[in]      elt_values  list of associated values, ignored if NULL
 */
/*----------------------------------------------------------------------------*/

void
cs_equation_enforce_vertex_dofs(cs_equation_param_t    *eqp,
                                cs_lnum_t               n_elts,
                                const cs_lnum_t         elt_ids[],
                                const cs_real_t         ref_value[],
                                const cs_real_t         elt_values[])
{
  if (n_elts < 1)
    return; /* Nothing to do */

  /* Sanity checks */
  if (eqp == NULL)
    bft_error(__FILE__, __LINE__, 0, "%s: %s\n", __func__, _err_empty_eqp);
  if (eqp->enforcement_type & CS_EQUATION_ENFORCE_BY_CELLS)
    bft_error(__FILE__, __LINE__, 0,
              "%s: Eq: %s: Two types of enforcement are requested (by DoFs and"
              " by cells).\n", __func__, eqp->name);
  if (eqp->space_scheme != CS_SPACE_SCHEME_CDOVB &&
      eqp->space_scheme != CS_SPACE_SCHEME_CDOVCB)
    bft_error(__FILE__, __LINE__, 0,
              "%s: Eq: %s: Invalid space scheme.\n"
              "This should be a vertex-based one.", __func__, eqp->name);
  if (ref_value == NULL && elt_values == NULL)
    bft_error(__FILE__, __LINE__, 0,
              "%s: Eq: %s: No enforcement value.\n",
              __func__, eqp->name);

  if (eqp->n_enforced_dofs > 0) { /* Reset the selection of DoFs */
    eqp->n_enforced_dofs = 0;
    BFT_FREE(eqp->enforced_dof_ids);
    BFT_FREE(eqp->enforced_dof_values);
  }
  if (eqp->n_enforced_cells > 0) { /* Reset the selection of cells */
    eqp->n_enforced_cells = 0;
    BFT_FREE(eqp->enforced_cell_ids);
    BFT_FREE(eqp->enforced_cell_values);
  }

  eqp->flag |= CS_EQUATION_FORCE_VALUES;
  eqp->n_enforced_dofs = n_elts;
  eqp->enforcement_type = CS_EQUATION_ENFORCE_BY_DOFS;

  BFT_MALLOC(eqp->enforced_dof_ids, n_elts, cs_lnum_t);
  memcpy(eqp->enforced_dof_ids, elt_ids, n_elts*sizeof(cs_lnum_t));

  if (elt_values == NULL) { /* Use a reference value for all enforced DoFs */

    eqp->enforcement_type |= CS_EQUATION_ENFORCE_BY_REFERENCE_VALUE;
    for (int i = 0; i < eqp->dim; i++)
      eqp->enforcement_ref_value[i] = ref_value[i];

  }
  else {

    /* Copy user-defined data in the structure */
    const cs_lnum_t  size = eqp->dim*n_elts;
    BFT_MALLOC(eqp->enforced_dof_values, size, cs_real_t);
    memcpy(eqp->enforced_dof_values, elt_values, size*sizeof(cs_real_t));

  }
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief  Add an enforcement of the value related to the degrees of freedom
 *         associated to the list of selected cells.
 *
 *         One assumes that values are interlaced if eqp->dim > 1
 *         ref_value or elt_values has to be defined. If both parameters are
 *         defined, one keeps the definition in elt_values
 *
 * \param[in, out] eqp         pointer to a cs_equation_param_t structure
 * \param[in]      n_elts      number of selected cells
 * \param[in]      elt_ids     list of cell ids
 * \param[in]      ref_value   ignored if NULL
 * \param[in]      elt_values  list of associated values, ignored if NULL
 */
/*----------------------------------------------------------------------------*/

void
cs_equation_enforce_by_cell_selection(cs_equation_param_t    *eqp,
                                      cs_lnum_t               n_elts,
                                      const cs_lnum_t         elt_ids[],
                                      const cs_real_t         ref_value[],
                                      const cs_real_t         elt_values[])
{
  if (n_elts < 1)
    return; /* Nothing to do */

  /* Sanity checks */
  if (eqp == NULL)
    bft_error(__FILE__, __LINE__, 0, "%s: %s\n", __func__, _err_empty_eqp);
  if (eqp->enforcement_type & CS_EQUATION_ENFORCE_BY_DOFS)
    bft_error(__FILE__, __LINE__, 0,
              "%s: Eq: %s: Two types of enforcement are requested (by DoFs and"
              " by cells).\n", __func__, eqp->name);
  if (ref_value == NULL && elt_values == NULL)
    bft_error(__FILE__, __LINE__, 0,
              "%s: Eq: %s: No enforcement value.\n",
              __func__, eqp->name);

  if (eqp->n_enforced_dofs > 0) { /* Reset the selection of DoFs */
    eqp->n_enforced_dofs = 0;
    BFT_FREE(eqp->enforced_dof_ids);
    BFT_FREE(eqp->enforced_dof_values);
  }
  if (eqp->n_enforced_cells > 0) { /* Reset the selection of cells */
    eqp->n_enforced_cells = 0;
    BFT_FREE(eqp->enforced_cell_ids);
    BFT_FREE(eqp->enforced_cell_values);
  }

  eqp->n_enforced_cells = n_elts;
  eqp->enforcement_type = CS_EQUATION_ENFORCE_BY_CELLS;
  eqp->flag |= CS_EQUATION_FORCE_VALUES;

  BFT_MALLOC(eqp->enforced_cell_ids, n_elts, cs_lnum_t);
  memcpy(eqp->enforced_cell_ids, elt_ids, n_elts*sizeof(cs_lnum_t));

  if (elt_values == NULL) { /* Use a reference value for all enforced DoFs */

    eqp->enforcement_type |= CS_EQUATION_ENFORCE_BY_REFERENCE_VALUE;
    for (int i = 0; i < eqp->dim; i++)
      eqp->enforcement_ref_value[i] = ref_value[i];

  }
  else {

    /* Copy user-defined data in the structure */
    const cs_lnum_t  size = eqp->dim*n_elts;
    BFT_MALLOC(eqp->enforced_cell_values, size, cs_real_t);
    memcpy(eqp->enforced_cell_values, elt_values, size*sizeof(cs_real_t));

  }
}

/*----------------------------------------------------------------------------*/

END_C_DECLS
