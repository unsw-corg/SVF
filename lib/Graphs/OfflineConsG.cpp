//===- OfflineConsG.cpp -- Offline constraint graph -----------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2017>  <Yulei Sui>
//

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//===----------------------------------------------------------------------===//

/*
 * OfflineConsG.cpp
 *
 *  Created on: Oct 28, 2018
 *      Author: Yuxiang Lei
 */

#include "Util/Options.h"
#include "Graphs/OfflineConsG.h"
#include "Graphs/GraphPrinter.h"

using namespace SVF;
using namespace SVFUtil;

/*!
 * Builder of offline constraint graph
 */
void OfflineConsG::buildOfflineCG()
{
    LoadEdges loads;
    StoreEdges stores;

    // Add a copy edge between the ref node of src node and dst node
    for (ConstraintEdge::ConstraintEdgeSetTy::iterator it = LoadCGEdgeSet.begin(), eit =
                LoadCGEdgeSet.end(); it != eit; ++it)
    {
        LoadCGEdge *load = SVFUtil::dyn_cast<LoadCGEdge>(*it);
        loads.insert(load);
        NodeID src = load->getSrcID();
        NodeID dst = load->getDstID();
        addRefLoadEdge(src, dst);
    }
    // Add a copy edge between src node and the ref node of dst node
    for (ConstraintEdge::ConstraintEdgeSetTy::iterator it = StoreCGEdgeSet.begin(), eit =
                StoreCGEdgeSet.end(); it != eit; ++it)
    {
        StoreCGEdge *store = SVFUtil::dyn_cast<StoreCGEdge>(*it);
        stores.insert(store);
        NodeID src = store->getSrcID();
        NodeID dst = store->getDstID();
        addRefStoreEdge(src, dst);
    }

    // Dump offline graph with all edges
    dump("oCG_initial");

    // Remove load and store edges in offline constraint graph
    for (LoadEdges::iterator it = loads.begin(), eit = loads.end(); it != eit; ++it)
    {
        removeLoadEdge(*it);
    }
    for (StoreEdges::iterator it = stores.begin(), eit = stores.end(); it != eit; ++it)
    {
        removeStoreEdge(*it);
    }

    // Dump offline graph with removed load and store edges
    dump("oCG_final");
}

/*!
 * Add a copy edge between the ref node of src node and dst node,
 * while meeting a LOAD constraint.
 */
bool  OfflineConsG::addRefLoadEdge(NodeID src, NodeID dst)
{
    createRefNode(src);
    NodeID ref = nodeToRefMap[src];
    return addCopyCGEdge(ref, dst);
}

/*!
 * Add a copy edge between src node and the ref node of dst node,
 * while meeting a STORE constraint.
 */
bool OfflineConsG::addRefStoreEdge(NodeID src, NodeID dst)
{
    createRefNode(dst);
    NodeID ref = nodeToRefMap[dst];
    return addCopyCGEdge(src, ref);
}

/*!
 * Create a ref node for a constraint node if it does not have one
 */
bool OfflineConsG::createRefNode(NodeID nodeId)
{
    if (hasRef(nodeId))
        return false;

    NodeID refId = pag->addDummyValNode();
    ConstraintNode* node = new ConstraintNode(refId);
    addConstraintNode(node, refId);
    refNodes.insert(refId);
    nodeToRefMap[nodeId] = refId;
    return true;
}

/*!
 * Use a offline SCC detector to solve node relations in OCG.
 * Generally, the 'oscc' should be solved first.
 */
void OfflineConsG::solveOfflineSCC(OSCC* oscc)
{
    // Build offline nodeToRepMap
    buildOfflineMap(oscc);
}

/*!
 * Build offline node to rep map, which only collect nodes having a ref node
 */
void OfflineConsG::buildOfflineMap(OSCC* oscc)
{
    for (NodeToRepMap::const_iterator it = nodeToRefMap.begin(); it != nodeToRefMap.end(); ++it)
    {
        NodeID node = it->first;
        NodeID ref = getRef(node);
        NodeID rep = solveRep(oscc,oscc->repNode(ref));
        if (!isaRef(rep) && !isaRef(node))
            setNorRep(node, rep);
    }
}

/*!
 * The rep nodes of offline constraint graph are possible to be 'ref' nodes.
 * These nodes should be replaced by one of its sub nodes which is not a ref node.
 */
NodeID OfflineConsG::solveRep(OSCC* oscc, NodeID rep)
{
    if (isaRef(rep))
    {
        NodeBS subNodes = oscc->subNodes(rep);
        for (NodeBS::iterator subIt = subNodes.begin(), subEit = subNodes.end(); subIt != subEit; ++subIt)
        {
            if (isaRef(*subIt))
            {
                rep = *subIt;
                break;
            }
        }
    }
    return rep;
}

/*!
 * Dump offline constraint graph
 */
void OfflineConsG::dump(std::string name)
{
    if (Options::OCGDotGraph)
        GraphPrinter::SelectiveWriteGraphToFile(outs(), name, this);
}



// --- GraphTraits specialization for offline constraint graph ---

namespace llvm
{
template<>
struct DOTGraphTraits<OfflineConsG*> : public DOTGraphTraits<PAG*>
{

    typedef ConstraintNode NodeType;
    DOTGraphTraits(bool isSimple = false) :
        DOTGraphTraits<PAG*>(isSimple)
    {
    }

    /// Return name of the graph
    static std::string getGraphName(OfflineConsG*)
    {
        return "Offline Constraint Graph";
    }

    /// isNodeHidden - If the function returns true, the given node is not
    /// displayed in the graph
    static bool isNodeHidden(NodeType *node) {
        return node->reallyHideNode(false);
    }

    /// Return label of a VFG node with two display mode
    /// Either you can choose to display the name of the value or the whole instruction
    static std::string getNodeLabel(NodeType *n, OfflineConsG*)
    {
        std::string str;
        raw_string_ostream rawstr(str);
        if (PAG::getPAG()->findPAGNode(n->getId()))
        {
            PAGNode *node = PAG::getPAG()->getPAGNode(n->getId());
            bool briefDisplay = true;
            bool nameDisplay = true;


            if (briefDisplay)
            {
                if (SVFUtil::isa<ValPN>(node))
                {
                    if (nameDisplay)
                        rawstr << node->getId() << ":" << node->getValueName();
                    else
                        rawstr << node->getId();
                }
                else
                    rawstr << node->getId();
            }
            else
            {
                // print the whole value
                if (!SVFUtil::isa<DummyValPN>(node) && !SVFUtil::isa<DummyObjPN>(node))
                    rawstr << *node->getValue();
                else
                    rawstr << "";

            }

            return rawstr.str();
        }
        else
        {
            rawstr<< n->getId();
            return rawstr.str();
        }
    }

    static std::string getNodeAttributes(NodeType *n, OfflineConsG*)
    {
        if (PAG::getPAG()->findPAGNode(n->getId()))
        {
            PAGNode *node = PAG::getPAG()->getPAGNode(n->getId());
            return node->getNodeAttrForDotDisplay();
        }
        else
        {
            return "shape=folder";
        }
    }

    template<class EdgeIter>
    static std::string getEdgeAttributes(NodeType*, EdgeIter EI, OfflineConsG*)
    {
        ConstraintEdge* edge = *(EI.getCurrent());
        assert(edge && "No edge found!!");
        if (edge->getEdgeKind() == ConstraintEdge::Addr)
        {
            return "color=green";
        }
        else if (edge->getEdgeKind() == ConstraintEdge::Copy)
        {
            return "color=black";
        }
        else if (edge->getEdgeKind() == ConstraintEdge::NormalGep
                 || edge->getEdgeKind() == ConstraintEdge::VariantGep)
        {
            return "color=purple";
        }
        else if (edge->getEdgeKind() == ConstraintEdge::Store)
        {
            return "color=blue";
        }
        else if (edge->getEdgeKind() == ConstraintEdge::Load)
        {
            return "color=red";
        }
        else
        {
            assert(0 && "No such kind edge!!");
        }
        return "";
    }

    template<class EdgeIter>
    static std::string getEdgeSourceLabel(NodeType*, EdgeIter)
    {
        return "";
    }
};
} // End namespace llvm
