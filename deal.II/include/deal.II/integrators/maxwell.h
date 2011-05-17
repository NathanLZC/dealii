//---------------------------------------------------------------------------
//    $Id$
//
//    Copyright (C) 2010, 2011 by the deal.II authors
//
//    This file is subject to QPL and may not be  distributed
//    without copyright and license information. Please refer
//    to the file deal.II/doc/license.html for the  text  and
//    further information on this license.
//
//---------------------------------------------------------------------------
#ifndef __deal2__integrators_maxwell_h
#define __deal2__integrators_maxwell_h


#include <deal.II/base/config.h>
#include <deal.II/base/exceptions.h>
#include <deal.II/base/quadrature.h>
#include <deal.II/lac/full_matrix.h>
#include <deal.II/fe/mapping.h>
#include <deal.II/fe/fe_values.h>
#include <deal.II/numerics/mesh_worker_info.h>

DEAL_II_NAMESPACE_OPEN

namespace LocalIntegrators
{
/**
 * @brief Local integrators related to curl operators and their
 * traces.
 *
 * We use the following conventions for curl
 * operators. First, in three space dimensions
 *
 * @f[
 * \nabla\times \mathbf u = \begin{pmatrix}
 *   \partial_3 u_2 - \partial_2 u_3 \\
 *   \partial_1 u_3 - \partial_3 u_1 \\
 *   \partial_2 u_1 - \partial_1 u_2
 * \end{pmatrix}
 * @f]
 *
 * In two space dimensions, the curl is obtained by extending a vector
 * <b>u</b> to $(u_1, u_2, 0)^T$ and a scalar <i>p</i> to $(0,0,p)^T$.
 * Computing the nonzero components, we obtain the scalar
 * curl of a vector function and the vector curl of a scalar
 * function. The current implementation exchanges the sign and we have:
 * @f[
 *  \nabla \times \mathbf u = \partial_1 u_2 - \partial 2 u_1
 *  \qquad
 *  \nabla \times p = \begin{pmatrix}
 *    \partial_2 p \\ -\partial_1 p
 *  \end{pmatrix}
 * @f]
 *
 * @ingroup Integrators
 * @author Guido Kanschat
 * @date 2010
 */
  namespace Maxwell
  {
/**
 * Auxiliary function. Given the tensors of <tt>dim</tt> second derivatives,
 * compute the curl of the curl of a vector function. The result in
 * two and three dimensions is:
 * @f[
 * \nabla\times\nabla\times \mathbf u = \begin{pmatrix}
 * \partial_1\partial_2 u_2 - \partial_2^2 u_1 \\
 * \partial_1\partial_2 u_1 - \partial_1^2 u_2
 * \end{pmatrix}
 *
 * \nabla\times\nabla\times \mathbf u = \begin{pmatrix}
 * \partial_1\partial_2 u_2 + \partial_1\partial_3 u_3
 * - (\partial_2^2+\partial_3^2) u_1 \\
 * \partial_2\partial_3 u_3 + \partial_2\partial_1 u_1
 * - (\partial_3^2+\partial_1^2) u_2 \\
 * \partial_3\partial_1 u_1 + \partial_3\partial_2 u_2
 * - (\partial_1^2+\partial_2^2) u_3
 * \end{pmatrix}
 * @f]
 *
 * @note The third tensor argument is not used in two dimensions and
 * can for instance duplicate one of the previous.
 *
 * @author Guido Kanschat
 * @date 2011
 */
    template <int dim>
    Tensor<1,dim>
    curl_curl (
      const Tensor<2,dim>& h0,
      const Tensor<2,dim>& h1,
      const Tensor<2,dim>& h2)
    {
      Tensor<1,dim> result;
      switch (dim)
	{
	  case 2:
		result[0] = h1[0][1]-h0[1][1];
		result[1] = h0[0][1]-h1[0][0];
		break;
	  case 3:
		result[0] = h1[0][1]+h2[0][2]-h0[1][1]-h0[2][2];
		result[1] = h2[1][2]+h0[1][0]-h1[2][2]-h1[0][0];
		result[2] = h0[2][0]+h1[2][1]-h2[0][0]-h2[1][1];
		break;
	  default:
		Assert(false, ExcNotImplemented());
	}
      return result;
    }

/**
 * Auxiliary function. Given <tt>dim</tt> tensors of first
 * derivatives and a normal vector, compute the tangential curl
 * @f[
 * \mathbf n \times \nabla \times u.
 * @f]
 *
 * @note The third tensor argument is not used in two dimensions and
 * can for instance duplicate one of the previous.
 *
 * @author Guido Kanschat
 * @date 2011
 */
    template <int dim>
    Tensor<1,dim>
    tangential_curl (
      const Tensor<1,dim>& g0,
      const Tensor<1,dim>& g1,
      const Tensor<1,dim>& g2,
      const Tensor<1,dim>& normal)
    {
      Tensor<1,dim> result;
      
      switch (dim)
	{
	  case 2:
		result[0] = normal[1] * (g1[0]-g0[1]);
		result[1] =-normal[0] * (g1[0]-g0[1]);
		break;
	  case 3:
		result[0] = normal[2]*(g2[1]-g0[2])+normal[1]*(g1[0]-g0[1]);
		result[1] = normal[0]*(g0[2]-g1[0])+normal[2]*(g2[1]-g1[2]);
		result[2] = normal[1]*(g1[0]-g2[1])+normal[0]*(g0[2]-g2[0]);
		break;
	  default:
		Assert(false, ExcNotImplemented());
	}
      return result;
    }
    
/**
 * The curl-curl operator
 * @f[
 * \int_Z \nabla\!\times\! u \cdot
 * \nabla\!\times\! v \,dx
 * @f]
 *
 * @ingroup Integrators
 * @author Guido Kanschat
 * @date 2011
 */
    template <int dim>
    void curl_curl_matrix (
      FullMatrix<double>& M,
      const FEValuesBase<dim>& fe,
      const double factor = 1.)
    {
      const unsigned int n_dofs = fe.dofs_per_cell;
      
      AssertDimension(fe.get_fe().n_components(), dim);
      AssertDimension(M.m(), n_dofs);
      AssertDimension(M.n(), n_dofs);
      
				       // Depending on the dimension,
				       // the cross product is either
				       // a scalar (2d) or a vector
				       // (3d). Accordingly, in the
				       // latter case we have to sum
				       // up three bilinear forms, but
				       // in 2d, we don't. Thus, we
				       // need to adapt the loop over
				       // all dimensions
      const unsigned int d_max = (dim==2) ? 1 : dim;
      
      for (unsigned k=0;k<fe.n_quadrature_points;++k)
	{
	  const double dx = factor * fe.JxW(k);
	  for (unsigned i=0;i<n_dofs;++i)
	    for (unsigned j=0;j<n_dofs;++j)
	      for (unsigned int d=0;d<d_max;++d)
		{
		  const unsigned int d1 = (d+1)%dim;
		  const unsigned int d2 = (d+2)%dim;
		  
		  const double cv = fe.shape_grad_component(i,k,d1)[d2] - fe.shape_grad_component(i,k,d2)[d1];
		  const double cu = fe.shape_grad_component(j,k,d1)[d2] - fe.shape_grad_component(j,k,d2)[d1];
		  
		  M(i,j) += dx * cu * cv;
		}
	}
    }
    
/**
 * The curl operator
 * @f[
 * \int_Z \nabla\!\times\! u \cdot v \,dx.
 * @f]
 *
 * This is the standard curl operator in 3D and the scalar curl in 2D.
  *
 * @ingroup Integrators
 * @author Guido Kanschat
 * @date 2011
*/
    template <int dim>
    void curl_matrix (
      FullMatrix<double>& M,
      const FEValuesBase<dim>& fe,
      const FEValuesBase<dim>& fetest,
      double factor = 1.)
    {
      unsigned int t_comp = (dim==3) ? dim : 1;
      const unsigned int n_dofs = fe.dofs_per_cell;
      const unsigned int t_dofs = fetest.dofs_per_cell;
      AssertDimension(fe.get_fe().n_components(), dim);
      AssertDimension(fetest.get_fe().n_components(), t_comp);
      AssertDimension(M.m(), t_dofs);
      AssertDimension(M.n(), n_dofs);
      
      const unsigned int d_max = (dim==2) ? 1 : dim;
      
      for (unsigned k=0;k<fe.n_quadrature_points;++k)
	{
	  const double dx = fe.JxW(k) * factor;
	  for (unsigned i=0;i<t_dofs;++i)
	    for (unsigned j=0;j<n_dofs;++j)
	      for (unsigned int d=0;d<d_max;++d)
		{
		  const unsigned int d1 = (d+1)%dim;
		  const unsigned int d2 = (d+2)%dim;
		  
		  const double vv = fetest.shape_value_component(i,k,d);
		  const double cu = fe.shape_grad_component(j,k,d1)[d2] - fe.shape_grad_component(j,k,d2)[d1];
		  M(i,j) += dx * cu * vv;
		}
	}
    }
  }
}


DEAL_II_NAMESPACE_CLOSE

#endif
