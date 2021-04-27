//===----- DCHG.cpp  CHG using DWARF debug info ------------------------//
//
/*
 * DCHG.cpp
 *
 *  Created on: Aug 24, 2019
 *      Author: Mohamad Barbar
 */

#include <sstream>

#include "Util/Options.h"
#include "SVF-FE/DCHG.h"
#include "SVF-FE/CPPUtil.h"
#include "Util/SVFUtil.h"
#include "llvm/IR/DebugInfo.h"
#include "Graphs/GraphPrinter.h"

using namespace SVF;


void DCHGraph::handleDIBasicType(const DIBasicType *basicType)
{
    getOrCreateNode(basicType);
}

void DCHGraph::handleDICompositeType(const DICompositeType *compositeType)
{
    switch (compositeType->getTag())
    {
    case dwarf::DW_TAG_array_type:
        if (extended) getOrCreateNode(compositeType);
        gatherAggs(compositeType);
        break;
    case dwarf::DW_TAG_class_type:
    case dwarf::DW_TAG_structure_type:
        getOrCreateNode(compositeType);
        // If we're extending, we need to add the first-field relation.
        if (extended)
        {
            DINodeArray fields = compositeType->getElements();
            if (!fields.empty())
            {
                // We want the first non-static, non-function member; it may not exist.
                DIDerivedType *firstMember = nullptr;
                for (DINode *n : fields)
                {
                    if (DIDerivedType *fm = SVFUtil::dyn_cast<DIDerivedType>(n))
                    {
                        if (fm->getTag() == dwarf::DW_TAG_member && !fm->isStaticMember())
                        {
                            firstMember = fm;
                            break;
                        }
                    }
                }

                if (firstMember != nullptr)
                {
                    // firstMember is a DW_TAG_member, we want the base type beneath it.
                    addEdge(compositeType, firstMember->getBaseType(), DCHEdge::FIRST_FIELD);
                }
            }
        }

        flatten(compositeType);
        gatherAggs(compositeType);

        break;
    case dwarf::DW_TAG_union_type:
        getOrCreateNode(compositeType);
        // All fields are first fields.
        if (extended)
        {
            DINodeArray fields = compositeType->getElements();
            for (DINode *field : fields)
            {
                // fields[0] gives a type which is DW_TAG_member, we want the member's type (getBaseType).
                DIDerivedType *firstMember = SVFUtil::dyn_cast<DIDerivedType>(field);
                assert(firstMember != nullptr && "DCHG: expected member type");
                addEdge(compositeType, firstMember->getBaseType(), DCHEdge::FIRST_FIELD);
            }
        }

        // flatten(compositeType);
        gatherAggs(compositeType);

        break;
    case dwarf::DW_TAG_enumeration_type:
        getOrCreateNode(compositeType);
        break;
    default:
        assert(false && "DCHGraph::buildCHG: unexpected CompositeType tag.");
    }
}

void DCHGraph::handleDIDerivedType(const DIDerivedType *derivedType)
{
    switch (derivedType->getTag())
    {
    case dwarf::DW_TAG_inheritance:
    {
        assert(SVFUtil::isa<DIType>(derivedType->getScope()) && "inheriting from non-type?");
        DCHEdge *edge = addEdge(SVFUtil::dyn_cast<DIType>(derivedType->getScope()),
                                derivedType->getBaseType(), DCHEdge::INHERITANCE);
        // If the offset does not exist (for primary base), getOffset should return 0.
        edge->setOffset(derivedType->getOffsetInBits());
        break;
    }
    case dwarf::DW_TAG_member:
    case dwarf::DW_TAG_friend:
        // Don't care.
        break;
    case dwarf::DW_TAG_typedef:
        handleTypedef(derivedType);
        break;
    case dwarf::DW_TAG_pointer_type:
    case dwarf::DW_TAG_ptr_to_member_type:
    case dwarf::DW_TAG_reference_type:
    case dwarf::DW_TAG_rvalue_reference_type:
        if (extended) getOrCreateNode(derivedType);
        break;
    case dwarf::DW_TAG_const_type:
    case dwarf::DW_TAG_atomic_type:
    case dwarf::DW_TAG_volatile_type:
    case dwarf::DW_TAG_restrict_type:
        break;
    default:
        assert(false && "DCHGraph::buildCHG: unexpected DerivedType tag.");
    }
}

void DCHGraph::handleDISubroutineType(const DISubroutineType *subroutineType)
{
    getOrCreateNode(subroutineType);
}

void DCHGraph::handleTypedef(const DIType *typedefType)
{
    assert(typedefType && typedefType->getTag() == dwarf::DW_TAG_typedef);

    // Need to gather them in a set first because we don't know the base type till
    // we get to the bottom of the (potentially many) typedefs.
    std::vector<const DIDerivedType *> typedefs;
    // Check for nullptr because you can typedef void.
    while (typedefType != nullptr && typedefType->getTag() == dwarf::DW_TAG_typedef)
    {
        const DIDerivedType *typedefDerivedType = SVFUtil::dyn_cast<DIDerivedType>(typedefType);
        // The typedef itself.
        typedefs.push_back(typedefDerivedType);

        // Next in the typedef line.
        typedefType = typedefDerivedType->getBaseType();
    }

    const DIType *baseType = typedefType;
    DCHNode *baseTypeNode = getOrCreateNode(baseType);

    for (const DIDerivedType *tdef : typedefs)
    {
        // Base type needs to hold its typedefs.
        baseTypeNode->addTypedef(tdef);
    }
}

void DCHGraph::buildVTables(const Module &module)
{
    for (Module::const_global_iterator gvI = module.global_begin(); gvI != module.global_end(); ++gvI)
    {
        // Though this will return more than GlobalVariables, we only care about GlobalVariables (for the vtbls).
        const GlobalVariable *gv = SVFUtil::dyn_cast<const GlobalVariable>(&(*gvI));
        if (gv == nullptr) continue;
        if (gv->hasMetadata(cppUtil::ctir::vtMDName) && gv->getNumOperands() > 0)
        {
            DIType *type = SVFUtil::dyn_cast<DIType>(gv->getMetadata(cppUtil::ctir::vtMDName));
            assert(type && "DCHG::buildVTables: bad metadata for ctir.vt");
            DCHNode *node = getOrCreateNode(type);
            node->setVTable(gv);
            vtblToTypeMap[gv] = getCanonicalType(type);

            const ConstantStruct *vtbls = SVFUtil::dyn_cast<ConstantStruct>(gv->getOperand(0));
            assert(vtbls && "unexpected vtable type");
            for (unsigned nthVtbl = 0; nthVtbl < vtbls->getNumOperands(); ++nthVtbl)
            {
                const ConstantArray *vtbl = SVFUtil::dyn_cast<ConstantArray>(vtbls->getOperand(nthVtbl));
                assert(vtbl && "Element of vtbl struct not an array");

                std::vector<const Function *> &vfns = node->getVfnVector(nthVtbl);

                // Iterating over the vtbl, we will run into:
                // 1. i8* null         (don't care for now).
                // 2. i8* inttoptr ... (don't care for now).
                // 3. i8* bitcast  ... (we only care when a function pointer is being bitcasted).
                for (unsigned cN = 0; cN < vtbl->getNumOperands(); ++cN)
                {
                    Constant *c = vtbl->getOperand(cN);
                    if (SVFUtil::isa<ConstantPointerNull>(c))
                    {
                        // Don't care for now.
                        continue;
                    }

                    ConstantExpr *ce = SVFUtil::dyn_cast<ConstantExpr>(c);
                    assert(ce && "non-ConstantExpr, non-ConstantPointerNull in vtable?");
                    if (ce->getOpcode() == Instruction::BitCast)
                    {
                        // Could be a GlobalAlias which we don't care about, or a virtual/thunk function.
                        const Function *vfn = SVFUtil::dyn_cast<Function>(ce->getOperand(0));
                        if (vfn != nullptr)
                        {
                            vfns.push_back(vfn);
                        }
                    }
                }
            }
        }
    }
}

const NodeBS &DCHGraph::cha(const DIType *type, bool firstField)
{
    type = getCanonicalType(type);
    Map<const DIType *, NodeBS> &cacheMap = firstField ? chaFFMap : chaMap;

    // Check if we've already computed.
    if (cacheMap.find(type) != cacheMap.end())
    {
        return cacheMap[type];
    }

    NodeBS children;
    const DCHNode *node = getOrCreateNode(type);
    // Consider oneself a child, otherwise the recursion will just come up with nothing.
    children.set(node->getId());
    for (const DCHEdge *edge : node->getInEdges())
    {
        // Don't care about anything but inheritance, first-field, and standard def. edges.
        if (   edge->getEdgeKind() != DCHEdge::INHERITANCE
                && edge->getEdgeKind() != DCHEdge::FIRST_FIELD
                && edge->getEdgeKind() != DCHEdge::STD_DEF)
        {
            continue;
        }

        // We only care about first-field edges if the flag is on.
        if (!firstField && edge->getEdgeKind() == DCHEdge::FIRST_FIELD)
        {
            continue;
        }

        const NodeBS &cchildren = cha(edge->getSrcNode()->getType(), firstField);
        // Children's children are my children.
        for (NodeID cchild : cchildren)
        {
            children.set(cchild);
        }
    }

    // Cache results.
    cacheMap.insert({type, children});
    // Return the permanent object; we're returning a reference.
    return cacheMap[type];
}

void DCHGraph::flatten(const DICompositeType *type)
{
    type = SVFUtil::dyn_cast<DICompositeType>(getCanonicalType(type));
    assert(type && "DCHG::flatten: canon type of struct/class is not struct/class");
    if (fieldTypes.find(type) != fieldTypes.end())
    {
        // Already done (necessary because of the recursion).
        return;
    }

    // Create empty vector.
    fieldTypes[type];

    assert(type != nullptr
           && (type->getTag() == dwarf::DW_TAG_class_type
               || type->getTag() == dwarf::DW_TAG_structure_type)
           && "DCHG::flatten: expected a class/struct");

    // Sort the fields from getElements. Especially a problem for classes; it's all jumbled up.
    std::vector<const DIDerivedType *> fields;
    DINodeArray fieldsDINA = type->getElements();
    for (unsigned i = 0; i < fieldsDINA.size(); ++i)
    {
        if (const DIDerivedType *dt = SVFUtil::dyn_cast<DIDerivedType>(fieldsDINA[i]))
        {
            // Don't care about subprograms, only member/inheritance.
            fields.push_back(dt);
        }
    }

    // TODO: virtual inheritance is not handled at all!
    std::sort(fields.begin(), fields.end(),
              [](const DIDerivedType *&a, const DIDerivedType *&b) -> bool
    { return a->getOffsetInBits() < b->getOffsetInBits(); });

    for (const DIDerivedType *mt : fields)
    {
        assert((mt->getTag() == dwarf::DW_TAG_member || mt->getTag() == dwarf::DW_TAG_inheritance)
               && "DCHG: expected member/inheritance");
        // Either we have a class, struct, array, or something not in need of flattening.
        const DIType *fieldType = mt->getBaseType();
        if (fieldType->getTag() == dwarf::DW_TAG_structure_type
                || fieldType->getTag() == dwarf::DW_TAG_class_type)
        {
            flatten(SVFUtil::dyn_cast<DICompositeType>(fieldType));
            for (const DIType *ft : fieldTypes[fieldType])
            {
                // ft is already a canonical type because the "root" additions insert
                // canonical types.
                fieldTypes[type].push_back(ft);
            }
        }
        else if (fieldType->getTag() == dwarf::DW_TAG_array_type)
        {
            const DICompositeType *arrayType = SVFUtil::dyn_cast<DICompositeType>(fieldType);
            const DIType *baseType = arrayType->getBaseType();
            if (const DICompositeType *cbt = SVFUtil::dyn_cast<DICompositeType>(baseType))
            {
                flatten(cbt);
                for (const DIType *ft : fieldTypes[cbt])
                {
                    // ft is already a canonical type like above.
                    fieldTypes[type].push_back(ft);
                }
            }
            else
            {
                fieldTypes[type].push_back(getCanonicalType(baseType));
            }
        }
        else
        {
            fieldTypes[type].push_back(getCanonicalType(fieldType));
        }
    }
}

bool DCHGraph::isAgg(const DIType *t)
{
    if (t == nullptr) return false;
    return    t->getTag() == dwarf::DW_TAG_array_type
              || t->getTag() == dwarf::DW_TAG_structure_type
              || t->getTag() == dwarf::DW_TAG_class_type;
}

void DCHGraph::gatherAggs(const DICompositeType *type)
{
    if (containingAggs.find(getCanonicalType(type)) != containingAggs.end())
    {
        return;
    }

    // Initialise an empty set. We want all aggregates to have a value in
    // this map, even if empty (e.g. struct has no aggs, only scalars).
    containingAggs[getCanonicalType(type)];

    if (type->getTag() == dwarf::DW_TAG_array_type)
    {
        const DIType *bt = type->getBaseType();
        bt = stripQualifiers(bt);

        if (isAgg(bt))
        {
            const DICompositeType *cbt = SVFUtil::dyn_cast<DICompositeType>(bt);
            containingAggs[getCanonicalType(type)].insert(getCanonicalType(cbt));
            gatherAggs(cbt);
            // These must be canonical already because of aggs.insert above/below.
            containingAggs[getCanonicalType(type)].insert(
                containingAggs[getCanonicalType(cbt)].begin(),
                containingAggs[getCanonicalType(cbt)].end());
        }
    }
    else
    {
        DINodeArray fields = type->getElements();
        for (unsigned i = 0; i < fields.size(); ++i)
        {
            // Unwrap the member (could be a subprogram, not type, so guard needed).
            if (const DIDerivedType *mt = SVFUtil::dyn_cast<DIDerivedType>(fields[i]))
            {
                const DIType *ft = mt->getBaseType();
                ft = stripQualifiers(ft);

                if (isAgg(ft))
                {
                    const DICompositeType *cft = SVFUtil::dyn_cast<DICompositeType>(ft);
                    containingAggs[getCanonicalType(type)].insert(getCanonicalType(cft));
                    gatherAggs(cft);
                    // These must be canonical already because of aggs.insert above.
                    containingAggs[getCanonicalType(type)].insert(
                        containingAggs[getCanonicalType(cft)].begin(),
                        containingAggs[getCanonicalType(cft)].end());
                }
            }
        }
    }
}

DCHNode *DCHGraph::getOrCreateNode(const DIType *type)
{
    type = getCanonicalType(type);

    // Check, does the node for type exist?
    if (diTypeToNodeMap[type] != nullptr)
    {
        return diTypeToNodeMap[type];
    }

    DCHNode *node = new DCHNode(type, numTypes++);
    addGNode(node->getId(), node);
    diTypeToNodeMap[type] = node;
    // TODO: handle templates.

    return node;
}

DCHEdge *DCHGraph::addEdge(const DIType *t1, const DIType *t2, DCHEdge::GEdgeKind et)
{
    DCHNode *src = getOrCreateNode(t1);
    DCHNode *dst = getOrCreateNode(t2);

    DCHEdge *edge = hasEdge(t1, t2, et);
    if (edge == nullptr)
    {
        // Create a new edge.
        edge = new DCHEdge(src, dst, et);
        src->addOutgoingEdge(edge);
        dst->addIncomingEdge(edge);
    }

    return edge;
}

DCHEdge *DCHGraph::hasEdge(const DIType *t1, const DIType *t2, DCHEdge::GEdgeKind et)
{
    DCHNode *src = getOrCreateNode(t1);
    DCHNode *dst = getOrCreateNode(t2);

    for (DCHEdge *edge : src->getOutEdges())
    {
        DCHNode *node = edge->getDstNode();
        DCHEdge::GEdgeKind edgeType = edge->getEdgeKind();
        if (node == dst && edgeType == et)
        {
            assert(SVFUtil::isa<DCHEdge>(edge) && "Non-DCHEdge in DCHNode edge set.");
            return edge;
        }
    }

    return nullptr;
}

void DCHGraph::buildCHG(bool extend)
{
    extended = extend;
    DebugInfoFinder finder;
    for (u32_t i = 0; i < LLVMModuleSet::getLLVMModuleSet()->getModuleNum(); ++i)
    {
        finder.processModule(*(LLVMModuleSet::getLLVMModuleSet()->getModule(i)));
    }

    // Create the void node regardless of whether it appears.
    getOrCreateNode(nullptr);
    // Find any char type.
    const DIType *charType = nullptr;
    /*
     * We want void at the top, char as a child, and everything is a child of char:
     *     void
     *      |
     *     char
     *    / | \
     *   x  y  z
     */


    for (const DIType *type : finder.types())
    {
        if (const DIBasicType *basicType = SVFUtil::dyn_cast<DIBasicType>(type))
        {
            if (basicType->getEncoding() == dwarf::DW_ATE_unsigned_char
                    || basicType->getEncoding() == dwarf::DW_ATE_signed_char)
            {
                charType = type;
            }

            handleDIBasicType(basicType);
        }
        else if (const DICompositeType *compositeType = SVFUtil::dyn_cast<DICompositeType>(type))
        {
            handleDICompositeType(compositeType);
        }
        else if (const DIDerivedType *derivedType = SVFUtil::dyn_cast<DIDerivedType>(type))
        {
            handleDIDerivedType(derivedType);
        }
        else if (const DISubroutineType *subroutineType = SVFUtil::dyn_cast<DISubroutineType>(type))
        {
            handleDISubroutineType(subroutineType);
        }
        else
        {
            assert(false && "DCHGraph::buildCHG: unexpected DIType.");
        }
    }

    for (u32_t i = 0; i < LLVMModuleSet::getLLVMModuleSet()->getModuleNum(); ++i)
    {
        buildVTables(*(LLVMModuleSet::getLLVMModuleSet()->getModule(i)));
    }

    // Build the void/char/everything else relation.
    if (extended && charType != nullptr)
    {
        // void <-- char
        addEdge(charType, nullptr, DCHEdge::STD_DEF);
        // char <-- x, char <-- y, ...
        for (iterator nodeI = begin(); nodeI != end(); ++nodeI)
        {
            // Everything without a parent gets char as a parent.
            if (nodeI->second->getType() != nullptr
                    && nodeI->second->getOutEdges().size() == 0)
            {
                addEdge(nodeI->second->getType(), charType, DCHEdge::STD_DEF);
            }
        }
    }

    if (Options::PrintDCHG)
    {
        print();
    }
}

const VFunSet &DCHGraph::getCSVFsBasedonCHA(CallSite cs)
{
    if (csCHAMap.find(cs) != csCHAMap.end())
    {
        return csCHAMap[cs];
    }

    VFunSet vfns;
    const VTableSet &vtbls = getCSVtblsBasedonCHA(cs);
    getVFnsFromVtbls(cs, vtbls, vfns);

    // Cache.
    csCHAMap.insert({cs, vfns});
    // Return cached object, not the stack object.
    return csCHAMap[cs];
}

const VTableSet &DCHGraph::getCSVtblsBasedonCHA(CallSite cs)
{
    const DIType *type = getCanonicalType(getCSStaticType(cs));
    // Check if we've already computed.
    if (vtblCHAMap.find(type) != vtblCHAMap.end())
    {
        return vtblCHAMap[type];
    }

    VTableSet vtblSet;
    const NodeBS &children = cha(type, false);
    for (NodeID childId : children)
    {
        DCHNode *child = getGNode(childId);
        const GlobalValue *vtbl = child->getVTable();
        // TODO: what if it is null?
        if (vtbl != nullptr)
        {
            vtblSet.insert(vtbl);
        }
    }

    // Cache.
    vtblCHAMap.insert({type, vtblSet});
    // Return cached version - not the stack object.
    return vtblCHAMap[type];
}

void DCHGraph::getVFnsFromVtbls(CallSite cs, const VTableSet &vtbls, VFunSet &virtualFunctions)
{
    size_t idx = cppUtil::getVCallIdx(cs);
    std::string funName = cppUtil::getFunNameOfVCallSite(cs);
    for (const GlobalValue *vtbl : vtbls)
    {
        assert(vtblToTypeMap.find(vtbl) != vtblToTypeMap.end() && "floating vtbl");
        const DIType *type = vtblToTypeMap[vtbl];
        assert(hasNode(type) && "trying to get vtbl for type not in graph");
        const DCHNode *node = getNode(type);
        std::vector<std::vector<const Function *>> allVfns = node->getVfnVectors();
        for (std::vector<const Function *> vfnV : allVfns)
        {
            // We only care about any virtual function corresponding to idx.
            if (idx >= vfnV.size())
            {
                continue;
            }

            const Function *callee = vfnV[idx];
            // Practically a copy of that in lib/MemoryModel/CHA.cpp
            if (cs.arg_size() == callee->arg_size() || (cs.getFunctionType()->isVarArg() && callee->isVarArg()))
            {
                cppUtil::DemangledName dname = cppUtil::demangle(callee->getName().str());
                std::string calleeName = dname.funcName;

                /*
                 * The compiler will add some special suffix (e.g.,
                 * "[abi:cxx11]") to the end of some virtual function:
                 * In dealII
                 * function: FE_Q<3>::get_name
                 * will be mangled as: _ZNK4FE_QILi3EE8get_nameB5cxx11Ev
                 * after demangling: FE_Q<3>::get_name[abi:cxx11]
                 * The special suffix ("[abi:cxx11]") needs to be removed
                 */
                const std::string suffix("[abi:cxx11]");
                size_t suffixPos = calleeName.rfind(suffix);
                if (suffixPos != std::string::npos)
                {
                    calleeName.erase(suffixPos, suffix.size());
                }

                /*
                 * if we can't get the function name of a virtual callsite, all virtual
                 * functions corresponding to idx will be valid
                 */
                if (funName.size() == 0)
                {
                    virtualFunctions.insert(SVFUtil::getFunction(callee->getName()));
                }
                else if (funName[0] == '~')
                {
                    /*
                     * if the virtual callsite is calling a destructor, then all
                     * destructors in the ch will be valid
                     * class A { virtual ~A(){} };
                     * class B: public A { virtual ~B(){} };
                     * int main() {
                     *   A *a = new B;
                     *   delete a;  /// the function name of this virtual callsite is ~A()
                     * }
                     */
                    if (calleeName[0] == '~')
                    {
                        virtualFunctions.insert(SVFUtil::getFunction(callee->getName()));
                    }
                }
                else
                {
                    /*
                     * For other virtual function calls, the function name of the callsite
                     * and the function name of the target callee should match exactly
                     */
                    if (funName.compare(calleeName) == 0)
                    {
                        virtualFunctions.insert(SVFUtil::getFunction(callee->getName()));
                    }
                }
            }
        }
    }
}

bool DCHGraph::isBase(const DIType *a, const DIType *b, bool firstField)
{
    a = getCanonicalType(a);
    b = getCanonicalType(b);
    assert(hasNode(a) && hasNode(b) && "DCHG: isBase query for non-existent node!");
    const DCHNode *bNode = getNode(b);

    const NodeBS &aChildren = cha(a, firstField);
    return aChildren.test(bNode->getId());
}

bool DCHGraph::isFieldOf(const DIType *f, const DIType *b)
{
    assert(f && b && "DCHG::isFieldOf: given nullptr!");

    f = getCanonicalType(f);
    b = getCanonicalType(b);
    if (f == b) return true;

    if (b->getTag() == dwarf::DW_TAG_array_type || b->getTag() == dwarf::DW_TAG_pointer_type)
    {
        const DIType *baseType = nullptr;
        if (const DICompositeType *arrayType = SVFUtil::dyn_cast<DICompositeType>(b))
        {
            baseType = arrayType->getBaseType();
        }
        else if (const DIDerivedType *ptrType = SVFUtil::dyn_cast<DIDerivedType>(b))
        {
            baseType = ptrType->getBaseType();
        }
        assert(baseType && "DCHG::isFieldOf: baseType is neither DIComposite nor DIDerived!");

        baseType = getCanonicalType(baseType);
        return f == baseType || (baseType != nullptr && isFieldOf(f, baseType));
    }
    else if (b->getTag() == dwarf::DW_TAG_class_type
             || b->getTag() == dwarf::DW_TAG_structure_type)
    {
        const std::vector<const DIType *> &fields = getFieldTypes(b);
        return std::find(fields.begin(), fields.end(), f) != fields.end();
    }
    else
    {
        return false;
    }
}

const DIType *DCHGraph::getCanonicalType(const DIType *t)
{
    // We want stripped types to be canonical.
    const DIType *unstrippedT = t;
    t = stripQualifiers(t);

    // Is there a mapping for the unstripped type? Yes - return it.
    if (canonicalTypeMap.find(unstrippedT) != canonicalTypeMap.end())
    {
        return canonicalTypeMap[unstrippedT];
    }

    // There is no mapping for unstripped type (^), is there one for the stripped
    // type? Yes - map the unstripped type to the same thing.
    if (unstrippedT != t)
    {
        if (canonicalTypeMap.find(t) != canonicalTypeMap.end())
        {
            canonicalTypeMap[unstrippedT] = canonicalTypeMap[t];
            return canonicalTypeMap[unstrippedT];
        }
    }

    // Canonical type for t is not cached, find one for it.
    for (const DIType *canonType : canonicalTypes)
    {
        if (teq(t, canonType))
        {
            // Found a canonical type.
            canonicalTypeMap[t] = canonType;
            return canonicalTypeMap[t];
        }
    }

    // No canonical type found, so t will be a canonical type.
    canonicalTypes.insert(t);
    canonicalTypeMap.insert({t, t});

    return canonicalTypeMap[t];
}

const DIType *DCHGraph::stripQualifiers(const DIType *t)
{
    while (true)
    {
        // nullptr means void.
        if (t == nullptr
                || SVFUtil::isa<DIBasicType>(t)
                || SVFUtil::isa<DISubroutineType>(t))
        {
            break;
        }

        unsigned tag = t->getTag();
        // Verbose for clarity.
        if (   tag == dwarf::DW_TAG_const_type
                || tag == dwarf::DW_TAG_atomic_type
                || tag == dwarf::DW_TAG_volatile_type
                || tag == dwarf::DW_TAG_restrict_type
                || tag == dwarf::DW_TAG_typedef)
        {
            // Qualifier - get underlying type.
            const DIDerivedType *dt = SVFUtil::dyn_cast<DIDerivedType>(t);
            assert(t && "DCHG: expected DerivedType");
            t = dt->getBaseType();
        }
        else if (   tag == dwarf::DW_TAG_array_type
                    || tag == dwarf::DW_TAG_class_type
                    || tag == dwarf::DW_TAG_structure_type
                    || tag == dwarf::DW_TAG_union_type
                    || tag == dwarf::DW_TAG_enumeration_type
                    || tag == dwarf::DW_TAG_member
                    || tag == dwarf::DW_TAG_pointer_type
                    || tag == dwarf::DW_TAG_ptr_to_member_type
                    || tag == dwarf::DW_TAG_reference_type
                    || tag == dwarf::DW_TAG_rvalue_reference_type)
        {
            // Hit a non-qualifier.
            break;
        }
        else if (   tag == dwarf::DW_TAG_inheritance
                    || tag == dwarf::DW_TAG_friend)
        {
            assert(false && "DCHG: unexpected tag when stripping qualifiers");
        }
        else
        {
            assert(false && "DCHG: unhandled tag when stripping qualifiers");
        }
    }

    return t;
}

const DIType *DCHGraph::stripArray(const DIType *t)
{
    t = stripQualifiers(t);
    if (t->getTag() == dwarf::DW_TAG_array_type)
    {
        const DICompositeType *at = SVFUtil::dyn_cast<DICompositeType>(t);
        return stripArray(at->getBaseType());
    }

    return t;
}

bool DCHGraph::teq(const DIType *t1, const DIType *t2)
{
    t1 = stripQualifiers(t1);
    t2 = stripQualifiers(t2);

    if (t1 == t2)
    {
        // Trivial case. Handles SubRoutineTypes too.
        return true;
    }

    if (t1 == nullptr || t2 == nullptr)
    {
        // Since t1 != t2 and one of them is null, it is
        // impossible for them to be equal.
        return false;
    }

    // Check if we need base type comparisons.
    if (SVFUtil::isa<DIBasicType>(t1) && SVFUtil::isa<DIBasicType>(t2))
    {
        const DIBasicType *b1 = SVFUtil::dyn_cast<DIBasicType>(t1);
        const DIBasicType *b2 = SVFUtil::dyn_cast<DIBasicType>(t2);

        unsigned enc1 = b1->getEncoding();
        unsigned enc2 = b2->getEncoding();
        bool okayEnc = ((enc1 == dwarf::DW_ATE_signed || enc1 == dwarf::DW_ATE_unsigned || enc1 == dwarf::DW_ATE_boolean)
                        && (enc2 == dwarf::DW_ATE_signed || enc2 == dwarf::DW_ATE_unsigned || enc2 == dwarf::DW_ATE_boolean))
                       ||
                       (enc1 == dwarf::DW_ATE_float && enc2 == dwarf::DW_ATE_float)
                       ||
                       ((enc1 == dwarf::DW_ATE_signed_char || enc1 == dwarf::DW_ATE_unsigned_char)
                        &&
                        (enc2 == dwarf::DW_ATE_signed_char || enc2 == dwarf::DW_ATE_unsigned_char));

        if (!okayEnc) return false;
        // Now we have split integers, floats, and chars, ignoring signedness.

        return t1->getSizeInBits() == t2->getSizeInBits()
               && t1->getAlignInBits() == t2->getAlignInBits();
    }

    // Check, do we need to compare base types?
    // This makes pointers, references, and arrays equivalent.
    // Will handle member types.
    if ((SVFUtil::isa<DIDerivedType>(t1) || t1->getTag() == dwarf::DW_TAG_array_type)
            && (SVFUtil::isa<DIDerivedType>(t2) || t2->getTag() == dwarf::DW_TAG_array_type))
    {
        const DIType *base1, *base2;

        // Set base1.
        if (const DIDerivedType *d1 = SVFUtil::dyn_cast<DIDerivedType>(t1))
        {
            base1 = d1->getBaseType();
        }
        else
        {
            const DICompositeType *c1 = SVFUtil::dyn_cast<DICompositeType>(t1);
            assert(c1 && "teq: bad cast for array type");
            base1 = c1->getBaseType();
        }

        // Set base2.
        if (const DIDerivedType *d2 = SVFUtil::dyn_cast<DIDerivedType>(t2))
        {
            base2 = d2->getBaseType();
        }
        else
        {
            const DICompositeType *c2 = SVFUtil::dyn_cast<DICompositeType>(t2);
            assert(c2 && "teq: bad cast for array type");
            base2 = c2->getBaseType();
        }

        // For ptr-to-member, there is some imprecision (but soundness) in
        // that we don't check the class type.
        return teq(base1, base2);
    }

    if (SVFUtil::isa<DICompositeType>(t1) && SVFUtil::isa<DICompositeType>(t2))
    {
        const DICompositeType *ct1 = SVFUtil::dyn_cast<DICompositeType>(t1);
        const DICompositeType *ct2 = SVFUtil::dyn_cast<DICompositeType>(t2);

        if (ct1->getTag() != ct2->getTag()) return false;

        // Treat all enums the same.
        if (ct1->getTag() == dwarf::DW_TAG_enumeration_type)
        {
            return true;
        }

        // C++ classes? Check mangled name.
        if (ct1->getTag() == dwarf::DW_TAG_class_type)
        {
            return ct1->getIdentifier() == ct2->getIdentifier();
        }

        // Either union or struct, simply test all fields are equal.
        // Seems like it is enough to check it was defined in the same place.
        // The elements sometimes differ but are referring to the same fields.
        return    ct1->getName() == ct2->getName()
                  && ct1->getFile() == ct2->getFile()
                  && ct1->getLine() == ct2->getLine();
    }

    // They were not equal base types (discounting signedness), nor were they
    // "equal" pointers/references/arrays, nor were they the structurally equivalent,
    // nor were they completely equal.
    return false;
}

bool DCHGraph::isFirstField(const DIType *f, const DIType *b)
{
    // TODO: some improvements.
    //   - cha should be changed to accept which edge types to use,
    //     then we can call cha(..., DCHEdge::FIRST_FIELD).
    //   - If not ^, this could benefit from caching.
    f = getCanonicalType(f);
    b = getCanonicalType(b);

    if (f == b) return true;

    const DCHNode *node = getNode(f);
    assert(node && "DCHG::isFirstField: node not found");
    // Consider oneself a child, otherwise the recursion will just come up with nothing.
    for (const DCHEdge *edge : node->getInEdges())
    {
        // Only care about first-field edges.
        if (edge->getEdgeKind() == DCHEdge::FIRST_FIELD)
        {
            if (edge->getSrcNode()->getType() == b) return true;
            if (isFirstField(edge->getSrcNode()->getType(), b)) return true;
        }
    }

    return false;
}

std::string DCHGraph::diTypeToStr(const DIType *t)
{
    std::stringstream ss;

    if (t == nullptr)
    {
        return "void";
    }

    if (const DIBasicType *bt = SVFUtil::dyn_cast<DIBasicType>(t))
    {
        ss << std::string(bt->getName());
    }
    else if (const DIDerivedType *dt = SVFUtil::dyn_cast<DIDerivedType>(t))
    {
        if (dt->getName() == "__vtbl_ptr_type")
        {
            ss << "(vtbl * =) __vtbl_ptr_type";
        }
        else if (dt->getTag() == dwarf::DW_TAG_const_type)
        {
            ss << "const " << diTypeToStr(dt->getBaseType());
        }
        else if (dt->getTag() == dwarf::DW_TAG_volatile_type)
        {
            ss << "volatile " << diTypeToStr(dt->getBaseType());
        }
        else if (dt->getTag() == dwarf::DW_TAG_restrict_type)
        {
            ss << "restrict " << diTypeToStr(dt->getBaseType());
        }
        else if (dt->getTag() == dwarf::DW_TAG_atomic_type)
        {
            ss << "atomic " << diTypeToStr(dt->getBaseType());
        }
        else if (dt->getTag() == dwarf::DW_TAG_pointer_type)
        {
            ss << diTypeToStr(dt->getBaseType()) << " *";
        }
        else if (dt->getTag() == dwarf::DW_TAG_ptr_to_member_type)
        {
            ss << diTypeToStr(dt->getBaseType())
               << " " << diTypeToStr(SVFUtil::dyn_cast<DIType>(dt->getExtraData())) << "::*";
        }
        else if (dt->getTag() == dwarf::DW_TAG_reference_type)
        {
            ss << diTypeToStr(dt->getBaseType()) << " &";
        }
        else if (dt->getTag() == dwarf::DW_TAG_rvalue_reference_type)
        {
            ss << diTypeToStr(dt->getBaseType()) << " &&";
        }
        else if (dt->getTag() == dwarf::DW_TAG_typedef)
        {
            ss << std::string(dt->getName()) << "->" << diTypeToStr(dt->getBaseType());
        }
    }
    else if (const DICompositeType *ct = SVFUtil::dyn_cast<DICompositeType>(t))
    {
        if (ct->getTag() == dwarf::DW_TAG_class_type
                || ct->getTag() == dwarf::DW_TAG_structure_type
                || ct->getTag() == dwarf::DW_TAG_union_type)
        {

            if (ct->getTag() == dwarf::DW_TAG_class_type)
            {
                ss << "class";
            }
            else if (ct->getTag() == dwarf::DW_TAG_structure_type)
            {
                ss << "struct";
            }
            else if (ct->getTag() == dwarf::DW_TAG_union_type)
            {
                ss << "union";
            }

            ss << ".";

            if (ct->getName() != "")
            {
                ss << std::string(ct->getName());
            }
            else
            {
                // Iterate over the element types.
                ss << "{ ";

                DINodeArray fields = ct->getElements();
                for (unsigned i = 0; i < fields.size(); ++i)
                {
                    // fields[i] gives a type which is DW_TAG_member, we want the member's type (getBaseType).
                    // It can also give a Subprogram type if the class just had non-virtual functions.
                    if (const DISubprogram *sp = SVFUtil::dyn_cast<DISubprogram>(fields[i]))
                    {
                        ss << std::string(sp->getName());
                    }
                    else if (const DIDerivedType *mt = SVFUtil::dyn_cast<DIDerivedType>(fields[i]))
                    {
                        assert(mt->getTag() == dwarf::DW_TAG_member && "DCHG: expected member");
                        ss << diTypeToStr(mt->getBaseType());
                    }

                    if (i != fields.size() - 1)
                    {
                        ss << ", ";
                    }
                }

                ss << " }";
            }
        }
        else if (ct->getTag() == dwarf::DW_TAG_array_type)
        {
            ss << diTypeToStr(ct->getBaseType());
            DINodeArray sizes = ct->getElements();
            for (unsigned i = 0; i < sizes.size(); ++i)
            {
                DISubrange *sr = SVFUtil::dyn_cast<DISubrange>(sizes[0]);
                assert(sr != nullptr && "DCHG: non-subrange as array element?");
                int64_t count = -1;
                if (const ConstantInt *ci = sr->getCount().dyn_cast<ConstantInt *>())
                {
                    count = ci->getSExtValue();
                }

                ss << "[" << count << "]";
            }
        }
        else if (ct->getTag() == dwarf::DW_TAG_enumeration_type)
        {
            ss << "enum " << diTypeToStr(ct->getBaseType());
        }
        else if (ct->getTag() == dwarf::DW_TAG_union_type)
        {

        }
    }
    else if (const DISubroutineType *st = SVFUtil::dyn_cast<DISubroutineType>(t))
    {
        DITypeRefArray types = st->getTypeArray();
        // Must have one element at least (the first type).
        ss << diTypeToStr(types[0]) << " fn(";
        if (types.size() == 1)
        {
            ss << "void)";
        }
        else
        {
            for (unsigned i = 1; i < types.size(); ++i)
            {
                ss << diTypeToStr(types[i]);
                if (i + 1 != types.size())
                {
                    // There's another type.
                    ss << ", ";
                }
            }

            ss << ")";
        }

        ss << std::string(st->getName());
    }

    return ss.str();
}

static std::string indent(size_t n)
{
    return std::string(n, ' ');
}

void DCHGraph::dump(const std::string& filename)
{
    GraphPrinter::SelectiveWriteGraphToFile(SVFUtil::outs(), filename, this);
}

void DCHGraph::print(void)
{
    static const std::string line = "-------------------------------------\n";
    static const std::string thickLine = "=====================================\n";
    static const size_t singleIndent = 2;

    size_t currIndent = 0;
    SVFUtil::outs() << thickLine;
    unsigned numStructs = 0;
    unsigned largestStruct = 0;
    NodeSet nodes;
    for (DCHGraph::const_iterator it = begin(); it != end(); ++it)
    {
        nodes.insert(it->first);
    }

    for (NodeID id : nodes)
    {
        if (*nodes.begin() != id)
        {
            SVFUtil::outs() << line;
        }

        const DCHNode *node = getGNode(id);

        SVFUtil::outs() << indent(currIndent) << id << ": " << diTypeToStr(node->getType()) << " [" << node->getType() << "]" << "\n";
        if (node->getType() != nullptr
                && (node->getType()->getTag() == dwarf::DW_TAG_class_type
                    || node->getType()->getTag() == dwarf::DW_TAG_structure_type))
        {
            ++numStructs;
            unsigned numFields = getFieldTypes(node->getType()).size();
            largestStruct = numFields > largestStruct ? numFields : largestStruct;
        }

        currIndent += singleIndent;
        SVFUtil::outs() << indent(currIndent) << "Virtual functions\n";
        currIndent += singleIndent;
        const std::vector<std::vector<const Function *>> &vfnVectors = node->getVfnVectors();
        for (unsigned i = 0; i < vfnVectors.size(); ++i)
        {
            SVFUtil::outs() << indent(currIndent) << "[ vtable #" << i << " ]\n";
            currIndent += singleIndent;
            for (unsigned j = 0; j < vfnVectors[i].size(); ++j)
            {
                struct cppUtil::DemangledName dname = cppUtil::demangle(vfnVectors[i][j]->getName().str());
                SVFUtil::outs() << indent(currIndent) << "[" << j << "] "
                                << dname.className << "::" << dname.funcName << "\n";
            }

            currIndent -= singleIndent;
        }

        // Nothing was printed.
        if (vfnVectors.size() == 0)
        {
            SVFUtil::outs() << indent(currIndent) << "(none)\n";
        }

        currIndent -= singleIndent;

        SVFUtil::outs() << indent(currIndent) << "Bases\n";
        currIndent += singleIndent;
        for (const DCHEdge *edge : node->getOutEdges())
        {
            std::string arrow;
            if (edge->getEdgeKind() == DCHEdge::INHERITANCE)
            {
                arrow = "--inheritance-->";
            }
            else if (edge->getEdgeKind() == DCHEdge::FIRST_FIELD)
            {
                arrow = "--first-field-->";
            }
            else if (edge->getEdgeKind() == DCHEdge::INSTANCE)
            {
                arrow = "---instance---->";
            }
            else if (edge->getEdgeKind() == DCHEdge::STD_DEF)
            {
                arrow = "---standard---->";
            }
            else
            {
                arrow = "----unknown---->";
            }

            SVFUtil::outs() << indent(currIndent) << "[ " << diTypeToStr(node->getType()) << " ] "
                            << arrow << " [ " << diTypeToStr(edge->getDstNode()->getType()) << " ]\n";
        }

        if (node->getOutEdges().size() == 0)
        {
            SVFUtil::outs() << indent(currIndent) << "(none)\n";
        }

        currIndent -= singleIndent;

        SVFUtil::outs() << indent(currIndent) << "Typedefs\n";

        currIndent += singleIndent;

        const Set<const DIDerivedType *> &typedefs = node->getTypedefs();
        for (const DIDerivedType *tdef : typedefs)
        {
            std::string typedefName = "void";
            if (tdef != nullptr)
            {
                typedefName = tdef->getName();
            }

            SVFUtil::outs() << indent(currIndent) << typedefName << "\n";
        }

        if (typedefs.size() == 0)
        {
            SVFUtil::outs() << indent(currIndent) << "(none)\n";
        }

        currIndent -= singleIndent;

        currIndent -= singleIndent;
    }

    SVFUtil::outs() << thickLine;

    SVFUtil::outs() << "Other stats\n";
    SVFUtil::outs() << line;
    SVFUtil::outs() << "# Canonical types : " << canonicalTypes.size() << "\n";
    SVFUtil::outs() << "# structs         : " << numStructs << "\n";
    SVFUtil::outs() << "Largest struct    : " << largestStruct << " fields\n";
    SVFUtil::outs() << thickLine;

    SVFUtil::outs().flush();
}
