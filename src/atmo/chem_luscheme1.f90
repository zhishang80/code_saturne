!-------------------------------------------------------------------------------

! This file is part of Code_Saturne, a general-purpose CFD tool.
!
! Copyright (C) 1998-2020 EDF S.A.
!
! This program is free software; you can redistribute it and/or modify it under
! the terms of the GNU General Public License as published by the Free Software
! Foundation; either version 2 of the License, or (at your option) any later
! version.
!
! This program is distributed in the hope that it will be useful, but WITHOUT
! ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
! FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
! details.
!
! You should have received a copy of the GNU General Public License along with
! this program; if not, write to the Free Software Foundation, Inc., 51 Franklin
! Street, Fifth Floor, Boston, MA 02110-1301, USA.

!-------------------------------------------------------------------------------

!> \file chem_luscheme1.f90
!> \brief Routines for atmospheric chemical scheme 1
!> \remarks
!>  These routines are automatically generated by SPACK
!>  See CEREA: http://cerea.enpc.fr/polyphemus

!> kinetic_1
!> \brief Computation of kinetic rates for atmospheric chemistry
!------------------------------------------------------------------------------

!------------------------------------------------------------------------------
! Arguments
!------------------------------------------------------------------------------
!   mode          name              role
!------------------------------------------------------------------------------
!> \param[in]     nr                total number of chemical reactions
!> \param[in]     option_photolysis flag to activate or not photolysis reactions
!> \param[in]     azi               solar zenith angle
!> \param[in]     att               atmospheric attenuation variable
!> \param[in]     temp              temperature
!> \param[in]     press             pressure
!> \param[in]     xlw               water massic fraction
!> \param[out]    rk(nr)            kinetic rates
!______________________________________________________________________________

subroutine kinetic_1(nr,rk,temp,xlw,press,azi,att,                  &
     option_photolysis)

implicit none

! Arguments

integer nr
double precision rk(nr),temp,xlw,press
double precision azi, att
integer option_photolysis

! Local variables

double precision summ
double precision ylh2o


!     Compute third body.
!     Conversion = Avogadro*1d-6/Perfect gas constant.
!     PRESS in Pascal, SUMM in molecules/cm3, TEMP in Kelvin

summ = press * 7.243d16 / temp

!     Number of water molecules computed from the massic fraction
!     (absolute humidity)

ylh2o = 29.d0*summ*xlw/(18.d0+11.d0*xlw)

!     For the zenithal angle at tropics

azi=abs(azi)

rk(  1) =  dexp(-0.8860689615829534d+02                           &
 - ( -0.5300000000000000d+03 )/temp)
rk(  1) = rk(  1) * summ * 0.2d0
rk(  2) =  dexp(-0.2653240882726044d+02                           &
 - (  0.1500000000000000d+04 )/temp)

if(option_photolysis.eq.2) then
 rk(  3)= 0.d0
elseif(option_photolysis.eq.1) then
if(azi.lt.0.d0)then
 stop
elseif(azi.ge. 0.00d+00 .and. azi.lt. 0.10d+02) then
 rk(  3)=-0.1302720567168795d-07
 rk(  3)=-0.7822279432831311d-06+(azi- 0.00d+00) * rk(  3)
 rk(  3)= 0.0000000000000000d+00+(azi- 0.00d+00) * rk(  3)
 rk(  3)= 0.9310260000000001d-02+(azi- 0.00d+00) * rk(  3)
elseif(azi.ge. 0.10d+02 .and. azi.lt. 0.20d+02) then
 rk(  3)= 0.3771617015067078d-08
 rk(  3)=-0.1173044113433769d-05+(azi- 0.10d+02) * rk(  3)
 rk(  3)=-0.1955272056716901d-04+(azi- 0.10d+02) * rk(  3)
 rk(  3)= 0.9219010000000000d-02+(azi- 0.10d+02) * rk(  3)
elseif(azi.ge. 0.20d+02 .and. azi.lt. 0.30d+02) then
 rk(  3)=-0.5859262388581815d-08
 rk(  3)=-0.1059895602981758d-05+(azi- 0.20d+02) * rk(  3)
 rk(  3)=-0.4188211773132428d-04+(azi- 0.20d+02) * rk(  3)
 rk(  3)= 0.8909950000000000d-02+(azi- 0.20d+02) * rk(  3)
elseif(azi.ge. 0.30d+02 .and. azi.lt. 0.40d+02) then
 rk(  3)=-0.7024567460738029d-08
 rk(  3)=-0.1235673474639213d-05+(azi- 0.30d+02) * rk(  3)
 rk(  3)=-0.6483780850753392d-04+(azi- 0.30d+02) * rk(  3)
 rk(  3)= 0.8379279999999999d-02+(azi- 0.30d+02) * rk(  3)
elseif(azi.ge. 0.40d+02 .and. azi.lt. 0.50d+02) then
 rk(  3)=-0.9202467768466835d-08
 rk(  3)=-0.1446410498461367d-05+(azi- 0.40d+02) * rk(  3)
 rk(  3)=-0.9165864823853972d-04+(azi- 0.40d+02) * rk(  3)
 rk(  3)= 0.7600310000000000d-02+(azi- 0.40d+02) * rk(  3)
elseif(azi.ge. 0.50d+02 .and. azi.lt. 0.60d+02) then
 rk(  3)=-0.1612556146540100d-07
 rk(  3)=-0.1722484531515342d-05+(azi- 0.50d+02) * rk(  3)
 rk(  3)=-0.1233475985383066d-03+(azi- 0.50d+02) * rk(  3)
 rk(  3)= 0.6529880000000000d-02+(azi- 0.50d+02) * rk(  3)
elseif(azi.ge. 0.60d+02 .and. azi.lt. 0.70d+02) then
 rk(  3)= 0.3226471363007382d-07
 rk(  3)=-0.2206251375477548d-05+(azi- 0.60d+02) * rk(  3)
 rk(  3)=-0.1626349576082332d-03+(azi- 0.60d+02) * rk(  3)
 rk(  3)= 0.5108030000000000d-02+(azi- 0.60d+02) * rk(  3)
elseif(azi.ge. 0.70d+02 .and. azi.lt. 0.78d+02) then
 rk(  3)= 0.2027078243961372d-06
 rk(  3)=-0.1238309966574737d-05+(azi- 0.70d+02) * rk(  3)
 rk(  3)=-0.1970805710287543d-03+(azi- 0.70d+02) * rk(  3)
 rk(  3)= 0.3293320000000000d-02+(azi- 0.70d+02) * rk(  3)
elseif(azi.ge. 0.78d+02 .and. azi.lt. 0.86d+02) then
 rk(  3)=-0.7448311471194499d-07
 rk(  3)= 0.3626677818932555d-05+(azi- 0.78d+02) * rk(  3)
 rk(  3)=-0.1779736282099126d-03+(azi- 0.78d+02) * rk(  3)
 rk(  3)= 0.1741210000000000d-02+(azi- 0.78d+02) * rk(  3)
elseif(azi.ge. 0.86d+02 .and. azi.lt. 0.90d+02) then
 rk(  3)= 0.2490309929270573d-05
 rk(  3)= 0.1839083065842406d-05+(azi- 0.86d+02) * rk(  3)
 rk(  3)=-0.1342475411316713d-03+(azi- 0.86d+02) * rk(  3)
 rk(  3)= 0.5113930000000000d-03+(azi- 0.86d+02) * rk(  3)
elseif(azi.ge.90.d0)then
 rk(  3)= 0.1632080000000000d-03
endif
if(att.lt.0.99999) rk(  3) = rk(  3) * att
endif

rk(  4) = summ * 6.0d-34 * (temp/3.d2) ** (-2.4d0)
rk(  4) = rk(  4) * summ * 0.2d0
rk(  5) =  dexp(-0.2590825451818744d+02                           &
 - ( -0.1800000000000000d+03 )/temp)

return
end subroutine kinetic_1


!===============================================================================

!> fexchem_1
!> \brief Computation of the chemical production terms
!------------------------------------------------------------------------------

!------------------------------------------------------------------------------
! Arguments
!------------------------------------------------------------------------------
!   mode          name              role
!------------------------------------------------------------------------------
!> \param[in]     ns                total number of chemical species
!> \param[in]     nr                total number of chemical reactions
!> \param[in]     y                 concentrations vector
!> \param[in]     rk                kinetic rates
!> \param[in]     zcsourc           source term
!> \param[in]     convers_factor    conversion factors
!> \param[out]    chem              chemical production terms for every species
!______________________________________________________________________________

subroutine fexchem_1(ns,nr,y,rk,zcsourc,convers_factor,chem)

implicit none

! Arguments

integer nr,ns
double precision rk(nr),y(ns),chem(ns),zcsourc(ns)
double precision convers_factor(ns)

! Local variables

integer i
double precision w(nr)
double precision conc(ns)

do i=1,ns
 chem(i)=0.d0
enddo

!     Conversion mug/m3 to molecules/cm3.

do i = 1, ns
   conc(i) = y(i) * convers_factor(i)
enddo

!     Compute reaction rates.

call rates_1(ns,nr,rk,conc,w)

!     Chemical production terms.

chem(  3) = chem(  3) +  0.2000000000000000d+01 * w(  1)
chem(  4) = chem(  4) -  0.2000000000000000d+01 * w(  1)
chem(  2) = chem(  2) - w(  2)
chem(  3) = chem(  3) + w(  2)
chem(  4) = chem(  4) - w(  2)
chem(  1) = chem(  1) + w(  3)
chem(  3) = chem(  3) - w(  3)
chem(  4) = chem(  4) + w(  3)
chem(  1) = chem(  1) - w(  4)
chem(  2) = chem(  2) + w(  4)
chem(  1) = chem(  1) - w(  5)
chem(  3) = chem(  3) - w(  5)
chem(  4) = chem(  4) + w(  5)

!    Conversion molecules/cm3 to mug/m3.

do i = 1, ns
   chem(i) = chem(i) / convers_factor(i)
enddo

!     Volumic source terms.

do i=1,ns
chem(i)=chem(i)+zcsourc(i)
enddo

return
end subroutine fexchem_1


!===============================================================================

!> jacdchemdc_1
!> \brief Computation of the Jacobian matrix for atmospheric chemistry
!------------------------------------------------------------------------------

!------------------------------------------------------------------------------
! Arguments
!------------------------------------------------------------------------------
!   mode          name               role
!------------------------------------------------------------------------------
!> \param[in]     nr                 total number of chemical reactions
!> \param[in]     ns                 total number of chemical species
!> \param[in]     y                  concentrations vector
!> \param[in]     convers_factor     conversion factors of mug/m3 to
!>                                   molecules/cm3
!> \param[in]     convers_factor_jac conversion factors for the Jacobian matrix
!>                                   (Wmol(i)/Wmol(j))
!> \param[in]     rk                 kinetic rates
!> \param[out]    jacc               Jacobian matrix
!______________________________________________________________________________

subroutine jacdchemdc_1(ns,nr,y,convers_factor,                     &
                    convers_factor_jac,rk,jacc)

implicit none

! Arguments

integer nr,ns
double precision rk(nr),y(ns),jacc(ns,ns)
double precision convers_factor(ns)
double precision convers_factor_jac(ns,ns)

! Local variables

integer i,j
double precision dw(nr,ns)
double precision conc(ns)

do j=1,ns
 do i=1,ns
  jacc(i,j)=0.d0
 enddo
enddo

!     Conversion mug/m3 to molecules/cm3.

do i = 1, ns
   conc(i) = y(i) * convers_factor(i)
enddo

call dratedc_1(ns,nr,rk,conc,dw)

jacc(  3,  4) = jacc(  3,  4)+ 0.2000000000000000d+01*dw(  1,  4)
jacc(  3,  4) = jacc(  3,  4)+ 0.2000000000000000d+01*dw(  1,  4)
jacc(  4,  4) = jacc(  4,  4)- 0.2000000000000000d+01*dw(  1,  4)
jacc(  4,  4) = jacc(  4,  4)- 0.2000000000000000d+01*dw(  1,  4)
jacc(  2,  2) = jacc(  2,  2) - dw(  2,  2)
jacc(  2,  4) = jacc(  2,  4) - dw(  2,  4)
jacc(  3,  2) = jacc(  3,  2) + dw(  2,  2)
jacc(  3,  4) = jacc(  3,  4) + dw(  2,  4)
jacc(  4,  2) = jacc(  4,  2) - dw(  2,  2)
jacc(  4,  4) = jacc(  4,  4) - dw(  2,  4)
jacc(  1,  3) = jacc(  1,  3) + dw(  3,  3)
jacc(  3,  3) = jacc(  3,  3) - dw(  3,  3)
jacc(  4,  3) = jacc(  4,  3) + dw(  3,  3)
jacc(  1,  1) = jacc(  1,  1) - dw(  4,  1)
jacc(  2,  1) = jacc(  2,  1) + dw(  4,  1)
jacc(  1,  1) = jacc(  1,  1) - dw(  5,  1)
jacc(  1,  3) = jacc(  1,  3) - dw(  5,  3)
jacc(  3,  1) = jacc(  3,  1) - dw(  5,  1)
jacc(  3,  3) = jacc(  3,  3) - dw(  5,  3)
jacc(  4,  1) = jacc(  4,  1) + dw(  5,  1)
jacc(  4,  3) = jacc(  4,  3) + dw(  5,  3)

do j = 1, ns
   do i = 1, ns
      jacc(i,j) = jacc(i,j) * convers_factor_jac(i,j)
   enddo
enddo


return
end subroutine jacdchemdc_1


!===============================================================================

!> rates_1
!> \brief Computation of reaction rates
!------------------------------------------------------------------------------

!------------------------------------------------------------------------------
! Arguments
!------------------------------------------------------------------------------
!   mode          name               role
!------------------------------------------------------------------------------
!> \param[in]     ns                 total number of chemical species
!> \param[in]     nr                 total number of chemical reactions
!> \param[in]     rk                 kinetic rates
!> \param[in]     y                  concentrations vector
!> \param[out]    w                  reaction rates
!______________________________________________________________________________

subroutine rates_1(ns,nr,rk,y,w)

implicit none

! Arguments

integer nr,ns
double precision rk(nr),y(ns)

! Local variables

double precision w(nr)

w(  1) =  rk(  1) * y(  4) * y(  4)
w(  2) =  rk(  2) * y(  2) * y(  4)
w(  3) =  rk(  3) * y(  3)
w(  4) =  rk(  4) * y(  1)
w(  5) =  rk(  5) * y(  1) * y(  3)

return
end subroutine rates_1


!===============================================================================

!> dratedc_1
!> \brief Computation of derivatives of reaction rates
!------------------------------------------------------------------------------

!------------------------------------------------------------------------------
! Arguments
!------------------------------------------------------------------------------
!   mode          name               role
!------------------------------------------------------------------------------
!> \param[in]     nr                 total number of chemical reactions
!> \param[in]     ns                 total number of chemical species
!> \param[in]     rk                 kinetic rates
!> \param[in]     y                  concentrations vector
!> \param[out]    dw                 derivatives of reaction rates
!______________________________________________________________________________

subroutine dratedc_1(ns,nr,rk,y,dw)

implicit none

! Arguments

integer nr,ns
double precision rk(nr),y(ns)

! Local variables

double precision dw(nr,ns)

dw(  1,  4) =  rk(  1) * y(  4)
dw(  1,  4) =  rk(  1) * y(  4)
dw(  2,  2) =  rk(  2) * y(  4)
dw(  2,  4) =  rk(  2) * y(  2)
dw(  3,  3) =  rk(  3)
dw(  4,  1) =  rk(  4)
dw(  5,  1) =  rk(  5) * y(  3)
dw(  5,  3) =  rk(  5) * y(  1)

return
end subroutine dratedc_1


!===============================================================================

!> lu_decompose_1
!> \brief Computation of LU factorization of matrix m
!------------------------------------------------------------------------------

!------------------------------------------------------------------------------
! Arguments
!------------------------------------------------------------------------------
!   mode          name               role
!------------------------------------------------------------------------------
!> \param[in]     ns                 matrix row number from the chemical species
!>                                   number
!> \param[in,out] m                  on entry, an invertible matrix.
!>                                   On exit, an LU factorization of m
!______________________________________________________________________________

subroutine lu_decompose_1 (ns,m)

implicit none

! Arguments

integer ns
double precision m(ns,ns)

! Local variables

double precision temp

!     Upper part.
m(1, 3) = m(1, 3) / m(1, 1)

!     Upper part.
temp = m(2, 1) * m(1, 3)
m(2, 3) = ( m(2, 3) - temp ) / m(2, 2)
!     Upper part.
m(2, 4) = m(2, 4) / m(2, 2)

!     Lower part.
temp = m(3, 1) * m(1, 3)
temp = temp + m(3, 2) * m(2, 3)
m(3, 3) = m(3, 3) - temp
!     Lower part.
temp = m(4, 1) * m(1, 3)
temp = temp + m(4, 2) * m(2, 3)
m(4, 3) = m(4, 3) - temp
!     Upper part.
temp = m(3, 2) * m(2, 4)
m(3, 4) = ( m(3, 4) - temp ) / m(3, 3)

!     Lower part.
temp = m(4, 2) * m(2, 4)
temp = temp + m(4, 3) * m(3, 4)
m(4, 4) = m(4, 4) - temp

return
end subroutine lu_decompose_1


!===============================================================================

!> lu_solve_1
!> \brief Resolution of MY=X where M is an LU factorization computed
!>        by lu_decompose_1
!------------------------------------------------------------------------------

!------------------------------------------------------------------------------
! Arguments
!------------------------------------------------------------------------------
!   mode          name         role
!------------------------------------------------------------------------------
!> \param[in]     ns           matrix row number from the chemical species number
!> \param[in]     m            an LU factorization computed by lu_decompose_1
!> \param[in,out] x            on entry, the right-hand side of the equation.
!                              on exit, the solution of the equation
!______________________________________________________________________________

subroutine lu_solve_1 (ns, m, x)

implicit none

! Arguments

integer ns
double precision m(ns,ns)
double precision x(ns)

! Local variables

double precision temp

!     Forward substitution.

x(1) = x(1) / m(1, 1)

temp = m(2, 1) * x(1)
x(2) = ( x(2) - temp ) / m(2, 2)

temp = m(3, 1) * x(1)
temp = temp + m(3, 2) * x(2)
x(3) = ( x(3) - temp ) / m(3, 3)

temp = m(4, 1) * x(1)
temp = temp + m(4, 2) * x(2)
temp = temp + m(4, 3) * x(3)
x(4) = ( x(4) - temp ) / m(4, 4)


!     Backward substitution.

temp = m(3, 4) * x(4)
x(3) = x(3) - temp

temp = m(2, 3) * x(3)
temp = temp + m(2, 4) * x(4)
x(2) = x(2) - temp

temp = m(1, 3) * x(3)
x(1) = x(1) - temp

return
end subroutine lu_solve_1

