// =============================================================================
// PROJECT CHRONO - http://projectchrono.org
//
// Copyright (c) 2018 projectchrono.org
// All rights reserved.
//
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file at the top level of the distribution and at
// http://projectchrono.org/license-chrono.txt.
//
// =============================================================================
// Authors: Dan Negrut, Nic Olsen
// =============================================================================
//
// Chrono::Granular demo using SMC method. A body who's geometry is described
// by a trinagle mesh is initialized under settling granular material. No friction present.
//
// The global reference frame has X to the right, Y into the screen, Z up.
// The global reference frame located in the left lower corner, close to the viewer.
// =============================================================================
/*! \file */

#include <iostream>
#include <string>
#include "chrono/core/ChFileutils.h"
#include "chrono/core/ChTimer.h"
#include "chrono_granular/physics/ChGranular.h"
#include "chrono_granular/physics/ChGranularTriMesh.h"
#include "chrono_thirdparty/SimpleOpt/SimpleOpt.h"

using namespace chrono;
using namespace chrono::granular;

// -----------------------------------------------------------------------------
// ID values to identify command line arguments
// There is no friction.
// -----------------------------------------------------------------------------
enum {
    OPT_HELP,
    OPT_MESH_FILE,
    OPT_SPH_RADIUS,
    OPT_TIMEEND,
    OPT_DENSITY,
    OPT_BOX_L,
    OPT_BOX_D,
    OPT_BOX_H,
    OPT_GRAV_ACC,
    OPT_COHESION_RATIO,
    OPT_STIFFNESS_S2S,
    OPT_STIFFNESS_S2W,
    OPT_STIFFNESS_MSH2S,
    OPT_WRITE_MODE,
    OPT_OUTPUT_DIR,
    OPT_VERBOSE
};

// Table of CSimpleOpt::Soption structures. Each entry specifies:
// - the ID for the option (returned from OptionId() during processing)
// - the option as it should appear on the command line
// - type of the option
// The last entry must be SO_END_OF_OPTIONS
CSimpleOptA::SOption g_options[] = {{OPT_MESH_FILE, "-m", SO_REQ_SEP},
                                    {OPT_SPH_RADIUS, "-sr", SO_REQ_SEP},
                                    {OPT_TIMEEND, "-e", SO_REQ_SEP},
                                    {OPT_DENSITY, "--density", SO_REQ_SEP},
                                    {OPT_WRITE_MODE, "--write_mode", SO_REQ_SEP},
                                    {OPT_OUTPUT_DIR, "--output_dir", SO_REQ_SEP},
                                    {OPT_BOX_L, "--box_size_X", SO_REQ_SEP},
                                    {OPT_BOX_D, "--box_size_Y", SO_REQ_SEP},
                                    {OPT_BOX_H, "--box_size_Z", SO_REQ_SEP},
                                    {OPT_GRAV_ACC, "--gravacc", SO_REQ_SEP},
                                    {OPT_COHESION_RATIO, "--cohes_ratio", SO_REQ_SEP},
                                    {OPT_STIFFNESS_S2S, "--normStiffS2S", SO_REQ_SEP},
                                    {OPT_STIFFNESS_S2W, "--normStiffS2W", SO_REQ_SEP},
                                    {OPT_STIFFNESS_MSH2S, "--normStiffMSH2S", SO_REQ_SEP},
                                    {OPT_VERBOSE, "--verbose", SO_NONE},
                                    {OPT_VERBOSE, "-v", SO_NONE},
                                    {OPT_HELP, "-?", SO_NONE},
                                    {OPT_HELP, "-h", SO_NONE},
                                    {OPT_HELP, "--help", SO_NONE},
                                    SO_END_OF_OPTIONS};

// -----------------------------------------------------------------------------
// Show command line usage
// -----------------------------------------------------------------------------
void showUsage() {
    std::cout << "Options:" << std::endl;
    std::cout << "-m=<mesh_file_name>" << std::endl;
    std::cout << "-sr <sphere_radius>" << std::endl;
    std::cout << "-v or --verbose" << std::endl;
    std::cout << "--density=<density>" << std::endl;
    std::cout << "--write_mode=<write_mode> (csv, binary, or none)" << std::endl;
    std::cout << "--output_dir=<output_dir>" << std::endl;
    std::cout << "-e=<time_end>" << std::endl;
    std::cout << "--box_size_X=<box_length>" << std::endl;
    std::cout << "--box_size_Y=<box_depth>" << std::endl;
    std::cout << "--box_size_Z=<box_height>" << std::endl;
    std::cout << "--gravacc=<accValue>" << std::endl;
    std::cout << "--cohes_ratio=<cohesValue>" << std::endl;
    std::cout << "--normStiffS2S=<stiffValuesS2S>" << std::endl;
    std::cout << "--normStiffS2W=<stiffValuesS2W>" << std::endl;
    std::cout << "--normStiffMSH2S=<stiffValuesMSH2S>" << std::endl;
    std::cout << "-h / --help / -? \t Show this help." << std::endl;
}

// -----------------------------------------------------------------------------
// Set up the problem parameters using command line input
// -----------------------------------------------------------------------------
bool GetProblemSpecs(int argc,
                     char** argv,
                     std::string& meshFileName,
                     float& ball_radius,
                     float& ballDensity,
                     float& box_L,
                     float& box_D,
                     float& box_H,
                     float& time_end,
                     float& gravAcc,
                     float& normalStiffS2S,
                     float& normalStiffS2W,
                     float& normalStiffMesh2S,
                     float& cohesion_ratio,
                     bool& verbose,
                     std::string& output_dir,
                     GRN_OUTPUT_MODE& write_mode) {
    // Create the option parser and pass it the program arguments and the array of valid options.
    CSimpleOptA args(argc, argv, g_options);

    // Then loop for as long as there are arguments to be processed.
    while (args.Next()) {
        // Exit immediately if we encounter an invalid argument.
        if (args.LastError() != SO_SUCCESS) {
            std::cout << "Invalid argument: " << args.OptionText() << std::endl;
            showUsage();
            return false;
        }

        // Process the current argument.
        switch (args.OptionId()) {
            case OPT_HELP:
                showUsage();
                return false;
            case OPT_DENSITY:
                ballDensity = std::stof(args.OptionArg());
                break;
            case OPT_WRITE_MODE:
                if (args.OptionArg() == std::string("binary")) {
                    write_mode = GRN_OUTPUT_MODE::BINARY;
                } else if (args.OptionArg() == std::string("csv")) {
                    write_mode = GRN_OUTPUT_MODE::CSV;
                } else if (args.OptionArg() == std::string("none")) {
                    write_mode = GRN_OUTPUT_MODE::NONE;
                } else {
                    std::cout << "Unknown file write mode! Options are 'csv', 'binary', or 'none'\n";
                }
                break;
            case OPT_OUTPUT_DIR:
                output_dir = args.OptionArg();
                break;
            case OPT_SPH_RADIUS:
                ball_radius = std::stof(args.OptionArg());
                break;
            case OPT_BOX_L:
                box_L = std::stof(args.OptionArg());
                break;
            case OPT_BOX_D:
                box_D = std::stof(args.OptionArg());
                break;
            case OPT_BOX_H:
                box_H = std::stof(args.OptionArg());
                break;
            case OPT_GRAV_ACC:
                gravAcc = std::stof(args.OptionArg());
                break;
            case OPT_STIFFNESS_S2S:
                normalStiffS2S = std::stof(args.OptionArg());
                break;
            case OPT_STIFFNESS_S2W:
                normalStiffS2W = std::stof(args.OptionArg());
                break;
            case OPT_STIFFNESS_MSH2S:
                normalStiffMesh2S = std::stof(args.OptionArg());
                break;
            case OPT_MESH_FILE:
                normalStiffMesh2S = std::stof(args.OptionArg());
                break;
            case OPT_COHESION_RATIO:
                cohesion_ratio = std::stof(args.OptionArg());
                break;
            case OPT_TIMEEND:
                time_end = std::stof(args.OptionArg());
                break;
            case OPT_VERBOSE:
                verbose = true;
                break;
        }
    }

    return true;
}

// Remains still for still_time and then begins to move up at Z_vel
double pos_func_Z(double t, float box_size_Z) {
    double still_time = 2;
    double Z_vel = 10;
    if (t < still_time) {
        return -box_size_Z / 4;
    } else {
        return (t - still_time) * Z_vel - box_size_Z / 4;
    }
}

// -----------------------------------------------------------------------------
// Demo for settling a monodisperse collection of shperes in a rectangular box.
// There is no friction. The units are always cm/s/g[L/T/M].
// -----------------------------------------------------------------------------
int main(int argc, char* argv[]) {
#define box_size_X_cm 40.f
#define box_size_Y_cm 40.f
#define box_size_Z_cm 40.f
#define RADIUS 1.f
#define SPH_DENSITY 1.50f
#define TIME_END 4.f
#define GRAV_ACCELERATION 980.f
#define NORMAL_STIFFNESS_S2S 1e7f
#define NORMAL_STIFFNESS_M2S 1e7f
#define NORMAL_STIFFNESS_S2W 1e7f

    std::string output_prefix = "../results";

    // Default values
    float ballRadius = RADIUS;
    float ballDensity = SPH_DENSITY;
    float box_size_X = box_size_X_cm;
    float box_size_Y = box_size_Y_cm;
    float box_size_Z = box_size_Z_cm;
    float timeEnd = TIME_END;
    float grav_acceleration = GRAV_ACCELERATION;
    float normStiffness_S2S = NORMAL_STIFFNESS_S2S;
    float normStiffness_S2W = NORMAL_STIFFNESS_S2W;
    float normStiffness_MSH2S = NORMAL_STIFFNESS_M2S;

    float iteration_step = 0.02;

    GRN_OUTPUT_MODE write_mode = GRN_OUTPUT_MODE::BINARY;
    bool verbose = false;
    float cohesion_ratio = 0;

    // Mesh values
    std::vector<std::string> mesh_filenames;
    std::string mesh_filename = std::string("sphere_fine.obj");

    std::vector<float3> mesh_scalings;
    float3 scaling;
    scaling.x = 8;
    scaling.y = 8;
    scaling.z = 8;
    mesh_scalings.push_back(scaling);

    // Some of the default values might be overwritten by user via command line
    if (GetProblemSpecs(argc, argv, mesh_filename, ballRadius, ballDensity, box_size_X, box_size_Y, box_size_Z, timeEnd,
                        grav_acceleration, normStiffness_S2S, normStiffness_S2W, normStiffness_MSH2S, cohesion_ratio,
                        verbose, output_prefix, write_mode) == false) {
        return 1;
    }

    mesh_filenames.push_back(mesh_filename);

    // Setup simulation
    ChSystemGranularMonodisperse_SMC_Frictionless_trimesh m_sys(ballRadius, ballDensity);
    m_sys.setBOXdims(box_size_X, box_size_Y, box_size_Z);
    m_sys.set_BD_Fixed(true);
    m_sys.setFillBounds(-1.f, -1.f, 0.f, 1.f, 1.f, 1.f);
    m_sys.set_K_n_SPH2SPH(normStiffness_S2S);
    m_sys.set_K_n_SPH2WALL(normStiffness_S2W);
    m_sys.set_K_n_SPH2MESH(normStiffness_MSH2S);
    m_sys.set_Cohesion_ratio(cohesion_ratio);
    m_sys.set_gravitational_acceleration(0.f, 0.f, -GRAV_ACCELERATION);
    m_sys.set_timeStepping(GRN_TIME_STEPPING::FIXED);
    m_sys.set_fixed_stepSize(1e-4);

    m_sys.load_meshes(mesh_filenames, mesh_scalings);

    /// output preferences
    m_sys.setOutputDirectory(output_prefix);
    m_sys.setOutputMode(write_mode);
    m_sys.setVerbose(verbose);
    ChFileutils::MakeDirectory(output_prefix.c_str());

    unsigned int nSoupFamilies = m_sys.nMeshesInSoup();
    std::cout << nSoupFamilies << " soup families \n";
    float* genForcesOnMeshSoup = new float[6 * nSoupFamilies];
    double* meshSoupLocOri = new double[7 * nSoupFamilies];

    m_sys.initialize();
    int currframe = 0;

    // Uncomment the following to test loading of a mesh
    // int fakeframe = 0;
    // for (float t = 0; t < timeEnd; t += iteration_step) {
    //     char filename[100];
    //     sprintf(filename, "%s/step%06d", output_prefix.c_str(), fakeframe++);
    //     meshSoupLocOri[0] = 0;  // Keep wheel centered in X and Y
    //     meshSoupLocOri[1] = 0;
    //     meshSoupLocOri[2] = pos_func_Z(t, box_size_Z);  // Get next position and orientation from the prescribed function
    //     meshSoupLocOri[3] = 1;                    // No rotation in this demo
    //     meshSoupLocOri[4] = 0;
    //     meshSoupLocOri[5] = 0;
    //     meshSoupLocOri[6] = 0;
    //     m_sys.meshSoup_applyRigidBodyMotion(meshSoupLocOri);
    //     m_sys.write_meshes(std::string(filename));
    //     m_sys.writeFileUU(std::string(filename));
    // }
    // return 0;

    // Run a loop that is typical of co-simulation. For instance, the wheeled is moved a bit, which moves the particles.
    // Conversely, the particles impress a force and torque upon the mesh soup
    for (float t = 0; t < timeEnd; t += iteration_step) {
        // Generate next tire location and orientation
        meshSoupLocOri[0] = 0.00001;  // Keep wheel centered in X and Y
        meshSoupLocOri[1] = 0;
        meshSoupLocOri[2] = pos_func_Z(t, box_size_Z);  // Get next position and orientation from the prescribed function
        meshSoupLocOri[3] = 1;                    // No rotation in this demo
        meshSoupLocOri[4] = 0;
        meshSoupLocOri[5] = 0;
        meshSoupLocOri[6] = 0;

        m_sys.meshSoup_applyRigidBodyMotion(meshSoupLocOri);  // Apply the mesh orientation data to the mesh

        m_sys.advance_simulation(iteration_step);

        printf("rendering frame %u\n", currframe);
        char filename[100];
        sprintf(filename, "%s/step%06d", output_prefix.c_str(), currframe++);
        m_sys.writeFileUU(std::string(filename));
        m_sys.write_meshes(std::string(filename));
    }

    delete[] genForcesOnMeshSoup;
    delete[] meshSoupLocOri;

    return 0;
}