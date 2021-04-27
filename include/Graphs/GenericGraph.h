//===- CenericGraph.h -- Generic graph ---------------------------------------//
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
 * GenericGraph.h
 *
 *  Created on: Mar 19, 2014
 *      Author: Yulei Sui
 */

#ifndef GENERICGRAPH_H_
#define GENERICGRAPH_H_

#include "Util/BasicTypes.h"
#include "Util/WorkList.h"
#include <llvm/ADT/GraphTraits.h>

namespace SVF
{

typedef u32_t SubgraphIdTy;
class GenericGraphBase;

/*!
 * Generic edge on the graph as base class
 */
template<class NodeTy>
class GenericEdge
{

public:
    /// Node type
    typedef NodeTy NodeType;
    /// Edge Flag
    /// Edge format as follows (from lowest bit):
    ///	(1) 0-7 bits encode an edge kind (allow maximum 16 kinds)
    /// (2) 8-63 bits encode a callsite instruction
    typedef u64_t GEdgeFlag;
    typedef s32_t GEdgeKind;
private:
    NodeTy* src;		///< source node
    NodeTy* dst;		///< destination node
    GEdgeFlag edgeFlag;	///< edge kind

public:
    /// Constructor
    GenericEdge(NodeTy* s, NodeTy* d, GEdgeFlag k): src(s),dst(d), edgeFlag(k)
    {

    }

    /// Destructor
    virtual ~GenericEdge()
    {
    }

    ///  get methods of the components
    //@{
    inline NodeID getSrcID() const
    {
        return src->getId();
    }
    inline NodeID getDstID() const
    {
        return dst->getId();
    }
    inline GEdgeKind getEdgeKind() const
    {
        return (EdgeKindMask & edgeFlag);
    }
    NodeType* getSrcNode() const
    {
        return src;
    }
    NodeType* getDstNode() const
    {
        return dst;
    }

    //@}

    /// Add the hash function for std::set (we also can overload operator< to implement this)
    //  and duplicated elements in the set are not inserted (binary tree comparison)
    //@{
    typedef struct equalGEdge_struct
    {
        bool operator()(const GenericEdge<NodeType>* lhs, const GenericEdge<NodeType>* rhs) const
        {
            if (lhs->edgeFlag != rhs->edgeFlag)
                return lhs->edgeFlag < rhs->edgeFlag;
            else if (lhs->getSrcID() != rhs->getSrcID())
                return lhs->getSrcID() < rhs->getSrcID();
            else
                return lhs->getDstID() < rhs->getDstID();
        }
    } equalGEdge;

    inline bool operator==(const GenericEdge<NodeType>* rhs) const
    {
        return (rhs->edgeFlag == this->edgeFlag && rhs->getSrcID() == this->getSrcID()
                && rhs->getDstID() == this->getDstID());
    }
    //@}

protected:
    static constexpr unsigned char EdgeKindMaskBits = 8;  ///< We use the lower 8 bits to denote edge kind
    static constexpr u64_t EdgeKindMask = (~0ULL) >> (64 - EdgeKindMaskBits);
};

/*!
 * Generic node base for field not depending on generic parameters
 * that might be needed in a non-generic context.
 */
class GenericNodeBase
{
public:
    /// Node kind
    typedef s32_t GNodeK;

private:
    NodeID id;		    ///< Node ID
    GNodeK nodeKind;	///< Node kind
    SubgraphIdTy subgraphID; ///< Which subgraph is the node in.
    GenericGraphBase* graph;

public:
    /// Constructor
    GenericNodeBase(NodeID i, GNodeK k): id(i),nodeKind(k)
    {

    }

    /// Destructor
    virtual ~GenericNodeBase()
    {

    }

    /// Get ID
    inline NodeID getId() const
    {
        return id;
    }


    /// Get node kind
    inline GNodeK getNodeKind() const
    {
        return nodeKind;
    }

    /// Get subgraph ID
    inline SubgraphIdTy getSubgraphID() const
    {
        return subgraphID;
    }

    /// Set ID
    inline void setSubgraphID(SubgraphIdTy newID)
    {
        subgraphID = newID;
    }

    // Get containing graph
    const GenericGraphBase* getGraph() const
    {
        return graph;
    }

    // Set containing graph
    void setGraph(GenericGraphBase* g)
    {
        graph = g;
    }

    /// Decide whether to hide a node in a dot graph.
    /// Based on command line options, decide whether a node
    /// should be hidden. This is used to implement dumping of
    /// connected subgraphs by ignoring nodes not in the selected
    /// connected subgraph. The maybe_hide parameter tells if this
    /// node is of a sort that is not important and need not be
    /// displayed ordinarily. However, if --dot-show-all is
    /// specified then even these are shown.
    bool reallyHideNode(bool maybe_hide);
};

/*!
 * Generic node on the graph as base class
 */
template<class NodeTy,class EdgeTy>
class GenericNode : public GenericNodeBase
{

public:
    typedef NodeTy NodeType;
    typedef EdgeTy EdgeType;
    typedef OrderedSet<EdgeType*, typename EdgeType::equalGEdge> GEdgeSetTy;
    /// Edge iterator
    ///@{
    typedef typename GEdgeSetTy::iterator iterator;
    typedef typename GEdgeSetTy::const_iterator const_iterator;
    ///@}

private:

    GEdgeSetTy InEdges; ///< all incoming edge of this node
    GEdgeSetTy OutEdges; ///< all outgoing edge of this node

public:
    /// Constructor
    GenericNode(NodeID i, GNodeK k): GenericNodeBase(i,k)
    {

    }

    /// Destructor
    virtual ~GenericNode()
    {

    }

    /// Get incoming/outgoing edge set
    ///@{
    inline const GEdgeSetTy& getOutEdges() const
    {
        return OutEdges;
    }
    inline const GEdgeSetTy& getInEdges() const
    {
        return InEdges;
    }
    ///@}

    /// Has incoming/outgoing edge set
    //@{
    inline bool hasIncomingEdge() const
    {
        return (InEdges.empty() == false);
    }
    inline bool hasOutgoingEdge() const
    {
        return (OutEdges.empty() == false);
    }
    //@}

    ///  iterators
    //@{
    inline iterator OutEdgeBegin()
    {
        return OutEdges.begin();
    }
    inline iterator OutEdgeEnd()
    {
        return OutEdges.end();
    }
    inline iterator InEdgeBegin()
    {
        return InEdges.begin();
    }
    inline iterator InEdgeEnd()
    {
        return InEdges.end();
    }
    inline const_iterator OutEdgeBegin() const
    {
        return OutEdges.begin();
    }
    inline const_iterator OutEdgeEnd() const
    {
        return OutEdges.end();
    }
    inline const_iterator InEdgeBegin() const
    {
        return InEdges.begin();
    }
    inline const_iterator InEdgeEnd() const
    {
        return InEdges.end();
    }
    //@}

    /// Iterators used for SCC detection, overwrite it in child class if necessory
    //@{
    virtual inline iterator directOutEdgeBegin()
    {
        return OutEdges.begin();
    }
    virtual inline iterator directOutEdgeEnd()
    {
        return OutEdges.end();
    }
    virtual inline iterator directInEdgeBegin()
    {
        return InEdges.begin();
    }
    virtual inline iterator directInEdgeEnd()
    {
        return InEdges.end();
    }

    virtual inline const_iterator directOutEdgeBegin() const
    {
        return OutEdges.begin();
    }
    virtual inline const_iterator directOutEdgeEnd() const
    {
        return OutEdges.end();
    }
    virtual inline const_iterator directInEdgeBegin() const
    {
        return InEdges.begin();
    }
    virtual inline const_iterator directInEdgeEnd() const
    {
        return InEdges.end();
    }
    //@}

    /// Add incoming and outgoing edges
    //@{
    inline bool addIncomingEdge(EdgeType* inEdge)
    {
        return InEdges.insert(inEdge).second;
    }
    inline bool addOutgoingEdge(EdgeType* outEdge)
    {
        return OutEdges.insert(outEdge).second;
    }
    //@}

    /// Remove incoming and outgoing edges
    ///@{
    inline Size_t removeIncomingEdge(EdgeType* edge)
    {
        iterator it = InEdges.find(edge);
        assert(it != InEdges.end() && "can not find in edge in SVFG node");
        return InEdges.erase(edge);
    }
    inline Size_t removeOutgoingEdge(EdgeType* edge)
    {
        iterator it = OutEdges.find(edge);
        assert(it != OutEdges.end() && "can not find out edge in SVFG node");
        return OutEdges.erase(edge);
    }
    ///@}

    /// Find incoming and outgoing edges
    //@{
    inline EdgeType* hasIncomingEdge(EdgeType* edge) const
    {
        const_iterator it = InEdges.find(edge);
        if (it != InEdges.end())
            return *it;
        else
            return nullptr;
    }
    inline EdgeType* hasOutgoingEdge(EdgeType* edge) const
    {
        const_iterator it = OutEdges.find(edge);
        if (it != OutEdges.end())
            return *it;
        else
            return nullptr;
    }
    //@}
};

/*
 * Base class for GenericGraph with some data that does not depend
 * on the generic parameters.
 */
class GenericGraphBase
{
private:
public:
    /// Constructor
    GenericGraphBase(): edgeNum(0),nodeNum(0), currentSubgraphId(0), subgraphNum(0), subgraphSizeMap()
    {
    }

    /// Destructor
    virtual ~GenericGraphBase()
    {
    }

    u32_t edgeNum;		///< total num of node
    u32_t nodeNum;		///< total num of edge

    /// To avoid clutter, sometimes we want to focus on just a subset of
    /// the graph. Nodes will be marked with a subgraph number, and
    /// are considered part of the current subgraph if their subGraphId matches
    /// the graph's currentSubgraphId.
    SubgraphIdTy currentSubgraphId;

    SubgraphIdTy subgraphNum; ///< total num of subgraphs

    Map<SubgraphIdTy, u32_t> subgraphSizeMap;

};

/*
 * Generic graph for program representation
 * It is base class and needs to be instantiated
 */
template<class NodeTy,class EdgeTy>
class GenericGraph : public GenericGraphBase
{

public:
    typedef NodeTy NodeType;
    typedef EdgeTy EdgeType;
    /// NodeID to GenericNode map
    typedef Map<NodeID, NodeType*> IDToNodeMapTy;

    /// Node Iterators
    //@{
    typedef typename IDToNodeMapTy::iterator iterator;
    typedef typename IDToNodeMapTy::const_iterator const_iterator;
    //@}

    /// Constructor
    GenericGraph(): worklist()
    {
    }

    /// Destructor
    virtual ~GenericGraph()
    {
        destroy();
    }

    /// Release memory
    void destroy()
    {
        for (iterator I = IDToNodeMap.begin(), E = IDToNodeMap.end(); I != E; ++I){
            // NodeType* node = I->second;
            // for(typename NodeType::iterator it = node->InEdgeBegin(), eit = node->InEdgeEnd(); it!=eit; ++it)
            //         delete *it;
        }
        for (iterator I = IDToNodeMap.begin(), E = IDToNodeMap.end(); I != E; ++I)
            delete I->second;
    }
    /// Iterators
    //@{
    inline iterator begin()
    {
        return IDToNodeMap.begin();
    }
    inline iterator end()
    {
        return IDToNodeMap.end();
    }
    inline const_iterator begin() const
    {
        return IDToNodeMap.begin();
    }
    inline const_iterator end() const
    {
        return IDToNodeMap.end();
    }
    //}@

    /// Add a Node
    inline void addGNode(NodeID id, NodeType* node)
    {
        IDToNodeMap[id] = node;
        node->setGraph(this);
        nodeNum++;
    }

    /// Get a node
    inline NodeType* getGNode(NodeID id) const
    {
        const_iterator it = IDToNodeMap.find(id);
        assert(it != IDToNodeMap.end() && "Node not found!");
        return it->second;
    }

    /// Has a node
    inline bool hasGNode(NodeID id) const
    {
        const_iterator it = IDToNodeMap.find(id);
        return it != IDToNodeMap.end();
    }

    /// Delete a node
    inline void removeGNode(NodeType* node)
    {
        assert(node->hasIncomingEdge() == false
               && node->hasOutgoingEdge() == false
               && "node which have edges can't be deleted");
        iterator it = IDToNodeMap.find(node->getId());
        assert(it != IDToNodeMap.end() && "can not find the node");
        IDToNodeMap.erase(it);
        node->setGraph(nullptr);
    }

    /// Get total number of node/edge
    inline u32_t getTotalNodeNum() const
    {
        return nodeNum;
    }
    inline u32_t getTotalEdgeNum() const
    {
        return edgeNum;
    }
    /// Increase number of node/edge
    inline void incNodeNum()
    {
        nodeNum++;
    }
    inline void incEdgeNum()
    {
        edgeNum++;
    }

    /// Get current subgraph id.
    inline SubgraphIdTy getCurrentSubgraphId() const
    {
        return currentSubgraphId;
    }

    /// Set current subgraph id.
    inline void setCurrentSubgraphId(SubgraphIdTy newId)
    {
        currentSubgraphId = newId;
    }

    /// Get current number of subgraphs
    inline SubgraphIdTy getSubgraphNum() const
    {
        return subgraphNum;
    }

    /// Increment number of subgraphs
    inline void incrementSubgraphNum()
    {
        subgraphNum++;
    }

    void worklist_push(NodeType* node)
    {
        auto id = node->getId();
        if (id == 264)
        {
            llvm::outs() << "Pushing " << id << "onto worklist\n";
        }
        worklist.push(node);
    }

    /// Create a single subgraph rooted at the given node.
    void createConnectedSubgraph(NodeType* root) {
        incrementSubgraphNum();
        SubgraphIdTy subid = getSubgraphNum();
        u32_t numNodes = 0;
        worklist_push(root);
        while (!worklist.empty())
        {
            NodeType* node = worklist.pop();
            node->setSubgraphID(subid);
            numNodes++;
            for (auto&& edge: node->getInEdges()) {
                auto src = edge->getSrcNode();
                if (src->getSubgraphID() == 0)
                {
                    // Not assigned to subgraph yet.
                    worklist_push(src);
                }
            }
            for (auto&& edge:node->getOutEdges()) {
                auto dst = edge->getDstNode();
                if (dst->getSubgraphID() == 0)
                {
                    // Not assigned to subgraph yet.
                    worklist_push(dst);
                }
            }
        }
        subgraphSizeMap[subid] = numNodes;
    }

    /// Create connected subgraphs
    void createConnectedSubgraphs()
    {
        if (subgraphNum > 0)
        {
            subgraphNum = 0;
            subgraphSizeMap.clear();
            for (auto&& entry: IDToNodeMap) {
                NodeID id = entry.first;
                NodeType* node = entry.second;
                node->setSubgraphID(0);
            }
        }
        for (auto&& entry: IDToNodeMap) {
            NodeID id = entry.first;
            NodeType* node = entry.second;
            if (node->getSubgraphID() == 0)
            {
                // Not yet assigned to a subgraph.
                createConnectedSubgraph(node);
            }
        }

        u32_t max_size = 0;
        SubgraphIdTy max_subgraph = 0;
        for (auto&& entry: subgraphSizeMap) {
            SubgraphIdTy subid = entry.first;
            u32_t subgraph_size = entry.second;
            if (subgraph_size > max_size) {
                max_size = subgraph_size;
                max_subgraph = subid;
            }
        }
        // By default, set current subgraph to the largest
        currentSubgraphId = max_subgraph;
    }
protected:
    IDToNodeMapTy IDToNodeMap; ///< node map
    FILOWorkList<NodeType*> worklist; ///< Nodes who edges need to be explored

public:
};

} // End namespace SVF

/* !
 * GraphTraits specializations for generic graph algorithms.
 * Provide graph traits for tranversing from a node using standard graph traversals.
 */
namespace llvm
{

template<class EdgeTy, class NodeTy>
struct edge_unary_function
{
    NodeTy operator()(EdgeTy edge) const {
      return edge->getDstNode();
    }
};

template<class PairTy, class NodeTy>
struct pair_unary_function
{
    NodeTy operator()(PairTy pair) const {
      return pair.second;
    }
};


/*!
 * GraphTraits for nodes
 */
template<class NodeTy,class EdgeTy> struct GraphTraits<SVF::GenericNode<NodeTy,EdgeTy>*  >
{
    typedef NodeTy NodeType;
    typedef EdgeTy EdgeType;

    typedef edge_unary_function<EdgeType*, NodeType*> DerefEdge;

    // nodes_iterator/begin/end - Allow iteration over all nodes in the graph
    typedef mapped_iterator<typename SVF::GenericNode<NodeTy,EdgeTy>::iterator, DerefEdge> ChildIteratorType;

    static NodeType* getEntryNode(NodeType* pagN)
    {
        return pagN;
    }

    static inline ChildIteratorType child_begin(const NodeType* N)
    {
        return map_iterator(N->OutEdgeBegin(), DerefEdge());
    }
    static inline ChildIteratorType child_end(const NodeType* N)
    {
        return map_iterator(N->OutEdgeEnd(), DerefEdge());
    }
    static inline ChildIteratorType direct_child_begin(const NodeType *N)
    {
        return map_iterator(N->directOutEdgeBegin(), DerefEdge());
    }
    static inline ChildIteratorType direct_child_end(const NodeType *N)
    {
        return map_iterator(N->directOutEdgeEnd(), DerefEdge());
    }

};

/*!
 * Inverse GraphTraits for node which is used for inverse traversal.
 */
template<class NodeTy,class EdgeTy>
struct GraphTraits<Inverse<SVF::GenericNode<NodeTy,EdgeTy>* > >
{
    typedef NodeTy NodeType;
    typedef EdgeTy EdgeType;

    typedef edge_unary_function<EdgeType*, NodeType*> DerefEdge;

    // nodes_iterator/begin/end - Allow iteration over all nodes in the graph
    typedef mapped_iterator<typename SVF::GenericNode<NodeTy,EdgeTy>::iterator, DerefEdge> ChildIteratorType;

    static inline NodeType* getEntryNode(Inverse<NodeType* > G)
    {
        return G.Graph;
    }

    static inline ChildIteratorType child_begin(const NodeType* N)
    {
        return map_iterator(N->InEdgeBegin(), DerefEdge());
    }
    static inline ChildIteratorType child_end(const NodeType* N)
    {
        return map_iterator(N->InEdgeEnd(), DerefEdge());
    }

    static inline unsigned getNodeID(const NodeType* N)
    {
        return N->getId();
    }
};

/*!
 * GraphTraints
 */
template<class NodeTy,class EdgeTy> struct GraphTraits<SVF::GenericGraph<NodeTy,EdgeTy>* > : public GraphTraits<SVF::GenericNode<NodeTy,EdgeTy>*  >
{
    typedef SVF::GenericGraph<NodeTy,EdgeTy> GenericGraphTy;
    typedef NodeTy NodeType;
    typedef EdgeTy EdgeType;

    static NodeType* getEntryNode(GenericGraphTy* pag)
    {
        return nullptr; // return null here, maybe later we could create a dummy node
    }
    typedef std::pair<SVF::NodeID, NodeType*> PairTy;
    typedef pair_unary_function<PairTy, NodeType*> DerefVal;

    // nodes_iterator/begin/end - Allow iteration over all nodes in the graph
    typedef mapped_iterator<typename GenericGraphTy::iterator, DerefVal> nodes_iterator;

    static nodes_iterator nodes_begin(GenericGraphTy *G)
    {
        return map_iterator(G->begin(), DerefVal());
    }
    static nodes_iterator nodes_end(GenericGraphTy *G)
    {
        return map_iterator(G->end(), DerefVal());
    }

    static unsigned graphSize(GenericGraphTy* G)
    {
        return G->getTotalNodeNum();
    }

    static inline unsigned getNodeID(NodeType* N)
    {
        return N->getId();
    }
    static NodeType* getNode(GenericGraphTy *G, SVF::NodeID id)
    {
        return G->getGNode(id);
    }
};

} // End namespace llvm

#endif /* GENERICGRAPH_H_ */
