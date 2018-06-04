// =============================================================================
// PROJECT CHRONO - http://projectchrono.org
//
// Copyright (c) 2015 projectchrono.org
// All right reserved.
//
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file at the top level of the distribution and at
// http://projectchrono.org/license-chrono.txt.
//
// =============================================================================
// Authors: Radu Serban
// =============================================================================
//
// Base class for a full vehicle co-simulation node.
//
// The global reference frame has Z up, X towards the front of the vehicle, and
// Y pointing to the left.
//
// =============================================================================

#ifndef HMMWV_COSIM_BASENODE_H
#define HMMWV_COSIM_BASENODE_H

#include <mpi.h>
#include <fstream>
#include <iostream>
#include <string>

#include "chrono/core/ChTimer.h"
#include "chrono/core/ChVector.h"

/// Ranks (within MPI_COMMM_WORLD) of the processes involved in co-simulation communication.
#define VEHICLE_NODE_RANK 0
#define TERRAIN_NODE_RANK 1
#define TIRE_NODE_RANK(i) (i + 2)

class BaseNode {
  public:
    /// Set the integration step size (default: 1e-4).
    void SetStepSize(double step) { m_step_size = step; }

    /// Get the integration step size.
    double GetStepSize() const { return m_step_size; }

    /// Set the name of the output directory and an identifying suffix.
    /// Output files will be created in subdirectories named
    ///    dir_name/[NodeName]suffix/
    /// where [NodeName] is "VEHICLE", "TIRE", or "TERRAIN".
    virtual void SetOutDir(const std::string& dir_name, const std::string& suffix);

    /// Get the output directory name for this node.
    const std::string& GetOutDirName() const { return m_node_out_dir; }

    /// Get a string prefix for console output.
    const std::string& GetPrefix() const { return m_prefix; }

    /// Get the simulation time for the current step on this node.
    double GetSimTime() const { return m_timer.GetTimeSeconds(); }

    /// Get the cumulative simulation time on this node.
    double GetTotalSimTime() const { return m_cum_sim_time; }

    /// Initialize this node.
    /// This function allows the node to initialize itself and, optionally, perform an
    /// initial data exchange with any other node.
    virtual void Initialize() = 0;

    /// Synchronize this node.
    /// This function is called at every co-simulation synchronization time to
    /// allow the node to exchange information with any other node.
    virtual void Synchronize(int step_number, double time) = 0;

    /// Advance simulation.
    /// This function is called after a synchronization to allow the node to advance
    /// its state by the specified time step.  A node is allowed to take as many internal
    /// integration steps as required, but no inter-node communication should occur.
    virtual void Advance(double step_size) = 0;

    /// Output logging and debugging data.
    virtual void OutputData(int frame) {}

    /// Control log detail
    void SetVerbose(bool val) { m_verbose = val; }

  protected:
    BaseNode(const std::string& name = "");
    virtual ~BaseNode() {}

    double m_step_size;  ///< integration step size

    std::string m_name;          ///< name of the node
    std::string m_out_dir;       ///< top-level output directory
    std::string m_node_out_dir;  ///< node-specific output directory
    std::ofstream m_outf;        ///< output file stream
    std::string m_prefix;        ///< prefix for console output

    chrono::ChTimer<double> m_timer;  ///< timer for integration cost
    double m_cum_sim_time;            ///< cumulative integration cost

    bool m_verbose;

    static const chrono::ChVector<> m_gacc;
};

#endif
