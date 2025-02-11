// ---------------------------------------------------------------------
//
// Copyright (C) 2018 by the deal.II authors
//
// This file is part of the deal.II library.
//
// The deal.II library is free software; you can use it, redistribute
// it, and/or modify it under the terms of the GNU Lesser General
// Public License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
// The full text of the license can be found in the file LICENSE.md at
// the top level directory of deal.II.
//
// ---------------------------------------------------------------------



// this function tests the correctness of the implementation of matrix free
// matrix-vector products by comparing with the result of deal.II sparse
// matrix. The mesh uses a hypercube mesh with hanging nodes (created by
// randomly refining some cells) and zero Dirichlet conditions.

#include <deal.II/base/function.h>

#include "../tests.h"

#include "matrix_vector_common.h"

template <int dim, int fe_degree, typename Number>
void
test()
{
  Triangulation<dim> tria;
  GridGenerator::hyper_cube(tria);
  typename Triangulation<dim>::active_cell_iterator cell = tria.begin_active(),
                                                    endc = tria.end();
  if (dim < 3 || fe_degree < 2)
    tria.refine_global(2);
  else
    tria.refine_global(1);
  tria.begin(tria.n_levels() - 1)->set_refine_flag();
  tria.last()->set_refine_flag();
  tria.execute_coarsening_and_refinement();
  for (unsigned int i = 0; i < 10 - 3 * dim; ++i)
    {
      cell                 = tria.begin_active();
      unsigned int counter = 0;
      for (; cell != endc; ++cell, ++counter)
        if (counter % (7 - i) == 0)
          cell->set_refine_flag();
      tria.execute_coarsening_and_refinement();
    }

  FE_Q<dim>       fe(fe_degree);
  DoFHandler<dim> dof(tria);
  dof.distribute_dofs(fe);
  AffineConstraints<Number> constraints;
  DoFTools::make_hanging_node_constraints(dof, constraints);

  VectorTools::interpolate_boundary_values(dof,
                                           0,
                                           Functions::ZeroFunction<dim>(),
                                           constraints);
  constraints.close();

  do_test<dim,
          fe_degree,
          Number,
          LinearAlgebra::CUDAWrappers::Vector<Number>,
          fe_degree + 1>(dof, constraints, tria.n_active_cells());
}
