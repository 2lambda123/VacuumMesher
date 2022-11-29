#include "removeDupeNodes.hpp"
#include <chrono>
int main(int argc, char** argv)
{
    std::string appName(argv[0]);
    std::vector<char*> libmeshArgv = {(char*)appName.data()};
    std::string path, filepath, filenameNoExt, surfFilename, boundFilename, tetFilename; 
    std::string surfFilepath, boundFilepath, tetFilepath; 
    std::string filename(argv[1]);

    path = "./Meshes/";
    filepath = path + filename;
    filenameNoExt = filename.substr(0, filename.find("."));

    surfFilename = filenameNoExt + "_surf.e";
    boundFilename = filenameNoExt + "_bound.e";
    tetFilename = filenameNoExt + "_tet.e";

    surfFilepath = path + surfFilename; 
    boundFilepath = path + boundFilename;
    tetFilepath = path + tetFilename; 
    
    libMesh::LibMeshInit init(libmeshArgv.size() - 1, libmeshArgv.data());
    //Create mesh object to store original model mesh
    libMesh::Mesh mesh(init.comm());
    //Create mesh object to store surface mesh
    libMesh::Mesh surfMesh(init.comm());
    // 
    libMesh::Mesh boundMesh(init.comm());
    //Create mesh object to store vacuum mesh
    libMesh::Mesh vacuumMesh(init.comm());

    mesh.read(filepath);
    // surfMesh.read(surfFilepath);
    vacuumMesh.read(tetFilepath);

    // Multimap to store which sides of the elements are boundary sides (i.e. which sides have the null neighbor)
    std::multimap<unsigned int, unsigned int> surfaceFaceMap;
    getSurface(mesh, surfMesh, surfaceFaceMap, false, surfFilepath);
    // createBoundary(init, surfMesh, boundMesh);
    // surfMesh.write("boundTest2.e");
    
    // Set up rTree with specified tolerance
    RTree<int, double, 3, float> rtree;
    double tol = 1e-07;


    // auto start1 = std::chrono::steady_clock::now();
    createTree(rtree, vacuumMesh, tol);
    combineMesh(rtree, tol, mesh, vacuumMesh, surfaceFaceMap);
    // auto end1 = std::chrono::steady_clock::now();
    // std::cout << "Elapsed time in milliseconds: "
    // << std::chrono::duration_cast<std::chrono::milliseconds>(end1 - start1).count()
    // << " ms" << std::endl;
    vacuumMesh.write("combMeshOut.e");

    return 0;
}