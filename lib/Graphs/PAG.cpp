//===- PAG.cpp -- Program assignment graph------------------------------------//
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
 * PAG.cpp
 *
 *  Created on: Nov 1, 2013
 *      Author: Yulei Sui
 */

#include "Util/Options.h"
#include "Graphs/PAG.h"
#include "SVF-FE/LLVMUtil.h"
#include "SVF-FE/ICFGBuilder.h"
#include "Graphs/GraphPrinter.h"

using namespace SVF;
using namespace SVFUtil;


u64_t PAGEdge::callEdgeLabelCounter = 0;
u64_t PAGEdge::storeEdgeLabelCounter = 0;
PAGEdge::Inst2LabelMap PAGEdge::inst2LabelMap;

PAG* PAG::pag = nullptr;


const std::string PAGNode::toString() const {
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "PAGNode ID: " << getId();
    return rawstr.str();
}

/// Get shape and/or color of node for .dot display.
const std::string PAGNode::getNodeAttrForDotDisplay() const {
    // TODO: Maybe use over-rides instead of these ifs,
    // But this puts them conveniently together.
    if (SVFUtil::isa<ValPN>(this))
    {
        if(SVFUtil::isa<GepValPN>(this))
            return "shape=hexagon";
        else if (SVFUtil::isa<DummyValPN>(this))
            return "shape=diamond";
        else
            return "shape=box";
    }
    else if (SVFUtil::isa<ObjPN>(this))
    {
        if(SVFUtil::isa<GepObjPN>(this))
            return "shape=doubleoctagon";
        else if(SVFUtil::isa<FIObjPN>(this))
            return "shape=box3d";
        else if (SVFUtil::isa<DummyObjPN>(this))
            return "shape=tab";
        else
            return "shape=component";
    }
    else if (SVFUtil::isa<RetPN>(this))
    {
        return "shape=Mrecord";
    }
    else if (SVFUtil::isa<VarArgPN>(this))
    {
        return "shape=octagon";
    }
    else
    {
        assert(0 && "no such kind!!");
    }
    return "";
}

void PAGNode::dump() const {
    outs() << this->toString() << "\n";
}

const std::string ValPN::toString() const {
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "ValPN ID: " << getId();
    if (Options::PAGDotGraphShorter) {
        rawstr << "\n";
    }
    rawstr << value2String(value);
    return rawstr.str();
}

const std::string ObjPN::toString() const {
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "ObjPN ID: " << getId();
    if (Options::PAGDotGraphShorter) {
        rawstr << "\n";
    }
    rawstr << value2String(value);
    return rawstr.str();
}

const std::string GepValPN::toString() const {
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "GepValPN ID: " << getId() << " with offset_" + llvm::utostr(getOffset());
    if (Options::PAGDotGraphShorter) {
        rawstr << "\n";
    }
    rawstr << value2String(value);
    return rawstr.str();
}

const std::string GepObjPN::toString() const {
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "GepObjPN ID: " << getId() << " with offset_" + llvm::itostr(ls.getOffset());
    if (Options::PAGDotGraphShorter) {
        rawstr << "\n";
    }
    rawstr << value2String(value);
    return rawstr.str();
}

const std::string FIObjPN::toString() const {
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "FIObjPN ID: " << getId() << " (base object)";
    if (Options::PAGDotGraphShorter) {
        rawstr << "\n";
    }
    rawstr << value2String(value);
    return rawstr.str();
}

const std::string RetPN::toString() const {
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "RetPN ID: " << getId() << " unique return node for function " << SVFUtil::cast<Function>(value)->getName();
    return rawstr.str();
}

const std::string VarArgPN::toString() const {
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "VarArgPN ID: " << getId() << " Var arg node for function " << SVFUtil::cast<Function>(value)->getName();
    return rawstr.str();
}

const std::string DummyValPN::toString() const {
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "DummyValPN ID: " << getId();
    return rawstr.str();
}

const std::string DummyObjPN::toString() const {
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "DummyObjPN ID: " << getId();
    return rawstr.str();
}

const std::string CloneDummyObjPN::toString() const {
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "CloneDummyObjPN ID: " << getId();
    return rawstr.str();
}

const std::string CloneGepObjPN::toString() const {
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "CloneGepObjPN ID: " << getId();
    return rawstr.str();
}

const std::string CloneFIObjPN::toString() const {
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "CloneFIObjPN ID: " << getId();
    return rawstr.str();
}

const std::string PAGEdge::toString() const {
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "PAGEdge: [" << getDstID() << "<--" << getSrcID() << "]\t";
    return rawstr.str();
}

const std::string AddrPE::toString() const{
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "AddrPE: [" << getDstID() << "<--" << getSrcID() << "]\t";
    if (Options::PAGDotGraphShorter) {
        rawstr << "\n";
    }
    rawstr << value2String(getValue());
    return rawstr.str();
}

const std::string CopyPE::toString() const{
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "CopyPE: [" << getDstID() << "<--" << getSrcID() << "]\t";
    if (Options::PAGDotGraphShorter) {
        rawstr << "\n";
    }
    rawstr << value2String(getValue());
    return rawstr.str();
}

const std::string CmpPE::toString() const{
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "CmpPE: [" << getDstID() << "<--" << getSrcID() << "]\t";
    if (Options::PAGDotGraphShorter) {
        rawstr << "\n";
    }
    rawstr << value2String(getValue());
    return rawstr.str();
}

const std::string BinaryOPPE::toString() const{
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "BinaryOPPE: [" << getDstID() << "<--" << getSrcID() << "]\t";
    if (Options::PAGDotGraphShorter) {
        rawstr << "\n";
    }
    rawstr << value2String(getValue());
    return rawstr.str();
}

const std::string UnaryOPPE::toString() const{
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "UnaryOPPE: [" << getDstID() << "<--" << getSrcID() << "]\t";
    if (Options::PAGDotGraphShorter) {
        rawstr << "\n";
    }
    rawstr << value2String(getValue());
    return rawstr.str();
}

const std::string LoadPE::toString() const{
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "LoadPE: [" << getDstID() << "<--" << getSrcID() << "]\t";
    if (Options::PAGDotGraphShorter) {
        rawstr << "\n";
    }
    rawstr << value2String(getValue());
    return rawstr.str();
}

const std::string StorePE::toString() const{
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "StorePE: [" << getDstID() << "<--" << getSrcID() << "]\t";
    if (Options::PAGDotGraphShorter) {
        rawstr << "\n";
    }
    rawstr << value2String(getValue());
    return rawstr.str();
}

const std::string GepPE::toString() const{
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "GepPE: [" << getDstID() << "<--" << getSrcID() << "]\t";
    if (Options::PAGDotGraphShorter) {
        rawstr << "\n";
    }
    rawstr << value2String(getValue());
    return rawstr.str();
}

const std::string NormalGepPE::toString() const{
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "NormalGepPE: [" << getDstID() << "<--" << getSrcID() << "]\t";
    if (Options::PAGDotGraphShorter) {
        rawstr << "\n";
    }
    rawstr << value2String(getValue());
    return rawstr.str();
}

const std::string VariantGepPE::toString() const{
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "VariantGepPE: [" << getDstID() << "<--" << getSrcID() << "]\t";
    if (Options::PAGDotGraphShorter) {
        rawstr << "\n";
    }
    rawstr << value2String(getValue());
    return rawstr.str();
}

const std::string CallPE::toString() const{
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "CallPE: [" << getDstID() << "<--" << getSrcID() << "]\t";
    if (Options::PAGDotGraphShorter) {
        rawstr << "\n";
    }
    rawstr << value2String(getValue());
    return rawstr.str();
}

const std::string RetPE::toString() const{
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "RetPE: [" << getDstID() << "<--" << getSrcID() << "]\t";
    if (Options::PAGDotGraphShorter) {
        rawstr << "\n";
    }
    rawstr << value2String(getValue());
    return rawstr.str();
}

const std::string TDForkPE::toString() const{
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "TDForkPE: [" << getDstID() << "<--" << getSrcID() << "]\t";
    if (Options::PAGDotGraphShorter) {
        rawstr << "\n";
    }
    rawstr << value2String(getValue());
    return rawstr.str();
}

const std::string TDJoinPE::toString() const{
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "TDJoinPE: [" << getDstID() << "<--" << getSrcID() << "]\t";
    if (Options::PAGDotGraphShorter) {
        rawstr << "\n";
    }
    rawstr << value2String(getValue());
    return rawstr.str();
}


PAG::PAG(bool buildFromFile) : fromFile(buildFromFile), nodeNumAfterPAGBuild(0), totalPTAPAGEdge(0)
{
    symInfo = SymbolTableInfo::SymbolInfo();
    icfg = new ICFG();
    ICFGBuilder builder(icfg);
    builder.build(getModule());
}

/*!
 * Add Address edge
 */
AddrPE* PAG::addAddrPE(NodeID src, NodeID dst)
{
    PAGNode* srcNode = getPAGNode(src);
    PAGNode* dstNode = getPAGNode(dst);
    if(PAGEdge* edge = hasNonlabeledEdge(srcNode,dstNode, PAGEdge::Addr))
        return SVFUtil::cast<AddrPE>(edge);
    else
    {
        AddrPE* addrPE = new AddrPE(srcNode, dstNode);
        addEdge(srcNode,dstNode, addrPE);
        return addrPE;
    }
}

/*!
 * Add Copy edge
 */
CopyPE* PAG::addCopyPE(NodeID src, NodeID dst)
{
    PAGNode* srcNode = getPAGNode(src);
    PAGNode* dstNode = getPAGNode(dst);
    if(PAGEdge* edge = hasNonlabeledEdge(srcNode,dstNode, PAGEdge::Copy))
        return SVFUtil::cast<CopyPE>(edge);
    else
    {
        CopyPE* copyPE = new CopyPE(srcNode, dstNode);
        addEdge(srcNode,dstNode, copyPE);
        return copyPE;
    }
}

/*!
 * Add Compare edge
 */
CmpPE* PAG::addCmpPE(NodeID src, NodeID dst)
{
    PAGNode* srcNode = getPAGNode(src);
    PAGNode* dstNode = getPAGNode(dst);
    if(PAGEdge* edge = hasNonlabeledEdge(srcNode,dstNode, PAGEdge::Cmp))
        return SVFUtil::cast<CmpPE>(edge);
    else
    {
        CmpPE* cmp = new CmpPE(srcNode, dstNode);
        addEdge(srcNode,dstNode, cmp);
        return cmp;
    }
}


/*!
 * Add Compare edge
 */
BinaryOPPE* PAG::addBinaryOPPE(NodeID src, NodeID dst)
{
    PAGNode* srcNode = getPAGNode(src);
    PAGNode* dstNode = getPAGNode(dst);
    if(PAGEdge* edge = hasNonlabeledEdge(srcNode,dstNode, PAGEdge::BinaryOp))
        return SVFUtil::cast<BinaryOPPE>(edge);
    else
    {
        BinaryOPPE* binaryOP = new BinaryOPPE(srcNode, dstNode);
        addEdge(srcNode,dstNode, binaryOP);
        return binaryOP;
    }
}

/*!
 * Add Unary edge
 */
UnaryOPPE* PAG::addUnaryOPPE(NodeID src, NodeID dst)
{
    PAGNode* srcNode = getPAGNode(src);
    PAGNode* dstNode = getPAGNode(dst);
    if(PAGEdge* edge = hasNonlabeledEdge(srcNode,dstNode, PAGEdge::UnaryOp))
        return SVFUtil::cast<UnaryOPPE>(edge);
    else
    {
        UnaryOPPE* unaryOP = new UnaryOPPE(srcNode, dstNode);
        addEdge(srcNode,dstNode, unaryOP);
        return unaryOP;
    }
}

/*!
 * Add Load edge
 */
LoadPE* PAG::addLoadPE(NodeID src, NodeID dst)
{
    PAGNode* srcNode = getPAGNode(src);
    PAGNode* dstNode = getPAGNode(dst);
    if(PAGEdge* edge = hasNonlabeledEdge(srcNode,dstNode, PAGEdge::Load))
        return SVFUtil::cast<LoadPE>(edge);
    else
    {
        LoadPE* loadPE = new LoadPE(srcNode, dstNode);
        addEdge(srcNode,dstNode, loadPE);
        return loadPE;
    }
}

/*!
 * Add Store edge
 * Note that two store instructions may share the same Store PAGEdge
 */
StorePE* PAG::addStorePE(NodeID src, NodeID dst, const IntraBlockNode* curVal)
{
    PAGNode* srcNode = getPAGNode(src);
    PAGNode* dstNode = getPAGNode(dst);
    if(PAGEdge* edge = hasLabeledEdge(srcNode,dstNode, PAGEdge::Store, curVal))
        return SVFUtil::cast<StorePE>(edge);
    else
    {
        StorePE* storePE = new StorePE(srcNode, dstNode, curVal);
        addEdge(srcNode,dstNode, storePE);
        return storePE;
    }
}

/*!
 * Add Call edge
 */
CallPE* PAG::addCallPE(NodeID src, NodeID dst, const CallBlockNode* cs)
{
    PAGNode* srcNode = getPAGNode(src);
    PAGNode* dstNode = getPAGNode(dst);
    if(PAGEdge* edge = hasLabeledEdge(srcNode,dstNode, PAGEdge::Call, cs))
        return SVFUtil::cast<CallPE>(edge);
    else
    {
        CallPE* callPE = new CallPE(srcNode, dstNode, cs);
        addEdge(srcNode,dstNode, callPE);
        return callPE;
    }
}

/*!
 * Add Return edge
 */
RetPE* PAG::addRetPE(NodeID src, NodeID dst, const CallBlockNode* cs)
{
    PAGNode* srcNode = getPAGNode(src);
    PAGNode* dstNode = getPAGNode(dst);
    if(PAGEdge* edge = hasLabeledEdge(srcNode,dstNode, PAGEdge::Ret, cs))
        return SVFUtil::cast<RetPE>(edge);
    else
    {
        RetPE* retPE = new RetPE(srcNode, dstNode, cs);
        addEdge(srcNode,dstNode, retPE);
        return retPE;
    }
}

/*!
 * Add blackhole/constant edge
 */
PAGEdge* PAG::addBlackHoleAddrPE(NodeID node)
{
    if(Options::HandBlackHole)
        return pag->addAddrPE(pag->getBlackHoleNode(), node);
    else
        return pag->addCopyPE(pag->getNullPtr(), node);
}

/*!
 * Add Thread fork edge for parameter passing from a spawner to its spawnees
 */
TDForkPE* PAG::addThreadForkPE(NodeID src, NodeID dst, const CallBlockNode* cs)
{
    PAGNode* srcNode = getPAGNode(src);
    PAGNode* dstNode = getPAGNode(dst);
    if(PAGEdge* edge = hasLabeledEdge(srcNode,dstNode, PAGEdge::ThreadFork, cs))
        return SVFUtil::cast<TDForkPE>(edge);
    else
    {
        TDForkPE* forkPE = new TDForkPE(srcNode, dstNode, cs);
        addEdge(srcNode,dstNode, forkPE);
        return forkPE;
    }
}

/*!
 * Add Thread fork edge for parameter passing from a spawnee back to its spawners
 */
TDJoinPE* PAG::addThreadJoinPE(NodeID src, NodeID dst, const CallBlockNode* cs)
{
    PAGNode* srcNode = getPAGNode(src);
    PAGNode* dstNode = getPAGNode(dst);
    if(PAGEdge* edge = hasLabeledEdge(srcNode,dstNode, PAGEdge::ThreadJoin, cs))
        return SVFUtil::cast<TDJoinPE>(edge);
    else
    {
        TDJoinPE* joinPE = new TDJoinPE(srcNode, dstNode, cs);
        addEdge(srcNode,dstNode, joinPE);
        return joinPE;
    }
}


/*!
 * Add Offset(Gep) edge
 * Find the base node id of src and connect base node to dst node
 * Create gep offset:  (offset + baseOff <nested struct gep size>)
 */
GepPE* PAG::addGepPE(NodeID src, NodeID dst, const LocationSet& ls, bool constGep)
{

    PAGNode* node = getPAGNode(src);
    if (!constGep || node->hasIncomingVariantGepEdge())
    {
        /// Since the offset from base to src is variant,
        /// the new gep edge being created is also a VariantGepPE edge.
        return addVariantGepPE(src, dst);
    }
    else
    {
        return addNormalGepPE(src, dst, ls);
    }
}

/*!
 * Add normal (Gep) edge
 */
NormalGepPE* PAG::addNormalGepPE(NodeID src, NodeID dst, const LocationSet& ls)
{
    const LocationSet& baseLS = getLocationSetFromBaseNode(src);
    PAGNode* baseNode = getPAGNode(getBaseValNode(src));
    PAGNode* dstNode = getPAGNode(dst);
    if(PAGEdge* edge = hasNonlabeledEdge(baseNode, dstNode, PAGEdge::NormalGep))
        return SVFUtil::cast<NormalGepPE>(edge);
    else
    {
        NormalGepPE* gepPE = new NormalGepPE(baseNode, dstNode, ls+baseLS);
        addEdge(baseNode, dstNode, gepPE);
        return gepPE;
    }
}

/*!
 * Add variant(Gep) edge
 * Find the base node id of src and connect base node to dst node
 */
VariantGepPE* PAG::addVariantGepPE(NodeID src, NodeID dst)
{

    PAGNode* baseNode = getPAGNode(getBaseValNode(src));
    PAGNode* dstNode = getPAGNode(dst);
    if(PAGEdge* edge = hasNonlabeledEdge(baseNode, dstNode, PAGEdge::VariantGep))
        return SVFUtil::cast<VariantGepPE>(edge);
    else
    {
        VariantGepPE* gepPE = new VariantGepPE(baseNode, dstNode);
        addEdge(baseNode, dstNode, gepPE);
        return gepPE;
    }
}



/*!
 * Add a temp field value node, this method can only invoked by getGepValNode
 * due to constaint expression, curInst is used to distinguish different instructions (e.g., memorycpy) when creating GepValPN.
 */
NodeID PAG::addGepValNode(const Value* curInst,const Value* gepVal, const LocationSet& ls, NodeID i, const Type *type, u32_t fieldidx)
{
    NodeID base = getBaseValNode(getValueNode(gepVal));
    //assert(findPAGNode(i) == false && "this node should not be created before");
    assert(0==GepValNodeMap[curInst].count(std::make_pair(base, ls))
           && "this node should not be created before");
    GepValNodeMap[curInst][std::make_pair(base, ls)] = i;
    GepValPN *node = new GepValPN(gepVal, i, ls, type, fieldidx);
    return addValNode(gepVal, node, i);
}

/*!
 * Given an object node, find its field object node
 */
NodeID PAG::getGepObjNode(NodeID id, const LocationSet& ls)
{
    PAGNode* node = pag->getPAGNode(id);
    if (GepObjPN* gepNode = SVFUtil::dyn_cast<GepObjPN>(node))
        return getGepObjNode(gepNode->getMemObj(), gepNode->getLocationSet() + ls);
    else if (FIObjPN* baseNode = SVFUtil::dyn_cast<FIObjPN>(node))
        return getGepObjNode(baseNode->getMemObj(), ls);
    else if (DummyObjPN* baseNode = SVFUtil::dyn_cast<DummyObjPN>(node))
        return getGepObjNode(baseNode->getMemObj(), ls);
    else
    {
        assert(false && "new gep obj node kind?");
        return id;
    }
}

/*!
 * Get a field obj PAG node according to base mem obj and offset
 * To support flexible field sensitive analysis with regard to MaxFieldOffset
 * offset = offset % obj->getMaxFieldOffsetLimit() to create limited number of mem objects
 * maximum number of field object creation is obj->getMaxFieldOffsetLimit()
 */
NodeID PAG::getGepObjNode(const MemObj* obj, const LocationSet& ls)
{
    NodeID base = getObjectNode(obj);

    /// if this obj is field-insensitive, just return the field-insensitive node.
    if (obj->isFieldInsensitive())
        return getFIObjNode(obj);

    LocationSet newLS = SymbolTableInfo::SymbolInfo()->getModulusOffset(obj,ls);

    // Base and first field are the same memory location.
    if (Options::FirstFieldEqBase && newLS.getOffset() == 0) return base;

    NodeLocationSetMap::iterator iter = GepObjNodeMap.find(std::make_pair(base, newLS));
    if (iter == GepObjNodeMap.end())
        return addGepObjNode(obj, newLS);
    else
        return iter->second;

}

/*!
 * Add a field obj node, this method can only invoked by getGepObjNode
 */
NodeID PAG::addGepObjNode(const MemObj* obj, const LocationSet& ls)
{
    //assert(findPAGNode(i) == false && "this node should not be created before");
    NodeID base = getObjectNode(obj);
    assert(0==GepObjNodeMap.count(std::make_pair(base, ls))
           && "this node should not be created before");

    NodeID gepId = NodeIDAllocator::get()->allocateGepObjectId(base, ls.getOffset(), StInfo::getMaxFieldLimit());
    GepObjNodeMap[std::make_pair(base, ls)] = gepId;
    GepObjPN *node = new GepObjPN(obj, gepId, ls);
    memToFieldsMap[base].set(gepId);
    return addObjNode(obj->getRefVal(), node, gepId);
}

/*!
 * Add a field-insensitive node, this method can only invoked by getFIGepObjNode
 */
NodeID PAG::addFIObjNode(const MemObj* obj)
{
    //assert(findPAGNode(i) == false && "this node should not be created before");
    NodeID base = getObjectNode(obj);
    memToFieldsMap[base].set(obj->getSymId());
    FIObjPN *node = new FIObjPN(obj->getRefVal(), obj->getSymId(), obj);
    return addObjNode(obj->getRefVal(), node, obj->getSymId());
}


/*!
 * Return true if it is an intra-procedural edge
 */
PAGEdge* PAG::hasNonlabeledEdge(PAGNode* src, PAGNode* dst, PAGEdge::PEDGEK kind)
{
    PAGEdge edge(src,dst,kind);
    PAGEdge::PAGEdgeSetTy::iterator it = PAGEdgeKindToSetMap[kind].find(&edge);
    if (it != PAGEdgeKindToSetMap[kind].end())
    {
        return *it;
    }
    return nullptr;
}

/*!
 * Return true if it is an inter-procedural edge
 */
PAGEdge* PAG::hasLabeledEdge(PAGNode* src, PAGNode* dst, PAGEdge::PEDGEK kind, const ICFGNode* callInst)
{
    PAGEdge edge(src,dst,PAGEdge::makeEdgeFlagWithCallInst(kind,callInst));
    PAGEdge::PAGEdgeSetTy::iterator it = PAGEdgeKindToSetMap[kind].find(&edge);
    if (it != PAGEdgeKindToSetMap[kind].end())
    {
        return *it;
    }
    return nullptr;
}


/*!
 * Add a PAG edge into edge map
 */
bool PAG::addEdge(PAGNode* src, PAGNode* dst, PAGEdge* edge)
{

    DBOUT(DPAGBuild,
          outs() << "add edge from " << src->getId() << " kind :"
          << src->getNodeKind() << " to " << dst->getId()
          << " kind :" << dst->getNodeKind() << "\n");
    src->addOutEdge(edge);
    dst->addInEdge(edge);
    bool added = PAGEdgeKindToSetMap[edge->getEdgeKind()].insert(edge).second;
    assert(added && "duplicated edge, not added!!!");
    if (edge->isPTAEdge())
    {
        totalPTAPAGEdge++;
        PTAPAGEdgeKindToSetMap[edge->getEdgeKind()].insert(edge);
    }
    return true;
}

/*!
 * Get all fields object nodes of an object
 */
NodeBS& PAG::getAllFieldsObjNode(const MemObj* obj)
{
    NodeID base = getObjectNode(obj);
    return memToFieldsMap[base];
}

/*!
 * Get all fields object nodes of an object
 */
NodeBS& PAG::getAllFieldsObjNode(NodeID id)
{
    const PAGNode* node = pag->getPAGNode(id);
    assert(SVFUtil::isa<ObjPN>(node) && "need an object node");
    const ObjPN* obj = SVFUtil::cast<ObjPN>(node);
    return getAllFieldsObjNode(obj->getMemObj());
}

/*!
 * Get all fields object nodes of an object
 * If this object is collapsed into one field insensitive object
 * Then only return this field insensitive object
 */
NodeBS PAG::getFieldsAfterCollapse(NodeID id)
{
    const PAGNode* node = pag->getPAGNode(id);
    assert(SVFUtil::isa<ObjPN>(node) && "need an object node");
    const MemObj* mem = SVFUtil::cast<ObjPN>(node)->getMemObj();
    if(mem->isFieldInsensitive())
    {
        NodeBS bs;
        bs.set(getFIObjNode(mem));
        return bs;
    }
    else
        return getAllFieldsObjNode(mem);
}

/*!
 * Get a base pointer given a pointer
 * Return the source node of its connected gep edge if this pointer has
 * Otherwise return the node id itself
 */
NodeID PAG::getBaseValNode(NodeID nodeId)
{
    PAGNode* node  = getPAGNode(nodeId);
    if (node->hasIncomingEdges(PAGEdge::NormalGep) ||  node->hasIncomingEdges(PAGEdge::VariantGep))
    {
        PAGEdge::PAGEdgeSetTy& ngeps = node->getIncomingEdges(PAGEdge::NormalGep);
        PAGEdge::PAGEdgeSetTy& vgeps = node->getIncomingEdges(PAGEdge::VariantGep);

        assert(((ngeps.size()+vgeps.size())==1) && "one node can only be connected by at most one gep edge!");

        PAGNode::iterator it;
        if(!ngeps.empty())
            it = ngeps.begin();
        else
            it = vgeps.begin();

        assert(SVFUtil::isa<GepPE>(*it) && "not a gep edge??");
        return (*it)->getSrcID();
    }
    else
        return nodeId;
}

/*!
 * Get a base PAGNode given a pointer
 * Return the source node of its connected normal gep edge
 * Otherwise return the node id itself
 * Size_t offset : gep offset
 */
LocationSet PAG::getLocationSetFromBaseNode(NodeID nodeId)
{
    PAGNode* node  = getPAGNode(nodeId);
    PAGEdge::PAGEdgeSetTy& geps = node->getIncomingEdges(PAGEdge::NormalGep);
    /// if this node is already a base node
    if(geps.empty())
        return LocationSet(0);

    assert(geps.size()==1 && "one node can only be connected by at most one gep edge!");
    PAGNode::iterator it = geps.begin();
    const PAGEdge* edge = *it;
    assert(SVFUtil::isa<NormalGepPE>(edge) && "not a get edge??");
    const NormalGepPE* gepEdge = SVFUtil::cast<NormalGepPE>(edge);
    return gepEdge->getLocationSet();
}

/*!
 * Clean up memory
 */
void PAG::destroy()
{
    for (PAGEdge::PAGKindToEdgeSetMapTy::iterator I =
                PAGEdgeKindToSetMap.begin(), E = PAGEdgeKindToSetMap.end(); I != E;
            ++I)
    {
        for (PAGEdge::PAGEdgeSetTy::iterator edgeIt = I->second.begin(),
                endEdgeIt = I->second.end(); edgeIt != endEdgeIt; ++edgeIt)
        {
            delete *edgeIt;
        }
    }
    SymbolTableInfo::releaseSymbolInfo();
    symInfo = nullptr;
}

/*!
 * Print this PAG graph including its nodes and edges
 */
void PAG::print()
{

    outs() << "-------------------PAG------------------------------------\n";
    PAGEdge::PAGEdgeSetTy& addrs = pag->getEdgeSet(PAGEdge::Addr);
    for (PAGEdge::PAGEdgeSetTy::iterator iter = addrs.begin(), eiter =
                addrs.end(); iter != eiter; ++iter)
    {
        outs() << (*iter)->getSrcID() << " -- Addr --> " << (*iter)->getDstID()
               << "\n";
    }

    PAGEdge::PAGEdgeSetTy& copys = pag->getEdgeSet(PAGEdge::Copy);
    for (PAGEdge::PAGEdgeSetTy::iterator iter = copys.begin(), eiter =
                copys.end(); iter != eiter; ++iter)
    {
        outs() << (*iter)->getSrcID() << " -- Copy --> " << (*iter)->getDstID()
               << "\n";
    }

    PAGEdge::PAGEdgeSetTy& calls = pag->getEdgeSet(PAGEdge::Call);
    for (PAGEdge::PAGEdgeSetTy::iterator iter = calls.begin(), eiter =
                calls.end(); iter != eiter; ++iter)
    {
        outs() << (*iter)->getSrcID() << " -- Call --> " << (*iter)->getDstID()
               << "\n";
    }

    PAGEdge::PAGEdgeSetTy& rets = pag->getEdgeSet(PAGEdge::Ret);
    for (PAGEdge::PAGEdgeSetTy::iterator iter = rets.begin(), eiter =
                rets.end(); iter != eiter; ++iter)
    {
        outs() << (*iter)->getSrcID() << " -- Ret --> " << (*iter)->getDstID()
               << "\n";
    }

    PAGEdge::PAGEdgeSetTy& tdfks = pag->getEdgeSet(PAGEdge::ThreadFork);
    for (PAGEdge::PAGEdgeSetTy::iterator iter = tdfks.begin(), eiter =
                tdfks.end(); iter != eiter; ++iter)
    {
        outs() << (*iter)->getSrcID() << " -- ThreadFork --> "
               << (*iter)->getDstID() << "\n";
    }

    PAGEdge::PAGEdgeSetTy& tdjns = pag->getEdgeSet(PAGEdge::ThreadJoin);
    for (PAGEdge::PAGEdgeSetTy::iterator iter = tdjns.begin(), eiter =
                tdjns.end(); iter != eiter; ++iter)
    {
        outs() << (*iter)->getSrcID() << " -- ThreadJoin --> "
               << (*iter)->getDstID() << "\n";
    }

    PAGEdge::PAGEdgeSetTy& ngeps = pag->getEdgeSet(PAGEdge::NormalGep);
    for (PAGEdge::PAGEdgeSetTy::iterator iter = ngeps.begin(), eiter =
                ngeps.end(); iter != eiter; ++iter)
    {
        NormalGepPE* gep = SVFUtil::cast<NormalGepPE>(*iter);
        outs() << gep->getSrcID() << " -- NormalGep (" << gep->getOffset()
               << ") --> " << gep->getDstID() << "\n";
    }

    PAGEdge::PAGEdgeSetTy& vgeps = pag->getEdgeSet(PAGEdge::VariantGep);
    for (PAGEdge::PAGEdgeSetTy::iterator iter = vgeps.begin(), eiter =
                vgeps.end(); iter != eiter; ++iter)
    {
        outs() << (*iter)->getSrcID() << " -- VariantGep --> "
               << (*iter)->getDstID() << "\n";
    }

    PAGEdge::PAGEdgeSetTy& loads = pag->getEdgeSet(PAGEdge::Load);
    for (PAGEdge::PAGEdgeSetTy::iterator iter = loads.begin(), eiter =
                loads.end(); iter != eiter; ++iter)
    {
        outs() << (*iter)->getSrcID() << " -- Load --> " << (*iter)->getDstID()
               << "\n";
    }

    PAGEdge::PAGEdgeSetTy& stores = pag->getEdgeSet(PAGEdge::Store);
    for (PAGEdge::PAGEdgeSetTy::iterator iter = stores.begin(), eiter =
                stores.end(); iter != eiter; ++iter)
    {
        outs() << (*iter)->getSrcID() << " -- Store --> " << (*iter)->getDstID()
               << "\n";
    }
    outs() << "----------------------------------------------------------\n";

}

/*
 * If this is a dummy node or node does not have incoming edges we assume it is not a pointer here
 */
bool PAG::isValidPointer(NodeID nodeId) const
{
    PAGNode* node = pag->getPAGNode(nodeId);
    if ((node->getInEdges().empty() && node->getOutEdges().empty()))
        return false;
    return node->isPointer();
}

bool PAG::isValidTopLevelPtr(const PAGNode* node)
{
    if (node->isTopLevelPtr())
    {
        if (isValidPointer(node->getId()) && node->hasValue())
        {
            if (SVFUtil::ArgInNoCallerFunction(node->getValue()))
                return false;
            return true;
        }
    }
    return false;
}
/*!
 * PAGEdge constructor
 */
PAGEdge::PAGEdge(PAGNode* s, PAGNode* d, GEdgeFlag k) :
    GenericPAGEdgeTy(s,d,k),value(nullptr),basicBlock(nullptr),icfgNode(nullptr)
{
    edgeId = PAG::getPAG()->getTotalEdgeNum();
    PAG::getPAG()->incEdgeNum();
}

/*!
 * Whether src and dst nodes are both pointer type
 */
bool PAGEdge::isPTAEdge() const
{
    return getSrcNode()->isPointer() && getDstNode()->isPointer();
}

/*!
 * PAGNode constructor
 */
PAGNode::PAGNode(const Value* val, NodeID i, PNODEK k) :
    GenericPAGNodeTy(i,k), value(val)
{

    assert( ValNode <= k && k <= CloneDummyObjNode && "new PAG node kind?");

    switch (k)
    {
    case ValNode:
    case GepValNode:
    {
        assert(val != nullptr && "value is nullptr for ValPN or GepValNode");
        isTLPointer = val->getType()->isPointerTy();
        isATPointer = false;
        break;
    }

    case RetNode:
    {
        assert(val != nullptr && "value is nullptr for RetNode");
        isTLPointer = SVFUtil::cast<Function>(val)->getReturnType()->isPointerTy();
        isATPointer = false;
        break;
    }

    case VarargNode:
    case DummyValNode:
    {
        isTLPointer = true;
        isATPointer = false;
        break;
    }

    case ObjNode:
    case GepObjNode:
    case FIObjNode:
    case DummyObjNode:
    case CloneGepObjNode:
    case CloneFIObjNode:
    case CloneDummyObjNode:
    {
        isTLPointer = false;
        isATPointer = true;
        break;
    }
    }
}

bool PAGNode::isIsolatedNode() const{
	if (getInEdges().empty() && getOutEdges().empty())
		return true;
	else if (isConstantData())
		return true;
	else if (value && SVFUtil::isa<Function>(value))
		return SVFUtil::isIntrinsicFun(SVFUtil::cast<Function>(value));
	else
		return false;
}


/*!
 * Dump this PAG
 */
void PAG::dump(std::string name)
{
    GraphPrinter::SelectiveWriteGraphToFile(outs(), name, this);
}

/*!
 * View PAG
 */
void PAG::view()
{
    GraphPrinter::SelectiveWriteGraphToFile(outs(), "PAG", this, false, true);
}

/*!
 * Whether to handle blackhole edge
 */
void PAG::handleBlackHole(bool b)
{
    Options::HandBlackHole = b;
}

namespace llvm
{
/*!
 * Write value flow graph into dot file for debugging
 */
template<>
struct DOTGraphTraits<PAG*> : public DefaultDOTGraphTraits
{

    typedef PAGNode NodeType;
    typedef NodeType::iterator ChildIteratorType;
    DOTGraphTraits(bool isSimple = false) :
        DefaultDOTGraphTraits(isSimple)
    {
    }

    /// Return name of the graph
    static std::string getGraphName(PAG *graph)
    {
        return graph->getGraphName();
    }

    /// isNodeHidden - If the function returns true, the given node is not
    /// displayed in the graph
    static bool isNodeHidden(PAGNode *node) {
        return node->reallyHideNode(node->isIsolatedNode());
    }

    /// Return label of a VFG node with two display mode
    /// Either you can choose to display the name of the value or the whole instruction
    static std::string getNodeLabel(PAGNode *node, PAG*)
    {
        std::string str;
        raw_string_ostream rawstr(str);
        if (SubgraphIdTy subid = node->getSubgraphID()) {
            rawstr << "sg" << subid << ":";
        }
        // print function info
        if (node->getFunction())
            rawstr << "[" << node->getFunction()->getName() << "] ";

        rawstr << node->toString();

        return rawstr.str();

    }

    static std::string getNodeAttributes(PAGNode *node, PAG*)
    {
        return node->getNodeAttrForDotDisplay();
    }

    template<class EdgeIter>
    static std::string getEdgeAttributes(PAGNode*, EdgeIter EI, PAG*)
    {
        const PAGEdge* edge = *(EI.getCurrent());
        assert(edge && "No edge found!!");
        if (SVFUtil::isa<AddrPE>(edge))
        {
            return "color=green";
        }
        else if (SVFUtil::isa<CopyPE>(edge))
        {
            return "color=black";
        }
        else if (SVFUtil::isa<GepPE>(edge))
        {
            return "color=purple";
        }
        else if (SVFUtil::isa<StorePE>(edge))
        {
            return "color=blue";
        }
        else if (SVFUtil::isa<LoadPE>(edge))
        {
            return "color=red";
        }
        else if (SVFUtil::isa<CmpPE>(edge))
        {
            return "color=grey";
        }
        else if (SVFUtil::isa<BinaryOPPE>(edge))
        {
            return "color=grey";
        }
        else if (SVFUtil::isa<UnaryOPPE>(edge))
        {
            return "color=grey";
        }
        else if (SVFUtil::isa<TDForkPE>(edge))
        {
            return "color=Turquoise";
        }
        else if (SVFUtil::isa<TDJoinPE>(edge))
        {
            return "color=Turquoise";
        }
        else if (SVFUtil::isa<CallPE>(edge))
        {
            return "color=black,style=dashed";
        }
        else if (SVFUtil::isa<RetPE>(edge))
        {
            return "color=black,style=dotted";
        }

        assert(false && "No such kind edge!!");
        exit(1);
    }

    template<class EdgeIter>
    static std::string getEdgeSourceLabel(PAGNode*, EdgeIter EI)
    {
        const PAGEdge* edge = *(EI.getCurrent());
        assert(edge && "No edge found!!");
        if(const CallPE* calledge = SVFUtil::dyn_cast<CallPE>(edge))
        {
            const Instruction* callInst= calledge->getCallSite()->getCallSite();
            return SVFUtil::getSourceLoc(callInst);
        }
        else if(const RetPE* retedge = SVFUtil::dyn_cast<RetPE>(edge))
        {
            const Instruction* callInst= retedge->getCallSite()->getCallSite();
            return SVFUtil::getSourceLoc(callInst);
        }
        return "";
    }
};
} // End namespace llvm
