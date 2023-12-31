#include <functional>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>

struct inputFlags {
  bool help = false;
  // be verbose when calling libIGL options, print out all
  bool verbose = false;
  // Length of the cubic boundary
  std::optional<double> boundLen;
  //  The number of subdivisions  (triangular elements) along the side of the
  //  mesh boundary
  std::optional<int> boundSubd = 20;
  // The mesh file to be read in
  std::optional<std::string> infile;
  // The name of the file to be output
  std::optional<std::string> outfile;
  // Order of the mesh
  int order{1};
  // Maximum area of tri elements on bound
  std::optional<double> maxTriArea;
  // Maximum volume of tet elements generated
  std::optional<double> maxTetVol;
  // Command line switches used by the triangle library
  std::string triSettings = "qYa";
  // Command line switches used by the tetgen library
  std::string tetSettings = "pqCYY";

  // Sideset ID's for coil boundary generation
  int coil_sideset_one_id = 1;

  int coil_sideset_two_id = 2;

  void setSwitches();
  // Method to print help message
  void helpMessage();
};

// Function that will return an inputFlags struct from which we can access CLI
// flags
inputFlags parse_settings(int argc, const char *argv[]);

// Method to print out help message
