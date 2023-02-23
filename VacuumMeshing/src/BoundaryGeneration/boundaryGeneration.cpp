

#include "BoundaryGeneration/boundaryGeneration.hpp"

void generateCoilBoundary(libMesh::Mesh &mesh, Eigen::MatrixXd &boundVerts,
                          Eigen::MatrixXi &boundElems, double length,
                          int subdivisions, std::string triSettings) {
  // Libmesh mesh that stores only sideset data
  libMesh::Mesh sidesetMesh(mesh.comm());

  libMesh::Mesh sidesetMeshSkinned(mesh.comm());
  //
  genSidesetMesh(mesh, sidesetMesh);

  // Skin the sideset mesh to get the edge elements that represent the
  // boundaries of the sidesets
  getSurface(sidesetMesh, sidesetMeshSkinned);
  // sidesetMeshSkinned.write("AreYouSecondOrder.e");
  // Create Eigen 3x3 matrix to store 3 points from the co-planar sidesets, that
  // will be used to
  //  define the plane that they sit on
  Eigen::Matrix3d planePoints;

  // Populate our plane points matrix with the nodes belonging to an element on
  // the sidesets
  for (int row_id = 0; row_id < 3; row_id++) {
    auto &node = sidesetMesh.elem_ref(0).node_ref(row_id);
    planePoints.row(row_id) << node(0), node(1), node(2);
  }
  // Eigen 3x3 Matrix that will store the basis vectors of our transformed
  // cartesian system
  Eigen::Matrix3d basisMatrix;

  // Define new origin as a point on the coil sideset(s), this will be fairly
  // central
  Eigen::Vector3d origin = planePoints.row(0);
  auto start1 = std::chrono::steady_clock::now();
  // Generate basis matrix based on 3 points that define a plane
  getBasisMatrix(basisMatrix, planePoints);

  Eigen::MatrixXd verts, vBoundary;
  Eigen::MatrixXi F, fBoundary;

  // Convert the sideset mesh to
  libMeshToIGL(sidesetMeshSkinned, verts, F);

  libMesh::Mesh test(mesh.comm());

  changeMeshBasis(verts, origin, basisMatrix);

  // Define seed points matrix
  Eigen::MatrixXd seed_points(2, 3);

  // Get the seed points of the coplanar coil boundaries
  getCoplanarSeedPoints(mesh, seed_points);

  // Transform seed points into new coordinate system
  for (int i = 0; i < seed_points.rows(); i++) {
    Eigen::Vector3d point(seed_points.row(i));
    seed_points.row(i) = calculateLocalCoords(point, origin, basisMatrix);
  }

  generateCoilFaceBound(verts, F, seed_points, boundVerts, boundElems, length,
                        subdivisions, triSettings, test);

  // IGLToLibMesh(test, bound_verts, bound_elems);
  // test.write("TopSurfSO.e");

  const double tol = 1e-05;
  //
  genRemainingBoundary(vBoundary, fBoundary, length, subdivisions, triSettings,
                       tol);

  combineMeshes(tol, boundVerts, boundElems, vBoundary, fBoundary);

  // Change back to original basis
  changeMeshBasis(boundVerts, {0, 0, 0}, Eigen::Matrix3d::Identity(), origin,
                  basisMatrix);
}

Eigen::Vector3d calculateLocalCoords(Eigen::Vector3d &point,
                                     Eigen::Vector3d newOrigin,
                                     Eigen::Matrix3d newBasis,
                                     Eigen::Vector3d oldOrigin,
                                     Eigen::Matrix3d oldBasis) {
  Eigen::Vector3d localCoords =
      newBasis.inverse() * (oldOrigin - newOrigin + (oldBasis * point));
  return localCoords;
}

void changeMeshBasis(libMesh::Mesh &mesh, Eigen::Vector3d newOrigin,
                     Eigen::Matrix3d newBasis, Eigen::Vector3d oldOrigin,
                     Eigen::Matrix3d oldBasis) {
  libMesh::Mesh meshCopy(mesh);
  mesh.clear();

  for (auto &node : meshCopy.node_ptr_range()) {
    // Eigen::Vector to store node coords
    Eigen::Vector3d point, newPoint;

    for (u_int i = 0; i < 3; i++) {
      point(i) = (*node)(i);
    }

    newPoint =
        calculateLocalCoords(point, newOrigin, newBasis, oldOrigin, oldBasis);
    // newPoint = calculateLocalCoords(basisMatrix, origin, origin);
    // std::cout << newPoint.transpose() << std::endl;

    double pnt[3];
    for (u_int i = 0; i < 3; i++) {
      pnt[i] = newPoint(i);
    }

    libMesh::Point xyz(pnt[0], pnt[1], pnt[2]);
    // std::cout << pnt[0] << " " << pnt[1] << " " << pnt[2] << std::endl;
    mesh.add_point(xyz, node->id());
  }

  for (auto &elem : meshCopy.element_ptr_range()) {
    libMesh::Elem *new_elem = libMesh::Elem::build(elem->type()).release();
    for (int j = 0; j < elem->n_nodes(); j++) {
      new_elem->set_node(j) = mesh.node_ptr(elem->node_ref(j).id());
    }
    mesh.add_elem(new_elem);
  }
  mesh.prepare_for_use();
}

// Change mesh basis for a libIGL (Eigen) mesh
void changeMeshBasis(Eigen::MatrixXd &verts, Eigen::Vector3d newOrigin,
                     Eigen::Matrix3d newBasis, Eigen::Vector3d oldOrigin,
                     Eigen::Matrix3d oldBasis) {
  // Inverse the new basis matrix
  Eigen::Matrix3d newBasisInverse = newBasis.inverse();
  // We assume we are changing basis from bog standard cartesian coords.
  //  i.e. an origin of 0,0,0 and basis vectors of (1,0,0), (0,1,0), (0,0,1)
  // Eigen::Matrix3d oldBasis(Eigen::Matrix3d::Identity());
  // Eigen::Vector3d oldOrigin(Eigen::Vector3d::Zero());
  // Do row wise operations on
  for (int i = 0; i < verts.rows(); i++) {
    verts.row(i) = newBasisInverse *
               (oldOrigin - newOrigin + (oldBasis * verts.row(i).transpose()));
  }
}

void translateMesh(Eigen::MatrixXd &verts, Eigen::Vector3d translationVector) {
  for (int i = 0; i < verts.rows(); i++) {
    verts.row(i) += translationVector;
  }
}

bool getBasisMatrix(Eigen::Matrix3d &basisMatrix,
                    Eigen::Matrix3d &planePoints) {
  // Basis vectors X Y and Z
  Eigen::Vector3d X, Y, Z;

  // Vectors between the 3 points that will define our plane
  Eigen::Vector3d AB = (planePoints.row(1) - planePoints.row(0));
  Eigen::Vector3d AC = (planePoints.row(2) - planePoints.row(0));

  // Normalise basis vectors so that their magnitude is one, relative to
  // original basis
  X = AB.normalized();
  Z = (AC.cross(AB)).normalized();
  Y = (X.cross(Z)).normalized();
  // Add our basis vectors to 'basisMatrix' matrix
  basisMatrix.col(0) << X;
  basisMatrix.col(1) << Y;
  basisMatrix.col(2) << Z;

  return true;
}

void generateCoilFaceBound(Eigen::MatrixXd &verts, Eigen::MatrixXi &elems,
                           Eigen::MatrixXd &holes, Eigen::MatrixXd &triVerts,
                           Eigen::MatrixXi &triElems, double length,
                           int subdivisions, std::string triSettings,
                           libMesh::Mesh &test) {
  Eigen::MatrixXd vacencies = holes.block(0, 0, 2, 2);
  Eigen::MatrixXd verts2D = verts.block(0, 0, verts.rows(), 2);
  Eigen::MatrixXi elems2D = elems;

  // Eigen::MatrixXi elems2D = elems.block(0,0,elems.rows(), 2);
  genSidesetBounds(verts2D, elems2D, length, subdivisions);

  // IGLToLibMesh(test, verts2D, elems2D);

  // std::cout << "writing" << std::endl;
  // test.write("IGltranstestSO.e");

  igl::triangle::triangulate(verts2D, elems2D, vacencies, triSettings, triVerts, triElems);

  // Resize triangle vertices matrix to have 3 cols instead of 2 (2D -> 3D), set
  // all z coords to 0
  triVerts.conservativeResize(triVerts.rows(), triVerts.cols() + 1);
  triVerts.col(triVerts.cols() - 1) = Eigen::VectorXd::Zero(triVerts.rows());
}

void generateCoilFaceBound(libMesh::Mesh &mesh, libMesh::Mesh &outputMesh,
                           libMesh::Mesh &remainingBoundary,
                           Eigen::MatrixXd &holes, double maxElemSize) {
  std::string tri_args = "qYa" + std::to_string(maxElemSize);
  Eigen::MatrixXd verts, newVerts;
  Eigen::MatrixXi elems, newElems;
  genSidesetBounds(mesh, remainingBoundary);
  libMeshToIGL(mesh, verts, elems, 2);
  igl::triangle::triangulate(verts, elems, holes, tri_args, newVerts, newElems);
  IGLToLibMesh(outputMesh, newVerts, newElems);
  combineMeshes(1e-05, outputMesh, remainingBoundary);
}

void genSidesetMesh(libMesh::Mesh &mesh, libMesh::Mesh &sidesetMesh,
                    std::vector<std::string> ssNames) {
  std::set<libMesh::boundary_id_type> ids;
  // Insert the sideset id's that correspond to the given ssNames into a set
  for (auto &ssName : ssNames) {
    ids.insert(mesh.get_boundary_info().get_id_by_name(ssName));
  }
  // Get sideset boundary
  mesh.get_boundary_info().sync(ids, sidesetMesh);
}

void genSidesetBounds(Eigen::MatrixXd &verts, Eigen::MatrixXi &elems, double length,
                      int subdivisions) {
  Eigen::MatrixXd squareVerts((4 * subdivisions), 2);
  Eigen::MatrixXi squareElems((4 * subdivisions), 2);

  genSquare(squareVerts, squareElems, length, subdivisions);

  combineIGLMeshes(verts, elems, squareVerts, squareElems);
}

void genSidesetBounds(libMesh::Mesh &sidesetMesh,
                      libMesh::Mesh &remainingBoundary) {

  std::unordered_map<u_int, libMesh::dof_id_type> id_map;
  libMesh::Mesh skinnedBound(sidesetMesh.comm());
  getSurface(remainingBoundary, skinnedBound);

  // Starting node ID for points
  libMesh::dof_id_type startingNode = sidesetMesh.max_node_id() + 1;
  libMesh::dof_id_type startingElem = sidesetMesh.max_elem_id() + 1;

  for (auto &node : skinnedBound.node_ptr_range()) {
    double pnt[2];
    for (u_int i = 0; i < 2; i++) {
      pnt[i] = (*node)(i);
    }
    libMesh::Point xyz(pnt[0], pnt[1]);
    id_map[node->id()] = startingNode;
    sidesetMesh.add_point(xyz, startingNode);
    startingNode++;
  }

  for (auto &elem : skinnedBound.element_ptr_range()) {
    libMesh::Elem *newElem = libMesh::Elem::build(elem->type()).release();
    for (int j = 0; j < elem->n_nodes(); j++) {
      // std::cout << id_map[elem->node_id(j)] << std::endl;
      newElem->set_node(j) = sidesetMesh.node_ptr(id_map[elem->node_id(j)]);
    }
    newElem->set_id(startingElem++);
    sidesetMesh.add_elem(newElem);
  }

  sidesetMesh.prepare_for_use();
}

void getCoplanarSeedPoints(libMesh::Mesh &mesh, Eigen::MatrixXd &seedPoints,
                           std::string ss1Name, std::string ss2Name) {
  // generate separate meshes for two coil sidesets
  std::set<libMesh::boundary_id_type> ss1_id, ss2_id;
  ss1_id.insert(mesh.boundary_info->get_id_by_name(ss1Name));
  ss2_id.insert(mesh.boundary_info->get_id_by_name(ss2Name));
  libMesh::Mesh ss1(mesh.comm());
  libMesh::Mesh ss2(mesh.comm());
  mesh.boundary_info->sync(ss1_id, ss1);
  mesh.boundary_info->sync(ss2_id, ss2);

  libMesh::Point centre1, centre2;

  // Create a bounding box around these 2D sidesets to figure out where a
  // seeding point should be placed
  auto box1 = libMesh::MeshTools::create_bounding_box(ss1);
  auto box2 = libMesh::MeshTools::create_bounding_box(ss2);

  centre1 = (box1.max() + box1.min()) / 2;
  centre2 = (box2.max() + box2.min()) / 2;
  //
  for (u_int i = 0; i < 3; i++) {
    seedPoints.row(0)(i) = centre1(i);
    seedPoints.row(1)(i) = centre2(i);
  }
}

void genSquare(Eigen::MatrixXd &verts, Eigen::MatrixXi &elems, double length,
               int subdivisions) {
  // Add second order capability!

  //
  double increment = (double)(length / subdivisions);
  // Offset so that the square mesh is symmetric about the x and y axes
  double offset = length / 2;

  long int row_num = 0;
  for (int i = 0; i <= subdivisions; i++) {
    verts.row(row_num) << -offset + (i * increment), -offset;
    row_num++;
  }

  for (int i = 1; i <= subdivisions; i++) {
    verts.row(row_num) << offset, (i * increment) - offset;
    row_num++;
  }

  for (int i = 1; i <= subdivisions; i++) {
    verts.row(row_num) << offset - (i * increment), offset;
    row_num++;
  }

  for (int i = 1; i <= subdivisions; i++) {
    verts.row(row_num) << -offset, offset - (i * increment);
    row_num++;
  }

  for (int i = 0; i < (4 * subdivisions) - 1; i++) {
    elems(i, 0) = i;
    elems(i, 1) = i + 1;
  }
  elems(((4 * subdivisions) - 1), 0) = 0;
  elems(((4 * subdivisions) - 1), 1) = (4 * subdivisions) - 1;
}

void combineIGLMeshes(Eigen::MatrixXd &vertsOne, Eigen::MatrixXi &elemsOne,
                      Eigen::MatrixXd &vertsTwo, Eigen::MatrixXi &elemsTwo) {
  if (vertsOne.cols() != vertsTwo.cols()) {
    throw std::invalid_argument(
        "combineIGLMeshes can only combined meshes of the same dimension");
    abort;
  }
  // if(elemsOne.cols() != elemsTwo.cols())
  // {
  //     throw std::invalid_argument("can only combine meshes of the same
  //     order"); abort;
  // }
  // Store rows in vertsOne before we resize it
  int initial_node_rows = vertsOne.rows();
  int initial_elem_rows = elemsOne.rows();
  // Resize vertsOne to store info from vertsTwo
  vertsOne.conservativeResize(vertsOne.rows() + vertsTwo.rows(), vertsOne.cols());
  elemsOne.conservativeResize(elemsOne.rows() + elemsTwo.rows(), elemsOne.cols());

  for (int i = 0; i < vertsTwo.rows(); i++) {
    vertsOne.row(initial_node_rows + i) = vertsTwo.row(i);
  }

  for (int i = 0; i < elemsTwo.rows(); i++) {
    elemsOne.row(initial_elem_rows + i) =
        elemsTwo.row(i) + Eigen::VectorXi::Constant(elemsOne.cols(), initial_node_rows);
  }
}

void genRemainingBoundary(Eigen::MatrixXd &triVerts, Eigen::MatrixXi &triElems,
                          double length, int subdivisions,
                          std::string triSettings, double tol) {
  Eigen::MatrixXd verts(4 * subdivisions, 2);
  Eigen::MatrixXi F(4 * subdivisions, 2);

  triVerts(4 * subdivisions, 2);
  triElems(4 * subdivisions, 3);

  Eigen::MatrixXd seeds;

  genSquare(verts, F, length, subdivisions);

  igl::triangle::triangulate(verts, F, seeds, triSettings, triVerts, triElems);

  // Rotational matrices to rotate elements
  Eigen::Matrix3d x_rot_base;
  x_rot_base << 1, 0, 0, 0, 0, -1, 0, 1, 0;

  Eigen::Matrix3d y_rot_base;
  y_rot_base << 0, 0, -1, 0, 1, 0, 1, 0, 0;

  // Resize triVerts to work in 3d, as it is currently a 2D mesh,
  //  this is done just by adding a column of zeroes as our "z" coord
  triVerts.conservativeResize(triVerts.rows(), triVerts.cols() + 1);
  triVerts.col(triVerts.cols() - 1) = Eigen::VectorXd::Zero(triVerts.rows());

  // Define our rotation matrices
  std::vector<Eigen::Matrix3d> rot_matrices = {x_rot_base, y_rot_base};
  std::vector<Eigen::MatrixXd> newFaces(4, triVerts);

  triVerts.col(triVerts.cols() - 1) = Eigen::VectorXd::Constant(triVerts.rows(), length);
  // triElems.conservativeResize(triElems.rows(), triElems.cols()+1);
  // triElems.col(triElems.cols()-1) = Eigen::VectorXi::Zero(triElems.rows());

  for (int i = 0; i < 4; i++) {
    newFaces[i] *= rot_matrices[(i % 2)];
  }

  translateMesh(newFaces[0], {0, length / 2, length / 2});
  translateMesh(newFaces[1], {length / 2, 0, length / 2});
  translateMesh(newFaces[2], {0, -length / 2, length / 2});
  translateMesh(newFaces[3], {-length / 2, 0, length / 2});

  Eigen::MatrixXi triElems_2 = triElems;

  for (int i = 0; i < 4; i++) {
    combineMeshes(tol, triVerts, triElems, newFaces[i], triElems_2);
  }
}
