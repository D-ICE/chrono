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
// Authors: Alessandro Tasora, Radu Serban
// =============================================================================

#include <algorithm>
#include <cstdlib>

#include "chrono/core/ChLinearAlgebra.h"
#include "chrono/core/ChTransform.h"
#include "chrono/physics/ChAssembly.h"
#include "chrono/physics/ChBodyAuxRef.h"
#include "chrono/physics/ChGlobal.h"
#include "chrono/physics/ChSystem.h"

namespace chrono {

using namespace collision;
using namespace geometry;

// Register into the object factory, to enable run-time dynamic creation and persistence
CH_FACTORY_REGISTER(ChAssembly)

ChAssembly::ChAssembly()
    : nbodies(0),
      nlinks(0),
      nphysicsitems(0),
      ndof(0),
      ndoc(0),
      ndoc_w(0),
      ndoc_w_C(0),
      ndoc_w_D(0),
      ncoords(0),
      ncoords_w(0),
      nsysvars(0),
      nsysvars_w(0),
      nbodies_sleep(0),
      nbodies_fixed(0) {}

ChAssembly::ChAssembly(const ChAssembly& other) : ChPhysicsItem(other) {
    nbodies = other.nbodies;
    nlinks = other.nlinks;
    nphysicsitems = other.nphysicsitems;
    ncoords = other.ncoords;
    ncoords_w = other.ncoords_w;
    ndoc = other.ndoc;
    ndoc_w = other.ndoc_w;
    ndoc_w_C = other.ndoc_w_C;
    ndoc_w_D = other.ndoc_w_D;
    ndof = other.ndof;
    nsysvars = other.nsysvars;
    nsysvars_w = other.nsysvars_w;
    nbodies_sleep = other.nbodies_sleep;
    nbodies_fixed = other.nbodies_fixed;

    //// RADU
    //// TODO:  deep copy of the object lists (bodylist, linklist, otherphysicslist)
}

ChAssembly::~ChAssembly() {
    RemoveAllBodies();
    RemoveAllLinks();
    RemoveAllOtherPhysicsItems();
}

void ChAssembly::Clear() {
    RemoveAllLinks();
    RemoveAllBodies();
    RemoveAllOtherPhysicsItems();

    nbodies = 0;
    nlinks = 0;
    nphysicsitems = 0;
    ndof = 0;
    ndoc = 0;
    ndoc_w = 0;
    ndoc_w_C = 0;
    ndoc_w_D = 0;
    nsysvars_w = 0;
    ncoords = 0;
    ncoords_w = 0;
    nsysvars = 0;
    ncoords_w = 0;
    nbodies_sleep = 0;
    nbodies_fixed = 0;
}

void ChAssembly::AddBody(std::shared_ptr<ChBody> newbody) {
    assert(std::find<std::vector<std::shared_ptr<ChBody>>::iterator>(bodylist.begin(), bodylist.end(), newbody) ==
           bodylist.end());
    assert(newbody->GetSystem() == 0);  // should remove from other system before adding here

    // set system and also add collision models to system
    newbody->SetSystem(this->GetSystem());
    bodylist.push_back(newbody);
}

void ChAssembly::RemoveBody(std::shared_ptr<ChBody> mbody) {
    assert(std::find<std::vector<std::shared_ptr<ChBody>>::iterator>(bodylist.begin(), bodylist.end(), mbody) !=
           bodylist.end());

    // warning! linear time search, to erase pointer from container.
    bodylist.erase(std::find<std::vector<std::shared_ptr<ChBody>>::iterator>(bodylist.begin(), bodylist.end(), mbody));

    // nullify backward link to system and also remove from collision system
    mbody->SetSystem(0);
}

void ChAssembly::AddLink(std::shared_ptr<ChLink> newlink) {
    assert(std::find<std::vector<std::shared_ptr<ChLink>>::iterator>(linklist.begin(), linklist.end(), newlink) ==
           linklist.end());

    newlink->SetSystem(this->GetSystem());
    linklist.push_back(newlink);
}

void ChAssembly::RemoveLink(std::shared_ptr<ChLink> mlink) {
    assert(std::find<std::vector<std::shared_ptr<ChLink>>::iterator>(linklist.begin(), linklist.end(), mlink) !=
           linklist.end());

    // warning! linear time search, to erase pointer from container!
    linklist.erase(std::find<std::vector<std::shared_ptr<ChLink>>::iterator>(linklist.begin(), linklist.end(), mlink));

    // nullify backward link to system
    mlink->SetSystem(0);
}

void ChAssembly::AddOtherPhysicsItem(std::shared_ptr<ChPhysicsItem> newitem) {
    assert(std::find<std::vector<std::shared_ptr<ChPhysicsItem>>::iterator>(
               otherphysicslist.begin(), otherphysicslist.end(), newitem) == otherphysicslist.end());
    // assert(newitem->GetSystem()==0); // should remove from other system before adding here

    // set system and also add collision models to system
    newitem->SetSystem(this->GetSystem());
    otherphysicslist.push_back(newitem);
}

void ChAssembly::RemoveOtherPhysicsItem(std::shared_ptr<ChPhysicsItem> mitem) {
    assert(std::find<std::vector<std::shared_ptr<ChPhysicsItem>>::iterator>(
               otherphysicslist.begin(), otherphysicslist.end(), mitem) != otherphysicslist.end());

    // warning! linear time search, to erase pointer from container.
    otherphysicslist.erase(std::find<std::vector<std::shared_ptr<ChPhysicsItem>>::iterator>(
        otherphysicslist.begin(), otherphysicslist.end(), mitem));

    // nullify backward link to system and also remove from collision system
    mitem->SetSystem(0);
}

void ChAssembly::Add(std::shared_ptr<ChPhysicsItem> newitem) {
    if (auto item = std::dynamic_pointer_cast<ChBody>(newitem)) {
        AddBody(item);
        return;
    }

    if (auto item = std::dynamic_pointer_cast<ChLink>(newitem)) {
        AddLink(item);
        return;
    }

    AddOtherPhysicsItem(newitem);
}

void ChAssembly::AddBatch(std::shared_ptr<ChPhysicsItem> newitem) {
    this->batch_to_insert.push_back(newitem);
}

void ChAssembly::FlushBatch() {
    for (int i = 0; i < this->batch_to_insert.size(); ++i) {
        this->Add(batch_to_insert[i]);
    }
    batch_to_insert.clear();
}

void ChAssembly::Remove(std::shared_ptr<ChPhysicsItem> newitem) {
    if (auto item = std::dynamic_pointer_cast<ChBody>(newitem)) {
        RemoveBody(item);
        return;
    }

    if (auto item = std::dynamic_pointer_cast<ChLink>(newitem)) {
        RemoveLink(item);
        return;
    }

    RemoveOtherPhysicsItem(newitem);
}

void ChAssembly::RemoveAllBodies() {
    for (unsigned int ip = 0; ip < bodylist.size(); ++ip) {
        // nullify backward link to system and also remove from collision system
        bodylist[ip]->SetSystem(0);
    }
    bodylist.clear();
}

void ChAssembly::RemoveAllLinks() {
    for (unsigned int ip = 0; ip < linklist.size(); ++ip) {
        // nullify backward link to system
        linklist[ip]->SetSystem(0);
    }
    linklist.clear();
}

void ChAssembly::RemoveAllOtherPhysicsItems() {
    for (unsigned int ip = 0; ip < otherphysicslist.size(); ++ip) {
        // nullify backward link to system and also remove from collision system
        otherphysicslist[ip]->SetSystem(0);
    }
    otherphysicslist.clear();
}

std::shared_ptr<ChBody> ChAssembly::SearchBody(const char* m_name) {
    return ChContainerSearchFromName<std::shared_ptr<ChBody>, std::vector<std::shared_ptr<ChBody>>::iterator>(
        m_name, bodylist.begin(), bodylist.end());
}

std::shared_ptr<ChLink> ChAssembly::SearchLink(const char* m_name) {
    return ChContainerSearchFromName<std::shared_ptr<ChLink>, std::vector<std::shared_ptr<ChLink>>::iterator>(
        m_name, linklist.begin(), linklist.end());
}

std::shared_ptr<ChPhysicsItem> ChAssembly::SearchOtherPhysicsItem(const char* m_name) {
    return ChContainerSearchFromName<std::shared_ptr<ChPhysicsItem>,
                                     std::vector<std::shared_ptr<ChPhysicsItem>>::iterator>(
        m_name, otherphysicslist.begin(), otherphysicslist.end());
}

std::shared_ptr<ChPhysicsItem> ChAssembly::Search(const char* m_name) {
    if (auto mbo = SearchBody(m_name))
        return mbo;

    if (auto mli = SearchLink(m_name))
        return mli;

    if (auto mph = SearchOtherPhysicsItem(m_name))
        return mph;

    return std::shared_ptr<ChPhysicsItem>();  // not found; return an empty shared_ptr
}

std::shared_ptr<ChMarker> ChAssembly::SearchMarker(const char* m_name) {
    // Iterate over all bodies and search in the body's marker list
    for (unsigned int ip = 0; ip < bodylist.size(); ++ip) {
        if (auto mmark = bodylist[ip]->SearchMarker(m_name))
            return mmark;
    }

    // Iterate over all physics items and search in the marker lists of ChBodyAuxRef
    for (unsigned int ip = 0; ip < otherphysicslist.size(); ++ip) {
        if (auto mbodyauxref = std::dynamic_pointer_cast<ChBodyAuxRef>(otherphysicslist[ip])) {
            if (auto mmark = mbodyauxref->SearchMarker(m_name))
                return mmark;
        }
    }

    return (std::shared_ptr<ChMarker>());  // not found; return an empty shared_ptr
}

std::shared_ptr<ChMarker> ChAssembly::SearchMarker(int markID) {
    // Iterate over all bodies and search in the body's marker list
    for (unsigned int ip = 0; ip < bodylist.size(); ++ip) {
        if (auto res = ChContainerSearchFromID<std::shared_ptr<ChMarker>,
                                               std::vector<std::shared_ptr<ChMarker>>::const_iterator>(
                markID, bodylist[ip]->GetMarkerList().begin(), bodylist[ip]->GetMarkerList().end()))
            return res;
    }

    // Iterate over all physics items and search in the marker lists of ChBodyAuxRef
    for (unsigned int ip = 0; ip < otherphysicslist.size(); ++ip) {
        if (auto mbodyauxref = std::dynamic_pointer_cast<ChBodyAuxRef>(otherphysicslist[ip])) {
            if (auto res = ChContainerSearchFromID<std::shared_ptr<ChMarker>,
                                                   std::vector<std::shared_ptr<ChMarker>>::const_iterator>(
                    markID, mbodyauxref->GetMarkerList().begin(), mbodyauxref->GetMarkerList().end()))
                return res;
        }
    }

    return (std::shared_ptr<ChMarker>());  // not found; return an empty shared_ptr
}

// -----------------------------------------------------------------------------

void ChAssembly::SetSystem(ChSystem* m_system) {
    system = m_system;

    for (int ip = 0; ip < bodylist.size(); ++ip) {
        bodylist[ip]->SetSystem(m_system);
    }
    for (int ip = 0; ip < linklist.size(); ++ip) {
        linklist[ip]->SetSystem(m_system);
    }
    for (int ip = 0; ip < otherphysicslist.size(); ++ip) {
        otherphysicslist[ip]->SetSystem(m_system);
    }
}

void ChAssembly::SyncCollisionModels() {
    for (int ip = 0; ip < bodylist.size(); ++ip) {
        bodylist[ip]->SyncCollisionModels();
    }
    for (int ip = 0; ip < linklist.size(); ++ip) {
        linklist[ip]->SyncCollisionModels();
    }
    for (int ip = 0; ip < otherphysicslist.size(); ++ip) {
        otherphysicslist[ip]->SyncCollisionModels();
    }
}

// -----------------------------------------------------------------------------
// UPDATING ROUTINES

// COUNT ALL BODIES AND LINKS, ETC, COMPUTE &SET DOF FOR STATISTICS,
// ALLOCATES OR REALLOCATE BOOKKEEPING DATA/VECTORS, IF ANY
void ChAssembly::Setup() {
    nbodies = 0;
    nbodies_sleep = 0;
    nbodies_fixed = 0;
    ncoords = 0;
    ncoords_w = 0;
    ndoc = 0;
    ndoc_w = 0;
    ndoc_w_C = 0;
    ndoc_w_D = 0;
    nlinks = 0;
    nphysicsitems = 0;

    // Any item being queued for insertion in system's lists? add it.
    this->FlushBatch();

    for (unsigned int ip = 0; ip < bodylist.size(); ++ip)  // ITERATE on bodies
    {
        std::shared_ptr<ChBody> Bpointer = bodylist[ip];

        if (Bpointer->GetBodyFixed())
            nbodies_fixed++;
        else if (Bpointer->GetSleeping())
            nbodies_sleep++;
        else {
            nbodies++;

            Bpointer->SetOffset_x(this->offset_x + ncoords);
            Bpointer->SetOffset_w(this->offset_w + ncoords_w);
            Bpointer->SetOffset_L(this->offset_L + ndoc_w);

            // Bpointer->Setup(); // not needed since in bodies does nothing

            ncoords += Bpointer->GetDOF();
            ncoords_w += Bpointer->GetDOF_w();
            ndoc_w += Bpointer->GetDOC();      // unneeded since ChBody introduces no constraints
            ndoc_w_C += Bpointer->GetDOC_c();  // unneeded since ChBody introduces no constraints
            ndoc_w_D += Bpointer->GetDOC_d();  // unneeded since ChBody introduces no constraints
        }
    }

    ndoc += nbodies;  // add one quaternion constr. for each active body.

    for (unsigned int ip = 0; ip < otherphysicslist.size(); ++ip)  // ITERATE on other physics
    {
        std::shared_ptr<ChPhysicsItem> PHpointer = otherphysicslist[ip];

        nphysicsitems++;

        PHpointer->SetOffset_x(this->offset_x + ncoords);
        PHpointer->SetOffset_w(this->offset_w + ncoords_w);
        PHpointer->SetOffset_L(this->offset_L + ndoc_w);

        PHpointer->Setup();  // compute DOFs etc. and sets the offsets also in child items, if assembly-type or
                             // mesh-type stuff

        ncoords += PHpointer->GetDOF();
        ncoords_w += PHpointer->GetDOF_w();
        ndoc_w += PHpointer->GetDOC();
        ndoc_w_C += PHpointer->GetDOC_c();
        ndoc_w_D += PHpointer->GetDOC_d();
    }

    for (unsigned int ip = 0; ip < linklist.size(); ++ip)  // ITERATE on links
    {
        std::shared_ptr<ChLink> Lpointer = linklist[ip];

        if (Lpointer->IsActive()) {
            nlinks++;

            Lpointer->SetOffset_x(this->offset_x + ncoords);
            Lpointer->SetOffset_w(this->offset_w + ncoords_w);
            Lpointer->SetOffset_L(this->offset_L + ndoc_w);

            Lpointer->Setup();  // compute DOFs etc. and sets the offsets also in child items, if any

            ncoords += Lpointer->GetDOF();
            ncoords_w += Lpointer->GetDOF_w();
            ndoc_w += Lpointer->GetDOC();
            ndoc_w_C += Lpointer->GetDOC_c();
            ndoc_w_D += Lpointer->GetDOC_d();
        }
    }

    ndoc = ndoc_w + nbodies;          // number of constraints including quaternion constraints.
    nsysvars = ncoords + ndoc;        // total number of variables (coordinates + lagrangian multipliers)
    nsysvars_w = ncoords_w + ndoc_w;  // total number of variables (with 6 dof per body)

    ndof =
        ncoords_w - ndoc_w;  // number of degrees of freedom (approximate - does not consider constr. redundancy, etc)
}

// Update assemblies own properties first (ChTime and assets, if any).
// Then update all contents of this assembly.
void ChAssembly::Update(double mytime, bool update_assets) {
    ChPhysicsItem::Update(mytime, update_assets);
    Update(update_assets);
}

// - ALL PHYSICAL ITEMS (BODIES, LINKS,ETC.) ARE UPDATED,
//   ALSO UPDATING THEIR AUXILIARY VARIABLES (ROT.MATRICES, ETC.).
// - UPDATES ALL FORCES  (AUTOMATIC, AS CHILDREN OF BODIES)
// - UPDATES ALL MARKERS (AUTOMATIC, AS CHILDREN OF BODIES).
void ChAssembly::Update(bool update_assets) {
    for (int ip = 0; ip < bodylist.size(); ++ip) {
        bodylist[ip]->Update(ChTime, update_assets);
    }
    for (unsigned int ip = 0; ip < otherphysicslist.size(); ++ip) {
        otherphysicslist[ip]->Update(ChTime, update_assets);
    }
    for (unsigned int ip = 0; ip < linklist.size(); ++ip) {
        linklist[ip]->Update(ChTime, update_assets);
    }
}

void ChAssembly::SetNoSpeedNoAcceleration() {
    for (int ip = 0; ip < bodylist.size(); ++ip) {
        bodylist[ip]->SetNoSpeedNoAcceleration();
    }
    for (int ip = 0; ip < linklist.size(); ++ip) {
        linklist[ip]->SetNoSpeedNoAcceleration();
    }
    for (int ip = 0; ip < otherphysicslist.size(); ++ip) {
        otherphysicslist[ip]->SetNoSpeedNoAcceleration();
    }
}

void ChAssembly::IntStateGather(const unsigned int off_x,
                                ChState& x,
                                const unsigned int off_v,
                                ChStateDelta& v,
                                double& T) {
    unsigned int displ_x = off_x - this->offset_x;
    unsigned int displ_v = off_v - this->offset_w;

    for (unsigned int ip = 0; ip < bodylist.size(); ++ip) {
        std::shared_ptr<ChBody> Bpointer = bodylist[ip];
        if (Bpointer->IsActive())
            Bpointer->IntStateGather(displ_x + Bpointer->GetOffset_x(), x, displ_v + Bpointer->GetOffset_w(), v, T);
    }
    for (unsigned int ip = 0; ip < linklist.size(); ++ip) {
        std::shared_ptr<ChLink> Lpointer = linklist[ip];
        if (Lpointer->IsActive())
            Lpointer->IntStateGather(displ_x + Lpointer->GetOffset_x(), x, displ_v + Lpointer->GetOffset_w(), v, T);
    }
    for (unsigned int ip = 0; ip < otherphysicslist.size(); ++ip) {
        std::shared_ptr<ChPhysicsItem> Ppointer = otherphysicslist[ip];
        Ppointer->IntStateGather(displ_x + Ppointer->GetOffset_x(), x, displ_v + Ppointer->GetOffset_w(), v, T);
    }
    T = this->GetChTime();
}

void ChAssembly::IntStateScatter(const unsigned int off_x,
                                 const ChState& x,
                                 const unsigned int off_v,
                                 const ChStateDelta& v,
                                 const double T) {
    unsigned int displ_x = off_x - this->offset_x;
    unsigned int displ_v = off_v - this->offset_w;

    for (unsigned int ip = 0; ip < bodylist.size(); ++ip) {
        std::shared_ptr<ChBody> Bpointer = bodylist[ip];
        if (Bpointer->IsActive())
            Bpointer->IntStateScatter(displ_x + Bpointer->GetOffset_x(), x, displ_v + Bpointer->GetOffset_w(), v, T);
    }
    for (unsigned int ip = 0; ip < linklist.size(); ++ip) {
        std::shared_ptr<ChLink> Lpointer = linklist[ip];
        if (Lpointer->IsActive())
            Lpointer->IntStateScatter(displ_x + Lpointer->GetOffset_x(), x, displ_v + Lpointer->GetOffset_w(), v, T);
    }
    for (unsigned int ip = 0; ip < otherphysicslist.size(); ++ip) {
        std::shared_ptr<ChPhysicsItem> Ppointer = otherphysicslist[ip];
        Ppointer->IntStateScatter(displ_x + Ppointer->GetOffset_x(), x, displ_v + Ppointer->GetOffset_w(), v, T);
    }
    this->SetChTime(T);

    // Note: all those IntStateScatter() above should call Update() automatically
    // for each object in the loop, therefore:
    // -do not call Update() on this.
    // -do not call ChPhysicsItem::IntStateScatter() -it calls this->Update() anyway-
    // because this would cause redundant updates.
}

void ChAssembly::IntStateGatherAcceleration(const unsigned int off_a, ChStateDelta& a) {
    unsigned int displ_a = off_a - this->offset_w;

    for (unsigned int ip = 0; ip < bodylist.size(); ++ip) {
        std::shared_ptr<ChBody> Bpointer = bodylist[ip];
        if (Bpointer->IsActive())
            Bpointer->IntStateGatherAcceleration(displ_a + Bpointer->GetOffset_w(), a);
    }
    for (unsigned int ip = 0; ip < linklist.size(); ++ip) {
        std::shared_ptr<ChLink> Lpointer = linklist[ip];
        if (Lpointer->IsActive())
            Lpointer->IntStateGatherAcceleration(displ_a + Lpointer->GetOffset_w(), a);
    }
    for (unsigned int ip = 0; ip < otherphysicslist.size(); ++ip) {
        std::shared_ptr<ChPhysicsItem> Ppointer = otherphysicslist[ip];
        Ppointer->IntStateGatherAcceleration(displ_a + Ppointer->GetOffset_w(), a);
    }
}

// From state derivative (acceleration) to system, sometimes might be needed
void ChAssembly::IntStateScatterAcceleration(const unsigned int off_a, const ChStateDelta& a) {
    unsigned int displ_a = off_a - this->offset_w;

    for (unsigned int ip = 0; ip < bodylist.size(); ++ip) {
        std::shared_ptr<ChBody> Bpointer = bodylist[ip];
        if (Bpointer->IsActive())
            Bpointer->IntStateScatterAcceleration(displ_a + Bpointer->GetOffset_w(), a);
    }
    for (unsigned int ip = 0; ip < linklist.size(); ++ip) {
        std::shared_ptr<ChLink> Lpointer = linklist[ip];
        if (Lpointer->IsActive())
            Lpointer->IntStateScatterAcceleration(displ_a + Lpointer->GetOffset_w(), a);
    }
    for (unsigned int ip = 0; ip < otherphysicslist.size(); ++ip) {
        std::shared_ptr<ChPhysicsItem> Ppointer = otherphysicslist[ip];
        Ppointer->IntStateScatterAcceleration(displ_a + Ppointer->GetOffset_w(), a);
    }
}

// From system to reaction forces (last computed) - some timestepper might need this
void ChAssembly::IntStateGatherReactions(const unsigned int off_L, ChVectorDynamic<>& L) {
    unsigned int displ_L = off_L - this->offset_L;

    for (unsigned int ip = 0; ip < bodylist.size(); ++ip) {
        std::shared_ptr<ChBody> Bpointer = bodylist[ip];
        if (Bpointer->IsActive())
            Bpointer->IntStateGatherReactions(displ_L + Bpointer->GetOffset_L(), L);
    }
    for (unsigned int ip = 0; ip < linklist.size(); ++ip) {
        std::shared_ptr<ChLink> Lpointer = linklist[ip];
        if (Lpointer->IsActive())
            Lpointer->IntStateGatherReactions(displ_L + Lpointer->GetOffset_L(), L);
    }
    for (unsigned int ip = 0; ip < otherphysicslist.size(); ++ip) {
        std::shared_ptr<ChPhysicsItem> Ppointer = otherphysicslist[ip];
        Ppointer->IntStateGatherReactions(displ_L + Ppointer->GetOffset_L(), L);
    }
}

// From reaction forces to system, ex. store last computed reactions in ChLink objects for plotting etc.
void ChAssembly::IntStateScatterReactions(const unsigned int off_L, const ChVectorDynamic<>& L) {
    unsigned int displ_L = off_L - this->offset_L;

    for (unsigned int ip = 0; ip < bodylist.size(); ++ip) {
        std::shared_ptr<ChBody> Bpointer = bodylist[ip];
        if (Bpointer->IsActive())
            Bpointer->IntStateScatterReactions(displ_L + Bpointer->GetOffset_L(), L);
    }
    for (unsigned int ip = 0; ip < linklist.size(); ++ip) {
        std::shared_ptr<ChLink> Lpointer = linklist[ip];
        if (Lpointer->IsActive())
            Lpointer->IntStateScatterReactions(displ_L + Lpointer->GetOffset_L(), L);
    }
    for (unsigned int ip = 0; ip < otherphysicslist.size(); ++ip) {
        std::shared_ptr<ChPhysicsItem> Ppointer = otherphysicslist[ip];
        Ppointer->IntStateScatterReactions(displ_L + Ppointer->GetOffset_L(), L);
    }
}

void ChAssembly::IntStateIncrement(const unsigned int off_x,
                                   ChState& x_new,
                                   const ChState& x,
                                   const unsigned int off_v,
                                   const ChStateDelta& Dv) {
    unsigned int displ_x = off_x - this->offset_x;
    unsigned int displ_v = off_v - this->offset_w;

    for (int ip = 0; ip < bodylist.size(); ++ip) {
        std::shared_ptr<ChBody> Bpointer = bodylist[ip];
        if (Bpointer->IsActive())
            Bpointer->IntStateIncrement(displ_x + Bpointer->GetOffset_x(), x_new, x, displ_v + Bpointer->GetOffset_w(),
                                        Dv);
    }

    for (int ip = 0; ip < linklist.size(); ++ip) {
        std::shared_ptr<ChLink> Lpointer = linklist[ip];
        if (Lpointer->IsActive())
            Lpointer->IntStateIncrement(displ_x + Lpointer->GetOffset_x(), x_new, x, displ_v + Lpointer->GetOffset_w(),
                                        Dv);
    }

    for (int ip = 0; ip < otherphysicslist.size(); ++ip) {
        std::shared_ptr<ChPhysicsItem> Ppointer = otherphysicslist[ip];
        Ppointer->IntStateIncrement(displ_x + Ppointer->GetOffset_x(), x_new, x, displ_v + Ppointer->GetOffset_w(), Dv);
    }
}

void ChAssembly::IntLoadResidual_F(const unsigned int off,  ///< offset in R residual
                                   ChVectorDynamic<>& R,    ///< result: the R residual, R += c*F
                                   const double c)          ///< a scaling factor
{
    unsigned int displ_v = off - this->offset_w;

    for (unsigned int ip = 0; ip < bodylist.size(); ++ip) {
        std::shared_ptr<ChBody> Bpointer = bodylist[ip];
        if (Bpointer->IsActive())
            Bpointer->IntLoadResidual_F(displ_v + Bpointer->GetOffset_w(), R, c);
    }
    for (unsigned int ip = 0; ip < linklist.size(); ++ip) {
        std::shared_ptr<ChLink> Lpointer = linklist[ip];
        if (Lpointer->IsActive())
            Lpointer->IntLoadResidual_F(displ_v + Lpointer->GetOffset_w(), R, c);
    }
    for (unsigned int ip = 0; ip < otherphysicslist.size(); ++ip) {
        std::shared_ptr<ChPhysicsItem> Ppointer = otherphysicslist[ip];
        Ppointer->IntLoadResidual_F(displ_v + Ppointer->GetOffset_w(), R, c);
    }
}

void ChAssembly::IntLoadResidual_Mv(const unsigned int off,      ///< offset in R residual
                                    ChVectorDynamic<>& R,        ///< result: the R residual, R += c*M*v
                                    const ChVectorDynamic<>& w,  ///< the w vector
                                    const double c               ///< a scaling factor
) {
    unsigned int displ_v = off - this->offset_w;

    for (unsigned int ip = 0; ip < bodylist.size(); ++ip) {
        std::shared_ptr<ChBody> Bpointer = bodylist[ip];
        if (Bpointer->IsActive())
            Bpointer->IntLoadResidual_Mv(displ_v + Bpointer->GetOffset_w(), R, w, c);
    }
    for (unsigned int ip = 0; ip < linklist.size(); ++ip) {
        std::shared_ptr<ChLink> Lpointer = linklist[ip];
        if (Lpointer->IsActive())
            Lpointer->IntLoadResidual_Mv(displ_v + Lpointer->GetOffset_w(), R, w, c);
    }
    for (unsigned int ip = 0; ip < otherphysicslist.size(); ++ip) {
        std::shared_ptr<ChPhysicsItem> Ppointer = otherphysicslist[ip];
        Ppointer->IntLoadResidual_Mv(displ_v + Ppointer->GetOffset_w(), R, w, c);
    }
}

void ChAssembly::IntLoadResidual_CqL(const unsigned int off_L,    ///< offset in L multipliers
                                     ChVectorDynamic<>& R,        ///< result: the R residual, R += c*Cq'*L
                                     const ChVectorDynamic<>& L,  ///< the L vector
                                     const double c               ///< a scaling factor
) {
    unsigned int displ_L = off_L - this->offset_L;

    for (unsigned int ip = 0; ip < bodylist.size(); ++ip) {
        std::shared_ptr<ChBody> Bpointer = bodylist[ip];
        if (Bpointer->IsActive())
            Bpointer->IntLoadResidual_CqL(displ_L + Bpointer->GetOffset_L(), R, L, c);
    }
    for (unsigned int ip = 0; ip < linklist.size(); ++ip) {
        std::shared_ptr<ChLink> Lpointer = linklist[ip];
        if (Lpointer->IsActive())
            Lpointer->IntLoadResidual_CqL(displ_L + Lpointer->GetOffset_L(), R, L, c);
    }
    for (unsigned int ip = 0; ip < otherphysicslist.size(); ++ip) {
        std::shared_ptr<ChPhysicsItem> Ppointer = otherphysicslist[ip];
        Ppointer->IntLoadResidual_CqL(displ_L + Ppointer->GetOffset_L(), R, L, c);
    }
}

void ChAssembly::IntLoadConstraint_C(const unsigned int off_L,  ///< offset in Qc residual
                                     ChVectorDynamic<>& Qc,     ///< result: the Qc residual, Qc += c*C
                                     const double c,            ///< a scaling factor
                                     bool do_clamp,             ///< apply clamping to c*C?
                                     double recovery_clamp      ///< value for min/max clamping of c*C
) {
    unsigned int displ_L = off_L - this->offset_L;

    for (unsigned int ip = 0; ip < bodylist.size(); ++ip) {
        std::shared_ptr<ChBody> Bpointer = bodylist[ip];
        if (Bpointer->IsActive())
            Bpointer->IntLoadConstraint_C(displ_L + Bpointer->GetOffset_L(), Qc, c, do_clamp, recovery_clamp);
    }
    for (unsigned int ip = 0; ip < linklist.size(); ++ip) {
        std::shared_ptr<ChLink> Lpointer = linklist[ip];
        if (Lpointer->IsActive())
            Lpointer->IntLoadConstraint_C(displ_L + Lpointer->GetOffset_L(), Qc, c, do_clamp, recovery_clamp);
    }
    for (unsigned int ip = 0; ip < otherphysicslist.size(); ++ip) {
        std::shared_ptr<ChPhysicsItem> Ppointer = otherphysicslist[ip];
        Ppointer->IntLoadConstraint_C(displ_L + Ppointer->GetOffset_L(), Qc, c, do_clamp, recovery_clamp);
    }
}

void ChAssembly::IntLoadConstraint_Ct(const unsigned int off_L,  ///< offset in Qc residual
                                      ChVectorDynamic<>& Qc,     ///< result: the Qc residual, Qc += c*Ct
                                      const double c             ///< a scaling factor
) {
    unsigned int displ_L = off_L - this->offset_L;

    for (unsigned int ip = 0; ip < bodylist.size(); ++ip) {
        std::shared_ptr<ChBody> Bpointer = bodylist[ip];
        if (Bpointer->IsActive())
            Bpointer->IntLoadConstraint_Ct(displ_L + Bpointer->GetOffset_L(), Qc, c);
    }
    for (unsigned int ip = 0; ip < linklist.size(); ++ip) {
        std::shared_ptr<ChLink> Lpointer = linklist[ip];
        if (Lpointer->IsActive())
            Lpointer->IntLoadConstraint_Ct(displ_L + Lpointer->GetOffset_L(), Qc, c);
    }
    for (unsigned int ip = 0; ip < otherphysicslist.size(); ++ip) {
        std::shared_ptr<ChPhysicsItem> Ppointer = otherphysicslist[ip];
        Ppointer->IntLoadConstraint_Ct(displ_L + Ppointer->GetOffset_L(), Qc, c);
    }
}

void ChAssembly::IntToDescriptor(const unsigned int off_v,
                                 const ChStateDelta& v,
                                 const ChVectorDynamic<>& R,
                                 const unsigned int off_L,
                                 const ChVectorDynamic<>& L,
                                 const ChVectorDynamic<>& Qc) {
    unsigned int displ_L = off_L - this->offset_L;
    unsigned int displ_v = off_v - this->offset_w;

    for (int ip = 0; ip < bodylist.size(); ++ip) {
        std::shared_ptr<ChBody> Bpointer = bodylist[ip];
        if (Bpointer->IsActive())
            Bpointer->IntToDescriptor(displ_v + Bpointer->GetOffset_w(), v, R, displ_L + Bpointer->GetOffset_L(), L,
                                      Qc);
    }

    for (int ip = 0; ip < linklist.size(); ++ip) {
        std::shared_ptr<ChLink> Lpointer = linklist[ip];
        if (Lpointer->IsActive())
            Lpointer->IntToDescriptor(displ_v + Lpointer->GetOffset_w(), v, R, displ_L + Lpointer->GetOffset_L(), L,
                                      Qc);
    }

    for (int ip = 0; ip < otherphysicslist.size(); ++ip) {
        std::shared_ptr<ChPhysicsItem> Ppointer = otherphysicslist[ip];
        Ppointer->IntToDescriptor(displ_v + Ppointer->GetOffset_w(), v, R, displ_L + Ppointer->GetOffset_L(), L, Qc);
    }
}

void ChAssembly::IntFromDescriptor(const unsigned int off_v,
                                   ChStateDelta& v,
                                   const unsigned int off_L,
                                   ChVectorDynamic<>& L) {
    unsigned int displ_L = off_L - this->offset_L;
    unsigned int displ_v = off_v - this->offset_w;

    for (int ip = 0; ip < bodylist.size(); ++ip) {
        std::shared_ptr<ChBody> Bpointer = bodylist[ip];
        if (Bpointer->IsActive())
            Bpointer->IntFromDescriptor(displ_v + Bpointer->GetOffset_w(), v, displ_L + Bpointer->GetOffset_L(), L);
    }

    for (int ip = 0; ip < linklist.size(); ++ip) {
        std::shared_ptr<ChLink> Lpointer = linklist[ip];
        if (Lpointer->IsActive())
            Lpointer->IntFromDescriptor(displ_v + Lpointer->GetOffset_w(), v, displ_L + Lpointer->GetOffset_L(), L);
    }

    for (int ip = 0; ip < otherphysicslist.size(); ++ip) {
        std::shared_ptr<ChPhysicsItem> Ppointer = otherphysicslist[ip];
        Ppointer->IntFromDescriptor(displ_v + Ppointer->GetOffset_w(), v, displ_L + Ppointer->GetOffset_L(), L);
    }
}

// -----------------------------------------------------------------------------

void ChAssembly::InjectVariables(ChSystemDescriptor& mdescriptor) {
    for (unsigned int ip = 0; ip < bodylist.size(); ++ip) {
        bodylist[ip]->InjectVariables(mdescriptor);
    }
    for (unsigned int ip = 0; ip < linklist.size(); ++ip) {
        linklist[ip]->InjectVariables(mdescriptor);
    }
    for (unsigned int ip = 0; ip < otherphysicslist.size(); ++ip) {
        otherphysicslist[ip]->InjectVariables(mdescriptor);
    }
}

void ChAssembly::VariablesFbReset() {
    for (unsigned int ip = 0; ip < bodylist.size(); ++ip) {
        bodylist[ip]->VariablesFbReset();
    }
    for (unsigned int ip = 0; ip < linklist.size(); ++ip) {
        linklist[ip]->VariablesFbReset();
    }
    for (unsigned int ip = 0; ip < otherphysicslist.size(); ++ip) {
        otherphysicslist[ip]->VariablesFbReset();
    }
}

void ChAssembly::VariablesFbLoadForces(double factor) {
    for (unsigned int ip = 0; ip < bodylist.size(); ++ip) {
        bodylist[ip]->VariablesFbLoadForces(factor);
    }
    for (unsigned int ip = 0; ip < linklist.size(); ++ip) {
        linklist[ip]->VariablesFbLoadForces(factor);
    }
    for (unsigned int ip = 0; ip < otherphysicslist.size(); ++ip) {
        otherphysicslist[ip]->VariablesFbLoadForces(factor);
    }
}

void ChAssembly::VariablesFbIncrementMq() {
    for (unsigned int ip = 0; ip < bodylist.size(); ++ip) {
        bodylist[ip]->VariablesFbIncrementMq();
    }
    for (unsigned int ip = 0; ip < linklist.size(); ++ip) {
        linklist[ip]->VariablesFbIncrementMq();
    }
    for (unsigned int ip = 0; ip < otherphysicslist.size(); ++ip) {
        otherphysicslist[ip]->VariablesFbIncrementMq();
    }
}

void ChAssembly::VariablesQbLoadSpeed() {
    for (unsigned int ip = 0; ip < bodylist.size(); ++ip) {
        bodylist[ip]->VariablesQbLoadSpeed();
    }
    for (unsigned int ip = 0; ip < linklist.size(); ++ip) {
        linklist[ip]->VariablesQbLoadSpeed();
    }
    for (unsigned int ip = 0; ip < otherphysicslist.size(); ++ip) {
        otherphysicslist[ip]->VariablesQbLoadSpeed();
    }
}

void ChAssembly::VariablesQbSetSpeed(double step) {
    for (unsigned int ip = 0; ip < bodylist.size(); ++ip) {
        bodylist[ip]->VariablesQbSetSpeed(step);
    }
    for (unsigned int ip = 0; ip < linklist.size(); ++ip) {
        linklist[ip]->VariablesQbSetSpeed(step);
    }
    for (unsigned int ip = 0; ip < otherphysicslist.size(); ++ip) {
        otherphysicslist[ip]->VariablesQbSetSpeed(step);
    }
}

void ChAssembly::VariablesQbIncrementPosition(double dt_step) {
    for (unsigned int ip = 0; ip < bodylist.size(); ++ip) {
        bodylist[ip]->VariablesQbIncrementPosition(dt_step);
    }
    for (unsigned int ip = 0; ip < linklist.size(); ++ip) {
        linklist[ip]->VariablesQbIncrementPosition(dt_step);
    }
    for (unsigned int ip = 0; ip < otherphysicslist.size(); ++ip) {
        otherphysicslist[ip]->VariablesQbIncrementPosition(dt_step);
    }
}

void ChAssembly::InjectConstraints(ChSystemDescriptor& mdescriptor) {
    for (unsigned int ip = 0; ip < bodylist.size(); ++ip) {
        bodylist[ip]->InjectConstraints(mdescriptor);
    }
    for (unsigned int ip = 0; ip < linklist.size(); ++ip) {
        linklist[ip]->InjectConstraints(mdescriptor);
    }
    for (unsigned int ip = 0; ip < otherphysicslist.size(); ++ip) {
        otherphysicslist[ip]->InjectConstraints(mdescriptor);
    }
}

void ChAssembly::ConstraintsBiReset() {
    for (unsigned int ip = 0; ip < bodylist.size(); ++ip) {
        bodylist[ip]->ConstraintsBiReset();
    }
    for (unsigned int ip = 0; ip < linklist.size(); ++ip) {
        linklist[ip]->ConstraintsBiReset();
    }
    for (unsigned int ip = 0; ip < otherphysicslist.size(); ++ip) {
        otherphysicslist[ip]->ConstraintsBiReset();
    }
}

void ChAssembly::ConstraintsBiLoad_C(double factor, double recovery_clamp, bool do_clamp) {
    for (unsigned int ip = 0; ip < bodylist.size(); ++ip) {
        bodylist[ip]->ConstraintsBiLoad_C(factor, recovery_clamp, do_clamp);
    }
    for (unsigned int ip = 0; ip < linklist.size(); ++ip) {
        linklist[ip]->ConstraintsBiLoad_C(factor, recovery_clamp, do_clamp);
    }
    for (unsigned int ip = 0; ip < otherphysicslist.size(); ++ip) {
        otherphysicslist[ip]->ConstraintsBiLoad_C(factor, recovery_clamp, do_clamp);
    }
}

void ChAssembly::ConstraintsBiLoad_Ct(double factor) {
    for (unsigned int ip = 0; ip < bodylist.size(); ++ip) {
        bodylist[ip]->ConstraintsBiLoad_Ct(factor);
    }
    for (unsigned int ip = 0; ip < linklist.size(); ++ip) {
        linklist[ip]->ConstraintsBiLoad_Ct(factor);
    }
    for (unsigned int ip = 0; ip < otherphysicslist.size(); ++ip) {
        otherphysicslist[ip]->ConstraintsBiLoad_Ct(factor);
    }
}

void ChAssembly::ConstraintsBiLoad_Qc(double factor) {
    for (unsigned int ip = 0; ip < bodylist.size(); ++ip) {
        bodylist[ip]->ConstraintsBiLoad_Qc(factor);
    }
    for (unsigned int ip = 0; ip < linklist.size(); ++ip) {
        linklist[ip]->ConstraintsBiLoad_Qc(factor);
    }
    for (unsigned int ip = 0; ip < otherphysicslist.size(); ++ip) {
        otherphysicslist[ip]->ConstraintsBiLoad_Qc(factor);
    }
}

void ChAssembly::ConstraintsFbLoadForces(double factor) {
    for (unsigned int ip = 0; ip < bodylist.size(); ++ip) {
        bodylist[ip]->ConstraintsFbLoadForces(factor);
    }
    for (unsigned int ip = 0; ip < linklist.size(); ++ip) {
        linklist[ip]->ConstraintsFbLoadForces(factor);
    }
    for (unsigned int ip = 0; ip < otherphysicslist.size(); ++ip) {
        otherphysicslist[ip]->ConstraintsFbLoadForces(factor);
    }
}

void ChAssembly::ConstraintsLoadJacobians() {
    for (unsigned int ip = 0; ip < bodylist.size(); ++ip) {
        bodylist[ip]->ConstraintsLoadJacobians();
    }
    for (unsigned int ip = 0; ip < linklist.size(); ++ip) {
        linklist[ip]->ConstraintsLoadJacobians();
    }
    for (unsigned int ip = 0; ip < otherphysicslist.size(); ++ip) {
        otherphysicslist[ip]->ConstraintsLoadJacobians();
    }
}

void ChAssembly::ConstraintsFetch_react(double factor) {
    for (unsigned int ip = 0; ip < bodylist.size(); ++ip) {
        bodylist[ip]->ConstraintsFetch_react(factor);
    }
    for (unsigned int ip = 0; ip < linklist.size(); ++ip) {
        linklist[ip]->ConstraintsFetch_react(factor);
    }
    for (unsigned int ip = 0; ip < otherphysicslist.size(); ++ip) {
        otherphysicslist[ip]->ConstraintsFetch_react(factor);
    }
}

void ChAssembly::InjectKRMmatrices(ChSystemDescriptor& mdescriptor) {
    for (unsigned int ip = 0; ip < bodylist.size(); ++ip) {
        bodylist[ip]->InjectKRMmatrices(mdescriptor);
    }
    for (unsigned int ip = 0; ip < linklist.size(); ++ip) {
        linklist[ip]->InjectKRMmatrices(mdescriptor);
    }
    for (unsigned int ip = 0; ip < otherphysicslist.size(); ++ip) {
        otherphysicslist[ip]->InjectKRMmatrices(mdescriptor);
    }
}

void ChAssembly::KRMmatricesLoad(double Kfactor, double Rfactor, double Mfactor) {
    for (unsigned int ip = 0; ip < bodylist.size(); ++ip) {
        bodylist[ip]->KRMmatricesLoad(Kfactor, Rfactor, Mfactor);
    }
    for (unsigned int ip = 0; ip < linklist.size(); ++ip) {
        linklist[ip]->KRMmatricesLoad(Kfactor, Rfactor, Mfactor);
    }
    for (unsigned int ip = 0; ip < otherphysicslist.size(); ++ip) {
        otherphysicslist[ip]->KRMmatricesLoad(Kfactor, Rfactor, Mfactor);
    }
}

// -----------------------------------------------------------------------------
//  STREAMING - FILE HANDLING

void ChAssembly::ShowHierarchy(ChStreamOutAscii& m_file, int level) {
    std::string mtabs;
    for (int i = 0; i < level; ++i)
        mtabs += "  ";

    m_file << "\n" << mtabs << "List of the " << (int)bodylist.size() << " added rigid bodies: \n";
    for (auto body : bodylist) {
        m_file << mtabs << "  BODY:       " << body->GetName() << "\n";

        for (auto marker : body->GetMarkerList()) {
            m_file << mtabs << "    MARKER:  " << marker->GetName() << "\n";
        }

        for (auto force : body->GetForceList()) {
            m_file << mtabs << "    FORCE:  " << force->GetName() << "\n";
        }
    }

    m_file << "\n" << mtabs << "List of the " << (int)linklist.size() << " added links: \n";
    for (auto link : linklist) {
        m_file << mtabs << "  LINK:  " << link->GetName() << " [" << typeid(link.get()).name() << "]\n";
        if (auto malink = std::dynamic_pointer_cast<ChLinkMarkers>(link)) {
            if (malink->GetMarker1())
                m_file << mtabs << "    marker1:  " << malink->GetMarker1()->GetName() << "\n";
            if (malink->GetMarker2())
                m_file << mtabs << "    marker2:  " << malink->GetMarker2()->GetName() << "\n";
        }
    }

    m_file << "\n" << mtabs << "List of other " << (int)otherphysicslist.size() << " added physic items: \n";
    for (auto ph : otherphysicslist) {
        m_file << mtabs << "  PHYSIC ITEM :   " << ph->GetName() << " [" << typeid(ph.get()).name() << "]\n";

        // recursion:
        if (auto assem = std::dynamic_pointer_cast<ChAssembly>(ph))
            assem->ShowHierarchy(m_file, level + 1);
    }

    m_file << "\n\n";
}

void ChAssembly::ArchiveOUT(ChArchiveOut& marchive) {
    // version number
    marchive.VersionWrite<ChAssembly>();

    // serialize parent class
    ChPhysicsItem::ArchiveOUT(marchive);

    // serialize all member data:

    marchive << CHNVP(bodylist, "bodies");
    marchive << CHNVP(linklist, "links");
    marchive << CHNVP(otherphysicslist , "other_physics_items");
}

void ChAssembly::ArchiveIN(ChArchiveIn& marchive) {
    // version number
    int version = marchive.VersionRead<ChAssembly>();

    // deserialize parent class
    ChPhysicsItem::ArchiveIN(marchive);

    // stream in all member data:
    std::vector< std::shared_ptr<ChBody>> tempbodies;
    std::vector< std::shared_ptr<ChLink>> templinks;
    std::vector< std::shared_ptr<ChPhysicsItem>> tempitems;
    marchive >> CHNVP(tempbodies, "bodies");
    marchive >> CHNVP(templinks,  "links");
    marchive >> CHNVP(tempitems , "other_physics_items");
    // trick needed because the "Add...()" functions are required
    this->RemoveAllBodies();
    for (auto i : tempbodies) {
        this->AddBody(i);
    }
    this->RemoveAllLinks();
    for (auto i : templinks) {
        this->AddLink(i);
    }
    this->RemoveAllOtherPhysicsItems();
    for (auto i : tempitems) {
        this->AddOtherPhysicsItem(i);
    }  

    // Recompute statistics, offsets, etc.
    this->Setup();
}

}  // end namespace chrono
