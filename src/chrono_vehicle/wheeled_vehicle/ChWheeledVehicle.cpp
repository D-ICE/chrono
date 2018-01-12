// =============================================================================
// PROJECT CHRONO - http://projectchrono.org
//
// Copyright (c) 2014 projectchrono.org
// All rights reserved.
//
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file at the top level of the distribution and at
// http://projectchrono.org/license-chrono.txt.
//
// =============================================================================
// Authors: Radu Serban, Justin Madsen
// =============================================================================
//
// Base class for a wheeled vehicle model.
//
// =============================================================================

#include "chrono_vehicle/wheeled_vehicle/ChWheeledVehicle.h"

namespace chrono {
namespace vehicle {

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
ChWheeledVehicle::ChWheeledVehicle(const std::string& name, ChMaterialSurface::ContactMethod contact_method)
    : ChVehicle(name, contact_method) {}

ChWheeledVehicle::ChWheeledVehicle(const std::string& name, ChSystem* system) : ChVehicle(name, system) {}

// -----------------------------------------------------------------------------
// Initialize this vehicle at the specified global location and orientation.
// This base class implementation only initializes the chassis subsystem.
// Derived classes must extend this function to initialize all other wheeled
// vehicle subsystems (steering, suspensions, wheels, brakes, and driveline).
// -----------------------------------------------------------------------------
void ChWheeledVehicle::Initialize(const ChCoordsys<>& chassisPos, double chassisFwdVel) {
    m_chassis->Initialize(m_system, chassisPos, chassisFwdVel, WheeledCollisionFamily::CHASSIS);
}

// -----------------------------------------------------------------------------
// Update the state of this vehicle at the current time.
// The vehicle system is provided the current driver inputs (throttle between
// 0 and 1, steering between -1 and +1, braking between 0 and 1), the torque
// from the powertrain, and tire forces (expressed in the global reference
// frame).
// The default implementation of this function invokes the update functions for
// all vehicle subsystems.
// -----------------------------------------------------------------------------
void ChWheeledVehicle::Synchronize(double time,
                                   double steering,
                                   double braking,
                                   double powertrain_torque,
                                   const TerrainForces& tire_forces) {
    // Apply powertrain torque to the driveline's input shaft.
    m_driveline->Synchronize(powertrain_torque);

    // Let the steering subsystems process the steering input.
    for (unsigned int i = 0; i < m_steerings.size(); i++) {
        m_steerings[i]->Synchronize(time, steering);
    }

    // Apply tire forces to spindle bodies and apply braking.
    for (unsigned int i = 0; i < m_suspensions.size(); i++) {
        m_suspensions[i]->Synchronize(LEFT, tire_forces[2 * i]);
        m_suspensions[i]->Synchronize(RIGHT, tire_forces[2 * i + 1]);

        m_brakes[2 * i]->Synchronize(braking);
        m_brakes[2 * i + 1]->Synchronize(braking);
    }

    m_chassis->Synchronize(time);
}

// -----------------------------------------------------------------------------
// Set visualization type for the various subsystems
// -----------------------------------------------------------------------------
void ChWheeledVehicle::SetSuspensionVisualizationType(VisualizationType vis) {
    for (size_t i = 0; i < m_suspensions.size(); ++i) {
        m_suspensions[i]->SetVisualizationType(vis);
    }
}

void ChWheeledVehicle::SetSteeringVisualizationType(VisualizationType vis) {
    for (size_t i = 0; i < m_steerings.size(); ++i) {
        m_steerings[i]->SetVisualizationType(vis);
    }
}

void ChWheeledVehicle::SetWheelVisualizationType(VisualizationType vis) {
    for (size_t i = 0; i < m_wheels.size(); ++i) {
        m_wheels[i]->SetVisualizationType(vis);
    }
}

// -----------------------------------------------------------------------------
// Enable/disable collision between the chassis and all other vehicle subsystems
// This only controls collisions between the chassis and the tire systems.
// -----------------------------------------------------------------------------
void ChWheeledVehicle::SetChassisVehicleCollide(bool state) {
    if (state) {
        // Chassis collides with tires
        m_chassis->GetBody()->GetCollisionModel()->SetFamilyMaskDoCollisionWithFamily(WheeledCollisionFamily::TIRES);
    } else {
        // Chassis does not collide with tires
        m_chassis->GetBody()->GetCollisionModel()->SetFamilyMaskNoCollisionWithFamily(WheeledCollisionFamily::TIRES);
    }
}

// -----------------------------------------------------------------------------
// Calculate and return the total vehicle mass
// -----------------------------------------------------------------------------
double ChWheeledVehicle::GetVehicleMass() const {
    double mass = m_chassis->GetMass();

    for (auto susp : m_suspensions) {
        mass += susp->GetMass();
    }
    for (auto antiroll : m_antirollbars) {
        mass += antiroll->GetMass();
    }
    for (auto steering : m_steerings) {
        mass += steering->GetMass();
    }
    for (auto wheel : m_wheels) {
        mass += wheel->GetMass();
    }

    return mass;
}

// -----------------------------------------------------------------------------
// Calculate and return the current vehicle COM location
// -----------------------------------------------------------------------------
ChVector<> ChWheeledVehicle::GetVehicleCOMPos() const {
    ChVector<> com(0, 0, 0);

    com += m_chassis->GetMass() * m_chassis->GetCOMPos();
    for (auto susp : m_suspensions) {
        com += susp->GetMass() * susp->GetCOMPos();
    }
    for (auto antiroll : m_antirollbars) {
        com += antiroll->GetMass() * antiroll->GetCOMPos();
    }
    for (auto steering : m_steerings) {
        com += steering->GetMass() * steering->GetCOMPos();
    }
    for (auto wheel : m_wheels) {
        com += wheel->GetMass() * wheel->GetCOMPos();
    }

    return com / GetVehicleMass();
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
std::shared_ptr<ChBody> ChWheeledVehicle::GetWheelBody(const WheelID& wheel_id) const {
    return m_suspensions[wheel_id.axle()]->GetSpindle(wheel_id.side());
}

const ChVector<>& ChWheeledVehicle::GetWheelPos(const WheelID& wheel_id) const {
    return m_suspensions[wheel_id.axle()]->GetSpindlePos(wheel_id.side());
}

const ChQuaternion<>& ChWheeledVehicle::GetWheelRot(const WheelID& wheel_id) const {
    return m_suspensions[wheel_id.axle()]->GetSpindleRot(wheel_id.side());
}

const ChVector<>& ChWheeledVehicle::GetWheelLinVel(const WheelID& wheel_id) const {
    return m_suspensions[wheel_id.axle()]->GetSpindleLinVel(wheel_id.side());
}

ChVector<> ChWheeledVehicle::GetWheelAngVel(const WheelID& wheel_id) const {
    return m_suspensions[wheel_id.axle()]->GetSpindleAngVel(wheel_id.side());
}

double ChWheeledVehicle::GetWheelOmega(const WheelID& wheel_id) const {
    return m_suspensions[wheel_id.axle()]->GetAxleSpeed(wheel_id.side());
}

// -----------------------------------------------------------------------------
// Return the complete state (expressed in the global frame) for the specified
// wheel body.
// -----------------------------------------------------------------------------
WheelState ChWheeledVehicle::GetWheelState(const WheelID& wheel_id) const {
    WheelState state;

    state.pos = GetWheelPos(wheel_id);
    state.rot = GetWheelRot(wheel_id);
    state.lin_vel = GetWheelLinVel(wheel_id);
    state.ang_vel = GetWheelAngVel(wheel_id);

    ChVector<> ang_vel_loc = state.rot.RotateBack(state.ang_vel);
    state.omega = ang_vel_loc.y();

    return state;
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
double ChWheeledVehicle::GetDriveshaftSpeed() const {
    return m_driveline->GetDriveshaftSpeed();
}

// -----------------------------------------------------------------------------
// Log constraint violations
// -----------------------------------------------------------------------------
void ChWheeledVehicle::LogConstraintViolations() {
    GetLog().SetNumFormat("%16.4e");

    // Report constraint violations for the suspension joints
    for (size_t i = 0; i < m_suspensions.size(); i++) {
        GetLog() << "\n---- AXLE " << i << " LEFT side suspension constraint violations\n\n";
        m_suspensions[i]->LogConstraintViolations(LEFT);
        GetLog() << "\n---- AXLE " << i << " RIGHT side suspension constraint violations\n\n";
        m_suspensions[i]->LogConstraintViolations(RIGHT);
    }

    // Report constraint violations for the steering joints
    for (size_t i = 0; i < m_steerings.size(); i++) {
        GetLog() << "\n---- STEERING subsystem " << i << " constraint violations\n\n";
        m_steerings[i]->LogConstraintViolations();
    }

    GetLog().SetNumFormat("%g");
}

}  // end namespace vehicle
}  // end namespace chrono
