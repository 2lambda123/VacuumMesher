#include "Utils/getElemInfo.hpp"

void 
getElemInfo(libMesh::ElemType& elem_type, libMesh::ElemType& face_type, 
            libMesh::Elem* element, int& num_elem_sides, int& num_face_nodes)
{
    // Retrieve a ptr to one of the sides of the element
    std::unique_ptr<libMesh::Elem> side = element->build_side_ptr(0);
    // Populate all of our arguments
    elem_type = element->type();
    num_elem_sides = element->n_sides();
    face_type = side->type();
    num_face_nodes = side->n_nodes();
}
