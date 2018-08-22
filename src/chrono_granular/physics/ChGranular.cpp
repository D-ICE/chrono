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
/*! \file */

#include <cuda.h>
#include <cuda_runtime.h>
#include <cmath>
#include <vector>
#include "ChGranular.h"
#include "chrono/utils/ChUtilsGenerators.h"
#include "chrono/core/ChVector.h"
#include "chrono_granular/ChGranularDefines.h"
#include "chrono_granular/utils/ChGranularUtilities_CUDA.cuh"

namespace chrono {
namespace granular {
ChSystemGranular::ChSystemGranular() : time_stepping(GRN_TIME_STEPPING::AUTO), nDEs(0), elapsedSimTime(0) {
    gpuErrchk(cudaMallocManaged(&gran_params, sizeof(GranParamsHolder), cudaMemAttachGlobal));
}

ChSystemGranular::~ChSystemGranular() {
    gpuErrchk(cudaFree(gran_params));
}

void ChSystemGranularMonodisperse::determine_new_stepSize_SU() {
    // std::cerr << "determining new step!\n";
    if (time_stepping == GRN_TIME_STEPPING::AUTO) {
        static float new_step_stop = 0;
        if (elapsedSimTime >= new_step_stop) {
            // printf("-------------------------\n");
            // std::cerr << "elapsed is " << elapsedSimTime << ", stop is " << new_step_stop << std::endl;
            new_step_stop += new_step_freq;  // assumes we never have a timestep larger than new_step_freq
            float max_v = get_max_vel();
            if (max_v <= 0) {
                // clearly we have an issue, just fallback to the fixed step
                stepSize_SU = fixed_step_UU / (gran_params->TIME_UNIT * PSI_h);
            } else {
                // maximum number of gravity displacements we allow moving in one timestep
                constexpr float num_disp_grav = 100;
                // maximum fraction of radius we allow moving in one timestep
                constexpr float num_disp_radius = .1;
                float max_displacement_grav = num_disp_grav * psi_T_Factor;
                float max_displacement_radius = num_disp_radius * sphereRadius_SU;

                // TODO consider gravity drift

                // find the highest position displacement we allow
                float max_displacement = std::min(max_displacement_grav, max_displacement_radius);
                float suggested_SU = max_displacement / max_v;
                float max_step_SU = max_adaptive_step_UU / (gran_params->TIME_UNIT * PSI_h);
                float min_step_SU = 1e-5 / (gran_params->TIME_UNIT * PSI_h);
                printf("grav step is %f, rad step is %f\n", max_displacement_grav / max_v,
                       max_displacement_radius / max_v);

                // don't go above max
                stepSize_SU = std::max(std::min(suggested_SU, max_step_SU), min_step_SU);
            }
            printf("new timestep is %f SU, %f UU\n", stepSize_SU, stepSize_SU * (gran_params->TIME_UNIT * PSI_h));
            // printf("z grav term with timestep %f is %f\n", stepSize_SU, stepSize_SU * stepSize_SU * gravity_Z_SU);
        }
    } else {
        stepSize_SU = fixed_step_UU / (gran_params->TIME_UNIT * PSI_h);
    }
}

double ChSystemGranularMonodisperse_SMC_Frictionless::get_max_K() {
    return std::max(YoungModulus_SPH2SPH, YoungModulus_SPH2WALL);
}

void ChSystemGranularMonodisperse_SMC_Frictionless::cleanup_simulation() {}

/** This method sets up the data structures used to perform a simulation.
 *
 */
void ChSystemGranularMonodisperse_SMC_Frictionless::setup_simulation() {
    partition_BD();

    // allocate mem for array saying for each SD how many spheres touch it
    // gpuErrchk(cudaMalloc(&SD_NumOf_DEs_Touching, nSDs * sizeof(unsigned int)));
    SD_NumOf_DEs_Touching.resize(nSDs);
    // allocate mem for array that for each SD has the list of all spheres touching it; big array
    // gpuErrchk(cudaMalloc(&DEs_in_SD_composite, MAX_COUNT_OF_DEs_PER_SD * nSDs));
    DEs_in_SD_composite.resize(MAX_COUNT_OF_DEs_PER_SD * nSDs);
}

// Set the bounds to fill in our box
void ChSystemGranularMonodisperse::setFillBounds(float xmin,
                                                 float ymin,
                                                 float zmin,
                                                 float xmax,
                                                 float ymax,
                                                 float zmax) {
    boxFillXmin = xmin;
    boxFillYmin = ymin;
    boxFillZmin = zmin;
    boxFillXmax = xmax;
    boxFillYmax = ymax;
    boxFillZmax = zmax;
}

// Set particle positions in UU
void ChSystemGranularMonodisperse::setParticlePositions(std::vector<ChVector<float>>& points) {
    h_points = points;  // Copy points to class vector
}

void ChSystemGranularMonodisperse::generate_DEs() {
    // Each fills h_points with positions to be copied
    if (h_points.size() == 0) {
        generate_DEs_FillBounds();
    } else {
        generate_DEs_positions();
    }

    nDEs = (unsigned int)h_points.size();
    std::cout << nDEs << " balls added!" << std::endl;

    // Allocate space for new bodies
    pos_X.resize(nDEs);
    pos_Y.resize(nDEs);
    pos_Z.resize(nDEs);
    pos_X_dt.resize(nDEs, 0);
    pos_Y_dt.resize(nDEs, 0);
    pos_Z_dt.resize(nDEs, 0);
    pos_X_dt_update.resize(nDEs, 0);
    pos_Y_dt_update.resize(nDEs, 0);
    pos_Z_dt_update.resize(nDEs, 0);

    // Copy from array of structs to 3 arrays
    for (unsigned int i = 0; i < nDEs; i++) {
        auto vec = h_points.at(i);
        pos_X.at(i) = (int)(vec.x());
        pos_Y.at(i) = (int)(vec.y());
        pos_Z.at(i) = (int)(vec.z());
    }
}

void ChSystemGranularMonodisperse::generate_DEs_FillBounds() {
    // Create the falling balls
    float ball_epsilon = sphereRadius_SU / 200.f;  // Margin between balls to ensure no overlap / DEM-splosion
    printf("eps is %f, rad is %5f\n", ball_epsilon, sphereRadius_SU * 1.0f);

    chrono::utils::HCPSampler<float> sampler(2.4 * sphereRadius_SU);  // Add epsilon

    // We need to pass in half-length box here

    // generate from bottom to twice the generateDepth
    // average high and low to get midpoint of generation
    float xmid = box_L * (boxFillXmax + boxFillXmin) / (4. * gran_params->LENGTH_UNIT);
    float ymid = box_D * (boxFillYmax + boxFillYmin) / (4. * gran_params->LENGTH_UNIT);
    float zmid = box_H * (boxFillZmax + boxFillZmin) / (4. * gran_params->LENGTH_UNIT);
    // half-spans in each dimension, the difference
    float xlen = abs(box_L * (boxFillXmax - boxFillXmin) / (4. * gran_params->LENGTH_UNIT));
    float ylen = abs(box_D * (boxFillYmax - boxFillYmin) / (4. * gran_params->LENGTH_UNIT));
    float zlen = abs(box_H * (boxFillZmax - boxFillZmin) / (4. * gran_params->LENGTH_UNIT));
    float generateHalfDepth = box_H / (3. * gran_params->LENGTH_UNIT);

    // float generateX = -box_D / (2. * gran_params->LENGTH_UNIT) + generateHalfDepth;
    // float generateY = -box_D / (2. * gran_params->LENGTH_UNIT) + generateHalfDepth;
    // float generateZ = -box_H / (2. * gran_params->LENGTH_UNIT) + generateHalfHeight;
    ChVector<float> boxCenter(xmid, ymid, zmid);
    // We need to subtract off a sphere radius to ensure we don't get put at the edge
    ChVector<float> hdims{xlen - sphereRadius_SU, ylen - sphereRadius_SU, zlen - sphereRadius_SU};
    h_points = sampler.SampleBox(boxCenter, hdims);  // Vector of points
}

void ChSystemGranularMonodisperse::generate_DEs_positions() {
    for (auto& point : h_points) {
        point /= gran_params->LENGTH_UNIT;
    }
}

/**
This method figures out how big a SD is, and how many SDs are going to be necessary
in order to cover the entire BD.
BD: Bid domain.
SD: Sub-domain.
*/
void ChSystemGranularMonodisperse::partition_BD() {
    double tempDIM = 2. * sphere_radius * AVERAGE_SPHERES_PER_SD_L_DIR;
    unsigned int howMany = (unsigned int)(std::ceil(box_L / tempDIM));
    // work with an even kFac to hit the CM of the box.
    if (howMany & 1)
        howMany++;
    tempDIM = box_L / howMany;
    SD_size_X_SU = (unsigned int)std::ceil(tempDIM / gran_params->LENGTH_UNIT);
    nSDs_X = howMany;

    tempDIM = 2. * sphere_radius * AVERAGE_SPHERES_PER_SD_D_DIR;
    howMany = (unsigned int)(std::ceil(box_D / tempDIM));
    // work with an even kFac to hit the CM of the box.
    if (howMany & 1)
        howMany++;
    tempDIM = box_D / howMany;
    SD_size_Y_SU = (unsigned int)std::ceil(tempDIM / gran_params->LENGTH_UNIT);
    nSDs_Y = howMany;

    tempDIM = 2. * sphere_radius * AVERAGE_SPHERES_PER_SD_H_DIR;
    howMany = (unsigned int)(std::ceil(box_H / tempDIM));
    // work with an even kFac to hit the CM of the box.
    if (howMany & 1)
        howMany++;
    tempDIM = box_H / howMany;
    SD_size_Z_SU = (unsigned int)std::ceil(tempDIM / gran_params->LENGTH_UNIT);
    nSDs_Z = howMany;

    nSDs = nSDs_X * nSDs_Y * nSDs_Z;
    printf("%u Sds as %u, %u, %u\n", nSDs, nSDs_X, nSDs_Y, nSDs_Z);

    // Place BD frame at bottom-left corner, one half-length in each direction
    // Can change later if desired
    BD_frame_X = -.5 * (nSDs_X * SD_size_X_SU);
    BD_frame_Y = -.5 * (nSDs_Y * SD_size_Y_SU);
    BD_frame_Z = -.5 * (nSDs_Z * SD_size_Z_SU);
    // BD starts at rest
    BD_frame_X_dot = 0;
    BD_frame_Y_dot = 0;
    BD_frame_Z_dot = 0;
}

/**
This method defines the mass, time, length Simulation Units. It also sets several other constants that enter the scaling
of various physical quantities set by the user.
*/
void ChSystemGranularMonodisperse_SMC_Frictionless::switch_to_SimUnits() {
    double massSphere = 4. / 3. * M_PI * sphere_radius * sphere_radius * sphere_radius * sphere_density;
    gran_params->MASS_UNIT = massSphere;
    double K_stiffness = get_max_K();
    gran_params->TIME_UNIT = sqrt(massSphere / (PSI_h * K_stiffness)) / PSI_T;

    double magGravAcc = sqrt(X_accGrav * X_accGrav + Y_accGrav * Y_accGrav + Z_accGrav * Z_accGrav);
    gran_params->LENGTH_UNIT = massSphere * magGravAcc / (PSI_L * K_stiffness);

    sphereRadius_SU = sphere_radius / gran_params->LENGTH_UNIT;

    float scalingFactor = ((float)PSI_L) / (PSI_T * PSI_T * PSI_h);
    gravity_X_SU = scalingFactor * X_accGrav / magGravAcc;
    gravity_Y_SU = scalingFactor * Y_accGrav / magGravAcc;
    gravity_Z_SU = scalingFactor * Z_accGrav / magGravAcc;

    /// SU values for normal stiffnesses for S2S and S2W
    scalingFactor = (1.f / (1.f * PSI_T * PSI_T * PSI_h));
    K_n_s2s_SU = scalingFactor * (YoungModulus_SPH2SPH / K_stiffness);
    K_n_s2w_SU = scalingFactor * (YoungModulus_SPH2WALL / K_stiffness);

    // TODO Make this legit, from user input
    Gamma_n_s2s_SU = .005;

    // Handy debug output
    printf("SU step size: %d\n", stepSize_SU);
    printf("SU gravity is %f, %f, %f\n", gravity_X_SU, gravity_Y_SU, gravity_Z_SU);
    printf("SU mass is %f\n", gran_params->MASS_UNIT);
    printf("SU radius is %u\n", sphereRadius_SU);
}
}  // namespace granular
}  // namespace chrono