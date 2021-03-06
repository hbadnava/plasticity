//implementation of the crystal plasticity material model
#ifndef CRYSTALPLASTICITY_HCP_H
#define CRYSTALPLASTICITY_HCP_H

//dealii headers
#include "../../../../include/ellipticBVP.h"
#include "../../../../src/utilityObjects/crystalOrientationsIO.cc"
#include <iostream>
#include <fstream>

typedef struct {
    unsigned int n_slip_systems,n_twin_systems; //No. of slip systems
    double q1,q2,a1,a2,a3,a4,a5,h01,h02,h03,h04,h05,s_s1,s_s2,s_s3,s_s4,s_s5,s01,s02,s03,s04,s05,C11,C12,C13,C33,C44;
    FullMatrix<double> m_alpha,n_alpha;
} materialProperties;

//material model class for crystal plasticity
//derives from ellipticBVP base abstract class
template <int dim>
class crystalPlasticity : public ellipticBVP<dim>
{
public:
    crystalPlasticity();
    void mesh();
    void reorient();
    void tangent_modulus(FullMatrix<double> &F_trial, FullMatrix<double> &Fpn_inv, FullMatrix<double> &SCHMID_TENSOR1, FullMatrix<double> &A,FullMatrix<double> &A_PA,FullMatrix<double> &B,FullMatrix<double> &T_tau, FullMatrix<double> &PK1_Stiff, Vector<double> &active, Vector<double> &resolved_shear_tau_trial, Vector<double> &x_beta, Vector<double> &PA, int &n_PA, double &det_F_tau, double &det_FE_tau );
    void inactive_slip_removal(Vector<double> &inactive,Vector<double> &active, Vector<double> &x_beta, int &n_PA, Vector<double> &PA, Vector<double> b,FullMatrix<double> A,FullMatrix<double> A_PA);
    //material properties
    materialProperties properties;
    //orientation maps
    crystalOrientationsIO<dim> orientations;
private:
    void init(unsigned int num_quad_points);
    void markBoundaries();
    void applyDirichletBCs();
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
    
    
    void odfpoint(FullMatrix <double> &OrientationMatrix,Vector<double> r);
    Vector<double> vecform(FullMatrix<double> A);
    void matform(FullMatrix<double> &A, Vector<double> Av);
    void right(FullMatrix<double> &Aright,FullMatrix<double> elm);
    void symmf(FullMatrix<double> &A,FullMatrix<double> elm);
    void left(FullMatrix<double> &Aleft,FullMatrix<double> elm);
    void ElasticProd(FullMatrix<double> &stress,FullMatrix<double> elm, FullMatrix<double> ElasticityTensor);
    void tracev(FullMatrix<double> &Atrace, FullMatrix<double> elm, FullMatrix<double> B);
    void Twin_image(double twin_pos,unsigned int cellID,unsigned int quadPtID);
    void rod2quat(Vector<double> &quat,Vector<double> &rod);
    void quatproduct(Vector<double> &quatp,Vector<double> &quat2,Vector<double> &quat1);
    void quat2rod(Vector<double> &quat,Vector<double> &rod);
    
    
    
    
    FullMatrix<double> F,F_tau,FP_tau,FE_tau,T,P,local_stress,local_strain,global_stress,global_strain;
    Tensor<4,dim,double> dP_dF;
    double No_Elem, N_qpts,local_F_e,local_F_r,F_e,F_r,local_microvol,microvol;
    
    //Store crystal orientations
    std::vector<std::vector<  Vector<double> > >  rot;
    std::vector<std::vector<  Vector<double> > >  rotnew;
    
    //Store history variables
    std::vector< std::vector< FullMatrix<double> > >   Fp_iter;
    std::vector< std::vector< FullMatrix<double> > > Fp_conv;
    std::vector< std::vector< FullMatrix<double> > >   Fe_iter;
    std::vector< std::vector< FullMatrix<double> > > Fe_conv;
    std::vector<std::vector<  Vector<double> > >  s_alpha_iter;
    std::vector<std::vector<  Vector<double> > >  s_alpha_conv;
    std::vector<std::vector<  vector<double> > >  twinfraction_iter, slipfraction_iter,twinfraction_conv, slipfraction_conv;
    std::vector<std::vector<double> >  twin;
    
    unsigned int n_slip_systems,n_twin_systems; //No. of slip systems
    FullMatrix<double> m_alpha,n_alpha,q,sres,Dmat;
    Vector<double> sres_tau;
    bool initCalled;
    
    //orientatations data for each quadrature point
    std::vector<std::vector<unsigned int> > quadratureOrientationsMap;
    void loadOrientations();
};

//constructor
template <int dim>
crystalPlasticity<dim>::crystalPlasticity() :
ellipticBVP<dim>(),
F(dim,dim),
F_tau(dim,dim),
FP_tau(dim,dim),
FE_tau(dim,dim),
T(dim,dim),
P(dim,dim)
{
    initCalled = false;
    
    //post processing
    ellipticBVP<dim>::numPostProcessedFields=4;
    ellipticBVP<dim>::postprocessed_solution_names.push_back("Eqv_strain");
    ellipticBVP<dim>::postprocessed_solution_names.push_back("Eqv_stress");
    ellipticBVP<dim>::postprocessed_solution_names.push_back("Grain_ID");
    ellipticBVP<dim>::postprocessed_solution_names.push_back("twin");
    
    
}

template <int dim>
void crystalPlasticity<dim>::calculatePlasticity(unsigned int cellID,
                                                 unsigned int quadPtID)
{
    
    F_tau=F; // Deformation Gradient
    FullMatrix<double> FE_t(dim,dim),FP_t(dim,dim);  //Elastic and Plastic deformation gradient
    Vector<double> s_alpha_t(n_slip_systems); // Slip resistance
    Vector<double> rot1(dim);// Crystal orientation (Rodrigues representation)
    
    int old_precision = std::cout.precision();
    
    
    std::cout.precision(16);
    
    FE_t=Fe_conv[cellID][quadPtID];
    FP_t=Fp_conv[cellID][quadPtID];
    s_alpha_t=s_alpha_conv[cellID][quadPtID];
    rot1=rot[cellID][quadPtID];
    
    // Rotation matrix of the crystal orientation
    FullMatrix<double> rotmat(dim,dim);
    rotmat=0.0;
    odfpoint(rotmat,rot1);
    
    FullMatrix<double> temp(dim,dim),temp1(dim,dim),temp2(dim,dim),temp3(dim,dim); // Temporary matrices
    
    //convert to crystal coordinates F_tau=R'*F_tau*R
    temp=0.0;
    rotmat.Tmmult(temp,F_tau);
    temp.mmult(F_tau,rotmat);
    
    //FE_tau_trial=F_tau*inv(FP_t)
    FullMatrix<double> Fpn_inv(dim,dim),FE_tau_trial(dim,dim),F_trial(dim,dim),CE_tau_trial(dim,dim),Ee_tau_trial(dim,dim);
    Fpn_inv=0.0; Fpn_inv.invert(FP_t);
    FE_tau_trial=0.0;
    F_trial=0.0;
    F_tau.mmult(FE_tau_trial,Fpn_inv);F_trial = FE_tau_trial;
    
     //% % % % % STEP 1 % % % % %
    //Calculate Trial Elastic Strain Ee_tau_trial
    //Ee_tau_trial=0.5(FE_tau_trial'*FE_tau_trial-I)
    temp.reinit(dim,dim); temp=0.0;
    temp=FE_tau_trial;
    FE_tau_trial.Tmmult(CE_tau_trial,temp);
    Ee_tau_trial=CE_tau_trial;
    temp=IdentityMatrix(dim);
    for(unsigned int i=0;i<dim;i++){
        for(unsigned int j=0;j<dim;j++){
            Ee_tau_trial[i][j] = 0.5*(Ee_tau_trial[i][j]-temp[i][j]);
        }
    }
    
    // Calculation of Schmid Tensors  and B= symm(FE_tau_trial'*FE_tau_trial*S_alpha)
    FullMatrix<double> SCHMID_TENSOR1(n_slip_systems*dim,dim),B(n_slip_systems*dim,dim);
    Vector<double> m1(dim),n1(dim);
    
    for(unsigned int i=0;i<n_slip_systems;i++){
        for (unsigned int j=0;j<dim;j++){
            m1(j)=m_alpha[i][j];
            n1(j)=n_alpha[i][j];
        }
        
        for (unsigned int j=0;j<dim;j++){
            for (unsigned int k=0;k<dim;k++){
                temp[j][k]=m1(j)*n1(k);
                SCHMID_TENSOR1[dim*i+j][k]=m1(j)*n1(k);
            }
        }
        
        CE_tau_trial.mmult(temp2,temp);
        temp2.symmetrize();
        for (unsigned int j=0;j<dim;j++){
            for (unsigned int k=0;k<dim;k++){
                B[dim*i+j][k]=2*temp2[j][k];
            }
        }
    }
    
    //% % % % % STEP 2 % % % % %
    // Calculate the trial stress T_star_tau_trial
    Vector<double> tempv1(6),tempv2(6);
    FullMatrix<double> T_star_tau_trial(dim,dim);
    tempv1=0.0;
    Dmat.vmult(tempv1, vecform(Ee_tau_trial));
    matform(T_star_tau_trial,tempv1);
    
    //% % % % % STEP 3 % % % % %
    // Calculate the trial resolved shear stress resolved_shear_tau_trial for each slip system
    int n_PA=0;	// Number of active slip systems
    Vector<double> PA, PA_temp(1);
    Vector<double> resolved_shear_tau_trial(n_slip_systems),b(n_slip_systems),resolved_shear_tau(n_slip_systems);
    resolved_shear_tau_trial=0.0;
    
    
    for(unsigned int i=0;i<n_slip_systems;i++){
        
        for (unsigned int j=0;j<dim;j++){
            for (unsigned int k=0;k<dim;k++){
                resolved_shear_tau_trial(i)+=T_star_tau_trial[j][k]*SCHMID_TENSOR1[dim*i+j][k];
            }
        }
        //% % % % % STEP 4 % % % % %
        //Determine the set set of the n potentially active slip systems
        b(i)=fabs(resolved_shear_tau_trial(i))-s_alpha_t(i);
        if( b(i)>=0){
            if(n_PA==0){
                n_PA=n_PA+1;
                PA.reinit(n_PA);
                PA(0)=i;
            }
            else{
                PA_temp=PA;
                n_PA=n_PA+1;
                PA.reinit(n_PA);
                for (unsigned int j=0;j<(n_PA-1);j++){
                    PA(j)=PA_temp(j);
                }
                PA(n_PA-1)=i;
                PA_temp.reinit(n_PA);     //%%%%% Potentially active slip systems
            }
        }
        resolved_shear_tau(i)=fabs(resolved_shear_tau_trial(i));
    }
    
    
    //% % % % % STEP 5 % % % % %
    //Calculate the shear increments from the consistency condition
    Vector<double> s_beta(n_slip_systems),h_beta(n_slip_systems),h0(n_slip_systems),a_pow(n_slip_systems),s_s(n_slip_systems);
    FullMatrix<double> h_alpha_beta_t(n_slip_systems,n_slip_systems),A(n_slip_systems,n_slip_systems);
    s_beta=s_alpha_t;
    
    
    for (unsigned int i=0;i<n_slip_systems;i++){
        if(i<3){
            h0(i)=properties.h01;
            a_pow(i)=properties.a1;
            s_s(i)=properties.s_s1;
        }
        else if(i<6){
            h0(i)=properties.h02;
            a_pow(i)=properties.a2;
            s_s(i)=properties.s_s2;
        }
        else if(i<12){
            h0(i)=properties.h03;
            a_pow(i)=properties.a3;
            s_s(i)=properties.s_s3;
        }
        else if(i<18){
            h0(i)=properties.h04;
            a_pow(i)=properties.a4;
            s_s(i)=properties.s_s4;
        }
        else if(i<24){
            h0(i)=properties.h05;
            a_pow(i)=properties.a5;
            s_s(i)=properties.s_s5;
        }
        
    }
    
    
    // Single slip hardening rate
    for(unsigned int i=0;i<n_slip_systems;i++){
        if(s_beta(i)>s_s(i))
            s_beta(i)=0.98*s_s(i);
        h_beta(i)=h0(i)*pow((1-s_beta(i)/s_s(i)),a_pow(i));
    }
    
    
    for(unsigned int i=0;i<n_slip_systems;i++){
        for(unsigned int j=0;j<n_slip_systems;j++){
            h_alpha_beta_t[i][j] = q[i][j]*h_beta(j);
            A[i][j]=h_alpha_beta_t[i][j];
        }
    }
    
    // Calculate the Stiffness Matrix A
    for(unsigned int i=0;i<n_slip_systems;i++){
        for(unsigned int j=0;j<n_slip_systems;j++){
            temp1.reinit(dim,dim); temp1=0.0;
            
            for(unsigned int k=0;k<dim;k++){
                for(unsigned int l=0;l<dim;l++){
                    temp[k][l]=SCHMID_TENSOR1(dim*j+k,l);
                }
            }
            temp2.reinit(dim,dim); CE_tau_trial.mmult(temp2,temp);
            temp2.symmetrize();
            tempv1=0.0; Dmat.vmult(tempv1, vecform(temp2));
            temp3=0.0; matform(temp3,tempv1);
            
            for(unsigned int k=0;k<dim;k++){
                for(unsigned int l=0;l<dim;l++){
                    if((resolved_shear_tau_trial(i)<0.0)^(resolved_shear_tau_trial(j)<0.0))
                        A[i][j]-=SCHMID_TENSOR1(dim*i+k,l)*temp3[k][l];
                    else
                        A[i][j]+=SCHMID_TENSOR1(dim*i+k,l)*temp3[k][l];
                    
                }
            }
        }
    }
    
    
    // Caluclate the trial Cauchy Stress T_tau and trial PK1 Stress P_tau
    FullMatrix<double> T_tau(dim,dim),P_tau(dim,dim);
    Vector<double> s_alpha_tau;
    FP_tau=FP_t;
    FE_tau.reinit(dim,dim);
    F_tau.mmult(FE_tau,Fpn_inv);
    
    double det_FE_tau,det_F_tau, det_FP_tau;
    det_FE_tau=FE_tau.determinant();
    temp.reinit(dim,dim); FE_tau.mmult(temp,T_star_tau_trial);
    temp.equ(1.0/det_FE_tau,temp); temp.mTmult(T_tau,FE_tau);
    det_F_tau=F_tau.determinant();
    temp=0.0;
    temp.invert(F_tau); T_tau.mTmult(P_tau,temp);
    P_tau.equ(det_F_tau,P_tau);
    s_alpha_tau=s_alpha_t;
    
    double gamma = 0;
    int iter=0,iter1=0,iter2=0,iter3=0;
    Vector<double> active;
    Vector<double> x_beta;
    FullMatrix<double> A_PA;
    
    
    // Determination of active slip systems and shear increments
    if (n_PA > 0){
        
        Vector<double> inactive(n_slip_systems-n_PA); // Set of inactive slip systems
        
        
        inactive_slip_removal(inactive,active,x_beta,n_PA, PA, b, A, A_PA);
        
        // % % % % % STEP 6 % % % % %
        temp.reinit(dim,dim);
        for (unsigned int i=0;i<n_slip_systems;i++){
            for (unsigned int j=0;j<dim;j++){
                for (unsigned int k=0;k<dim;k++){
                    temp[j][k]=SCHMID_TENSOR1[dim*i+j][k];
                }
            }
            
            temp.mmult(temp2,FP_t);
            for (unsigned int j=0;j<dim;j++){
                for (unsigned int k=0;k<dim;k++){
                    if(resolved_shear_tau_trial(i)>0)
                        FP_tau[j][k]=FP_tau[j][k]+x_beta(i)*temp2[j][k];
                    else
                        FP_tau[j][k]=FP_tau[j][k]-x_beta(i)*temp2[j][k];
                }
            }
            
        }
        
        //% % % % % STEP 7 % % % % %
        //%     FP_tau = FP_tau/(det(FP_tau)^(1/3));
        
        det_FP_tau=FP_tau.determinant();
        FP_tau.equ(1.0/pow(det_FP_tau,1.0/3.0),FP_tau);
        
        
        // % % % % % STEP 8 % % % % %
        temp.invert(FP_tau);
        F_tau.mmult(FE_tau,temp);
        FullMatrix<double> Ce_tau(dim,dim),T_star_tau(dim,dim);
        FE_tau.Tmmult(Ce_tau,FE_tau);
        T_star_tau=0.0;
        
        for(unsigned int i=0;i<n_slip_systems;i++){
            for(unsigned int j=0;j<dim;j++){
                for(unsigned int k=0;k<dim;k++){
                    temp[j][k]=SCHMID_TENSOR1(dim*i+j,k);
                }
            }
            
            CE_tau_trial.mmult(temp2,temp);
            temp2.symmetrize();
            tempv1=0.0; Dmat.vmult(tempv1, vecform(temp2));
            matform(temp3,tempv1);
            
            
            for(unsigned int j=0;j<dim;j++){
                for(unsigned int k=0;k<dim;k++){
                    if(resolved_shear_tau_trial(i)>0.0)
                        T_star_tau[j][k]=  T_star_tau[j][k] - x_beta(i)*temp3[j][k];
                    else
                        T_star_tau[j][k]=  T_star_tau[j][k] + x_beta(i)*temp3[j][k];
                }
            }
        }
        
        for (unsigned int i=0;i<6;i++){
            twinfraction_iter[cellID][quadPtID][i]=twinfraction_conv[cellID][quadPtID][i]+x_beta[i+18]/0.129;
        }
        
        for (unsigned int i=0;i<18;i++){
            slipfraction_iter[cellID][quadPtID][i]=slipfraction_conv[cellID][quadPtID][i]+x_beta[i]/0.129;
        }
        
        
        
        T_star_tau.add(1.0,T_star_tau_trial);
        
        // % % % % % STEP 9 % % % % %
        
        temp.reinit(dim,dim);
        det_FE_tau=FE_tau.determinant();
        FE_tau.mmult(temp,T_star_tau); temp.equ(1.0/det_FE_tau,temp);
        temp.mTmult(T_tau,FE_tau);
        
        det_F_tau=F_tau.determinant();
        temp=0.0;
        temp.invert(F_tau); T_tau.mTmult(P_tau,temp);
        P_tau.equ(det_F_tau,P_tau);
        
        double h1=0;
        for(unsigned int i=0;i<n_slip_systems;i++){
            h1=0;
            for(unsigned int j=0;j<n_slip_systems;j++){
                h1=h1+h_alpha_beta_t(i,j)*x_beta(j);
            }
            s_alpha_tau(i)=s_alpha_t(i)+h1;
        }
        
        //% see whether shear resistance is passed
        for(unsigned int i=0;i<n_slip_systems;i++){
            for(unsigned int j=0;j<n_slip_systems;j++){
                temp.reinit(dim,dim);
                for(unsigned int k=0;k<dim;k++){
                    for(unsigned int l=0;l<dim;l++){
                        temp[k][l]=SCHMID_TENSOR1(dim*j+k,l);
                    }
                }
                temp2.reinit(dim,dim); CE_tau_trial.mmult(temp2,temp);
                temp2.symmetrize();
                tempv1.reinit(2*dim); tempv1=0.0;
                Dmat.vmult(tempv1, vecform(temp2));
                temp3.reinit(dim,dim); 	matform(temp3,tempv1);
                temp.reinit(dim,dim);
                
                for(unsigned int k=0;k<dim;k++){
                    
                    for(unsigned int l=0;l<dim;l++){
                        
                        temp[k][l]=SCHMID_TENSOR1(dim*i+k,l);
                    }
                }
                
                //Update the resolved shear stress
                for(unsigned int k=0;k<dim;k++){
                    for(unsigned int l=0;l<dim;l++){
                        if((resolved_shear_tau_trial(i)<0.0)^(resolved_shear_tau_trial(j)<0.0))
                            resolved_shear_tau(i) = resolved_shear_tau(i)+temp[k][l]*temp3[k][l]*x_beta(j);
                        else
                            resolved_shear_tau(i) = resolved_shear_tau(i)-temp[k][l]*temp3[k][l]*x_beta(j);
                    }
                }
                
            }
            
            gamma=gamma+resolved_shear_tau(i)*x_beta(i);
        }
        
        
    }
    
    FullMatrix<double> PK1_Stiff(dim*dim,dim*dim);
    
    tangent_modulus(F_trial, Fpn_inv, SCHMID_TENSOR1,A,A_PA,B,T_tau, PK1_Stiff, active, resolved_shear_tau_trial, x_beta, PA, n_PA,det_F_tau,det_FE_tau );
    
    
    
    temp.reinit(dim,dim);
    temp=0.0;
    T_tau.mTmult(temp,rotmat);
    T_tau=0.0;
    rotmat.mmult(T_tau,temp);
    temp=0.0;
    temp.reinit(dim,dim); P_tau.mTmult(temp,rotmat);
    P_tau=0.0;
    rotmat.mmult(P_tau,temp);
    
    dP_dF=0.0;
    FullMatrix<double> L(dim,dim),mn(dim,dim);
    L=0.0;
    temp1.reinit(dim,dim); temp1=IdentityMatrix(dim);
    rotmat.Tmmult(L,temp1);
    
    // Transform the tangent modulus back to crystal frame
    
    
    for(unsigned int m=0;m<dim;m++){
        for(unsigned int n=0;n<dim;n++){
            for(unsigned int o=0;o<dim;o++){
                for(unsigned int p=0;p<dim;p++){
                    for(unsigned int i=0;i<dim;i++){
                        for(unsigned int j=0;j<dim;j++){
                            for(unsigned int k=0;k<dim;k++){
                                for(unsigned int l=0;l<dim;l++){
                                    dP_dF[m][n][o][p]=dP_dF[m][n][o][p]+PK1_Stiff(dim*i+j,dim*k+l)*L(i,m)*L(j,n)*L(k,o)*L(l,p);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    
    
    
    P.reinit(dim,dim);
    P=P_tau;
    T=T_tau;
    
    
    sres_tau.reinit(n_slip_systems);
    sres_tau = s_alpha_tau;
    
    // Update the history variables
    Fe_iter[cellID][quadPtID]=FE_tau;
    Fp_iter[cellID][quadPtID]=FP_tau;
    s_alpha_iter[cellID][quadPtID]=sres_tau;
    
}

//implementation of the getElementalValues method
template <int dim>
void crystalPlasticity<dim>::getElementalValues(FEValues<dim>& fe_values,
                                                unsigned int dofs_per_cell,
                                                unsigned int num_quad_points,
                                                FullMatrix<double>& elementalJacobian,
                                                Vector<double>&     elementalResidual)
{
    
    //Initialized history variables and pfunction variables if unititialized
    if(initCalled == false){
        init(num_quad_points);
    }
    
    unsigned int cellID = fe_values.get_cell()->user_index();
    std::vector<unsigned int> local_dof_indices(dofs_per_cell);
    Vector<double> Ulocal(dofs_per_cell);
    
    typename DoFHandler<dim>::active_cell_iterator cell(& this->triangulation,
                                                        fe_values.get_cell()->level(),
                                                        fe_values.get_cell()->index(),
                                                        & this->dofHandler);
    cell->set_user_index(fe_values.get_cell()->user_index());
    cell->get_dof_indices (local_dof_indices);
    for(unsigned int i=0; i<dofs_per_cell; i++){
        Ulocal[i] = this->solutionWithGhosts[local_dof_indices[i]];
    }
    
    //local data structures
    FullMatrix<double> K_local(dofs_per_cell,dofs_per_cell),CE_tau(dim,dim),E_tau(dim,dim),temp,temp2,temp3;
    Vector<double> Rlocal (dofs_per_cell);
    K_local = 0.0; Rlocal = 0.0;
    
    
    //loop over quadrature points
    for (unsigned int q=0; q<num_quad_points; ++q){
        //Get deformation gradient
        F=0.0;
        for (unsigned int d=0; d<dofs_per_cell; ++d){
            unsigned int i = fe_values.get_fe().system_to_component_index(d).first;
            for (unsigned int j=0; j<dim; ++j){
                F[i][j]+=Ulocal(d)*fe_values.shape_grad(d, q)[j]; // u_{i,j}= U(d)*N(d)_{,j}, where d is the DOF correonding to the i'th dimension
            }
        }
        for (unsigned int i=0; i<dim; ++i){
            F[i][i]+=1;
        }
        
        
        //Update strain, stress, and tangent for current time step/quadrature point
        calculatePlasticity(cellID, q);
        
        //Fill local residual
        for (unsigned int d=0; d<dofs_per_cell; ++d) {
            unsigned int i = fe_values.get_fe().system_to_component_index(d).first;
            for (unsigned int j = 0; j < dim; j++){
                Rlocal(d) -=  fe_values.shape_grad(d, q)[j]*P[i][j]*fe_values.JxW(q);
                
                
            }
            //if(q==7)
            // this->pcout<<Rlocal(d)<<'\n';
        }
        
        temp.reinit(dim,dim); temp=0.0;
        temp2.reinit(dim,dim); temp2=0.0;
        temp3.reinit(dim,dim); temp3=0.0;
        CE_tau=0.0;
        temp=F;
        F.Tmmult(CE_tau,temp);
        E_tau=CE_tau;
        temp=IdentityMatrix(dim);
        for(unsigned int i=0;i<dim;i++){
            for(unsigned int j=0;j<dim;j++){
                E_tau[i][j] = 0.5*(E_tau[i][j]-temp[i][j]);
                temp2[i][j]=E_tau[i][j]*fe_values.JxW(q);
                temp3[i][j]=T[i][j]*fe_values.JxW(q);
            }
        }
        //cout<<E_tau[0][0]<<"\t"<<T[0][0]<<"\t"<<fe_values.JxW(q)<<"\n";
        local_strain.add(1.0,temp2);
        local_stress.add(1.0,temp3);
        local_microvol=local_microvol+fe_values.JxW(q);
        
        double traceE, traceT,vonmises,eqvstrain;
        FullMatrix<double> deve(dim,dim),devt(dim,dim);
        
        
        traceE=E_tau.trace();
        traceT=T.trace();
        temp=IdentityMatrix(3);
        temp.equ(traceE/3,temp);
        
        deve=E_tau;
        deve.add(-1.0,temp);
        
        temp=IdentityMatrix(3);
        temp.equ(traceT/3,temp);
        
        devt=T;
        devt.add(-1.0,temp);
        
        vonmises= devt.frobenius_norm();
        vonmises=sqrt(3.0/2.0)*vonmises;
        eqvstrain=deve.frobenius_norm();
        eqvstrain=sqrt(2.0/3.0)*eqvstrain;
        
        
        //fill in post processing field values
        
        this->postprocessValues(cellID, q, 0, 0)=vonmises;
        this->postprocessValues(cellID, q, 1, 0)=eqvstrain;
        this->postprocessValues(cellID, q, 2, 0)=quadratureOrientationsMap[cellID][q];
        this->postprocessValues(cellID, q, 3, 0)=twin[cellID][q];
        
        
        
        
        std::cout.precision(3);
        
        //evaluate elemental stiffness matrix, K_{ij} = N_{i,k}*C_{mknl}*F_{im}*F{jn}*N_{j,l} + N_{i,k}*F_{kl}*N_{j,l}*del{ij} dV
        for (unsigned int d1=0; d1<dofs_per_cell; ++d1) {
            unsigned int i = fe_values.get_fe().system_to_component_index(d1).first;
            for (unsigned int d2=0; d2<dofs_per_cell; ++d2) {
                unsigned int j = fe_values.get_fe().system_to_component_index(d2).first;
                for (unsigned int k = 0; k < dim; k++){
                    for (unsigned int l= 0; l< dim; l++){
                        K_local(d1,d2) +=  fe_values.shape_grad(d1, q)[k]*dP_dF[i][k][j][l]*fe_values.shape_grad(d2, q)[l]*fe_values.JxW(q);
                        
                    }
                }
                //if(q==7)
                //this->pcout<<K_local(d1,d2)<<'\t';
            }
            //if(q==7)
            //this->pcout<<'\n';
        }
    }
    elementalJacobian = K_local;
    elementalResidual = Rlocal;
}


template <int dim>
void crystalPlasticity<dim>::updateBeforeIteration()
{
    local_strain=0.0;
    local_stress=0.0;
    local_microvol=0.0;
    
    //call base class project() function to project post processed fields
    //ellipticBVP<dim>::project();
}

template <int dim>
void crystalPlasticity<dim>::updateBeforeIncrement()
{
    local_F_e=0.0;
    local_F_r=0.0;
    F_e=0.0;
    F_r=0.0;
    microvol=0.0;
    //call base class project() function to project post processed fields
    //ellipticBVP<dim>::project();
}





//implementation of the getElementalValues method
template <int dim>
void crystalPlasticity<dim>::updateAfterIncrement()
{
    reorient();
    
    twinfraction_conv=twinfraction_iter;
    slipfraction_conv=slipfraction_iter;
    
    
    //copy rotnew to output
    orientations.outputOrientations.clear();
    QGauss<dim>  quadrature(quadOrder);
    const unsigned int num_quad_points = quadrature.size();
    FEValues<dim> fe_values (this->FE, quadrature, update_quadrature_points | update_JxW_values);
    //loop over elements
    unsigned int cellID=0;
    typename DoFHandler<dim>::active_cell_iterator cell = this->dofHandler.begin_active(), endc = this->dofHandler.end();
    for (; cell!=endc; ++cell) {
        if (cell->is_locally_owned()){
            fe_values.reinit(cell);
            //loop over quadrature points
            for (unsigned int q=0; q<num_quad_points; ++q){
                std::vector<double> temp;
                temp.push_back(fe_values.get_quadrature_points()[q][0]);
                temp.push_back(fe_values.get_quadrature_points()[q][1]);
                temp.push_back(fe_values.get_quadrature_points()[q][2]);
                temp.push_back(rotnew[cellID][q][0]);
                temp.push_back(rotnew[cellID][q][1]);
                temp.push_back(rotnew[cellID][q][2]);
                temp.push_back(fe_values.JxW(q));
                temp.push_back(quadratureOrientationsMap[cellID][q]);
                temp.push_back(slipfraction_conv[cellID][q][0]);
                temp.push_back(slipfraction_conv[cellID][q][1]);
                temp.push_back(slipfraction_conv[cellID][q][2]);
                temp.push_back(slipfraction_conv[cellID][q][3]);
                temp.push_back(slipfraction_conv[cellID][q][4]);
                temp.push_back(slipfraction_conv[cellID][q][5]);
                
                temp.push_back(twinfraction_conv[cellID][q][0]);
                temp.push_back(twinfraction_conv[cellID][q][1]);
                temp.push_back(twinfraction_conv[cellID][q][2]);
                temp.push_back(twinfraction_conv[cellID][q][3]);
                temp.push_back(twinfraction_conv[cellID][q][4]);
                temp.push_back(twinfraction_conv[cellID][q][5]);
                temp.push_back(twin[cellID][q]);
                orientations.addToOutputOrientations(temp);
                local_F_e=local_F_e+twin[cellID][q]*fe_values.JxW(q);
                for(unsigned int i=0;i<6;i++){
                    local_F_r=local_F_r+twinfraction_conv[cellID][q][i]*fe_values.JxW(q);
                }
                
            }
            cellID++;
        }
    }
    orientations.writeOutputOrientations();
    
    //Update the history variables when convergence is reached for the current increment
    Fe_conv=Fe_iter;
    Fp_conv=Fp_iter;
    s_alpha_conv=s_alpha_iter;
    
    //double temp4,temp5;
    //temp4=Lambda[0][0];
    //temp5=Utilities::MPI::sum(temp4,this->mpi_communicator);
    //cout << temp5<<"\n";
    microvol=Utilities::MPI::sum(local_microvol,this->mpi_communicator);
    
    for(unsigned int i=0;i<dim;i++){
        for(unsigned int j=0;j<dim;j++){
            global_strain[i][j]=Utilities::MPI::sum(local_strain[i][j]/microvol,this->mpi_communicator);
            global_stress[i][j]=Utilities::MPI::sum(local_stress[i][j]/microvol,this->mpi_communicator);
        }
        
    }
    F_e=Utilities::MPI::sum(local_F_e/microvol,this->mpi_communicator);
    F_r=Utilities::MPI::sum(local_F_r/microvol,this->mpi_communicator);
    
    
    
    
    ofstream outputFile;
    
    if(this->currentIncrement==0){
        outputFile.open("stressstrain.txt");
        outputFile.close();
    }
    outputFile.open("stressstrain.txt",ios::app);
    if(Utilities::MPI::this_mpi_process(this->mpi_communicator)==0){
        outputFile << global_strain[0][0]<<'\t'<<global_strain[1][1]<<'\t'<<global_strain[2][2]<<'\t'<<global_strain[1][2]<<'\t'<<global_strain[0][2]<<'\t'<<global_strain[0][1]<<'\t'<<global_stress[0][0]<<'\t'<<global_stress[1][1]<<'\t'<<global_stress[2][2]<<'\t'<<global_stress[1][2]<<'\t'<<global_stress[0][2]<<'\t'<<global_stress[0][1]<<'\n';
    }
    outputFile.close();
    global_strain=0.0;
    global_stress=0.0;
    
    
    cellID=0;
    cell = this->dofHandler.begin_active(), endc = this->dofHandler.end();
    for (; cell!=endc; ++cell) {
        if (cell->is_locally_owned()){
            fe_values.reinit(cell);
            //loop over quadrature points
            for (unsigned int q=0; q<num_quad_points; ++q){
                std::vector<double> local_twin;
                local_twin.resize(6,0.0);
                local_twin=twinfraction_conv[cellID][q];
                std::vector<double>::iterator result;
                result = std::max_element(local_twin.begin(), local_twin.end());
                double twin_pos, twin_max;
                twin_pos= std::distance(local_twin.begin(), result);
                twin_max=local_twin[twin_pos];
                
                if(twin_max>=(0.25+0.25*F_e/F_r)){
                    
                    FullMatrix<double> FE_t(dim,dim), FP_t(dim,dim),Twin_T(dim,dim),temp(dim,dim);
                    
                    FE_t=Fe_conv[cellID][q];
                    FP_t=Fp_conv[cellID][q];
                    
                    
                    Twin_image(twin_pos,cellID,q);
                    double s_alpha_twin=s_alpha_conv[cellID][q][18+twin_pos];
                    for(unsigned int i=0;i<6;i++){
                        twinfraction_conv[cellID][q][i]=0;
                        s_alpha_conv[cellID][q][18+twin_pos]=s_alpha_twin;
                        
                    }
                    
                    Vector<double> n(dim);
                    n(0)=n_alpha[18+twin_pos][0];
                    n(1)=n_alpha[18+twin_pos][1];
                    n(2)=n_alpha[18+twin_pos][2];
                    
                    for(unsigned int i=0;i<dim;i++){
                        for(unsigned int j=0;j<dim;j++){
                            Twin_T(i,j)=2.0*n(i)*n(j)-1.0*(i==j);
                            
                        }
                        
                    }
                    
                    //this->pcout<<Twin_T(0,0)<<'\t'<<Twin_T(1,1)<<'\t'<<Twin_T(2,2)<<'\t'<<Twin_T(0,1)<<'\n';
                    
                    FE_t.mmult(temp,Twin_T);
                    Twin_T.mmult(FE_t,temp);
                    
                    FP_t.mmult(temp,Twin_T);
                    Twin_T.mmult(FP_t,temp);
                    
                    Fe_conv[cellID][q]=FE_t;
                    Fp_conv[cellID][q]=FP_t;
                    
                    
                    twin[cellID][q]=1.0;
                    
                }
                
                
            }
            cellID++;
        }
    }
    
    
    
    //call base class project() function to project post processed fields
    ellipticBVP<dim>::project();
}



template <int dim>
void crystalPlasticity<dim>::Twin_image(double twin_pos,unsigned int cellID,
                                        unsigned int quadPtID)
{
    Vector<double> quat1(4),rod(3),quat2(4),quatprod(4);
    rod(0) = rot[cellID][quadPtID][0];rod(1) = rot[cellID][quadPtID][1];rod(2) = rot[cellID][quadPtID][2];
    
    rod2quat(quat2,rod);
    
    quat1(0) = 0;quat1(1) = n_alpha[18+twin_pos][0];
    quat1(2) = n_alpha[18+twin_pos][1];quat1(3) = n_alpha[18+twin_pos][2];
    
    
    quatproduct(quatprod,quat2,quat1);
    
    quat2rod(quatprod,rod);
    
    //this->pcout<<rod(0)<<'\t'<<rod(1)<<'\t'<<rod(2)<<'\n';
    
    rot[cellID][quadPtID][0]=rod(0);rot[cellID][quadPtID][1]=rod(1);rot[cellID][quadPtID][2]=rod(2);
    rotnew[cellID][quadPtID][0]=rod(0);rotnew[cellID][quadPtID][1]=rod(1);rotnew[cellID][quadPtID][2]=rod(2);
    
    
}


//------------------------------------------------------------------------
template <int dim>
void crystalPlasticity<dim>::rod2quat(Vector<double> &quat,Vector<double> &rod)
{
    double dotrod = rod(0)*rod(0) + rod(1)*rod(1) + rod(2)*rod(2);
    double cphiby2   = cos(atan(sqrt(dotrod)));
    quat(0) = cphiby2;
    quat(1) = cphiby2* rod(0);
    quat(2) = cphiby2* rod(1);
    quat(3) = cphiby2* rod(2);
}
//------------------------------------------------------------------------
template <int dim>
void crystalPlasticity<dim>::quatproduct(Vector<double> &quatp,Vector<double> &quat2,Vector<double> &quat1)
//R(qp) = R(q2)R(q1)
{
    double a = quat2(0);
    double b = quat1(0);
    double dot1 = quat1(1)*quat2(1) + quat1(2)*quat2(2) + quat1(3)*quat2(3);
    quatp(0) = (a*b) - dot1;
    quatp(1) = a*quat1(1) + b*quat2(1)+ quat2(2)*quat1(3) - quat1(2)*quat2(3);
    quatp(2) = a*quat1(2) + b*quat2(2)- quat2(1)*quat1(3) + quat1(1)*quat2(3);
    quatp(3) = a*quat1(3) + b*quat2(3)+ quat2(1)*quat1(2) - quat1(1)*quat2(2);
    //if (quatp(0) < 0)
    //quatp.mult(-1);
}
//------------------------------------------------------------------------
template <int dim>
void crystalPlasticity<dim>::quat2rod(Vector<double> &quat,Vector<double> &rod)
{
    double invquat1 = 1/quat(0);
    
    for (int i = 0;i <= 2;i++)
        rod(i) = quat(i+1)*invquat1;
    
}


#include "rotationOperations.cc"
#include "init.cc"
#include "matrixOperations.cc"
#include "tangentModulus.cc"
#include "inactiveSlipRemoval.cc"
#include "reorient.cc"
#include "loadOrientations.cc"

#endif