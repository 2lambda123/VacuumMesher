#include "Tetrahedralisation/removeDupeNodes.hpp"

// Global hits
std::vector<int> hits;

// callback function for rtree
bool callback(int id) {  
  hits.push_back(id);
  return true;
}

void 
createTree(RTree<int, double, 3, float> &rtree, libMesh::Mesh& meshOne, const double tol)
{
    // Add all existing verts to rTree
    for(auto& node: meshOne.local_node_ptr_range())
    {
        std::array node_coords = {(*node)(0), (*node)(1), (*node)(2)};
        Box node_box = Box(node_coords, tol, node->id());
        insertNode(rtree, node_box);
    }
}

void 
insertNode(RTree<int, double, 3, float> &rtree, Box& node_box)
{
    // Check if node exists in rTree, if not, insert, if it does, make sure connectivity still works
    rtree.Insert(node_box.min.data(), node_box.max.data(), node_box.node_id);
}

bool
searchTree(RTree<int, double, 3, float> &rtree, 
           const double tol,
           std::map<unsigned int, unsigned int>& id_map,
           libMesh::Mesh& meshOne,
           libMesh::Node* node)
{
    // Empty global hits vector so it doesn't contain results of previous search
    hits.clear();
    // 
    int node_indent = meshOne.n_nodes();
    // 
    std::array node_coords = {(*node)(0), (*node)(1), (*node)(2)};
    Box node_box = Box(node_coords, tol, node->id());
    
    
    // Search tree to see if thie node already exists 
    int nhits = rtree.Search(node_box.min.data(), node_box.max.data(), callback);
    if(!nhits)
    {
        id_map[node->id()] = node_indent + 1;
        return true;
    }
    else if(nhits == 1)
    {   
        id_map[node->id()] = hits[0];
        return false;        
    }
    return true;
}

void 
combineMeshes(const double tol,
              libMesh::Mesh& meshOne,
              libMesh::Mesh& meshTwo, 
              std::multimap<unsigned int, unsigned int> surfaceFaceMap)
{
    std::set<libMesh::subdomain_id_type> sub_ids;
    meshOne.subdomain_ids(sub_ids);
    libMesh::subdomain_id_type vacId = 2;
    if (!sub_ids.empty())
    {
        vacId  = 1 + (*(sub_ids.rbegin()));
        std::cout << "vac Id will be " << vacId << std::endl;
    }
    
    // Instantiate an rTree. Using a rTree data structure significantly reduces the amount of
    // time taken to discover duplicate nodes.
    RTree<int, double, 3, float> rtree;

    // Generate the initial tree, containing all the nodes of the vacuum mesh. We will 
    //  check the nodes of the part mesh against this tree to search for duplicate nodes
    createTree(rtree, meshOne, tol);

    // This is a map which helps us keep track of the node id's of the duplicate nodes. For example,
    // if Node 4 in the surface mesh and Node 6 in the vacuum mesh are the same, "id_map[4]" will return 6
    std::map<unsigned int, unsigned int> id_map;
    
    // Loop over all the nodes in the original geometry. If the node is not a duplicate, add it to the mesh
    // , if the node is a duplicate, it is  
    for(auto& node: meshTwo.local_node_ptr_range())
    {
        // If there are no matches in the vacuumMesh, then we add the point 
        if(searchTree(rtree, tol, id_map, meshOne, node))
        {
            double pnt[3];
            pnt[0] = (*node)(0);
            pnt[1] = (*node)(1);
            pnt[2] = (*node)(2);
            libMesh::Point xyz(pnt[0], pnt[1], pnt[2]);
            meshOne.add_point(xyz, id_map[node->id()]);
        }
    }
    
    // Loop adds all elements from original geometry to vacuum mesh. The id_map is used to make sure the correct connectivity
    // is used where there are duplicate nodes
    for(auto& elem: libMesh::as_range(meshTwo.local_elements_begin(), meshTwo.local_elements_end()))
    {
        unsigned int el_id = 1 + meshOne.n_elem(); 
        // Element type being build must be consistent between meshes
        libMesh::Elem* new_elem = libMesh::Elem::build(meshOne.elem_ptr(0)->type()).release();
        for(int j = 0; j < new_elem->n_nodes(); j++)
        {
            new_elem->set_node(j) = meshOne.node_ptr(id_map[elem->node_ptr(j)->id()]);
        }
        new_elem->set_id(el_id);
        new_elem->subdomain_id() = vacId;
        meshOne.add_elem(new_elem);

        // Makes the boundary between the original geometry and the vacuum mesh a sideset in the mesh,
        // and names it "vacuum_boundary"
        for(auto& boundSide: libMesh::as_range(surfaceFaceMap.equal_range(elem->id())))
        {
            meshOne.get_boundary_info().add_side(el_id, boundSide.second, 1);
        }
        // Set boundary name!
    }
    meshOne.subdomain_name(vacId) = "VacuumMesh";
    // Prepare the mesh for use. This libmesh method does some id renumbering etc, generally a good idea
    // to call it after constructing a mesh
    meshOne.prepare_for_use();
}

// Version of the method that doesn't require a surfaceFaceMap, just combines the meshes
void 
combineMeshes(const double tol,
              libMesh::Mesh& meshOne,
              libMesh::Mesh& meshTwo)
{
    // Instantiate an rTree. Using a rTree data structure significantly reduces the amount of
    // time taken to discover duplicate nodes.
    RTree<int, double, 3, float> rtree;

    // Generate the initial tree, containing all the nodes of the vacuum mesh. We will 
    //  check the nodes of the part mesh against this tree to search for duplicate nodes
    createTree(rtree, meshOne, tol);

    // This is a map which helps us keep track of the node id's of the duplicate nodes. For example,
    // if Node 4 in the surface mesh and Node 6 in the vacuum mesh are the same, "id_map[4]" will return 6
    std::map<unsigned int, unsigned int> id_map;
    
    // Loop over all the nodes in the original geometry. If the node is not a duplicate, add it to the mesh
    // , if the node is a duplicate, it is  
    for(auto& node: meshTwo.local_node_ptr_range())
    {
        // If there are no matches in the vacuumMesh, then we add the point 
        if(searchTree(rtree, tol, id_map, meshOne, node))
        {
            double pnt[3];
            pnt[0] = (*node)(0);
            pnt[1] = (*node)(1);
            pnt[2] = (*node)(2);
            libMesh::Point xyz(pnt[0], pnt[1], pnt[2]);
            meshOne.add_point(xyz, id_map[node->id()]);
        }
    }
    
    // Loop adds all elements from original geometry to vacuum mesh. The id_map is used to make sure the correct connectivity
    // is used where there are duplicate nodes
    for(auto& elem: libMesh::as_range(meshTwo.local_elements_begin(), meshTwo.local_elements_end()))
    {
        unsigned int el_id = 1 + meshOne.n_elem(); 
        // Element type being build must be consistent between meshes
        libMesh::Elem* new_elem = libMesh::Elem::build(meshOne.elem_ptr(0)->type()).release();
        for(int j = 0; j < new_elem->n_nodes(); j++)
        {
            new_elem->set_node(j) = meshOne.node_ptr(id_map[elem->node_ptr(j)->id()]);
        }
        new_elem->set_id(el_id);
        // new_elem->subdomain_id() = 2;
        meshOne.add_elem(new_elem);     
    }
    // Prepare the mesh for use. This libmesh method does some id renumbering etc, generally a good idea
    // to call it after constructing a mesh
    meshOne.prepare_for_use();
}
