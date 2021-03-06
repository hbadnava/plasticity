//implementation of the crystal plasticity material model for BCC crystal structure
#ifndef MODEL_BCC_H
#define MODEL_BCC_H

//dealii headers
#include "../../../../include/ellipticBVP.h"
#include "../../../../src/utilityObjects/crystalOrientationsIO.cc"
#include <iostream>
#include <fstream>

typedef struct {
    
} materialProperties;

//material model class for crystal plasticity
//derives from ellipticBVP base abstract class
template <int dim>
class crystalPlasticity : public ellipticBVP<dim>
{
public:
    /**
     *crystalPlasticity class constructor.
     */
    crystalPlasticity();
#ifdef readExternalMeshes
#if readExternalMeshes==true
    void mesh();
#endif
#endif
    /**
     *calculates the texture of the deformed polycrystal
     */
    void reorient();
    /** 
     *calculates the material tangent modulus dPK1/dF at the quadrature point
     F_trial-trial Elastic strain (Fe_trial)
     Fpn_inv -inverse of plastic strain (Fp^-1)
     SCHMID_TENSOR1 (Schmid Tensor S)
     A->Stiffness Matrix A
     A-PA-> Stiffness matrix of active slip systems
     B->Refer to Eqn (5)
     T_tau-Cauchy Stress
     PK1Stiff-dP/dF -Tangent Modulus
     active-set of active slip systems
     resolved_shear_tau_trial- trial resolved shear stress
     x_beta-incremental shear strain delta_gamma
     PA-active slip systems
     det_F_tau-det(F_tau)
     det_FE_tau-det(FE_tau)
    */
    void tangent_modulus(FullMatrix<double> &F_trial, FullMatrix<double> &Fpn_inv, FullMatrix<double> &SCHMID_TENSOR1, FullMatrix<double> &A,FullMatrix<double> &A_PA,FullMatrix<double> &B,FullMatrix<double> &T_tau, FullMatrix<double> &PK1_Stiff, Vector<double> &active, Vector<double> &resolved_shear_tau_trial, Vector<double> &x_beta, Vector<double> &PA, int &n_PA, double &det_F_tau, double &det_FE_tau );
    /**
     *calculates the incremental shear strain in the slip systems by active set search and removal of inactive slip systems
     A->Stiffness Matrix A
     A-PA-> Stiffness matrix of active slip systems
     active-set of active slip systems
     inactive-set of inactive slip systems
     n_PA-No. of active slip systems
     x_beta-incremental shear strain delta_gamma
     PA-active slip systems
    */
     

    void inactive_slip_removal(Vector<double> &active,Vector<double> &x_beta_old, Vector<double> &x_beta, int &n_PA, Vector<double> &PA, Vector<double> b,FullMatrix<double> A,FullMatrix<double> &A_PA);
    /**
     * Structure to hold material parameters
     */
    materialProperties properties;
    //orientation maps
    crystalOrientationsIO<dim> orientations;
private:
    void init(unsigned int num_quad_points);
    void setBoundaryValues(const Point<dim>& node, const unsigned int dof, bool& flag, double& value);
    /**
     * Updates the stress and tangent modulus at a given quadrature point in a element for
     * the given constitutive model. Takes the deformation gradient at the current nonlinear
     * iteration and the elastic and plastic deformation gradient of the previous time step to 
     *calculate the stress and tangent modulus
     */
    
    void calculatePlasticity(unsigned int cellID,
                             unsigned int quadPtID);
    void getElementalValues(FEValues<dim>& fe_values,
                            unsigned int dofs_per_cell,
                            unsigned int num_quad_points,
                            FullMatrix<double>& elementalJacobian,
                            Vector<double>&     elementalResidual);
    void updateAfterIncrement();
    void updateBeforeIteration();
    void updateBeforeIncrement();
    
    
    /**
     *calculates the rotation matrix (OrientationMatrix) from the rodrigues vector (r)
     */
    void odfpoint(FullMatrix <double> &OrientationMatrix,Vector<double> r);
    /**
     *calculates the vector form (Voigt Notation) of the symmetric matrix A
     */
    Vector<double> vecform(FullMatrix<double> A);
    /**
     *calculates the symmetric matrix (A) from the vector form Av (Voigt Notation)
     */
    void matform(FullMatrix<double> &A, Vector<double> Av);
    
    /**
     *calculates the equivalent matrix Aright for the second order tensorial operation XA=B => A_r*{x}={b}
    */
     
    void right(FullMatrix<double> &Aright,FullMatrix<double> elm);
    
    /**
     *calculates the equivalent matrix A for the second order tensorial operation symm(AX)=B => A_r*{x}={b}
     */
    void symmf(FullMatrix<double> &A,FullMatrix<double> elm);
    /**
     *calculates the equivalent matrix Aleft for the second order tensorial operation AX=B => A_r*{x}={b}
     */
    void left(FullMatrix<double> &Aleft,FullMatrix<double> elm);
    
    /** 
     *calculates the product of a fourth-order tensor and second-order Tensor to calculate stress
     */
    void ElasticProd(FullMatrix<double> &stress,FullMatrix<double> elm, FullMatrix<double> ElasticityTensor);
    
    /**
     *calculates the equivalent matrix Aleft for the second order tensorial operation trace(AX)=B => A_r*{x}={b}
     */
    void tracev(FullMatrix<double> &Atrace, FullMatrix<double> elm, FullMatrix<double> B);
    
    /**
     *calculates the matrix exponential of matrix A
     */
    
    FullMatrix<double> matrixExponential(FullMatrix<double> A);
    
    
    /**
     * Global deformation gradient F
    */
    FullMatrix<double> F;
    /**
     * Deformation gradient in crystal plasticity formulation. By default F=F_tau
     */
    FullMatrix<double> F_tau;
    /**
     * Plastic deformation gradient in crystal plasticity formulation. F_tau=Fe_tau*Fp_tau
     */
    FullMatrix<double> FP_tau;
    /**
     * Elastic deformation gradient in crystal plasticity formulation. F_tau=Fe_tau*Fp_tau
     */
    FullMatrix<double> FE_tau;
    /**
     * Cauchy Stress T
     */
    FullMatrix<double> T;
    /**
     * First Piola-Kirchhoff stress
     */
    FullMatrix<double> P;
    
    /**
     * volume weighted Cauchy stress per core
     */
    FullMatrix<double> local_stress;
    /**
     * volume weighted Lagrangian strain per core
     */
    FullMatrix<double> local_strain;
    /**
     * volume averaged global Cauchy stress
     */
    FullMatrix<double> global_stress;
    /**
     * volume averaged global Lagrangian strain
     */
    FullMatrix<double> global_strain;
    
    /**
     * Tangent modulus dPK1/dF
     */
    Tensor<4,dim,double> dP_dF;
    
    /**
     * No. of elements
     */
    double No_Elem;
    /**
     * No. of quadrature points per element
     */
    double N_qpts;
    /**
     * volume of elements per core
     */
    double local_microvol;
    /**
     * global volume
     */
    double microvol;
    
    double signstress;
    
    
    //Store crystal orientations
    /**
     * Stores original crystal orientations as rodrigues vectors by element number and quadratureID
     */
    std::vector<std::vector<  Vector<double> > >  rot;
    /**
     * Stores deformed crystal orientations as rodrigues vectors by element number and quadratureID
     */
    std::vector<std::vector<  Vector<double> > >  rotnew;
    
    //Store history variables
    /**
     * Stores Plastic deformation gradient by element number and quadratureID at each iteration
     */
    std::vector< std::vector< FullMatrix<double> > >   Fp_iter;
    /**
     * Stores Plastic deformation gradient by element number and quadratureID at each increment
     */
    std::vector< std::vector< FullMatrix<double> > > Fp_conv;
    /**
     * Stores Elastic deformation gradient by element number and quadratureID at each iteration
     */
    std::vector< std::vector< FullMatrix<double> > >   Fe_iter;
    /**
     * Stores Elastic deformation gradient by element number and quadratureID at each increment
     */
    std::vector< std::vector< FullMatrix<double> > > Fe_conv;
    /**
     * Stores slip resistance by element number and quadratureID at each iteration
     */
    std::vector<std::vector<  Vector<double> > >  s_alpha_iter;
    /**
     * Stores slip resistance by element number and quadratureID at each increment
     */
    std::vector<std::vector<  Vector<double> > >  s_alpha_conv;
    
    /**
     * No. of slip systems
     */
    unsigned int n_slip_systems; //No. of slip systems
    /**
     * Slip directions
     */
    FullMatrix<double> m_alpha;
    /**
     * Slip Normals
     */
    FullMatrix<double> n_alpha;
    /**
     * Latent Hardening Matrix
     */
    FullMatrix<double> q;

    FullMatrix<double> sres;
    /**
     * Elastic Stiffness Matrix
     */
    FullMatrix<double> Dmat;
    /**
     * slip resistance
     */
    Vector<double> sres_tau;
    bool initCalled;
    
    //orientatations data for each quadrature point
    /**
     * Stores grainID No. by element number and quadratureID
     */
    std::vector<std::vector<unsigned int> > quadratureOrientationsMap;
    void loadOrientations();
};

//(these are source files, which will are temporarily treated as
//header files till library packaging scheme is finalized)
#include "model.cc"
#include "calculatePlasticity.cc"
#include "rotationOperations.cc"
#include "init.cc"
#include "matrixOperations.cc"
//#include "tangentModulus.cc"
#include "inactiveSlipRemoval.cc"
#include "reorient.cc"
#include "loadOrientations.cc"

#endif
