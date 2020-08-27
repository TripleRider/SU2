/*!
 * \file roe.cpp
 * \brief Implementations of Roe-type schemes.
 * \author F. Palacios, T. Economon
 * \version 7.0.6 "Blackbird"
 *
 * SU2 Project Website: https://su2code.github.io
 *
 * The SU2 Project is maintained by the SU2 Foundation
 * (http://su2foundation.org)
 *
 * Copyright 2012-2020, SU2 Contributors (cf. AUTHORS.md)
 *
 * SU2 is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * SU2 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with SU2. If not, see <http://www.gnu.org/licenses/>.
 */

#include "../../../../include/numerics/flow/convection/roe.hpp"

CUpwRoeBase_Flow::CUpwRoeBase_Flow(unsigned short val_nDim, unsigned short val_nVar, const CConfig* config,
                                   bool val_low_dissipation, bool val_muscl) : CNumerics(val_nDim, val_nVar, config) {

  implicit = (config->GetKind_TimeIntScheme_Flow() == EULER_IMPLICIT);
  /* A grid is defined as dynamic if there's rigid grid movement or grid deformation AND the problem is time domain */
  dynamic_grid = config->GetDynamic_Grid();
  kappa = config->GetRoe_Kappa(); // 1 is unstable

  muscl_kappa = 0.5*config->GetMUSCL_Kappa();
  muscl = val_muscl;
  tkeNeeded = (config->GetKind_Turb_Model() == SST) || (config->GetKind_Turb_Model() == SST_SUST);
  nPrimVarTot = nVar + tkeNeeded;

  Gamma = config->GetGamma();
  Gamma_Minus_One = Gamma - 1.0;

  roe_low_dissipation = val_low_dissipation;

  Flux = new su2double [nVar];
  Diff_U = new su2double [nPrimVarTot];
  ProjFlux_i = new su2double [nVar];
  ProjFlux_j = new su2double [nVar];
  Conservatives_i = new su2double [nVar];
  Conservatives_j = new su2double [nVar];
  Lambda = new su2double [nPrimVarTot];
  Epsilon = new su2double [nPrimVarTot];
  P_Tensor = new su2double* [nVar];
  invP_Tensor = new su2double* [nPrimVarTot];
  Jacobian_i = new su2double* [nVar];
  Jacobian_j = new su2double* [nVar];
  for (auto iVar = 0; iVar < nVar; iVar++) {
    P_Tensor[iVar] = new su2double [nPrimVarTot];
    invP_Tensor[iVar] = new su2double [nVar];
    Jacobian_i[iVar] = new su2double [nVar];
    Jacobian_j[iVar] = new su2double [nVar];
  }

  if (tkeNeeded) invP_Tensor[nVar] = new su2double[nVar];
}

CUpwRoeBase_Flow::~CUpwRoeBase_Flow(void) {

  delete [] Flux;
  delete [] Diff_U;
  delete [] ProjFlux_i;
  delete [] ProjFlux_j;
  delete [] Conservatives_i;
  delete [] Conservatives_j;
  delete [] Lambda;
  delete [] Epsilon;
  for (auto iVar = 0; iVar < nVar; iVar++) {
    delete [] P_Tensor[iVar];
    delete [] invP_Tensor[iVar];
    delete [] Jacobian_i[iVar];
    delete [] Jacobian_j[iVar];
  }
  if (tkeNeeded) delete [] invP_Tensor[nVar];
  delete [] P_Tensor;
  delete [] invP_Tensor;
  delete [] Jacobian_i;
  delete [] Jacobian_j;

}

// void CUpwRoeBase_Flow::GetMUSCLJac(su2double **jac_i, su2double **jac_j,
//                                    const su2double *lim_i, const su2double *lim_j,
//                                    const su2double *turblim_i, const su2double *turblim_j,
//                                    const su2double *primvar_i, const su2double *primvar_j,
//                                    const su2double *primvar_n_i, const su2double *primvar_n_j,
//                                    const su2double *k_i, const su2double *k_j,
//                                    const su2double *k_n_i, const su2double *k_n_j) {
//   const bool wasActive = AD::BeginPassive();
//   constexpr size_t MAXNVAR = 5;

//   su2double dFdV_i[MAXNVAR][MAXNVAR+1] = {0.0},
//             dFdV_j[MAXNVAR][MAXNVAR+1] = {0.0},
//             dUdV_i[MAXNVAR][MAXNVAR+1] = {0.0},
//             dUdV_j[MAXNVAR][MAXNVAR+1] = {0.0},
//             dVdU_i[MAXNVAR+1][MAXNVAR] = {0.0},
//             dVdU_j[MAXNVAR+1][MAXNVAR] = {0.0};

//   /*--- Store primitives ---*/

//   const su2double r_i = primvar_i[nDim+2], inv_r_n_i = 1.0/primvar_n_i[nDim+2],
//                   r_j = primvar_j[nDim+2], inv_r_n_j = 1.0/primvar_n_j[nDim+2];
//   su2double vel_i[MAXNDIM] = {0.0}, vel_n_i[MAXNDIM] = {0.0},
//             vel_j[MAXNDIM] = {0.0}, vel_n_j[MAXNDIM] = {0.0};

//   for (auto iDim = 0; iDim < nDim; iDim++) {
//     vel_i[iDim] = primvar_i[iDim+1]; vel_n_i[iDim] = primvar_n_i[iDim+1];
//     vel_j[iDim] = primvar_j[iDim+1]; vel_n_j[iDim] = primvar_n_j[iDim+1];
//   }

//   su2double sq_vel_i = 0.0, sq_vel_n_i = 0.0,
//             sq_vel_j = 0.0, sq_vel_n_j = 0.0;
//   for (auto iDim = 0; iDim < nDim; iDim++) {
//     sq_vel_i += pow(vel_i[iDim], 2.0); sq_vel_n_i += pow(vel_n_i[iDim], 2.0);
//     sq_vel_j += pow(vel_j[iDim], 2.0); sq_vel_n_j += pow(vel_n_j[iDim], 2.0);
//   }

//   /*--- Store limiters in single vector in proper order ---*/
//   su2double l_i[MAXNVAR+1] = {0.0}, l_j[MAXNVAR+1] = {0.0};
//   l_i[0] = lim_i[nDim+2]; l_j[0] = lim_j[nDim+2];
//   for (auto iDim = 0; iDim < nDim; iDim++) {
//     l_i[iDim+1] = lim_i[iDim+1]; l_j[iDim+1] = lim_j[iDim+1];
//   }
//   l_i[nDim+1] = lim_i[nDim+1]; l_j[nDim+1] = lim_j[nDim+1];
//   if (tkeNeeded) {
//     l_i[nDim+2] = turblim_i[0]; l_j[nDim+2] = turblim_j[0];
//   }

//   /*--- dU/d{r,v,p,k}, evaluated at face ---*/

//   dUdV_i[0][0] = dUdV_j[0][0] = 1.0;

//   for (auto iDim = 0; iDim < nDim; iDim++) {
//     dUdV_i[iDim+1][0] = vel_i[iDim];
//     dUdV_j[iDim+1][0] = vel_j[iDim];

//     dUdV_i[iDim+1][iDim+1] = r_i;
//     dUdV_j[iDim+1][iDim+1] = r_j;

//     dUdV_i[nDim+1][iDim+1] = r_i*vel_i[iDim];
//     dUdV_j[nDim+1][iDim+1] = r_j*vel_j[iDim];
//   }

//   dUdV_i[nDim+1][0] = 0.5*sq_vel_i+(*k_i);
//   dUdV_j[nDim+1][0] = 0.5*sq_vel_j+(*k_j);

//   dUdV_i[nDim+1][nDim+1] = dUdV_j[nDim+1][nDim+1] = 1.0/Gamma_Minus_One;

//   if (tkeNeeded) {
//     dUdV_i[nDim+1][nDim+2] = r_i;
//     dUdV_j[nDim+1][nDim+2] = r_j;
//   }

//   /*--- d{r,v,p,k}/dU, evaluated at node ---*/

//   dVdU_i[0][0] = dVdU_j[0][0] = 1.0;

//   for (auto iDim = 0; iDim < nDim; iDim++) {
//     dVdU_i[iDim+1][0] = -vel_n_i[iDim]*inv_r_n_i;
//     dVdU_j[iDim+1][0] = -vel_n_j[iDim]*inv_r_n_j;

//     dVdU_i[iDim+1][iDim+1] = inv_r_n_i;
//     dVdU_j[iDim+1][iDim+1] = inv_r_n_j;

//     dVdU_i[nDim+1][iDim+1] = -Gamma_Minus_One*vel_n_i[iDim];
//     dVdU_j[nDim+1][iDim+1] = -Gamma_Minus_One*vel_n_j[iDim];
//   }

//   dVdU_i[nDim+1][0] = 0.5*Gamma_Minus_One*sq_vel_n_i;
//   dVdU_j[nDim+1][0] = 0.5*Gamma_Minus_One*sq_vel_n_j;

//   dVdU_i[nDim+1][nDim+1] = dVdU_j[nDim+1][nDim+1] = Gamma_Minus_One;

//   if (tkeNeeded) {
//     dVdU_i[nDim+2][0] = -(*k_n_i)*inv_r_n_i;
//     dVdU_j[nDim+2][0] = -(*k_n_j)*inv_r_n_j;
//   }

//   /*--- Now multiply them all together ---*/

//   for (auto iVar = 0; iVar < nVar; iVar++) {
//     for (auto jVar = 0; jVar < nPrimVarTot; jVar++) {
//       for (auto kVar = 0; kVar < nVar; kVar++) {
//         dFdV_i[iVar][jVar] += jac_i[iVar][kVar]*dUdV_i[kVar][jVar];
//         dFdV_j[iVar][jVar] += jac_j[iVar][kVar]*dUdV_j[kVar][jVar];
//       }
//     }
//   }

//   for (auto iVar = 0; iVar < nVar; iVar++) {
//     for (auto jVar = 0; jVar < nVar; jVar++) {
//       jac_i[iVar][jVar] = 0.0;
//       jac_j[iVar][jVar] = 0.0;
//       for (auto kVar = 0; kVar < nPrimVarTot; kVar++) {
//         jac_i[iVar][jVar] += (dFdV_i[iVar][kVar]*(1.0-muscl_kappa*l_i[kVar])
//                            +  dFdV_j[iVar][kVar]*muscl_kappa*l_j[kVar])*dVdU_i[kVar][jVar];
//         jac_j[iVar][jVar] += (dFdV_j[iVar][kVar]*(1.0-muscl_kappa*l_j[kVar])
//                            +  dFdV_i[iVar][kVar]*muscl_kappa*l_i[kVar])*dVdU_j[kVar][jVar];
//       }
//     }
//   }

//   AD::EndPassive(wasActive);
// }

void CUpwRoeBase_Flow::GetMUSCLJac(su2double **jac_i, su2double **jac_j,
                                   const su2double *lim_i, const su2double *lim_j,
                                   const su2double *turblim_i, const su2double *turblim_j,
                                   const su2double *primvar_i, const su2double *primvar_j,
                                   const su2double *primvar_n_i, const su2double *primvar_n_j,
                                   const su2double *k_i, const su2double *k_j,
                                   const su2double *k_n_i, const su2double *k_n_j) {
  const bool wasActive = AD::BeginPassive();

  for (auto iVar = 0; iVar < nVar; iVar++) {
    for (auto jVar = 0; jVar < nVar; jVar++) {
      const su2double dFidUi = jac_i[iVar][jVar]*(1.0-muscl_kappa*lim_i[jVar]);
      const su2double dFjdUj = jac_j[iVar][jVar]*(1.0-muscl_kappa*lim_j[jVar]);
      const su2double dFjdUi = jac_i[iVar][jVar]*muscl*lim_j[jVar];
      const su2double dFidUj = jac_j[iVar][jVar]*muscl*lim_i[jVar];

      jac_i[iVar][jVar] = dFidUi+dFjdUi;
      jac_j[iVar][jVar] = dFidUj+dFjdUj;
    }
  }

  AD::EndPassive(wasActive);
}

void CUpwRoeBase_Flow::FinalizeResidual(su2double *val_residual, su2double **val_Jacobian_i,
                                        su2double **val_Jacobian_j, const CConfig* config) {
/*---
 CUpwRoeBase_Flow::ComputeResidual initializes the residual (flux) and its Jacobians with the standard Roe averaging
 fc_{1/2} = kappa*(fc_i+fc_j)*Normal. It then calls this method, which derived classes specialize, to account for
 the dissipation part.
---*/
}

CNumerics::ResidualType<> CUpwRoeBase_Flow::ComputeResidual(const CConfig* config) {

  implicit = (config->GetKind_TimeIntScheme() == EULER_IMPLICIT);

  su2double ProjGridVel = 0.0, Energy_i, Energy_j;

  AD::StartPreacc();
  // AD::SetPreaccIn(V_i, nDim+4); AD::SetPreaccIn(V_j, nDim+4); 
  AD::SetPreaccIn(U_i, nVar); AD::SetPreaccIn(U_j, nVar); 
  AD::SetPreaccIn(Normal, nDim);
  if (dynamic_grid) {
    AD::SetPreaccIn(GridVel_i, nDim); AD::SetPreaccIn(GridVel_j, nDim);
  }
  if (roe_low_dissipation){
    AD::SetPreaccIn(Sensor_i); AD::SetPreaccIn(Sensor_j);
    AD::SetPreaccIn(Dissipation_i); AD::SetPreaccIn(Dissipation_j);
  }
  AD::SetPreaccIn(turb_ke_i); AD::SetPreaccIn(turb_ke_j);

  /*--- Face area (norm or the normal vector) and unit normal ---*/

  Area = 0.0;
  for (auto iDim = 0; iDim < nDim; iDim++)
    Area += Normal[iDim]*Normal[iDim];
  Area = sqrt(Area);

  for (auto iDim = 0; iDim < nDim; iDim++)
    UnitNormal[iDim] = Normal[iDim]/Area;

  // /*--- Primitive variables at point i ---*/

  // for (auto iDim = 0; iDim < nDim; iDim++)
  //   Velocity_i[iDim] = V_i[iDim+1];
  // Pressure_i = V_i[nDim+1];
  // Density_i  = V_i[nDim+2];
  // Enthalpy_i = V_i[nDim+3];
  // Energy_i = Enthalpy_i - Pressure_i/Density_i;
  // SoundSpeed_i = sqrt(fabs(Pressure_i*Gamma/Density_i));

  // /*--- Primitive variables at point j ---*/

  // for (auto iDim = 0; iDim < nDim; iDim++)
  //   Velocity_j[iDim] = V_j[iDim+1];
  // Pressure_j = V_j[nDim+1];
  // Density_j  = V_j[nDim+2];
  // Enthalpy_j = V_j[nDim+3];
  // Energy_j = Enthalpy_j - Pressure_j/Density_j;
  // SoundSpeed_j = sqrt(fabs(Pressure_j*Gamma/Density_j));

  /*--- Primitive variables at point i ---*/

  su2double SqVel_i = 0.0;
  Density_i  = U_i[0];
  for (auto iDim = 0; iDim < nDim; iDim++) {
    Velocity_i[iDim] = U_i[iDim+1]/U_i[0];
    SqVel_i += Velocity_i[iDim]*Velocity_i[iDim];

  }
  turb_ke_i /= Density_i;
  Energy_i = U_i[nDim+1]/U_i[0];
  Pressure_i = Gamma_Minus_One*(U_i[nDim+1]-0.5*Density_i*SqVel_i-Density_i*turb_ke_i);
  Enthalpy_i = Energy_i+Pressure_i/Density_i;
  SoundSpeed_i = sqrt(fabs(Pressure_i*Gamma/Density_i));

  /*--- Primitive variables at point j ---*/

  su2double SqVel_j = 0.0;
  Density_j  = U_j[0];
  for (auto iDim = 0; iDim < nDim; iDim++) {
    Velocity_j[iDim] = U_j[iDim+1]/U_j[0];
    SqVel_j += Velocity_j[iDim]*Velocity_j[iDim];

  }
  turb_ke_j /= Density_j;
  Energy_j = U_j[nDim+1]/U_j[0];
  Pressure_j = Gamma_Minus_One*(U_j[nDim+1]-0.5*Density_j*SqVel_j-Density_j*turb_ke_j);
  Enthalpy_j = Energy_j+Pressure_j/Density_j;
  SoundSpeed_j = sqrt(fabs(Pressure_j*Gamma/Density_j));

  /*--- Compute variables that are common to the derived schemes ---*/

  /*--- Roe-averaged variables at interface between i & j ---*/

  su2double R = sqrt(fabs(Density_j/Density_i));
  RoeDensity = R*Density_i;
  RoeSqVel = 0.0;
  for (auto iDim = 0; iDim < nDim; iDim++) {
    RoeVelocity[iDim] = (R*Velocity_j[iDim]+Velocity_i[iDim])/(R+1.);
    RoeSqVel += RoeVelocity[iDim]*RoeVelocity[iDim];
  }
  RoeEnthalpy = (R*Enthalpy_j+Enthalpy_i)/(R+1.);
  RoeTke = (R*turb_ke_j+turb_ke_i)/(R+1.);
  RoeSoundSpeed2 = Gamma_Minus_One*(RoeEnthalpy-0.5*RoeSqVel-RoeTke);

  /*--- Negative RoeSoundSpeed^2, the jump variables is too large, clear fluxes and exit. ---*/

  if (RoeSoundSpeed2 <= 0.0) {
    for (auto iVar = 0; iVar < nVar; iVar++) {
      Flux[iVar] = 0.0;
      if (implicit){
        for (auto jVar = 0; jVar < nVar; jVar++) {
          Jacobian_i[iVar][jVar] = 0.0;
          Jacobian_j[iVar][jVar] = 0.0;
        }
      }
    }
    AD::SetPreaccOut(Flux, nVar);
    AD::EndPreacc();

    return ResidualType<>(Flux, Jacobian_i, Jacobian_j);
  }

  RoeSoundSpeed = sqrt(RoeSoundSpeed2);

  /*--- P tensor ---*/

  GetPMatrix(&RoeDensity, RoeVelocity, &RoeTke, &RoeSoundSpeed, UnitNormal, P_Tensor);

  /*--- Projected velocity adjusted for mesh motion ---*/

  ProjVelocity = 0.0; ProjVelocity_i = 0.0; ProjVelocity_j = 0.0;
  for (auto iDim = 0; iDim < nDim; iDim++) {
    ProjVelocity   += RoeVelocity[iDim]*UnitNormal[iDim];
    ProjVelocity_i += Velocity_i[iDim]*UnitNormal[iDim];
    ProjVelocity_j += Velocity_j[iDim]*UnitNormal[iDim];
  }

  if (dynamic_grid) {
    su2double ProjGridVel = 0.0;
    for (auto iDim = 0; iDim < nDim; iDim++) {
      ProjGridVel   += 0.5*(GridVel_i[iDim]+GridVel_j[iDim])*UnitNormal[iDim];
    }
    ProjVelocity   -= ProjGridVel;
    ProjVelocity_i -= ProjGridVel;
    ProjVelocity_j -= ProjGridVel;
  }

  /*--- Flow eigenvalues ---*/

  for (auto iDim = 0; iDim < nDim; iDim++)
    Lambda[iDim] = ProjVelocity;

  Lambda[nVar-2] = ProjVelocity + RoeSoundSpeed;
  Lambda[nVar-1] = ProjVelocity - RoeSoundSpeed;

  /*--- Harten and Hyman (1983) entropy correction ---*/

  for (auto iDim = 0; iDim < nDim; iDim++)
    Epsilon[iDim] = 4.0*max(0.0, max(Lambda[iDim]-ProjVelocity_i, ProjVelocity_j-Lambda[iDim]));

  Epsilon[nVar-2] = 4.0*max(0.0, max(Lambda[nVar-2]-(ProjVelocity_i+SoundSpeed_i),(ProjVelocity_j+SoundSpeed_j)-Lambda[nVar-2]));
  Epsilon[nVar-1] = 4.0*max(0.0, max(Lambda[nVar-1]-(ProjVelocity_i-SoundSpeed_i),(ProjVelocity_j-SoundSpeed_j)-Lambda[nVar-1]));

  if (tkeNeeded) {
    Lambda[nVar] = ProjVelocity;
    Epsilon[nVar] = 4.0*max(0.0, max(Lambda[nVar]-ProjVelocity_i,ProjVelocity_j-Lambda[nVar]));
  }

  for (auto iVar = 0; iVar < nPrimVarTot; iVar++) {
    if ( fabs(Lambda[iVar]) < Epsilon[iVar] )
      Lambda[iVar] = 0.5*(Lambda[iVar]*Lambda[iVar]/Epsilon[iVar] + Epsilon[iVar]);
    else
      Lambda[iVar] = fabs(Lambda[iVar]);
  }

  /*--- Reconstruct conservative variables ---*/

  Conservatives_i[0] = Density_i;
  Conservatives_j[0] = Density_j;

  for (auto iDim = 0; iDim < nDim; iDim++) {
    Conservatives_i[iDim+1] = Density_i*Velocity_i[iDim];
    Conservatives_j[iDim+1] = Density_j*Velocity_j[iDim];
  }
  Conservatives_i[nDim+1] = Density_i*Energy_i;
  Conservatives_j[nDim+1] = Density_j*Energy_j;

  /*--- Compute left and right fluxes ---*/

  GetInviscidProjFlux(&Density_i, Velocity_i, &Pressure_i, &Enthalpy_i, Normal, ProjFlux_i);
  GetInviscidProjFlux(&Density_j, Velocity_j, &Pressure_j, &Enthalpy_j, Normal, ProjFlux_j);

  /*--- Initialize residual (flux) and Jacobians ---*/

  for (auto iVar = 0; iVar < nVar; iVar++)
    Flux[iVar] = kappa*(ProjFlux_i[iVar]+ProjFlux_j[iVar]);

  if (implicit) {
    GetInviscidProjJac(Velocity_i, &Energy_i, &turb_ke_i, Normal, kappa, Jacobian_i);
    GetInviscidProjJac(Velocity_j, &Energy_j, &turb_ke_j, Normal, kappa, Jacobian_j);
  }

  /*--- Finalize in children class ---*/

  FinalizeResidual(Flux, Jacobian_i, Jacobian_j, config);

  /*--- Correct for grid motion ---*/

  if (dynamic_grid) {
    for (auto iVar = 0; iVar < nVar; iVar++) {
      Flux[iVar] -= ProjGridVel*Area * 0.5*(Conservatives_i[iVar]+Conservatives_j[iVar]);

      if (implicit) {
        Jacobian_i[iVar][iVar] -= 0.5*ProjGridVel*Area;
        Jacobian_j[iVar][iVar] -= 0.5*ProjGridVel*Area;
      }
    }
  }

  if (implicit && muscl) {

    /*--- Extract nodal values ---*/

    su2double turb_ke_n_i = 0.0, turb_ke_n_j = 0.0;
    if (tkeNeeded) {
      turb_ke_n_i = TurbVarn_i[0];
      turb_ke_n_j = TurbVarn_j[0];
    }

    /*--- Compute Jacobian wrt extrapolation ---*/

    GetMUSCLJac(Jacobian_i, Jacobian_j, Limiter_i, Limiter_j, TurbLimiter_i, TurbLimiter_j, 
                V_i, V_j, Vn_i, Vn_j, &turb_ke_i, &turb_ke_j, &turb_ke_n_i, &turb_ke_n_j);
  }

  AD::SetPreaccOut(Flux, nVar);
  AD::EndPreacc();

  return ResidualType<>(Flux, Jacobian_i, Jacobian_j);

}

CUpwRoe_Flow::CUpwRoe_Flow(unsigned short val_nDim, unsigned short val_nVar, const CConfig* config,
              bool val_low_dissipation, bool val_muscl) : CUpwRoeBase_Flow(val_nDim, val_nVar, config, val_low_dissipation, val_muscl) {}

void CUpwRoe_Flow::FinalizeResidual(su2double *val_residual, su2double **val_Jacobian_i,
                                    su2double **val_Jacobian_j, const CConfig* config) {

  unsigned short iVar, jVar, kVar;

  /*--- Compute inverse P tensor ---*/
  GetPMatrix_inv(&RoeDensity, RoeVelocity, &RoeTke, &RoeSoundSpeed, UnitNormal, invP_Tensor);

  if (tkeNeeded) {
    for (auto iVar = 0; iVar < nVar-1; iVar++) {
      P_Tensor[iVar][nVar]      = 0.0;
    }
    P_Tensor[nVar-1][nVar] = (Gamma - FIVE3)*RoeSqVel/(2.*RoeSoundSpeed2);

    invP_Tensor[nVar][0] = -RoeTke;
    for (auto iDim = 0; iDim < nDim; iDim++) {
      invP_Tensor[nVar][iDim+1] = (Gamma - FIVE3)*RoeVelocity[iDim]/(2.*RoeSoundSpeed2);
    }
    invP_Tensor[nVar][nVar-1] = 0.0;
  }

  /*--- Diference between conservative variables at jPoint and iPoint ---*/
  for (auto iVar = 0; iVar < nVar; iVar++)
    Diff_U[iVar] = Conservatives_j[iVar]-Conservatives_i[iVar];

  /*--- Low dissipation formulation ---*/
  if (roe_low_dissipation)
    Dissipation_ij = GetRoe_Dissipation(Dissipation_i, Dissipation_j, Sensor_i, Sensor_j, config);
  else
    Dissipation_ij = 1.0;

  /*--- Standard Roe "dissipation" ---*/

  for (auto iVar = 0; iVar < nVar; iVar++) {
    for (auto jVar = 0; jVar < nVar; jVar++) {
      /*--- Compute |Proj_ModJac_Tensor| = P x |Lambda| x inverse P ---*/
      su2double Proj_ModJac_Tensor_ij = 0.0;
      for (kVar = 0; kVar < nPrimVarTot; kVar++)
        Proj_ModJac_Tensor_ij += P_Tensor[iVar][kVar]*Lambda[kVar]*invP_Tensor[kVar][jVar];

      /*--- Update residual and Jacobians ---*/
      val_residual[iVar] -= (1.0-kappa)*Proj_ModJac_Tensor_ij*Diff_U[jVar]*Area*Dissipation_ij;

      if(implicit){
        val_Jacobian_i[iVar][jVar] += (1.0-kappa)*Proj_ModJac_Tensor_ij*Area*Dissipation_ij;
        val_Jacobian_j[iVar][jVar] -= (1.0-kappa)*Proj_ModJac_Tensor_ij*Area*Dissipation_ij;
      }
    }
  }

}

CUpwL2Roe_Flow::CUpwL2Roe_Flow(unsigned short val_nDim, unsigned short val_nVar, const CConfig* config, bool val_muscl) :
                CUpwRoeBase_Flow(val_nDim, val_nVar, config, false, val_muscl) {}

void CUpwL2Roe_Flow::FinalizeResidual(su2double *val_residual, su2double **val_Jacobian_i,
                                      su2double **val_Jacobian_j, const CConfig* config) {

  /*--- L2Roe: a low dissipation version of Roe's approximate Riemann solver for low Mach numbers. IJNMF 2015 ---*/

  unsigned short iVar, jVar, kVar, iDim;

  /*--- Clamped Mach number ---*/

  su2double M_i = 0.0, M_j = 0.0;
  for (auto iDim = 0; iDim < nDim; iDim++) {
    M_i += Velocity_i[iDim]*Velocity_i[iDim];
    M_j += Velocity_j[iDim]*Velocity_j[iDim];
  }
  M_i = sqrt(M_i / fabs(Pressure_i*Gamma/Density_i));
  M_j = sqrt(M_j / fabs(Pressure_j*Gamma/Density_j));

  su2double zeta = max(0.05,min(max(M_i,M_j),1.0));

  /*--- Compute wave amplitudes (characteristics) ---*/

  su2double proj_delta_vel = 0.0, delta_vel[3];
  for (auto iDim = 0; iDim < nDim; iDim++) {
    delta_vel[iDim] = Velocity_j[iDim] - Velocity_i[iDim];
    proj_delta_vel += delta_vel[iDim]*UnitNormal[iDim];
  }
  proj_delta_vel *= zeta;
  su2double delta_p = Pressure_j - Pressure_i;
  su2double delta_rho = Density_j - Density_i;

  su2double delta_wave[5] = {0.0, 0.0, 0.0, 0.0, 0.0};
  if (nDim == 2) {
    delta_wave[0] = delta_rho - delta_p/RoeSoundSpeed2;
    delta_wave[1] = (UnitNormal[1]*delta_vel[0]-UnitNormal[0]*delta_vel[1])*zeta;
    delta_wave[2] = proj_delta_vel + delta_p/(RoeDensity*RoeSoundSpeed);
    delta_wave[3] = -proj_delta_vel + delta_p/(RoeDensity*RoeSoundSpeed);
  } else {
    delta_wave[0] = delta_rho - delta_p/RoeSoundSpeed2;
    delta_wave[1] = (UnitNormal[0]*delta_vel[2]-UnitNormal[2]*delta_vel[0])*zeta;
    delta_wave[2] = (UnitNormal[1]*delta_vel[0]-UnitNormal[0]*delta_vel[1])*zeta;
    delta_wave[3] = proj_delta_vel + delta_p/(RoeDensity*RoeSoundSpeed);
    delta_wave[4] = -proj_delta_vel + delta_p/(RoeDensity*RoeSoundSpeed);
  }

  /*--- Update residual ---*/

  for (auto iVar = 0; iVar < nVar; iVar++)
    for (kVar = 0; kVar < nVar; kVar++)
      val_residual[iVar] -= (1.0-kappa)*Lambda[kVar]*delta_wave[kVar]*P_Tensor[iVar][kVar]*Area;

  if (!implicit) return;

  /*--- If implicit use the Jacobians of the standard Roe scheme as an approximation ---*/

  GetPMatrix_inv(&RoeDensity, RoeVelocity, &RoeTke, &RoeSoundSpeed, UnitNormal, invP_Tensor);

  for (auto iVar = 0; iVar < nVar; iVar++) {
    for (auto jVar = 0; jVar < nVar; jVar++) {
      /*--- Compute |Proj_ModJac_Tensor| = P x |Lambda| x inverse P ---*/
      su2double Proj_ModJac_Tensor_ij = 0.0;
      for (kVar = 0; kVar < nVar; kVar++)
        Proj_ModJac_Tensor_ij += P_Tensor[iVar][kVar]*Lambda[kVar]*invP_Tensor[kVar][jVar];

      val_Jacobian_i[iVar][jVar] += (1.0-kappa)*Proj_ModJac_Tensor_ij*Area;
      val_Jacobian_j[iVar][jVar] -= (1.0-kappa)*Proj_ModJac_Tensor_ij*Area;
    }
  }

}

CUpwLMRoe_Flow::CUpwLMRoe_Flow(unsigned short val_nDim, unsigned short val_nVar, const CConfig* config, bool val_muscl) :
                CUpwRoeBase_Flow(val_nDim, val_nVar, config, false, val_muscl) {}

void CUpwLMRoe_Flow::FinalizeResidual(su2double *val_residual, su2double **val_Jacobian_i,
                                      su2double **val_Jacobian_j, const CConfig* config) {

  /*--- Rieper, A low-Mach number fix for Roe's approximate Riemman Solver, JCP 2011 ---*/

  unsigned short iVar, jVar, kVar, iDim;

  /*--- Clamped Mach number ---*/

  su2double M_i = 0.0, M_j = 0.0;
  for (auto iDim = 0; iDim < nDim; iDim++) {
    M_i += Velocity_i[iDim]*Velocity_i[iDim];
    M_j += Velocity_j[iDim]*Velocity_j[iDim];
  }
  M_i = sqrt(M_i / fabs(Pressure_i*Gamma/Density_i));
  M_j = sqrt(M_j / fabs(Pressure_j*Gamma/Density_j));

  su2double zeta = max(0.05,min(max(M_i,M_j),1.0));

  /*--- Compute wave amplitudes (characteristics) ---*/

  su2double proj_delta_vel = 0.0, delta_vel[3];
  for (auto iDim = 0; iDim < nDim; iDim++) {
    delta_vel[iDim] = Velocity_j[iDim] - Velocity_i[iDim];
    proj_delta_vel += delta_vel[iDim]*UnitNormal[iDim];
  }
  proj_delta_vel *= zeta;
  su2double delta_p = Pressure_j - Pressure_i;
  su2double delta_rho = Density_j - Density_i;

  su2double delta_wave[5] = {0.0, 0.0, 0.0, 0.0, 0.0};
  if (nDim == 2) {
    delta_wave[0] = delta_rho - delta_p/RoeSoundSpeed2;
    delta_wave[1] = (UnitNormal[1]*delta_vel[0]-UnitNormal[0]*delta_vel[1]);
    delta_wave[2] = proj_delta_vel + delta_p/(RoeDensity*RoeSoundSpeed);
    delta_wave[3] = -proj_delta_vel + delta_p/(RoeDensity*RoeSoundSpeed);
  } else {
    delta_wave[0] = delta_rho - delta_p/RoeSoundSpeed2;
    delta_wave[1] = (UnitNormal[0]*delta_vel[2]-UnitNormal[2]*delta_vel[0]);
    delta_wave[2] = (UnitNormal[1]*delta_vel[0]-UnitNormal[0]*delta_vel[1]);
    delta_wave[3] = proj_delta_vel + delta_p/(RoeDensity*RoeSoundSpeed);
    delta_wave[4] = -proj_delta_vel + delta_p/(RoeDensity*RoeSoundSpeed);
  }

  /*--- Update residual ---*/

  for (auto iVar = 0; iVar < nVar; iVar++)
    for (kVar = 0; kVar < nVar; kVar++)
      val_residual[iVar] -= (1.0-kappa)*Lambda[kVar]*delta_wave[kVar]*P_Tensor[iVar][kVar]*Area;

  if (!implicit) return;

  /*--- If implicit use the Jacobians of the standard Roe scheme as an approximation ---*/

  GetPMatrix_inv(&RoeDensity, RoeVelocity, &RoeTke, &RoeSoundSpeed, UnitNormal, invP_Tensor);

  for (auto iVar = 0; iVar < nVar; iVar++) {
    for (auto jVar = 0; jVar < nVar; jVar++) {
      /*--- Compute |Proj_ModJac_Tensor| = P x |Lambda| x inverse P ---*/
      su2double Proj_ModJac_Tensor_ij = 0.0;
      for (kVar = 0; kVar < nVar; kVar++)
        Proj_ModJac_Tensor_ij += P_Tensor[iVar][kVar]*Lambda[kVar]*invP_Tensor[kVar][jVar];

      val_Jacobian_i[iVar][jVar] += (1.0-kappa)*Proj_ModJac_Tensor_ij*Area;
      val_Jacobian_j[iVar][jVar] -= (1.0-kappa)*Proj_ModJac_Tensor_ij*Area;
    }
  }

}

CUpwTurkel_Flow::CUpwTurkel_Flow(unsigned short val_nDim, unsigned short val_nVar, const CConfig* config) : CNumerics(val_nDim, val_nVar, config) {

  implicit = (config->GetKind_TimeIntScheme_Flow() == EULER_IMPLICIT);
  /* A grid is defined as dynamic if there's rigid grid movement or grid deformation AND the problem is time domain */
  dynamic_grid = config->GetDynamic_Grid();

  Gamma = config->GetGamma();
  Gamma_Minus_One = Gamma - 1.0;

  Beta_min = config->GetminTurkelBeta();
  Beta_max = config->GetmaxTurkelBeta();

  Flux = new su2double [nVar];
  Diff_U = new su2double [nVar];
  Velocity_i = new su2double [nDim];
  Velocity_j = new su2double [nDim];
  RoeVelocity = new su2double [nDim];
  ProjFlux_i = new su2double [nVar];
  ProjFlux_j = new su2double [nVar];
  Lambda = new su2double [nVar];
  Epsilon = new su2double [nVar];
  absPeJac = new su2double* [nVar];
  invRinvPe = new su2double* [nVar];
  R_Tensor  = new su2double* [nVar];
  Matrix    = new su2double* [nVar];
  Art_Visc  = new su2double* [nVar];
  Jacobian_i = new su2double* [nVar];
  Jacobian_j = new su2double* [nVar];
  for (auto iVar = 0; iVar < nVar; iVar++) {
    absPeJac[iVar] = new su2double [nVar];
    invRinvPe[iVar] = new su2double [nVar];
    Matrix[iVar] = new su2double [nVar];
    Art_Visc[iVar] = new su2double [nVar];
    R_Tensor[iVar] = new su2double [nVar];
    Jacobian_i[iVar] = new su2double [nVar];
    Jacobian_j[iVar] = new su2double [nVar];
  }
}

CUpwTurkel_Flow::~CUpwTurkel_Flow(void) {

  delete [] Flux;
  delete [] Diff_U;
  delete [] Velocity_i;
  delete [] Velocity_j;
  delete [] RoeVelocity;
  delete [] ProjFlux_i;
  delete [] ProjFlux_j;
  delete [] Lambda;
  delete [] Epsilon;
  for (auto iVar = 0; iVar < nVar; iVar++) {
    delete [] absPeJac[iVar];
    delete [] invRinvPe[iVar];
    delete [] Matrix[iVar];
    delete [] Art_Visc[iVar];
    delete [] R_Tensor[iVar];
    delete [] Jacobian_i[iVar];
    delete [] Jacobian_j[iVar];
  }
  delete [] Matrix;
  delete [] Art_Visc;
  delete [] absPeJac;
  delete [] invRinvPe;
  delete [] R_Tensor;
  delete [] Jacobian_i;
  delete [] Jacobian_j;

}

CNumerics::ResidualType<> CUpwTurkel_Flow::ComputeResidual(const CConfig* config) {

  implicit = (config->GetKind_TimeIntScheme() == EULER_IMPLICIT);

  su2double U_i[5] = {0.0}, U_j[5] = {0.0};

  /*--- Face area (norm or the normal vector) ---*/

  Area = 0.0;
  for (auto iDim = 0; iDim < nDim; iDim++)
    Area += Normal[iDim]*Normal[iDim];
  Area = sqrt(Area);

  /*-- Unit Normal ---*/

  for (auto iDim = 0; iDim < nDim; iDim++)
    UnitNormal[iDim] = Normal[iDim]/Area;

  /*--- Primitive variables at point i ---*/

  for (auto iDim = 0; iDim < nDim; iDim++)
    Velocity_i[iDim] = V_i[iDim+1];
  Pressure_i = V_i[nDim+1];
  Density_i = V_i[nDim+2];
  Enthalpy_i = V_i[nDim+3];
  Energy_i = Enthalpy_i - Pressure_i/Density_i;
  SoundSpeed_i = sqrt(fabs(Pressure_i*Gamma/Density_i));

  /*--- Primitive variables at point j ---*/

  for (auto iDim = 0; iDim < nDim; iDim++)
    Velocity_j[iDim] = V_j[iDim+1];
  Pressure_j = V_j[nDim+1];
  Density_j = V_j[nDim+2];
  Enthalpy_j = V_j[nDim+3];
  Energy_j = Enthalpy_j - Pressure_j/Density_j;
  SoundSpeed_j = sqrt(fabs(Pressure_j*Gamma/Density_j));

  /*--- Recompute conservative variables ---*/

  U_i[0] = Density_i; U_j[0] = Density_j;
  for (auto iDim = 0; iDim < nDim; iDim++) {
    U_i[iDim+1] = Density_i*Velocity_i[iDim]; U_j[iDim+1] = Density_j*Velocity_j[iDim];
  }
  U_i[nDim+1] = Density_i*Energy_i; U_j[nDim+1] = Density_j*Energy_j;

  /*--- Roe-averaged variables at interface between i & j ---*/

  R = sqrt(fabs(Density_j/Density_i));
  RoeDensity = R*Density_i;
  sq_vel = 0.0;
  for (auto iDim = 0; iDim < nDim; iDim++) {
    RoeVelocity[iDim] = (R*Velocity_j[iDim]+Velocity_i[iDim])/(R+1.);
    sq_vel += RoeVelocity[iDim]*RoeVelocity[iDim];
  }
  RoeEnthalpy = (R*Enthalpy_j+Enthalpy_i)/(R+1.);
  RoeSoundSpeed = sqrt(fabs(Gamma_Minus_One*(RoeEnthalpy-0.5*sq_vel)));
  RoePressure = RoeDensity/Gamma*RoeSoundSpeed*RoeSoundSpeed;

  /*--- Compute ProjFlux_i ---*/
  GetInviscidProjFlux(&Density_i, Velocity_i, &Pressure_i, &Enthalpy_i, Normal, ProjFlux_i);

  /*--- Compute ProjFlux_j ---*/
  GetInviscidProjFlux(&Density_j, Velocity_j, &Pressure_j, &Enthalpy_j, Normal, ProjFlux_j);

  ProjVelocity = 0.0; ProjVelocity_i = 0.0; ProjVelocity_j = 0.0;
  for (auto iDim = 0; iDim < nDim; iDim++) {
    ProjVelocity   += RoeVelocity[iDim]*UnitNormal[iDim];
    ProjVelocity_i += Velocity_i[iDim]*UnitNormal[iDim];
    ProjVelocity_j += Velocity_j[iDim]*UnitNormal[iDim];
  }

  /*--- Projected velocity adjustment due to mesh motion ---*/
  if (dynamic_grid) {
    su2double ProjGridVel = 0.0;
    for (auto iDim = 0; iDim < nDim; iDim++) {
      ProjGridVel   += 0.5*(GridVel_i[iDim]+GridVel_j[iDim])*UnitNormal[iDim];
    }
    ProjVelocity   -= ProjGridVel;
    ProjVelocity_i -= ProjGridVel;
    ProjVelocity_j -= ProjGridVel;
  }

  /*--- First few flow eigenvalues of A.Normal with the normal---*/
  for (auto iDim = 0; iDim < nDim; iDim++)
    Lambda[iDim] = ProjVelocity;

  local_Mach = sqrt(sq_vel)/RoeSoundSpeed;
  Beta       = max(Beta_min, min(local_Mach, Beta_max));
  Beta2      = Beta*Beta;

  one_m_Betasqr        = 1.0 - Beta2;  // 1-Beta*Beta
  one_p_Betasqr        = 1.0 + Beta2;  // 1+Beta*Beta
  sqr_one_m_Betasqr_Lam1 = pow((one_m_Betasqr*Lambda[0]),2); // [(1-Beta^2)*Lambda[0]]^2
  sqr_two_Beta_c_Area    = pow(2.0*Beta*RoeSoundSpeed*Area,2); // [2*Beta*c*Area]^2

  /*--- The rest of the flow eigenvalues of preconditioned matrix---*/
  Lambda[nVar-2] = 0.5 * ( one_p_Betasqr*Lambda[0] + sqrt( sqr_one_m_Betasqr_Lam1 + sqr_two_Beta_c_Area));
  Lambda[nVar-1] = 0.5 * ( one_p_Betasqr*Lambda[0] - sqrt( sqr_one_m_Betasqr_Lam1 + sqr_two_Beta_c_Area));

  s_hat = 1.0/Area * (Lambda[nVar-1] - Lambda[0]*Beta2);
  r_hat = 1.0/Area * (Lambda[nVar-2] - Lambda[0]*Beta2);
  t_hat = 0.5/Area * (Lambda[nVar-1] - Lambda[nVar-2]);
  rhoB2a2 = RoeDensity*Beta2*RoeSoundSpeed*RoeSoundSpeed;

  /*--- Diference variables iPoint and jPoint and absolute value of the eigen values---*/
  for (auto iVar = 0; iVar < nVar; iVar++) {
    Diff_U[iVar] = U_j[iVar]-U_i[iVar];
    Lambda[iVar] = fabs(Lambda[iVar]);
  }

  /*--- Compute the absolute Preconditioned Jacobian in entropic Variables (do it with the Unitary Normal) ---*/
  GetPrecondJacobian(Beta2, r_hat, s_hat, t_hat, rhoB2a2, Lambda, UnitNormal, absPeJac);

  /*--- Compute the matrix from entropic variables to conserved variables ---*/
  GetinvRinvPe(Beta2, RoeEnthalpy, RoeSoundSpeed, RoeDensity, RoeVelocity, invRinvPe);

  /*--- Compute the matrix from entropic variables to conserved variables ---*/
  GetRMatrix(RoePressure, RoeSoundSpeed, RoeDensity, RoeVelocity, R_Tensor);

  if (implicit) {
    /*--- Jacobians of the inviscid flux, scaled by
     0.5 because Flux ~ 0.5*(fc_i+fc_j)*Normal ---*/
    GetInviscidProjJac(Velocity_i, &Energy_i, &turb_ke_i, Normal, 0.5, Jacobian_i);
    GetInviscidProjJac(Velocity_j, &Energy_j, &turb_ke_j, Normal, 0.5, Jacobian_j);
  }

  for (auto iVar = 0; iVar < nVar; iVar ++) {
    for (auto jVar = 0; jVar < nVar; jVar ++) {
      Matrix[iVar][jVar] = 0.0;
      for (kVar = 0; kVar < nVar; kVar++)
        Matrix[iVar][jVar]  += absPeJac[iVar][kVar]*R_Tensor[kVar][jVar];
    }
  }

  for (auto iVar = 0; iVar < nVar; iVar ++) {
    for (auto jVar = 0; jVar < nVar; jVar ++) {
      Art_Visc[iVar][jVar] = 0.0;
      for (kVar = 0; kVar < nVar; kVar++)
        Art_Visc[iVar][jVar]  += invRinvPe[iVar][kVar]*Matrix[kVar][jVar];
    }
  }

  /*--- Roe's Flux approximation ---*/
  for (auto iVar = 0; iVar < nVar; iVar++) {
    Flux[iVar] = 0.5*(ProjFlux_i[iVar]+ProjFlux_j[iVar]);
    for (auto jVar = 0; jVar < nVar; jVar++) {
      Flux[iVar] -= 0.5*Art_Visc[iVar][jVar]*Diff_U[jVar];
      if (implicit) {
        Jacobian_i[iVar][jVar] += 0.5*Art_Visc[iVar][jVar];
        Jacobian_j[iVar][jVar] -= 0.5*Art_Visc[iVar][jVar];
      }
    }
  }

  /*--- Contributions due to mesh motion---*/
  if (dynamic_grid) {
    ProjVelocity = 0.0;
    for (auto iDim = 0; iDim < nDim; iDim++)
      ProjVelocity += 0.5*(GridVel_i[iDim]+GridVel_j[iDim])*UnitNormal[iDim];
    for (auto iVar = 0; iVar < nVar; iVar++) {
      Flux[iVar] -= ProjVelocity * 0.5*(U_i[iVar]+U_j[iVar]);
      /*--- Implicit terms ---*/
      if (implicit) {
        Jacobian_i[iVar][iVar] -= 0.5*ProjVelocity;
        Jacobian_j[iVar][iVar] -= 0.5*ProjVelocity;
      }
    }
  }

  return ResidualType<>(Flux, Jacobian_i, Jacobian_j);

}

CUpwGeneralRoe_Flow::CUpwGeneralRoe_Flow(unsigned short val_nDim, unsigned short val_nVar, const CConfig* config) : CNumerics(val_nDim, val_nVar, config) {

  implicit = (config->GetKind_TimeIntScheme_Flow() == EULER_IMPLICIT);
  /* A grid is defined as dynamic if there's rigid grid movement or grid deformation AND the problem is time domain */
  dynamic_grid = config->GetDynamic_Grid();
  kappa = config->GetRoe_Kappa(); // 1 is unstable


  Flux = new su2double [nVar];
  Diff_U = new su2double [nVar];
  Velocity_i = new su2double [nDim];
  Velocity_j = new su2double [nDim];
  RoeVelocity = new su2double [nDim];
  delta_vel  = new su2double [nDim];
  delta_wave = new su2double [nVar];
  ProjFlux_i = new su2double [nVar];
  ProjFlux_j = new su2double [nVar];
  Lambda = new su2double [nVar];
  Epsilon = new su2double [nVar];
  P_Tensor = new su2double* [nVar];
  invP_Tensor = new su2double* [nVar];
  Jacobian_i = new su2double* [nVar];
  Jacobian_j = new su2double* [nVar];
  for (auto iVar = 0; iVar < nVar; iVar++) {
    P_Tensor[iVar] = new su2double [nVar];
    invP_Tensor[iVar] = new su2double [nVar];
    Jacobian_i[iVar] = new su2double [nVar];
    Jacobian_j[iVar] = new su2double [nVar];
  }
}

CUpwGeneralRoe_Flow::~CUpwGeneralRoe_Flow(void) {

  delete [] Flux;
  delete [] Diff_U;
  delete [] Velocity_i;
  delete [] Velocity_j;
  delete [] RoeVelocity;
  delete [] delta_vel;
  delete [] delta_wave;
  delete [] ProjFlux_i;
  delete [] ProjFlux_j;
  delete [] Lambda;
  delete [] Epsilon;
  for (auto iVar = 0; iVar < nVar; iVar++) {
    delete [] P_Tensor[iVar];
    delete [] invP_Tensor[iVar];
    delete [] Jacobian_i[iVar];
    delete [] Jacobian_j[iVar];
  }
  delete [] P_Tensor;
  delete [] invP_Tensor;
  delete [] Jacobian_i;
  delete [] Jacobian_j;

}

CNumerics::ResidualType<> CUpwGeneralRoe_Flow::ComputeResidual(const CConfig* config) {

  AD::StartPreacc();
  AD::SetPreaccIn(V_i, nDim+4); AD::SetPreaccIn(V_j, nDim+4); AD::SetPreaccIn(Normal, nDim);
  AD::SetPreaccIn(S_i, 2); AD::SetPreaccIn(S_j, 2);
  if (dynamic_grid) {
    AD::SetPreaccIn(GridVel_i, nDim); AD::SetPreaccIn(GridVel_j, nDim);
  }
  su2double U_i[5] = {0.0,0.0,0.0,0.0,0.0}, U_j[5] = {0.0,0.0,0.0,0.0,0.0};

  /*--- Face area (norm or the normal vector) ---*/

  Area = 0.0;
  for (auto iDim = 0; iDim < nDim; iDim++)
  Area += Normal[iDim]*Normal[iDim];
  Area = sqrt(Area);

  /*-- Unit Normal ---*/

  for (auto iDim = 0; iDim < nDim; iDim++)
    UnitNormal[iDim] = Normal[iDim]/Area;

  /*--- Primitive variables at point i ---*/

  Velocity2_i = 0.0;
  for (auto iDim = 0; iDim < nDim; iDim++) {
    Velocity_i[iDim] = V_i[iDim+1];
    Velocity2_i += Velocity_i[iDim]*Velocity_i[iDim];
  }

  Pressure_i = V_i[nDim+1];
  Density_i = V_i[nDim+2];
  Enthalpy_i = V_i[nDim+3];
  Energy_i = Enthalpy_i - Pressure_i/Density_i;
  StaticEnthalpy_i = Enthalpy_i - 0.5*Velocity2_i;
  StaticEnergy_i = StaticEnthalpy_i - Pressure_i/Density_i;

  Kappa_i = S_i[1]/Density_i;
  Chi_i = S_i[0] - Kappa_i*StaticEnergy_i;
  SoundSpeed_i = sqrt(Chi_i + StaticEnthalpy_i*Kappa_i);

  /*--- Primitive variables at point j ---*/

  Velocity2_j = 0.0;
  for (auto iDim = 0; iDim < nDim; iDim++) {
    Velocity_j[iDim] = V_j[iDim+1];
    Velocity2_j += Velocity_j[iDim]*Velocity_j[iDim];
  }

  Pressure_j = V_j[nDim+1];
  Density_j = V_j[nDim+2];
  Enthalpy_j = V_j[nDim+3];
  Energy_j = Enthalpy_j - Pressure_j/Density_j;

  StaticEnthalpy_j = Enthalpy_j - 0.5*Velocity2_j;
  StaticEnergy_j = StaticEnthalpy_j - Pressure_j/Density_j;

  Kappa_j = S_j[1]/Density_j;
  Chi_j = S_j[0] - Kappa_j*StaticEnergy_j;
  SoundSpeed_j = sqrt(Chi_j + StaticEnthalpy_j*Kappa_j);

  /*--- Recompute conservative variables ---*/

  U_i[0] = Density_i; U_j[0] = Density_j;
  for (auto iDim = 0; iDim < nDim; iDim++) {
    U_i[iDim+1] = Density_i*Velocity_i[iDim]; U_j[iDim+1] = Density_j*Velocity_j[iDim];
  }
  U_i[nDim+1] = Density_i*Energy_i; U_j[nDim+1] = Density_j*Energy_j;

  /*--- Roe-averaged variables at interface between i & j ---*/

  ComputeRoeAverage();

  if (RoeSoundSpeed2 <= 0.0) {
    for (auto iVar = 0; iVar < nVar; iVar++) {
      Flux[iVar] = 0.0;
      for (auto jVar = 0; jVar < nVar; jVar++) {
      Jacobian_i[iVar][iVar] = 0.0;
      Jacobian_j[iVar][iVar] = 0.0;
      }
    }
    AD::SetPreaccOut(Flux, nVar);
    AD::EndPreacc();

    return ResidualType<>(Flux, Jacobian_i, Jacobian_j);
  }

  RoeSoundSpeed = sqrt(RoeSoundSpeed2);

  /*--- Compute ProjFlux_i ---*/
  GetInviscidProjFlux(&Density_i, Velocity_i, &Pressure_i, &Enthalpy_i, Normal, ProjFlux_i);

  /*--- Compute ProjFlux_j ---*/
  GetInviscidProjFlux(&Density_j, Velocity_j, &Pressure_j, &Enthalpy_j, Normal, ProjFlux_j);

  /*--- Compute P and Lambda (do it with the Normal) ---*/

  GetPMatrix(&RoeDensity, RoeVelocity, &RoeSoundSpeed, &RoeEnthalpy, &RoeChi, &RoeKappa, UnitNormal, P_Tensor);

  ProjVelocity = 0.0; ProjVelocity_i = 0.0; ProjVelocity_j = 0.0;
  for (auto iDim = 0; iDim < nDim; iDim++) {
    ProjVelocity   += RoeVelocity[iDim]*UnitNormal[iDim];
    ProjVelocity_i += Velocity_i[iDim]*UnitNormal[iDim];
    ProjVelocity_j += Velocity_j[iDim]*UnitNormal[iDim];
  }

  /*--- Projected velocity adjustment due to mesh motion ---*/
  if (dynamic_grid) {
    su2double ProjGridVel = 0.0;
    for (auto iDim = 0; iDim < nDim; iDim++) {
      ProjGridVel   += 0.5*(GridVel_i[iDim]+GridVel_j[iDim])*UnitNormal[iDim];
    }
    ProjVelocity   -= ProjGridVel;
    ProjVelocity_i -= ProjGridVel;
    ProjVelocity_j -= ProjGridVel;
  }

  /*--- Flow eigenvalues and entropy correctors ---*/
  for (auto iDim = 0; iDim < nDim; iDim++)
    Lambda[iDim] = ProjVelocity;

  Lambda[nVar-2] = ProjVelocity + RoeSoundSpeed;
  Lambda[nVar-1] = ProjVelocity - RoeSoundSpeed;

  /*--- Compute absolute value with Mavriplis' entropy correction ---*/

  MaxLambda = fabs(ProjVelocity) + RoeSoundSpeed;
  Delta = config->GetEntropyFix_Coeff();

  for (auto iVar = 0; iVar < nVar; iVar++) {
    Lambda[iVar] = max(fabs(Lambda[iVar]), Delta*MaxLambda);
   }

//  /*--- Harten and Hyman (1983) entropy correction ---*/
//  for (auto iDim = 0; iDim < nDim; iDim++)
//    Epsilon[iDim] = 4.0*max(0.0, max(Lambda[iDim]-ProjVelocity_i, ProjVelocity_j-Lambda[iDim]));
//
//  Epsilon[nVar-2] = 4.0*max(0.0, max(Lambda[nVar-2]-(ProjVelocity_i+SoundSpeed_i),(ProjVelocity_j+SoundSpeed_j)-Lambda[nVar-2]));
//  Epsilon[nVar-1] = 4.0*max(0.0, max(Lambda[nVar-1]-(ProjVelocity_i-SoundSpeed_i),(ProjVelocity_j-SoundSpeed_j)-Lambda[nVar-1]));
//
//  for (auto iVar = 0; iVar < nVar; iVar++)
//    if ( fabs(Lambda[iVar]) < Epsilon[iVar] )
//      Lambda[iVar] = (Lambda[iVar]*Lambda[iVar] + Epsilon[iVar]*Epsilon[iVar])/(2.0*Epsilon[iVar]);
//    else
//      Lambda[iVar] = fabs(Lambda[iVar]);

//  for (auto iVar = 0; iVar < nVar; iVar++)
//    Lambda[iVar] = fabs(Lambda[iVar]);

  if (!implicit) {

    /*--- Compute wave amplitudes (characteristics) ---*/
    proj_delta_vel = 0.0;
    for (auto iDim = 0; iDim < nDim; iDim++) {
      delta_vel[iDim] = Velocity_j[iDim] - Velocity_i[iDim];
      proj_delta_vel += delta_vel[iDim]*Normal[iDim];
    }
    delta_p = Pressure_j - Pressure_i;
    delta_rho = Density_j - Density_i;
    proj_delta_vel = proj_delta_vel/Area;

    if (nDim == 2) {
      delta_wave[0] = delta_rho - delta_p/(RoeSoundSpeed*RoeSoundSpeed);
      delta_wave[1] = UnitNormal[1]*delta_vel[0]-UnitNormal[0]*delta_vel[1];
      delta_wave[2] = proj_delta_vel + delta_p/(RoeDensity*RoeSoundSpeed);
      delta_wave[3] = -proj_delta_vel + delta_p/(RoeDensity*RoeSoundSpeed);
    } else {
      delta_wave[0] = delta_rho - delta_p/(RoeSoundSpeed*RoeSoundSpeed);
      delta_wave[1] = UnitNormal[0]*delta_vel[2]-UnitNormal[2]*delta_vel[0];
      delta_wave[2] = UnitNormal[1]*delta_vel[0]-UnitNormal[0]*delta_vel[1];
      delta_wave[3] = proj_delta_vel + delta_p/(RoeDensity*RoeSoundSpeed);
      delta_wave[4] = -proj_delta_vel + delta_p/(RoeDensity*RoeSoundSpeed);
    }

    /*--- Roe's Flux approximation ---*/
    for (auto iVar = 0; iVar < nVar; iVar++) {
      Flux[iVar] = 0.5*(ProjFlux_i[iVar]+ProjFlux_j[iVar]);
      for (auto jVar = 0; jVar < nVar; jVar++)
        Flux[iVar] -= 0.5*Lambda[jVar]*delta_wave[jVar]*P_Tensor[iVar][jVar]*Area;
    }

    /*--- Flux contribution due to grid motion ---*/
    if (dynamic_grid) {
      ProjVelocity = 0.0;
      for (auto iDim = 0; iDim < nDim; iDim++)
        ProjVelocity += 0.5*(GridVel_i[iDim]+GridVel_j[iDim])*Normal[iDim];
      for (auto iVar = 0; iVar < nVar; iVar++) {
        Flux[iVar] -= ProjVelocity * 0.5*(U_i[iVar]+U_j[iVar]);
      }
    }
  }
  else {

    /*--- Compute inverse P ---*/

    GetPMatrix_inv(invP_Tensor, &RoeDensity, RoeVelocity, &RoeSoundSpeed, &RoeChi , &RoeKappa, UnitNormal);

     /*--- Jacobians of the inviscid flux, scaled by
      kappa because val_resconv ~ kappa*(fc_i+fc_j)*Normal ---*/

    GetInviscidProjJac(Velocity_i, &Enthalpy_i, &Chi_i, &Kappa_i, Normal, kappa, Jacobian_i);

    GetInviscidProjJac(Velocity_j, &Enthalpy_j, &Chi_j, &Kappa_j, Normal, kappa, Jacobian_j);


    /*--- Diference variables iPoint and jPoint ---*/
    for (auto iVar = 0; iVar < nVar; iVar++)
      Diff_U[iVar] = U_j[iVar]-U_i[iVar];

    /*--- Roe's Flux approximation ---*/
    for (auto iVar = 0; iVar < nVar; iVar++) {
      Flux[iVar] = kappa*(ProjFlux_i[iVar]+ProjFlux_j[iVar]);
      for (auto jVar = 0; jVar < nVar; jVar++) {
        Proj_ModJac_Tensor_ij = 0.0;

        /*--- Compute |Proj_ModJac_Tensor| = P x |Lambda| x inverse P ---*/

        for (kVar = 0; kVar < nVar; kVar++)
          Proj_ModJac_Tensor_ij += P_Tensor[iVar][kVar]*Lambda[kVar]*invP_Tensor[kVar][jVar];

        Flux[iVar] -= (1.0-kappa)*Proj_ModJac_Tensor_ij*Diff_U[jVar]*Area;
        Jacobian_i[iVar][jVar] += (1.0-kappa)*Proj_ModJac_Tensor_ij*Area;
        Jacobian_j[iVar][jVar] -= (1.0-kappa)*Proj_ModJac_Tensor_ij*Area;
      }
    }

    /*--- Jacobian contributions due to grid motion ---*/
    if (dynamic_grid) {
      ProjVelocity = 0.0;
      for (auto iDim = 0; iDim < nDim; iDim++)
        ProjVelocity += 0.5*(GridVel_i[iDim]+GridVel_j[iDim])*Normal[iDim];
      for (auto iVar = 0; iVar < nVar; iVar++) {
        Flux[iVar] -= ProjVelocity * 0.5*(U_i[iVar]+U_j[iVar]);
        /*--- Implicit terms ---*/
        Jacobian_i[iVar][iVar] -= 0.5*ProjVelocity;
        Jacobian_j[iVar][iVar] -= 0.5*ProjVelocity;
      }
    }

  }

  AD::SetPreaccOut(Flux, nVar);
  AD::EndPreacc();

  return ResidualType<>(Flux, Jacobian_i, Jacobian_j);

}

void CUpwGeneralRoe_Flow::ComputeRoeAverage() {

  //su2double delta_rhoStaticEnergy, err_P, s, D;
  // su2double tol = 10-6;

  R = sqrt(fabs(Density_j/Density_i));
  RoeDensity = R*Density_i;
  sq_vel = 0;  for (auto iDim = 0; iDim < nDim; iDim++) {
    RoeVelocity[iDim] = (R*Velocity_j[iDim]+Velocity_i[iDim])/(R+1.);
    sq_vel += RoeVelocity[iDim]*RoeVelocity[iDim];
  }

  RoeEnthalpy = (R*Enthalpy_j+Enthalpy_i)/(R+1.);
  delta_rho = Density_j - Density_i;
  delta_p = Pressure_j - Pressure_i;
  RoeKappa = 0.5*(Kappa_i + Kappa_j);
  RoeKappa = (Kappa_i + Kappa_j + 4*RoeKappa)/6;
  RoeChi = 0.5*(Chi_i + Chi_j);
  RoeChi = (Chi_i + Chi_j + 4*RoeChi)/6;

//  RoeKappaStaticEnthalpy = 0.5*(StaticEnthalpy_i*Kappa_i + StaticEnthalpy_j*Kappa_j);
//  RoeKappaStaticEnthalpy = (StaticEnthalpy_i*Kappa_i + StaticEnthalpy_j*Kappa_j + 4*RoeKappaStaticEnthalpy)/6;
//  s = RoeChi + RoeKappaStaticEnthalpy;
//  D = s*s*delta_rho*delta_rho + delta_p*delta_p;
//  delta_rhoStaticEnergy = Density_j*StaticEnergy_j - Density_i*StaticEnergy_i;
//  err_P = delta_p - RoeChi*delta_rho - RoeKappa*delta_rhoStaticEnergy;
//
//
//  if (abs((D - delta_p*err_P)/Density_i)>1e-3 && abs(delta_rho/Density_i)>1e-3 && s/Density_i > 1e-3) {
//
//    RoeKappa = (D*RoeKappa)/(D - delta_p*err_P);
//    RoeChi = (D*RoeChi+ s*s*delta_rho*err_P)/(D - delta_p*err_P);
//
//  }

  RoeSoundSpeed2 = RoeChi + RoeKappa*(RoeEnthalpy-0.5*sq_vel);

}
