//===- VersionedFlowSensitive.cpp -- Sparse flow-sensitive pointer analysis------------//

/*
 * VersionedFlowSensitive.cpp
 *
 *  Created on: Jun 26, 2020
 *      Author: Mohamad Barbar
 */

#include "WPA/Andersen.h"
#include "WPA/VersionedFlowSensitive.h"
#include <iostream>

using namespace SVF;

VersionedFlowSensitive::VersionedFlowSensitive(PAG *_pag, PTATY type)
    : FlowSensitive(_pag, type)
{
    numPrecolouredNodes = numPrecolourVersions = 0;
    relianceTime = precolouringTime = colouringTime = meldMappingTime = versionPropTime = 0.0;
}

void VersionedFlowSensitive::initialize()
{
    FlowSensitive::initialize();
    // Overwrite the stat FlowSensitive::initialize gave us.
    delete stat;
    stat = new VersionedFlowSensitiveStat(this);

    vPtD = getVDFPTDataTy();
    assert(vPtD && "VFS::initialize: Expected VDFPTData");

    precolour();
    colour();

    mapMeldVersions();

    determineReliance();

    vPtD->setConsume(&consume);
    vPtD->setYield(&yield);
}

void VersionedFlowSensitive::finalize()
{
    FlowSensitive::finalize();
    //vPtD->dumpPTData();
    printf("DONE! TODO\n"); fflush(stdout);
    //exit(0);
}

void VersionedFlowSensitive::precolour(void)
{
    double start = stat->getClk(true);
    for (SVFG::iterator it = svfg->begin(); it != svfg->end(); ++it)
    {
        NodeID l = it->first;
        const SVFGNode *sn = it->second;

        if (const StoreSVFGNode *stn = SVFUtil::dyn_cast<StoreSVFGNode>(sn))
        {
            NodeID p = stn->getPAGDstNodeID();
            for (NodeID o : ander->getPts(p))
            {
                meldYield[l][o] = newMeldVersion(o);
            }

            vWorklist.push(l);

            if (ander->getPts(p).count() != 0)
            {
                ++numPrecolouredNodes;
            }
        }
        else if (delta(l))
        {
            // If l may change at runtime (new incoming edges), it's unknown whether
            // a new consume version is required, so we give it one in case.
            for (const SVFGEdge *e : sn->getOutEdges())
            {
                const IndirectSVFGEdge *ie = SVFUtil::dyn_cast<IndirectSVFGEdge>(e);
                if (!ie) continue;
                for (NodeID o : ie->getPointsTo())
                {
                    meldConsume[l][o] = newMeldVersion(o);
                }

                // Push into worklist because its consume == its yield.
                vWorklist.push(l);

                if (ie->getPointsTo().count() != 0)
                {
                    ++numPrecolouredNodes;
                }
            }
        }
    }

    double end = stat->getClk(true);
    precolouringTime = (end - start) / TIMEINTERVAL;
}

void VersionedFlowSensitive::colour(void) {
    double start = stat->getClk(true);

    while (!vWorklist.empty()) {
        NodeID l = vWorklist.pop();
        const SVFGNode *sl = svfg->getSVFGNode(l);

        // Propagate l's y to lp's c for all l --o--> lp.
        for (const SVFGEdge *e : sl->getOutEdges()) {
            const IndirectSVFGEdge *ie = SVFUtil::dyn_cast<IndirectSVFGEdge>(e);
            if (!ie) continue;

            NodeID lp = ie->getDstNode()->getId();
            // Delta nodes had c set already.
            if (delta(lp)) continue;

            // For stores, yield != consume, otherwise they are the same.
            ObjToMeldVersionMap &myl = SVFUtil::isa<StoreSVFGNode>(sl) ? meldYield[l]
                                                                       : meldConsume[l];
            for (NodeID o : ie->getPointsTo()) {
                if (myl.find(o) == myl.end()) continue;
                if (meld(meldConsume[lp][o], myl[o])) {
                    const SVFGNode *slp = svfg->getSVFGNode(lp);
                    // Store nodes had their y set already.
                    if (!SVFUtil::isa<StoreSVFGNode>(slp)) {
                        // Consume was updated, and yield == consume when not a store.
                        vWorklist.push(lp);
                    }
                }
            }
        }
    }

    double end = stat->getClk(true);
    colouringTime = (end - start) / TIMEINTERVAL;
}

bool VersionedFlowSensitive::meld(MeldVersion &mv1, MeldVersion &mv2)
{
    // Meld operator is union of bit vectors.
    return mv1 |= mv2;
}


void VersionedFlowSensitive::mapMeldVersions(void)
{
    double start = stat->getClk(true);

    // We want to uniquely map MeldVersions (SparseBitVectors) to a Version (unsigned integer).
    // mvv keeps track, and curVersion is used to generate new Versions.
    static DenseMap<MeldVersion, Version> mvv;
    static Version curVersion = 1;

    // meldConsume -> consume.
    for (LocMeldVersionMap::value_type &lomv : meldConsume)
    {
        NodeID l = lomv.first;
        bool isStore = SVFUtil::isa<StoreSVFGNode>(svfg->getSVFGNode(l));
        ObjToVersionMap &consumel = consume[l];
        ObjToVersionMap &yieldl = yield[l];
        for (ObjToMeldVersionMap::value_type &omv : lomv.second)
        {
            NodeID o = omv.first;
            MeldVersion &mv = omv.second;

            DenseMap<MeldVersion, Version>::const_iterator foundVersion = mvv.find(mv);
            Version v = foundVersion == mvv.end() ? mvv[mv] = ++curVersion : foundVersion->second;
            consumel[o] = v;
            // At non-stores, consume == yield.
            if (!isStore) yieldl[o] = v;
        }
    }

    // meldYield -> yield.
    for (LocMeldVersionMap::value_type &lomv : meldYield)
    {
        NodeID l = lomv.first;
        ObjToVersionMap &yieldl = yield[l];
        for (ObjToMeldVersionMap::value_type &omv : lomv.second)
        {
            NodeID o = omv.first;
            MeldVersion &mv = omv.second;

            DenseMap<MeldVersion, Version>::const_iterator foundVersion = mvv.find(mv);
            Version v = foundVersion == mvv.end() ? mvv[mv] = ++curVersion : foundVersion->second;
            yieldl[o] = v;
        }
    }

    // No longer necessary.
    meldConsume.clear();
    meldYield.clear();

    double end = stat->getClk(true);
    meldMappingTime += (end - start) / TIMEINTERVAL;
}

bool VersionedFlowSensitive::delta(NodeID l)
{
    DenseMap<NodeID, bool>::const_iterator isDelta = deltaCache.find(l);
    if (isDelta != deltaCache.end()) return isDelta->second;

    // vfs-TODO: double check.
    const SVFGNode *s = svfg->getSVFGNode(l);
    // Cases:
    //  * Function entry: can get new inc. ind. edges through ind. callsites.
    //  * Callsite returns: can get new inc. ind. edges if the callsite is indirect.
    //  * Otherwise: static.
    if (const SVFFunction *fn = svfg->isFunEntrySVFGNode(s))
    {
        PTACallGraphEdge::CallInstSet callsites;
        /// use pre-analysis call graph to approximate all potential callsites
        ander->getPTACallGraph()->getIndCallSitesInvokingCallee(fn, callsites);

        deltaCache[l] = !callsites.empty();
        return !callsites.empty();
    }
    else if (const CallBlockNode *cbn = svfg->isCallSiteRetSVFGNode(s))
    {
        deltaCache[l] = cbn->isIndirectCall();
        return cbn->isIndirectCall();
    }

    return false;
}

/// Returns a new version for o.
MeldVersion VersionedFlowSensitive::newMeldVersion(NodeID o)
{
    ++numPrecolourVersions;
    MeldVersion nv;
    nv.set(++meldVersions[o]);
    return nv;
}

bool VersionedFlowSensitive::hasVersion(NodeID l, NodeID o, enum VersionType v) const
{
    // Choose which map we are checking.
    const LocVersionMap &m = v == CONSUME ? consume : yield;
    const ObjToVersionMap &ml = m.lookup(l);
    return ml.find(o) != ml.end();
}

bool VersionedFlowSensitive::hasMeldVersion(NodeID l, NodeID o, enum VersionType v) const
{
    // Choose which map we are checking.
    const LocMeldVersionMap &m = v == CONSUME ? meldConsume : meldYield;
    const ObjToMeldVersionMap &ml = m.lookup(l);
    return ml.find(o) != ml.end();
}

void VersionedFlowSensitive::determineReliance(void)
{
    double start = stat->getClk(true);

    for (SVFG::iterator it = svfg->begin(); it != svfg->end(); ++it)
    {
        NodeID l = it->first;
        const SVFGNode *sn = it->second;
        for (const SVFGEdge *e : sn->getOutEdges())
        {
            const IndirectSVFGEdge *ie = SVFUtil::dyn_cast<IndirectSVFGEdge>(e);
            if (!ie) continue;
            for (NodeID o : ie->getPointsTo())
            {
                if (!hasVersion(l, o, YIELD)) continue;
                // Given l --o--> lp, c at lp relies on y at l.
                NodeID lp = ie->getDstNode()->getId();
                Version &y = yield[l][o];
                Version &cp = consume[lp][o];
                if (cp != y)
                {
                    versionReliance[o][y].insert(cp);
                }
            }
        }

        if (SVFUtil::isa<LoadSVFGNode>(sn) || SVFUtil::isa<StoreSVFGNode>(sn))
        {
            for (ObjToVersionMap::value_type &ov : consume[l])
            {
                NodeID o = ov.first;
                Version &v = ov.second;
                stmtReliance[o][v].insert(l);
            }
        }
    }

    double end = stat->getClk(true);
    relianceTime = (end - start) / TIMEINTERVAL;
}

void VersionedFlowSensitive::propagateVersion(NodeID o, Version v)
{
    double start = stat->getClk();

    for (Version r : versionReliance[o][v])
    {
        if (vPtD->updateATVersion(o, r, v))
        {
            propagateVersion(o, r);
        }
    }

    for (NodeID s : stmtReliance[o][v])
    {
        pushIntoWorklist(s);
    }

    double end = stat->getClk();
    versionPropTime += (end - start) / TIMEINTERVAL;
}

void VersionedFlowSensitive::processNode(NodeID n)
{
    SVFGNode* sn = svfg->getSVFGNode(n);
    if (processSVFGNode(sn)) propagate(&sn);
}

void VersionedFlowSensitive::updateConnectedNodes(const SVFGEdgeSetTy& newEdges)
{
    for (const SVFGEdge *e : newEdges)
    {
        const SVFGNode *src = e->getSrcNode();
        const SVFGNode *dst = e->getDstNode();
        NodeID srcId = src->getId();
        NodeID dstId = dst->getId();

        if (SVFUtil::isa<PHISVFGNode>(dst))
        {
            pushIntoWorklist(dstId);
        }
        else if (SVFUtil::isa<FormalINSVFGNode>(dst) || SVFUtil::isa<ActualOUTSVFGNode>(dst))
        {
            const IndirectSVFGEdge *ie = SVFUtil::dyn_cast<IndirectSVFGEdge>(e);
            assert(ie && "VFS::updateConnectedNodes: given direct edge?");
            bool changed = false;

            const PointsTo &ept = ie->getPointsTo();
            // For every o, such that src --o--> dst, we need to set up reliance (and propagate).
            for (NodeID o : ept)
            {
                // Nothing to propagate.
                if (yield[srcId].find(o) == yield[srcId].end()) continue;

                Version &srcY = yield[srcId][o];
                Version &dstC = consume[dstId][o];
                versionReliance[o][srcY].insert(dstC);
                propagateVersion(o, srcY);
            }

            if (changed) pushIntoWorklist(dst->getId());
        }
    }
}

bool VersionedFlowSensitive::processLoad(const LoadSVFGNode* load)
{
    double start = stat->getClk();

    bool changed = false;

    NodeID l = load->getId();
    NodeID p = load->getPAGDstNodeID();
    NodeID q = load->getPAGSrcNodeID();

    const PointsTo& qpt = getPts(q);
    for (NodeID o : qpt)
    {
        if (pag->isConstantObj(o) || pag->isNonPointerObj(o)) continue;

        if (vPtD->unionTLFromAT(l, p, o))
        {
            changed = true;
        }

        if (isFieldInsensitive(o))
        {
            /// If o is a field-insensitive node, we should also get all field nodes'
            /// points-to sets and pass them to p.
            const NodeBS& fields = getAllFieldsObjNode(o);
            for (NodeID of : fields)
            {
                if (vPtD->unionTLFromAT(l, p, of))
                {
                    changed = true;
                }
            }
        }
    }

    double end = stat->getClk();
    loadTime += (end - start) / TIMEINTERVAL;
    return changed;
}

bool VersionedFlowSensitive::processStore(const StoreSVFGNode* store)
{
    NodeID p = store->getPAGDstNodeID();
    const PointsTo &ppt = getPts(p);

    if (ppt.empty()) return false;

    NodeID q = store->getPAGSrcNodeID();
    const PointsTo &qpt = getPts(q);

    NodeID l = store->getId();
    double start = stat->getClk();
    bool changed = false;
    // The version for these objects would be y_l(o).
    NodeBS changedObjects;

    if (!qpt.empty())
    {
        for (NodeID o : ppt)
        {
            if (pag->isConstantObj(o) || pag->isNonPointerObj(o)) continue;

            if (vPtD->unionATFromTL(l, q, o))
            {
                changed = true;
                changedObjects.set(o);
            }
        }
    }

    double end = stat->getClk();
    storeTime += (end - start) / TIMEINTERVAL;

    double updateStart = stat->getClk();

    NodeID singleton = 0;
    bool isSU = isStrongUpdate(store, singleton);
    if (isSU) svfgHasSU.set(l);
    else svfgHasSU.reset(l);

    changed = vPtD->propWithinLoc(l, isSU, singleton, changedObjects) || changed;

    double updateEnd = stat->getClk();
    updateTime += (updateEnd - updateStart) / TIMEINTERVAL;

    // TODO: time.
    for (NodeID o : changedObjects)
    {
        propagateVersion(o, yield[l][o]);
    }

    return changed;
}

void VersionedFlowSensitive::dumpReliances(void) const
{
    SVFUtil::outs() << "# Reliances\n";
    for (DenseMap<NodeID, DenseMap<Version, DenseSet<Version>>>::value_type ovrv : versionReliance)
    {
        NodeID o = ovrv.first;
        SVFUtil::outs() << "  Object " << o << "\n";
        for (DenseMap<Version, DenseSet<Version>>::value_type vrv : ovrv.second)
        {
            Version v = vrv.first;
            SVFUtil::outs() << "    Version " << v << " is a reliance for: ";

            bool first = true;
            for (Version rv : vrv.second)
            {
                if (!first)
                {
                    SVFUtil::outs() << ", ";
                }

                SVFUtil::outs() << rv;
                first = false;
            }

            SVFUtil::outs() << "\n";
        }
    }

    // TODO: stmt reliances.
}