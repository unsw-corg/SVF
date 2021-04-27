//===- ICFG.cpp -- Sparse value-flow graph-----------------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2018>  <Yulei Sui>
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
 * ICFG.cpp
 *
 *  Created on: Sep 11, 2018
 *      Author: Yulei Sui
 */

#include <Util/Options.h>
#include "SVF-FE/LLVMUtil.h"
#include "Util/SVFModule.h"
#include "Graphs/ICFG.h"
#include "Graphs/PAG.h"
#include "Graphs/PTACallGraph.h"
#include "Graphs/GraphPrinter.h"

using namespace SVF;
using namespace SVFUtil;


FunEntryBlockNode::FunEntryBlockNode(NodeID id, const SVFFunction* f) : InterBlockNode(id, FunEntryBlock)
{
    fun = f;
    // if function is implemented
    if (f->getLLVMFun()->begin() != f->getLLVMFun()->end()) {
        bb = &(f->getLLVMFun()->getEntryBlock());
    }
}

FunExitBlockNode::FunExitBlockNode(NodeID id, const SVFFunction* f) : InterBlockNode(id, FunExitBlock), fun(f), formalRet(nullptr)
{
    fun = f;
    // if function is implemented
    if (f->getLLVMFun()->begin() != f->getLLVMFun()->end()) {
        bb = SVFUtil::getFunExitBB(f->getLLVMFun());
    }

}


const std::string ICFGNode::toString() const {
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "ICFGNode ID: " << getId();
    return rawstr.str();
}

void ICFGNode::dump() const {
    outs() << this->toString() << "\n";
}

const std::string GlobalBlockNode::toString() const {
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "GlobalBlockNode ID: " << getId();
    return rawstr.str();
}


const std::string IntraBlockNode::toString() const {
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "IntraBlockNode ID: " << getId();
    rawstr << value2String(getInst()) << " {fun: " << getFun()->getName() << "}";
    return rawstr.str();
}


const std::string FunEntryBlockNode::toString() const {
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "FunEntryBlockNode ID: " << getId();
    if (isExtCall(getFun()))
        rawstr << " Entry(" << ")\n";
    else
        rawstr << " Entry(" << getSourceLoc(getFun()->getLLVMFun()) << ")\n";
    rawstr << " {fun: " << getFun()->getName() << "}";
    return rawstr.str();
}

const std::string FunExitBlockNode::toString() const {
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "FunExitBlockNode ID: " << getId();
    if (isExtCall(getFun()))
        rawstr << " Exit(" << ")\n";
    else
        rawstr << " Exit(" << getSourceLoc(getFunExitBB(getFun()->getLLVMFun())->getFirstNonPHI()) << ")\n";
    rawstr << " {fun: " << getFun()->getName() << "}";
    return rawstr.str();
}


const std::string CallBlockNode::toString() const {
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "CallBlockNode ID: " << getId();
    rawstr << value2String(getCallSite()) << " {fun: " << getFun()->getName() << "}";
    return rawstr.str();
}

const std::string RetBlockNode::toString() const {
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "RetBlockNode ID: " << getId();
    rawstr << value2String(getCallSite()) << " {fun: " << getFun()->getName() << "}";
    return rawstr.str();
}

const std::string ICFGEdge::toString() const {
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "ICFGEdge: [" << getDstID() << "<--" << getSrcID() << "]\t";
    return rawstr.str();
}

const std::string IntraCFGEdge::toString() const {
    std::string str;
    raw_string_ostream rawstr(str);
    if(brCondition.first == nullptr)
        rawstr << "IntraCFGEdge: [" << getDstID() << "<--" << getSrcID() << "]\t";
    else
        rawstr << "IntraCFGEdge: [" << getDstID() << "<--" << getSrcID() << "] with condition (" << *brCondition.first << "==" << brCondition.second << ") \t";

    return rawstr.str();
}

const std::string CallCFGEdge::toString() const {
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "CallCFGEdge " << " [";
    rawstr << getDstID() << "<--" << getSrcID() << "]\t CallSite: " << *cs << "\t";
    return rawstr.str();
}

const std::string RetCFGEdge::toString() const {
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "RetCFGEdge " << " [";
    rawstr << getDstID() << "<--" << getSrcID() << "]\t CallSite: " << *cs << "\t";
    return rawstr.str();
}

/*!
 * Constructor
 *  * Build ICFG
 * 1) build ICFG nodes
 *    statements for top level pointers (PAGEdges)
 * 2) connect ICFG edges
 *    between two statements (PAGEdges)
 */
ICFG::ICFG(): totalICFGNode(0)
{
    DBOUT(DGENERAL, outs() << pasMsg("\tCreate ICFG ...\n"));
    globalBlockNode = new GlobalBlockNode(totalICFGNode++);
    addICFGNode(globalBlockNode);
}


/// Get a basic block ICFGNode
ICFGNode* ICFG::getBlockICFGNode(const Instruction* inst)
{
    ICFGNode* node;
    if(SVFUtil::isNonInstricCallSite(inst))
        node = getCallBlockNode(inst);
    else if(SVFUtil::isIntrinsicInst(inst))
        node = getIntraBlockNode(inst);
//			assert (false && "associating an intrinsic instruction with an ICFGNode!");
    else
        node = getIntraBlockNode(inst);

    assert (node!=nullptr && "no ICFGNode for this instruction?");
    return node;
}


CallBlockNode* ICFG::getCallBlockNode(const Instruction* inst)
{
	if(SVFUtil::isCallSite(inst) ==false)
		outs() << *inst << "\n";
    assert(SVFUtil::isCallSite(inst) && "not a call instruction?");
    assert(SVFUtil::isNonInstricCallSite(inst) && "associating an intrinsic debug instruction with an ICFGNode!");
    CallBlockNode* node = getCallICFGNode(inst);
    if(node==nullptr)
        node = addCallICFGNode(inst);
    assert (node!=nullptr && "no CallBlockNode for this instruction?");
    return node;
}

RetBlockNode* ICFG::getRetBlockNode(const Instruction* inst)
{
    assert(SVFUtil::isCallSite(inst) && "not a call instruction?");
    assert(SVFUtil::isNonInstricCallSite(inst) && "associating an intrinsic debug instruction with an ICFGNode!");
    RetBlockNode* node = getRetICFGNode(inst);
    if(node==nullptr)
        node = addRetICFGNode(inst);
    assert (node!=nullptr && "no RetBlockNode for this instruction?");
    return node;
}

IntraBlockNode* ICFG::getIntraBlockNode(const Instruction* inst)
{
    IntraBlockNode* node = getIntraBlockICFGNode(inst);
    if(node==nullptr)
        node = addIntraBlockICFGNode(inst);
    return node;
}

/// Add a function entry node
FunEntryBlockNode* ICFG::getFunEntryBlockNode(const SVFFunction*  fun)
{
    FunEntryBlockNode* b = getFunEntryICFGNode(fun);
    if (b == nullptr)
        return addFunEntryICFGNode(fun);
    else
        return b;
}
/// Add a function exit node
FunExitBlockNode* ICFG::getFunExitBlockNode(const SVFFunction*  fun)
{
    FunExitBlockNode* b = getFunExitICFGNode(fun);
    if (b == nullptr)
        return addFunExitICFGNode(fun);
    else
        return b;
}

/*!
 * Whether we has an intra ICFG edge
 */
ICFGEdge* ICFG::hasIntraICFGEdge(ICFGNode* src, ICFGNode* dst, ICFGEdge::ICFGEdgeK kind)
{
    ICFGEdge edge(src,dst,kind);
    ICFGEdge* outEdge = src->hasOutgoingEdge(&edge);
    ICFGEdge* inEdge = dst->hasIncomingEdge(&edge);
    if (outEdge && inEdge)
    {
        assert(outEdge == inEdge && "edges not match");
        return outEdge;
    }
    else
        return nullptr;
}

/*!
 * Whether we has an inter ICFG edge
 */
ICFGEdge* ICFG::hasInterICFGEdge(ICFGNode* src, ICFGNode* dst, ICFGEdge::ICFGEdgeK kind)
{
    ICFGEdge edge(src,dst, kind);
    ICFGEdge* outEdge = src->hasOutgoingEdge(&edge);
    ICFGEdge* inEdge = dst->hasIncomingEdge(&edge);
    if (outEdge && inEdge)
    {
        assert(outEdge == inEdge && "edges not match");
        return outEdge;
    }
    else
        return nullptr;
}

/*!
 * Whether we has an thread ICFG edge
 */
ICFGEdge* ICFG::hasThreadICFGEdge(ICFGNode* src, ICFGNode* dst, ICFGEdge::ICFGEdgeK kind)
{
    ICFGEdge edge(src,dst,kind);
    ICFGEdge* outEdge = src->hasOutgoingEdge(&edge);
    ICFGEdge* inEdge = dst->hasIncomingEdge(&edge);
    if (outEdge && inEdge)
    {
        assert(outEdge == inEdge && "edges not match");
        return outEdge;
    }
    else
        return nullptr;
}


/*!
 * Return the corresponding ICFGEdge
 */
ICFGEdge* ICFG::getICFGEdge(const ICFGNode* src, const ICFGNode* dst, ICFGEdge::ICFGEdgeK kind)
{

    ICFGEdge * edge = nullptr;
    Size_t counter = 0;
    for (ICFGEdge::ICFGEdgeSetTy::iterator iter = src->OutEdgeBegin();
            iter != src->OutEdgeEnd(); ++iter)
    {
        if ((*iter)->getDstID() == dst->getId() && (*iter)->getEdgeKind() == kind)
        {
            counter++;
            edge = (*iter);
        }
    }
    assert(counter <= 1 && "there's more than one edge between two ICFG nodes");
    return edge;

}

/*!
 * Add intraprocedural edges between two nodes
 */
ICFGEdge* ICFG::addIntraEdge(ICFGNode* srcNode, ICFGNode* dstNode)
{
    checkIntraEdgeParents(srcNode, dstNode);
    if(ICFGEdge* edge = hasIntraICFGEdge(srcNode,dstNode, ICFGEdge::IntraCF))
    {
        assert(edge->isIntraCFGEdge() && "this should be an intra CFG edge!");
        return nullptr;
    }
    else
    {
        IntraCFGEdge* intraEdge = new IntraCFGEdge(srcNode,dstNode);
        return (addICFGEdge(intraEdge) ? intraEdge : nullptr);
    }
}

/*!
 * Add conditional intraprocedural edges between two nodes
 */
ICFGEdge* ICFG::addConditionalIntraEdge(ICFGNode* srcNode, ICFGNode* dstNode, const Value* condition, NodeID branchID){

    checkIntraEdgeParents(srcNode, dstNode);
    if(ICFGEdge* edge = hasIntraICFGEdge(srcNode,dstNode, ICFGEdge::IntraCF))
    {
        assert(edge->isIntraCFGEdge() && "this should be an intra CFG edge!");
        return nullptr;
    }
    else
    {
        IntraCFGEdge* intraEdge = new IntraCFGEdge(srcNode,dstNode);
        intraEdge->setBranchCondtion(condition,branchID);
        return (addICFGEdge(intraEdge) ? intraEdge : nullptr);
    }
}


/*!
 * Add interprocedural call edges between two nodes
 */
ICFGEdge* ICFG::addCallEdge(ICFGNode* srcNode, ICFGNode* dstNode, const Instruction*  cs)
{
    if(ICFGEdge* edge = hasInterICFGEdge(srcNode,dstNode, ICFGEdge::CallCF))
    {
        assert(edge->isCallCFGEdge() && "this should be a call CFG edge!");
        return nullptr;
    }
    else
    {
        CallCFGEdge* callEdge = new CallCFGEdge(srcNode,dstNode,cs);
        return (addICFGEdge(callEdge) ? callEdge : nullptr);
    }
}

/*!
 * Add interprocedural return edges between two nodes
 */
ICFGEdge* ICFG::addRetEdge(ICFGNode* srcNode, ICFGNode* dstNode, const Instruction*  cs)
{
    if(ICFGEdge* edge = hasInterICFGEdge(srcNode,dstNode, ICFGEdge::RetCF))
    {
        assert(edge->isRetCFGEdge() && "this should be a return CFG edge!");
        return nullptr;
    }
    else
    {
        RetCFGEdge* retEdge = new RetCFGEdge(srcNode,dstNode,cs);
        return (addICFGEdge(retEdge) ? retEdge : nullptr);
    }
}


/*!
 * Dump ICFG
 */
void ICFG::dump(const std::string& file, bool simple)
{
    GraphPrinter::SelectiveWriteGraphToFile(outs(), file, this, simple);
}

/*!
 * View ICFG
 */
void ICFG::view()
{
    GraphPrinter::SelectiveWriteGraphToFile(outs(), "ICFG", this, false, true);
}

/*!
 * Update ICFG for indirect calls
 */
void ICFG::updateCallGraph(PTACallGraph* callgraph)
{
    PTACallGraph::CallEdgeMap::const_iterator iter = callgraph->getIndCallMap().begin();
    PTACallGraph::CallEdgeMap::const_iterator eiter = callgraph->getIndCallMap().end();
    for (; iter != eiter; iter++)
    {
        const CallBlockNode* callBlock = iter->first;
        const Instruction* cs = callBlock->getCallSite();
        assert(callBlock->isIndirectCall() && "this is not an indirect call?");
        const PTACallGraph::FunctionSet & functions = iter->second;
        for (PTACallGraph::FunctionSet::const_iterator func_iter = functions.begin(); func_iter != functions.end(); func_iter++)
        {
            const SVFFunction*  callee = *func_iter;
            CallBlockNode* CallBlockNode = getCallBlockNode(cs);
            if (!isExtCall(callee))
            {
                FunEntryBlockNode* calleeEntryNode = getFunEntryICFGNode(callee);
                addCallEdge(CallBlockNode, calleeEntryNode, cs);
                RetBlockNode* retBlockNode = getRetBlockNode(cs);
                FunExitBlockNode* calleeExitNode = getFunExitICFGNode(callee);
                addRetEdge(calleeExitNode, retBlockNode, cs);
            }
        }
    }
}

/*!
 * GraphTraits specialization
 */
namespace llvm
{
template<>
struct DOTGraphTraits<ICFG*> : public DOTGraphTraits<PAG*>
{

    typedef ICFGNode NodeType;
    DOTGraphTraits(bool isSimple = false) :
        DOTGraphTraits<PAG*>(isSimple)
    {
    }

    /// Return name of the graph
    static std::string getGraphName(ICFG*)
    {
        return "ICFG";
    }

    /// isNodeHidden - If the function returns true, the given node is not
    /// displayed in the graph
    static bool isNodeHidden(NodeType *node) {
        return node->reallyHideNode(false);
    }

    std::string getNodeLabel(NodeType *node, ICFG *graph)
    {
        return getSimpleNodeLabel(node, graph);
    }

    /// Return the label of an ICFG node
    static std::string getSimpleNodeLabel(NodeType *node, ICFG*)
    {
        std::string str;
        raw_string_ostream rawstr(str);
        rawstr << "NodeID: " << node->getId() << "\n";
        if (IntraBlockNode* bNode = SVFUtil::dyn_cast<IntraBlockNode>(node))
        {
            rawstr << "IntraBlockNode ID: " << bNode->getId() << " \t";
            PAG::PAGEdgeList&  edges = PAG::getPAG()->getInstPTAPAGEdgeList(bNode);
            if (edges.empty()) {
                rawstr << value2String(bNode->getInst()) << " \t";
            } else {
                for (PAG::PAGEdgeList::iterator it = edges.begin(), eit = edges.end(); it != eit; ++it)
                {
                    const PAGEdge* edge = *it;
                    rawstr << edge->toString();
                }
            }
            rawstr << " {fun: " << bNode->getFun()->getName() << "}";
        }
        else if (FunEntryBlockNode* entry = SVFUtil::dyn_cast<FunEntryBlockNode>(node))
        {
            rawstr << entry->toString();
        }
        else if (FunExitBlockNode* exit = SVFUtil::dyn_cast<FunExitBlockNode>(node))
        {
            rawstr << exit->toString();
        }
        else if (CallBlockNode* call = SVFUtil::dyn_cast<CallBlockNode>(node))
        {
            rawstr << call->toString();
        }
        else if (RetBlockNode* ret = SVFUtil::dyn_cast<RetBlockNode>(node))
        {
            rawstr << ret->toString();
        }
        else if (GlobalBlockNode* glob  = SVFUtil::dyn_cast<GlobalBlockNode>(node) )
        {
            PAG::PAGEdgeList&  edges = PAG::getPAG()->getInstPTAPAGEdgeList(glob);
            for (PAG::PAGEdgeList::iterator it = edges.begin(), eit = edges.end(); it != eit; ++it)
            {
                const PAGEdge* edge = *it;
                rawstr << edge->toString();
            }
        }
        else
            assert(false && "what else kinds of nodes do we have??");

        return rawstr.str();
    }

    static std::string getNodeAttributes(NodeType *node, ICFG*)
    {
        std::string str;
        raw_string_ostream rawstr(str);

        if(SVFUtil::isa<IntraBlockNode>(node))
        {
            rawstr <<  "color=black";
        }
        else if(SVFUtil::isa<FunEntryBlockNode>(node))
        {
            rawstr <<  "color=yellow";
        }
        else if(SVFUtil::isa<FunExitBlockNode>(node))
        {
            rawstr <<  "color=green";
        }
        else if(SVFUtil::isa<CallBlockNode>(node))
        {
            rawstr <<  "color=red";
        }
        else if(SVFUtil::isa<RetBlockNode>(node))
        {
            rawstr <<  "color=blue";
        }
        else if(SVFUtil::isa<GlobalBlockNode>(node))
        {
            rawstr <<  "color=purple";
        }
        else
            assert(false && "no such kind of node!!");

        rawstr <<  "";

        return rawstr.str();
    }

    template<class EdgeIter>
    static std::string getEdgeAttributes(NodeType*, EdgeIter EI, ICFG*)
    {
        ICFGEdge* edge = *(EI.getCurrent());
        assert(edge && "No edge found!!");
        if (SVFUtil::isa<CallCFGEdge>(edge))
            return "style=solid,color=red";
        else if (SVFUtil::isa<RetCFGEdge>(edge))
            return "style=solid,color=blue";
        else
            return "style=solid";
        return "";
    }

    template<class EdgeIter>
    static std::string getEdgeSourceLabel(NodeType*, EdgeIter EI)
    {
        ICFGEdge* edge = *(EI.getCurrent());
        assert(edge && "No edge found!!");

        std::string str;
        raw_string_ostream rawstr(str);
        if (CallCFGEdge* dirCall = SVFUtil::dyn_cast<CallCFGEdge>(edge))
            rawstr << dirCall->getCallSite();
        else if (RetCFGEdge* dirRet = SVFUtil::dyn_cast<RetCFGEdge>(edge))
            rawstr << dirRet->getCallSite();

        return rawstr.str();
    }
};
} // End namespace llvm
