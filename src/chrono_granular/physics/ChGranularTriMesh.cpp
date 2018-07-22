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

#include <vector>
#include <algorithm>
#include "chrono/core/ChVector.h"
#include "chrono/core/ChQuaternion.h"
#include "ChGranularTriMesh.h"
#include "chrono_granular/utils/ChGranularUtilities_CUDA.cuh"

namespace chrono {
namespace granular {

double ChSystemGranularMonodisperse_SMC_Frictionless_trimesh::get_max_K() {
    return std::max(std::max(YoungModulus_SPH2SPH, YoungModulus_SPH2WALL), YoungModulus_SPH2MESH);
}

ChSystemGranularMonodisperse_SMC_Frictionless_trimesh::ChSystemGranularMonodisperse_SMC_Frictionless_trimesh(
    float radiusSPH,
    float density)
    : ChSystemGranularMonodisperse_SMC_Frictionless(radiusSPH, density),
      problemSetupFinished(false),
      timeToWhichDEsHaveBeenPropagated(0.f) {}

ChSystemGranularMonodisperse_SMC_Frictionless_trimesh::~ChSystemGranularMonodisperse_SMC_Frictionless_trimesh() {
    // work to do here
    cleanupTriMesh_DEVICE();
}

void ChSystemGranularMonodisperse_SMC_Frictionless_trimesh::load_meshes(std::vector<std::string> objfilenames,
                                                                        std::vector<float3> scalings) {
    if (objfilenames.size() != scalings.size()) {
        GRANULAR_ERROR("Vectors of obj files and scalings must have same size\n");
    }

    // Load all shapes into vectors of shape_t
    unsigned int nTriangles = 0;
    unsigned int nFamiliesInSoup = 0;
    std::vector<std::vector<tinyobj::shape_t>> all_shapes;
    for (auto mesh_filename : objfilenames) {
        nFamiliesInSoup++;
        std::vector<tinyobj::shape_t> shapes;

        // The mesh soup stored in an obj file
        std::string load_result = tinyobj::LoadObj(shapes, mesh_filename.c_str());
        if (load_result.length() != 0) {
            std::cerr << load_result << "\n";
            GRANULAR_ERROR("Failed to load triangle mesh\n");
        }

        for (auto shape : shapes) {
            nTriangles += shape.mesh.indices.size() / 3;
        }
        all_shapes.push_back(shapes);
    }

    printf("nTriangles is %u\n", nTriangles);
    printf("nTrianglesFailiesInSoup is %u\n", nFamiliesInSoup);

    // Allocate triangle collision parameters
    gpuErrchk(cudaMallocManaged(&tri_params, sizeof(GranParamsHolder_trimesh), cudaMemAttachGlobal));

    // Allocate memory to store mesh soup in unified memory
    printf("Allocating mesh unified memory\n");
    setupTriMesh_DEVICE(all_shapes, nTriangles);
    printf("Done allocating mesh unified memory\n");

    // Allocate triangle collision memory
    BUCKET_countsOfTrianglesTouching.resize(TRIANGLEBUCKET_COUNT);
    triangles_in_BUCKET_composite.resize(TRIANGLEBUCKET_COUNT * MAX_TRIANGLE_COUNT_PER_BUCKET);
    triangles_in_BUCKET_composite.resize(nSDs);
}

void ChSystemGranularMonodisperse_SMC_Frictionless_trimesh::cleanupTriMesh_DEVICE() {
    cudaFree(meshSoup_DEVICE->triangleFamily_ID);

    cudaFree(meshSoup_DEVICE->node1_X);
    cudaFree(meshSoup_DEVICE->node1_Y);
    cudaFree(meshSoup_DEVICE->node1_Z);

    cudaFree(meshSoup_DEVICE->node2_X);
    cudaFree(meshSoup_DEVICE->node2_Y);
    cudaFree(meshSoup_DEVICE->node2_Z);

    cudaFree(meshSoup_DEVICE->node3_X);
    cudaFree(meshSoup_DEVICE->node3_Y);
    cudaFree(meshSoup_DEVICE->node3_Z);

    cudaFree(meshSoup_DEVICE->node1_XDOT);
    cudaFree(meshSoup_DEVICE->node1_YDOT);
    cudaFree(meshSoup_DEVICE->node1_ZDOT);

    cudaFree(meshSoup_DEVICE->node2_XDOT);
    cudaFree(meshSoup_DEVICE->node2_YDOT);
    cudaFree(meshSoup_DEVICE->node2_ZDOT);

    cudaFree(meshSoup_DEVICE->node3_XDOT);
    cudaFree(meshSoup_DEVICE->node3_YDOT);
    cudaFree(meshSoup_DEVICE->node3_ZDOT);

    cudaFree(meshSoup_DEVICE->generalizedForcesPerFamily);
}

void ChSystemGranularMonodisperse_SMC_Frictionless_trimesh::setupTriMesh_DEVICE(
    const std::vector<std::vector<tinyobj::shape_t>>& all_shapes,
    unsigned int nTriangles) {
    // Allocate the device soup storage
    gpuErrchk(cudaMallocManaged(&meshSoup_DEVICE, sizeof(meshSoup_DEVICE), cudaMemAttachGlobal));

    meshSoup_DEVICE->nTrianglesInSoup = nTriangles;

    // Allocate all of the requisite pointers
    gpuErrchk(
        cudaMallocManaged(&meshSoup_DEVICE->triangleFamily_ID, nTriangles * sizeof(unsigned int), cudaMemAttachGlobal));

    gpuErrchk(cudaMallocManaged(&meshSoup_DEVICE->node1_X, nTriangles * sizeof(float), cudaMemAttachGlobal));
    gpuErrchk(cudaMallocManaged(&meshSoup_DEVICE->node1_Y, nTriangles * sizeof(float), cudaMemAttachGlobal));
    gpuErrchk(cudaMallocManaged(&meshSoup_DEVICE->node1_Z, nTriangles * sizeof(float), cudaMemAttachGlobal));

    gpuErrchk(cudaMallocManaged(&meshSoup_DEVICE->node2_X, nTriangles * sizeof(float), cudaMemAttachGlobal));
    gpuErrchk(cudaMallocManaged(&meshSoup_DEVICE->node2_Y, nTriangles * sizeof(float), cudaMemAttachGlobal));
    gpuErrchk(cudaMallocManaged(&meshSoup_DEVICE->node2_Z, nTriangles * sizeof(float), cudaMemAttachGlobal));

    gpuErrchk(cudaMallocManaged(&meshSoup_DEVICE->node3_X, nTriangles * sizeof(float), cudaMemAttachGlobal));
    gpuErrchk(cudaMallocManaged(&meshSoup_DEVICE->node3_Y, nTriangles * sizeof(float), cudaMemAttachGlobal));
    gpuErrchk(cudaMallocManaged(&meshSoup_DEVICE->node3_Z, nTriangles * sizeof(float), cudaMemAttachGlobal));

    gpuErrchk(cudaMallocManaged(&meshSoup_DEVICE->node1_XDOT, nTriangles * sizeof(float), cudaMemAttachGlobal));
    gpuErrchk(cudaMallocManaged(&meshSoup_DEVICE->node1_YDOT, nTriangles * sizeof(float), cudaMemAttachGlobal));
    gpuErrchk(cudaMallocManaged(&meshSoup_DEVICE->node1_ZDOT, nTriangles * sizeof(float), cudaMemAttachGlobal));

    gpuErrchk(cudaMallocManaged(&meshSoup_DEVICE->node2_XDOT, nTriangles * sizeof(float), cudaMemAttachGlobal));
    gpuErrchk(cudaMallocManaged(&meshSoup_DEVICE->node2_YDOT, nTriangles * sizeof(float), cudaMemAttachGlobal));
    gpuErrchk(cudaMallocManaged(&meshSoup_DEVICE->node2_ZDOT, nTriangles * sizeof(float), cudaMemAttachGlobal));

    gpuErrchk(cudaMallocManaged(&meshSoup_DEVICE->node3_XDOT, nTriangles * sizeof(float), cudaMemAttachGlobal));
    gpuErrchk(cudaMallocManaged(&meshSoup_DEVICE->node3_YDOT, nTriangles * sizeof(float), cudaMemAttachGlobal));
    gpuErrchk(cudaMallocManaged(&meshSoup_DEVICE->node3_ZDOT, nTriangles * sizeof(float), cudaMemAttachGlobal));

    printf("Done allocating nodes for %d triangles\n", nTriangles);

    // Setup the clean HOST copy of the mesh soup from the obj file data
    size_t tri_index = 0;
    unsigned int family = 0;
    // for each obj file data set
    for (auto shapes : all_shapes) {
        // for each shape in this obj file data set
        for (auto shape : shapes) {
            std::vector<unsigned int>& indices = shape.mesh.indices;
            std::vector<float>& positions = shape.mesh.positions;
            std::vector<float>& normals = shape.mesh.normals;

            // Grab three indices which indicate the vertices of a triangle
            // for each triangle in this shape
            for (size_t i = 0; i < indices.size(); i += 9, tri_index++) {
                meshSoup_DEVICE->node1_X[tri_index] = positions[indices[i + 0]];
                meshSoup_DEVICE->node1_Y[tri_index] = positions[indices[i + 1]];
                meshSoup_DEVICE->node1_Z[tri_index] = positions[indices[i + 2]];

                meshSoup_DEVICE->node2_X[tri_index] = positions[indices[i + 3]];
                meshSoup_DEVICE->node2_Y[tri_index] = positions[indices[i + 4]];
                meshSoup_DEVICE->node2_Z[tri_index] = positions[indices[i + 5]];

                meshSoup_DEVICE->node3_X[tri_index] = positions[indices[i + 6]];
                meshSoup_DEVICE->node3_Y[tri_index] = positions[indices[i + 7]];
                meshSoup_DEVICE->node3_Z[tri_index] = positions[indices[i + 8]];

                meshSoup_DEVICE->triangleFamily_ID[tri_index] = family;

                // Normal of a vertex... Should still work TODO: test
                float norm_vert[3] = {0};
                norm_vert[0] = normals[indices[i + 0]];
                norm_vert[1] = normals[indices[i + 1]];
                norm_vert[2] = normals[indices[i + 2]];

                // Generate normal using RHR from nodes 1, 2, and 3
                float AB[3];
                AB[0] = positions[indices[i + 3]] - positions[indices[i + 0]];
                AB[1] = positions[indices[i + 4]] - positions[indices[i + 1]];
                AB[2] = positions[indices[i + 5]] - positions[indices[i + 2]];

                float AC[3];
                AC[0] = positions[indices[i + 6]] - positions[indices[i + 0]];
                AC[1] = positions[indices[i + 7]] - positions[indices[i + 1]];
                AC[2] = positions[indices[i + 8]] - positions[indices[i + 2]];

                float cross[3];
                cross[0] = AB[1] * AC[2] - AB[2] * AC[1];
                cross[1] = -(AB[0] * AC[2] - AB[2] * AC[0]);
                cross[2] = AB[0] * AC[1] - AB[1] * AC[0];

                // If the normal created by a RHR traversal is not correct, switch two vertices
                if (norm_vert[0] * cross[0] + norm_vert[1] * cross[1] + norm_vert[2] * cross[2] < 0) {
                    // GRANULAR_ERROR("Input mesh has inside-out elements.")
                    std::swap(meshSoup_DEVICE->node2_X[tri_index], meshSoup_DEVICE->node3_X[tri_index]);
                    std::swap(meshSoup_DEVICE->node2_Y[tri_index], meshSoup_DEVICE->node3_Y[tri_index]);
                    std::swap(meshSoup_DEVICE->node2_Z[tri_index], meshSoup_DEVICE->node3_Z[tri_index]);
                }
                tri_index++;
            }
        }
        printf("Done writing family %d\n", family);
        family++;
    }
    meshSoup_DEVICE->nFamiliesInSoup = family;

    // Allocate memory for the float and double frames
    gpuErrchk(cudaMallocManaged(&tri_params->fam_frame_broad,
                                meshSoup_DEVICE->nFamiliesInSoup * sizeof(tri_params->fam_frame_broad),
                                cudaMemAttachGlobal));
    gpuErrchk(cudaMallocManaged(&tri_params->fam_frame_narrow,
                                meshSoup_DEVICE->nFamiliesInSoup * sizeof(tri_params->fam_frame_narrow),
                                cudaMemAttachGlobal));
}

void ChSystemGranularMonodisperse_SMC_Frictionless_trimesh::update_DMeshSoup_Location() {
    // TODO implement this in the unified-memory mesh setup
}

/**
* \brief Collects the forces that each mesh feels as a result of their interaction with the DEs.
*
* The generalized forces felt by each family in the triangle soup are copied from the device into the host array
that is
* provided to this function. Each generalized force acting on a family in the soup has six components: three forces
&
* three torques.
*
* The forces measured are based on the position of the DEs as they are at the beginning of this function call,
before
* the DEs are moved forward in time. In other words, the forces are associated with the configuration of the DE
system
* from time timeToWhichDEsHaveBeenPropagated upon entrying this function.
* The logic is this: when the time integration is carried out, the forces are measured and saved in
* meshSoup_DEVICE->nFamiliesInSoup. Then, the numerical integration takes places and the state of DEs is update
along
* with the value of timeToWhichDEsHaveBeenPropagated.
*
* \param [in] crntTime The time at which the force is computed
* \param [out] genForcesOnSoup Array that stores the generalized forces on the meshes in the soup

* \return nothing
*
* \attention The size of genForcesOnSoup should be 6 * nFamiliesInSoup
*/
void ChSystemGranularMonodisperse_SMC_Frictionless_trimesh::collectGeneralizedForcesOnMeshSoup(float crntTime,
                                                                                               float* genForcesOnSoup) {
    if (!problemSetupFinished) {
        setupSimulation();
        problemSetupFinished = true;
    }

    // update the time that the DE system has reached
    timeToWhichDEsHaveBeenPropagated = crntTime;

    // Values in meshSoup_DEVICE are legit and ready to be loaded in user provided array.
    gpuErrchk(cudaMemcpy(genForcesOnSoup, meshSoup_DEVICE->generalizedForcesPerFamily,
                         6 * meshSoup_DEVICE->nFamiliesInSoup * sizeof(float), cudaMemcpyDeviceToHost));
}

/**
 * \brief Function sets up data structures, allocates space on the device, generates the spheres, etc.
 *
 */
void ChSystemGranularMonodisperse_SMC_Frictionless_trimesh::setupSimulation() {
    switch_to_SimUnits();
    generate_DEs();

    // Set aside memory for holding data structures worked with. Get some initializations going
    setup_simulation();
    copy_const_data_to_device();
    copy_triangle_data_to_device();
    gpuErrchk(cudaDeviceSynchronize());

    // Seed arrays that are populated by the kernel call
    // Set all the offsets to zero
    gpuErrchk(cudaMemset(SD_NumOf_DEs_Touching.data(), 0, nSDs * sizeof(unsigned int)));
    // For each SD, all the spheres touching that SD should have their ID be NULL_GRANULAR_ID
    gpuErrchk(cudaMemset(DEs_in_SD_composite.data(), NULL_GRANULAR_ID,
                         MAX_COUNT_OF_DEs_PER_SD * nSDs * sizeof(unsigned int)));

    // Figure our the number of blocks that need to be launched to cover the box
    printf("doing priming!\n");
    printf("max possible composite offset is %zu\n", (size_t)nSDs * MAX_COUNT_OF_DEs_PER_SD);
}

void ChSystemGranularMonodisperse_SMC_Frictionless_trimesh::meshSoup_applyRigidBodyMotion(
    double* position_orientation_data) {
    // Create both broadphase and narrowphase frames for each family
    for (unsigned int fam = 0; fam < meshSoup_DEVICE->nFamiliesInSoup; fam++) {
        generate_rot_matrix<float>(position_orientation_data + 7 * fam + 3, tri_params->fam_frame_broad[fam].rot_mat);
        tri_params->fam_frame_broad[fam].pos[0] = (float)position_orientation_data[7 * fam + 0];
        tri_params->fam_frame_broad[fam].pos[1] = (float)position_orientation_data[7 * fam + 1];
        tri_params->fam_frame_broad[fam].pos[2] = (float)position_orientation_data[7 * fam + 2];

        generate_rot_matrix<double>(position_orientation_data + 7 * fam + 3, tri_params->fam_frame_narrow[fam].rot_mat);
        tri_params->fam_frame_narrow[fam].pos[0] = position_orientation_data[7 * fam + 0];
        tri_params->fam_frame_narrow[fam].pos[1] = position_orientation_data[7 * fam + 1];
        tri_params->fam_frame_narrow[fam].pos[2] = position_orientation_data[7 * fam + 2];
    }
}

template <typename T>
void ChSystemGranularMonodisperse_SMC_Frictionless_trimesh::generate_rot_matrix(double* ep, T* rot_mat) {
    rot_mat[0] = ep[0] * ep[0] + ep[1] * ep[1] - ep[2] * ep[2] - ep[3] * ep[3];
    rot_mat[1] = 2 * (ep[1] * ep[2] + ep[0] * ep[3]);
    rot_mat[2] = 2 * (ep[1] * ep[3] - ep[0] * ep[2]);

    rot_mat[3] = 2 * (ep[1] * ep[2] - ep[0] * ep[3]);
    rot_mat[4] = ep[0] * ep[0] - ep[1] * ep[1] + ep[2] * ep[2] - ep[3] * ep[3];
    rot_mat[5] = 2 * (ep[2] * ep[3] + ep[0] * ep[1]);

    rot_mat[6] = 2 * (ep[1] * ep[3] + ep[0] * ep[2]);
    rot_mat[7] = 2 * (ep[2] * ep[3] - ep[0] * ep[1]);
    rot_mat[8] = ep[0] * ep[0] - ep[1] * ep[1] - ep[2] * ep[2] + ep[3] * ep[3];
}

}  // namespace granular
}  // namespace chrono