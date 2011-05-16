!-------------------------------------------------------------------------------

!     This file is part of the Code_Saturne Kernel, element of the
!     Code_Saturne CFD tool.

!     Copyright (C) 1998-2009 EDF S.A., France

!     contact: saturne-support@edf.fr

!     The Code_Saturne Kernel is free software; you can redistribute it
!     and/or modify it under the terms of the GNU General Public License
!     as published by the Free Software Foundation; either version 2 of
!     the License, or (at your option) any later version.

!     The Code_Saturne Kernel is distributed in the hope that it will be
!     useful, but WITHOUT ANY WARRANTY; without even the implied warranty
!     of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
!     GNU General Public License for more details.

!     You should have received a copy of the GNU General Public License
!     along with the Code_Saturne Kernel; if not, write to the
!     Free Software Foundation, Inc.,
!     51 Franklin St, Fifth Floor,
!     Boston, MA  02110-1301  USA

!-------------------------------------------------------------------------------

subroutine futsvc &
!================

 ( idbia0 , idbra0 ,                                              &
   nvar   , nscal  , ncepdp , ncesmp ,                            &
   iscal  , iscala ,                                              &
   itypfb ,                                                       &
   icepdc , icetsm , itypsm ,                                     &
   ia     ,                                                       &
   dt     , rtpa   , rtp    , propce , propfa , propfb ,          &
   coefa  , coefb  ,                                              &
   smbrs  , rovsdt ,                                              &
   wfb    ,                                                       &
   w1     , w2     , w3     , w4     , w5     ,                   &
   w6     , w7     , w8     ,                                     &
   ra     )

!===============================================================================
! FONCTION :
! ----------

! ROUTINE PHYSIQUE PARTICULIERE : FLAMME FUEL
!   TERMES SOURCES DE PRODUCTION ET DE DISSIPATION POUR
!   LA VARIANCE (BILANS EXPLICITE ET IMPLICITE)

!-------------------------------------------------------------------------------
!ARGU                             ARGUMENTS
!__________________.____._____.________________________________________________.
! name             !type!mode ! role                                           !
!__________________!____!_____!________________________________________________!
! idbia0           ! i  ! <-- ! number of first free position in ia            !
! idbra0           ! i  ! <-- ! number of first free position in ra            !
! nvar             ! i  ! <-- ! total number of variables                      !
! nscal            ! i  ! <-- ! total number of scalars                        !
! ncepdp           ! i  ! <-- ! number of cells with head loss                 !
! ncesmp           ! i  ! <-- ! number of cells with mass source term          !
! iscal            ! i  ! <-- ! scalar number                                  !
! iscala           ! e  ! <-- ! numero du scalaire associe                     !
! itypfb           ! ia ! <-- ! boundary face types                            !
! icepdc(ncelet    ! te ! <-- ! numero des ncepdp cellules avec pdc            !
! icetsm(ncesmp    ! te ! <-- ! numero des cellules a source de masse          !
! itypsm           ! te ! <-- ! type de source de masse pour les               !
! (ncesmp,nvar)    !    !     !  variables (cf. ustsma)                        !
! ia(*)            ! ia ! --- ! main integer work array                        !
! dt(ncelet)       ! ra ! <-- ! time step (per cell)                           !
! rtp, rtpa        ! ra ! <-- ! calculated variables at cell centers           !
!  (ncelet, *)     !    !     !  (at current and previous time steps)          !
! propce(ncelet, *)! ra ! <-- ! physical properties at cell centers            !
! propfa(nfac, *)  ! ra ! <-- ! physical properties at interior face centers   !
! propfb(nfabor, *)! ra ! <-- ! physical properties at boundary face centers   !
! coefa, coefb     ! ra ! <-- ! boundary conditions                            !
!  (nfabor, *)     !    !     !                                                !
! smbrs(ncelet)    ! tr ! --> ! second membre explicite                        !
! rovsdt(ncelet    ! tr ! --> ! partie diagonale implicite                     !
! wfb(nfabor)      ! tr ! --- ! tableau de travail    faces de bord            !
! w1..8(ncelet)    ! tr ! --- ! tableau de travail    cellules                 !
! ra(*)            ! ra ! --- ! main real work array                           !
!__________________!____!_____!________________________________________________!

!     TYPE : E (ENTIER), R (REEL), A (ALPHANUMERIQUE), T (TABLEAU)
!            L (LOGIQUE)   .. ET TYPES COMPOSES (EX : TR TABLEAU REEL)
!     MODE : <-- donnee, --> resultat, <-> Donnee modifiee
!            --- tableau de travail
!===============================================================================

!===============================================================================
! Module files
!===============================================================================

use paramx
use numvar
use entsor
use optcal
use cstphy
use cstnum
use parall
use period
use ppppar
use ppthch
use coincl
use cpincl
use fuincl
use ppincl
use mesh

!===============================================================================

implicit none

! Arguments

integer          idbia0 , idbra0
integer          nvar   , nscal
integer          ncepdp , ncesmp
integer          iscal  , iscala

integer          itypfb(nfabor)
integer          icepdc(ncepdp)
integer          icetsm(ncesmp), itypsm(ncesmp,nvar)
integer          ia(*)

double precision dt(ncelet), rtp(ncelet,*), rtpa(ncelet,*)
double precision propce(ncelet,*)
double precision propfa(nfac,*), propfb(nfabor,*)
double precision coefa(nfabor,*), coefb(nfabor,*)
double precision smbrs(ncelet), rovsdt(ncelet)
double precision wfb(nfabor)
double precision w1(ncelet), w2(ncelet), w3(ncelet)
double precision w4(ncelet), w5(ncelet), w6(ncelet)
double precision w7(ncelet), w8(ncelet)
double precision ra(*)

! Local variables

integer          idebia , idebra
integer          ivar   , ivarsc , ivarut , ivar0
integer          iel    , ifac   , icla
integer          ipcrom , ipcvst
integer          ikiph  , ieiph , iomgip , iphydp
integer          ir11ip , ir22ip, ir33ip
integer          inc    , iccocg , nswrgp , imligp , iwarnp
integer          ifinra , icoefa , icoefb

double precision xk     , xe     , rhovst
double precision epsrgp , climgp , extrap

!===============================================================================

!===============================================================================
! 1. INITIALISATION
!===============================================================================

! Initialize variables to avoid compiler warnings

ieiph = 0
ikiph = 0
iomgip = 0
ir11ip = 0
ir22ip = 0
ir33ip = 0

xe = 0.d0
xk = 0.d0

! Memoire

idebia = idbia0
idebra = idbra0

! --- Numero du scalaire a traiter : ISCAL

! --- Numero de la variable associee au scalaire a traiter ISCAL
ivar   = isca(iscal)

! --- Numero du scalaire eventuel associe dans le cas fluctuation
!         ISCALA et numero de variable de calcul
if (iscala.gt.0) then
  ivarsc = isca(iscala)
else
  ivarsc = 0
endif

! --- Numero des variables de calcul
if ( itytur.eq.2 .or. iturb.eq.50 ) then
  ikiph  = ik
  ieiph  = iep
elseif ( itytur.eq.3 ) then
  ir11ip = ir11
  ir22ip = ir22
  ir33ip = ir33
  ieiph  = iep
elseif ( iturb.eq.60 ) then
  ikiph  = ik
  iomgip = iomg
endif

! --- Numero des grandeurs physiques
ipcrom = ipproc(irom)
ipcvst = ipproc(ivisct)


!===============================================================================
! 2. PRISE EN COMPTE DES TERMES SOURCES DE PRODUCTION PAR LES GRADIENTS
!    ET DE DISSIPATION
!===============================================================================

if ( itytur.eq.2 .or. itytur.eq.3                   &
     .or. iturb.eq.50 .or. iturb.eq.60) then

  inc = 1
  iccocg = 1
  if (ivarsc.gt.0) then
    ivarut = ivarsc
  else
! A defaut de savoir pour F4M on prend comme pour IFVAP
    ivarut = isca(ifvap)
  endif
  nswrgp = nswrgr(ivarut)
  imligp = imligr(ivarut)
  iwarnp = iwarni(ivarut)
  epsrgp = epsrgr(ivarut)
  climgp = climgr(ivarut)
  extrap = extrag(ivarut)

! --> Calcul de FIM et X2 dans W7 et W8

  do iel = 1, ncel
    w1(iel) = zero
    w2(iel) = zero
    w3(iel) = zero
    w4(iel) = zero
    w5(iel) = zero
    w6(iel) = zero
    w7(iel) = zero
    w8(iel) = 1.d0
  enddo

! ---- W8 = X1

  do iel = 1, ncel
    w8(iel) = 1.d0
  enddo
  do icla = 1, nclafu
    do iel = 1, ncel
      w8(iel) = w8(iel) - rtp(iel,isca(iyfol(icla)))
    enddo
  enddo

! ---- W7 = FJM (kg/kg du melange gazeux)

  if (ivarsc.eq.0) then
    do iel = 1, ncel
      w7(iel) = 1.d0                                              &
               -( rtp(iel,isca(ifhtf))                            &
                 +rtp(iel,isca(ifvap)) ) / w8(iel)

    enddo
  else
    do iel = 1, ncel
      w7(iel) = rtp(iel,ivarsc) / w8(iel)
    enddo
  endif

! --> Calcul des COEFA et COEFB de FIM afin d'en calculer son gradient
!     On alloue localement 2 tableaux de NFABOR pour le calcul
!       de COEFA et COEFB de FIM

  icoefa = idebra
  icoefb = icoefa + nfabor
  ifinra = icoefb + nfabor
  call rasize ('futsvc',ifinra)
  !==========

  do ifac = 1, nfabor
    ra(icoefa+ifac-1) = zero
    ra(icoefb+ifac-1) = 1.d0
    if ( itypfb(ifac).eq.ientre ) then
      ra(icoefa+ifac-1) = zero
      ra(icoefb+ifac-1) = zero
      if (ivarsc.eq.0) ra(icoefa+ifac-1) = 1.d0
    endif
  enddo

  ! En periodique et parallele, echange avant calcul du gradient
  if (irangp.ge.0.or.iperio.eq.1) then
    call synsca(w7)
    !==========
  endif

!  IVAR0 = 0 (indique pour la periodicite de rotation que la variable
!     n'est pas la vitesse ni Rij)
  ivar0  = 0
  iphydp = 0
  call grdcel                                                     &
  !==========
 ( ivar0  , imrgra , inc    , iccocg , nswrgp , imligp , iphydp , &
   iwarnp , nfecra , epsrgp , climgp , extrap ,                   &
   ia     ,                                                       &
   w7     , w7     , w7     ,                                     &
   w7     , ra(icoefa) , ra(icoefb)  ,                            &
!          FIM      COEFA        COEFB
   w1     , w2     , w3     ,                                     &
!        ------   ------   ------
   w4     , w5     , w6     ,                                     &
   ra     )

  do iel = 1, ncel
    if ( itytur.eq.2 .or. iturb.eq.50 ) then
      xk = rtpa(iel,ikiph)
      xe = rtpa(iel,ieiph)
    elseif ( itytur.eq.3 ) then
      xk =                                                        &
       0.5d0*(rtpa(iel,ir11ip)+rtpa(iel,ir22ip)+rtpa(iel,ir33ip))
      xe = rtpa(iel,ieiph)
    elseif ( iturb.eq.60 ) then
      xk = rtpa(iel,ikiph)
      xe = cmu*xk*rtpa(iel,iomgip)
    endif

    rhovst = propce(iel,ipcrom)*xe/                               &
             (xk * rvarfl(iscal))*volume(iel)
    rovsdt(iel) = rovsdt(iel) + max(zero,rhovst)
    smbrs (iel) = smbrs (iel) +                                   &
                2.d0*propce(iel,ipcvst)*volume(iel)/sigmas(iscal) &
                * (w1(iel)**2 + w2(iel)**2 + w3(iel)**2) * w8(iel)&
                - rhovst*rtpa(iel,ivar)

  enddo

endif

!--------
! FORMATS
!--------



!----
! FIN
!----

return

end subroutine
