/** \file
 * As part of the process of vacuum mesh generation, the original part must be
 * "skinned". Methods pertaining to mesh skinning are found here, along with a
 * few other potentially useful methods.
 */

#pragma once
#include "Utils/getElemInfo.hpp"
#include "libmesh/boundary_info.h"
#include "libmesh/elem.h"
#include "libmesh/enum_elem_type.h"
#include "libmesh/enum_io_package.h"
#include "libmesh/libmesh.h"
#include "libmesh/mesh.h"
#include "libmesh/mesh_tools.h"

// using namespace libMesh;

/**
 * Method to retrieve the "skin" of an input \p mesh, and store it in \p
 * surfaceMesh. \p surfaceFaceMap is a multimap which maps from element id's in
 * the original mesh to the face id's of that element that lie on the skin. This
 * information can be used later to define a sideset in the final mesh that
 * represents the elements of the skin. This can be useful for defining
 * radiative boundary conditions or something of that ilk. \p setting writeMesh
 * to true will write the surfaceMesh out with a given \p outputFilename, which
 * by default is "surface_mesh.e"
 */
void getSurface(libMesh::Mesh &mesh, libMesh::Mesh &surfaceMesh,
                std::multimap<unsigned int, unsigned int> &surfaceFaceMap,
                bool writeMesh = false,
                std::string outputFilename = "surface_mesh.e");

/**
 * Method to retrieve the "skin" of an input \p mesh, and store it in \p
 * surfaceMesh. \p setting writeMesh to true will write the surfaceMesh out with
 * a given \p outputFilename, which by default is "surface_mesh.e".
 */
void getSurface(libMesh::Mesh &mesh, libMesh::Mesh &surfaceMesh,
                bool writeMesh = false,
                std::string outputFilename = "surface_mesh.e");

/**
 * Method to retrieve the "skin" of a subest of the input \p mesh, and store it
 * in \p surfaceMesh. This subset is defined by \p elSet, a set of element id's
 * in \p mesh that you want the skin of. \p setting writeMesh to true will write
 * the surfaceMesh out with a given \p outputFilename, which by default is
 * "surface_mesh.e".
 */
void getSurface(libMesh::Mesh &mesh, libMesh::Mesh &surfaceMesh,
                std::vector<int> &elSet, bool writeMesh = false,
                std::string outputFilename = "surface_mesh.e");

/**
 *
 *
 */
int getNumSurfaceElems();

/** Method for checking whether an element has sides which should be in the
 * skin. Looks at the sides (faces or edges, depends if 2D or 3D element) of \p
 * element, and checks whether they are null. If they are, this side of \p
 * element must lie on the skin. If a side does lie of the skin then it's local
 * ID is stored in surfaceFaces.
 */
void isElementSurface(libMesh::Elem *element, std::vector<int> &surfaceFaces);

/** Method for checking whether an element has sides which should be in the
 * skin. This method is used for when you are skinning a subset of the mesh. The
 * sides of \p element are checked first to see if their neighbor element is
 * null. If null, then this side must lie on the skin, If not, then we check if
 * the neighbor element ID exists within the vector elSet. If it does, then this
 * element does not lie on the skin are is not stored in \p surfaceFaces If it
 * does NOT, then it's neighboring element is not part of the subset of the mesh
 * we are trying to skin. Henceforth it must lie on the skin of the mesh, and
 * this side's ID is stored in surfaceFaces.
 */
void isElementSurface(libMesh::Elem *element, std::vector<int> &elSet,
                      std::vector<int> &surfaceFaces);

/** Method for grouping a discontinuous mesh into its continuous chunks.
 */
void groupElems(libMesh::Mesh mesh,
                std::vector<std::vector<libMesh::dof_id_type>> &groups);

/** Method for checking whether an element has sides which should be in the
 * skin.
 */
void saveGroupedElems(libMesh::LibMeshInit &init, libMesh::Mesh &surfaceMesh,
                      std::vector<std::vector<libMesh::dof_id_type>> &groups,
                      std::string componentFilename);