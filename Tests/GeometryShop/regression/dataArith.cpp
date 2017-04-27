/*
 *       {_       {__       {__{_______              {__      {__
 *      {_ __     {_ {__   {___{__    {__             {__   {__  
 *     {_  {__    {__ {__ { {__{__    {__     {__      {__ {__   
 *    {__   {__   {__  {__  {__{_ {__       {_   {__     {__     
 *   {______ {__  {__   {_  {__{__  {__    {_____ {__  {__ {__   
 *  {__       {__ {__       {__{__    {__  {_         {__   {__  
 * {__         {__{__       {__{__      {__  {____   {__      {__
 *
 */

#include "AMReX_BaseIVFactory.H"
#include "AMReX_EBIndexSpace.H"
#include "AMReX_EBISLayout.H"
#include "AMReX_BoxIterator.H"
#include "AMReX_ParmParse.H"
#include "AMReX_GeometryShop.H"
#include "AMReX_EBCellFAB.H"
#include "AMReX_EBLevelGrid.H"
#include "AMReX_LayoutData.H"
#include "AMReX_EBCellFactory.H"
#include "AMReX_VoFIterator.H"
#include "AMReX_EBArith.H"
#include "AMReX_AllRegularService.H"
#include "AMReX_PlaneIF.H"
#include "AMReX_SPMD.H"
#include "AMReX_Print.H"
#include "AMReX_EBFluxFactory.H"
#include "AMReX_EBFluxFAB.H"
#include "AMReX_EBCellFactory.H"
#include "AMReX_EBCellFAB.H"
#include "AMReX_IrregFABFactory.H"
#include "AMReX_IrregFAB.H"

namespace amrex
{
/***************/
int makeGeometry(Box& a_domain,
                 Real& a_dx)
{
  int eekflag =  0;
  //parse input file
  ParmParse pp;
  RealVect origin = RealVect::Zero;
  std::vector<int> n_cell;
  pp.getarr("n_cell", n_cell, 0, SpaceDim);

  IntVect lo = IntVect::TheZeroVector();
  IntVect hi;
  for (int ivec = 0; ivec < SpaceDim; ivec++)
    {
      if (n_cell[ivec] <= 0)
        {
          amrex::Print() << " bogus number of cells input = " << n_cell[ivec];
          return(-1);
        }
      hi[ivec] = n_cell[ivec] - 1;
    }

  a_domain.setSmall(lo);
  a_domain.setBig(hi);

  Real prob_hi;
  pp.get("prob_hi",prob_hi);
  a_dx = prob_hi/n_cell[0];

  int whichgeom;
  pp.get("which_geom",whichgeom);
  if (whichgeom == 0)
    {
      //allregular
      amrex::Print() << "all regular geometry" << "\n";
      AllRegularService regserv;
      EBIndexSpace* ebisPtr = AMReX_EBIS::instance();
      ebisPtr->define(a_domain, origin, a_dx, regserv);
    }
  else if (whichgeom == 1)
    {
      amrex::Print() << "ramp geometry" << "\n";
      int upDir;
      int indepVar;
      Real startPt;
      Real slope;
      pp.get("up_dir",upDir);
      pp.get("indep_var",indepVar);
      pp.get("start_pt", startPt);
      pp.get("ramp_slope", slope);

      RealVect normal = RealVect::Zero;
      normal[upDir] = 1.0;
      normal[indepVar] = -slope;

      RealVect point = RealVect::Zero;
      point[upDir] = -slope*startPt;

      bool normalInside = true;

      PlaneIF ramp(normal,point,normalInside);


      GeometryShop workshop(ramp,0, a_dx);
      //this generates the new EBIS
      EBIndexSpace* ebisPtr = AMReX_EBIS::instance();
      ebisPtr->define(a_domain, origin, a_dx, workshop);
    }
  else
    {
      //bogus which_geom
      amrex::Print() << " bogus which_geom input = "
             << whichgeom << "\n";
      eekflag = 33;
    }

  return eekflag;
}
/***************/
int testArith()
{
  Box domain;
  Real dx;
  makeGeometry(domain, dx);
  int maxboxsize;
  ParmParse pp;
  pp.get("maxboxsize", maxboxsize);
  BoxArray ba(domain);
  ba.maxSize(maxboxsize);
  DistributionMapping dm(ba);
  EBLevelGrid eblg(ba, dm, domain, 2);
  int ncomp = 1;

  EBCellFactory  ebcellfact(eblg.getEBISL());
  EBFluxFactory  ebfluxfact(eblg.getEBISL());
  IrregFABFactory irregfact(eblg.getEBISL());
  BaseIVFactory<Real> baseivfact(eblg.getEBISL());
  FabArray<EBCellFAB>        cell1(ba, dm,  ncomp, 0, MFInfo(), ebcellfact);
  FabArray<EBCellFAB>        cell2(ba, dm,  ncomp, 0, MFInfo(), ebcellfact);
  FabArray<EBCellFAB>        cell3(ba, dm,  ncomp, 0, MFInfo(), ebcellfact);
  FabArray<EBFluxFAB>        flux1(ba, dm,  ncomp, 0, MFInfo(), ebfluxfact);
  FabArray<EBFluxFAB>        flux2(ba, dm,  ncomp, 0, MFInfo(), ebfluxfact);
  FabArray<EBFluxFAB>        flux3(ba, dm,  ncomp, 0, MFInfo(), ebfluxfact);
  FabArray<IrregFAB>        irreg1(ba, dm,  ncomp, 0, MFInfo(), irregfact);
  FabArray<IrregFAB>        irreg2(ba, dm,  ncomp, 0, MFInfo(), irregfact);
  FabArray<IrregFAB>        irreg3(ba, dm,  ncomp, 0, MFInfo(), irregfact);

  Real tol = 1.0e-10;
  //let us just try addition for now
  for(MFIter  mfi(ba, dm); mfi.isValid(); ++mfi)
    {
      cell1[mfi].setVal(1.);
      cell2[mfi].setVal(2.);
      cell3[mfi].setVal(0.);
      cell3[mfi] += cell1[mfi];
      cell3[mfi] += cell2[mfi];

      flux1[mfi].setVal(1.);
      flux2[mfi].setVal(2.);
      flux3[mfi].setVal(0.);
      flux3[mfi] += flux1[mfi];
      flux3[mfi] += flux2[mfi];
      Box grid = ba[mfi];
      EBISBox ebisBox = eblg.getEBISL()[mfi];
      IntVectSet ivsBox(grid);
      IntVectSet ivsIrreg = ebisBox.getIrregIVS(grid);
      for (VoFIterator vofit(ivsBox, ebisBox.getEBGraph());
          vofit.ok(); ++vofit)
      {
        const VolIndex& vof = vofit();
        Real val1 = cell1[mfi](vof, 0);
        Real val2 = cell2[mfi](vof, 0);
        Real val3 = cell3[mfi](vof, 0);
        if(std::abs(val1-1.0) > tol) return -1;
        if(std::abs(val2-2.0) > tol) return -2;
        if(std::abs(val3-3.0) > tol) return -3;
      }

      for (VoFIterator vofit(ivsIrreg, ebisBox.getEBGraph());
           vofit.ok(); ++vofit)
      {
        const VolIndex& vof = vofit();
        Real val1 = flux1[mfi].getEBFlux()(vof, 0);
        Real val2 = flux2[mfi].getEBFlux()(vof, 0);
        Real val3 = flux3[mfi].getEBFlux()(vof, 0);
        if(std::abs(val1-1.0) > tol) return -11;
        if(std::abs(val2-2.0) > tol) return -22;
        if(std::abs(val3-3.0) > tol) return -33;
      }
      for(int idir = 0; idir < SpaceDim; idir++)
      {
        for(FaceIterator faceit(ivsBox, ebisBox.getEBGraph(), idir, 
                                FaceStop::SurroundingWithBoundary); faceit.ok(); ++faceit)
        {
          Real val1 = flux1[mfi][idir](faceit(), 0);
          Real val2 = flux2[mfi][idir](faceit(), 0);
          Real val3 = flux3[mfi][idir](faceit(), 0);
          if(std::abs(val1-1.0) > tol) return -111;
          if(std::abs(val2-2.0) > tol) return -222;
          if(std::abs(val3-3.0) > tol) return -333;
        }
      }
    }

  return 0;
}
}
/***************/
int
main(int argc, char* argv[])
{
  int retval = 0;
  amrex::Initialize(argc,argv);

  retval = amrex::testArith();
  if(retval != 0)
  {
    amrex::Print() << "simple arith test failed with code " << retval << "\n";
  }
  else
  {
    amrex::Print() << "simple arith test passed \n";
  }
  amrex::Finalize();
  return retval;
}
