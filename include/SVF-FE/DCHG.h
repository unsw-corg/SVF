//===----- DCHG.h -- CHG using DWARF debug info ---------------------------//
// This is based upon the original CHA.h (now CHG.h).

/*
 * DCHG.h
 *
 *  Created on: Aug 23, 2019
 *      Author: Mohamad Barbar
 */

// TODO: add a flag such that getCanonicalType returns its arg so
//       that the impl. does not "node collapsing" based on teq.

#ifndef DCHG_H_
#define DCHG_H_

#include "Graphs/GenericGraph.h"
#include "SVF-FE/CommonCHG.h"
#include "SVF-FE/CPPUtil.h"
#include "Util/SVFModule.h"
#include "Util/SVFUtil.h"
#include "Util/WorkList.h"

namespace SVF
{

class SVFModule;
class DCHNode;

class DCHEdge : public GenericEdge<DCHNode>
{
public:
    enum
    {
        INHERITANCE,  // inheritance relation
        INSTANCE,     // template-instance relation
        FIRST_FIELD,  // src -ff-> dst => dst is first field of src
        STD_DEF       // Edges defined by the standard like (int -std-> char)
        // We also make the char --> void edge a STD_DEF edge.
    };

    typedef GenericNode<DCHNode, DCHEdge>::GEdgeSetTy DCHEdgeSetTy;

    DCHEdge(DCHNode *src, DCHNode *dst, GEdgeFlag k = 0)
        : GenericEdge<DCHNode>(src, dst, k), offset(0)
    {
    }

    unsigned int getOffset(void) const
    {
        return offset;
    }

    void setOffset(unsigned int offset)
    {
        this->offset = offset;
    }

private:
    unsigned int offset;
};

class DCHNode : public GenericNode<DCHNode, DCHEdge>
{
public:
    typedef enum
    {
        PURE_ABSTRACT = 0x1,     // pure virtual abstract class
        MULTI_INHERITANCE = 0x2, // multi inheritance class
        TEMPLATE = 0x04,         // template class
        SCALAR = 0x08            // non-class scalar type
    } CLASSATTR;

    typedef std::vector<const Function*> FuncVector;

    DCHNode(const DIType *diType, NodeID i = 0, GNodeK k = 0)
        : GenericNode<DCHNode, DCHEdge>(i, k), vtable(nullptr), flags(0)
    {
        this->diType = diType;
        if (diType == nullptr)
        {
            typeName = "null-void";
        }
        else if (diType->getRawName() != nullptr)
        {
            typeName = diType->getName();
        }
        else
        {
            typeName = "unnamed!";
        }
    }

    ~DCHNode() { }

    const DIType *getType(void) const
    {
        return diType;
    }

    std::string getName() const
    {
        return typeName;
    }

    /// Flags
    //@{
    inline void setFlag(CLASSATTR mask)
    {
        flags |= mask;
    }
    inline bool hasFlag(CLASSATTR mask) const
    {
        return (flags & mask) == mask;
    }
    //@}

    /// Attribute
    //@{
    inline void setPureAbstract()
    {
        setFlag(PURE_ABSTRACT);
    }
    inline void setMultiInheritance()
    {
        setFlag(MULTI_INHERITANCE);
    }
    inline void setTemplate()
    {
        setFlag(TEMPLATE);
    }
    inline void setScalar()
    {
        setFlag(SCALAR);
    }
    inline bool isPureAbstract() const
    {
        return hasFlag(PURE_ABSTRACT);
    }
    inline bool isMultiInheritance() const
    {
        return hasFlag(MULTI_INHERITANCE);
    }
    inline bool isTemplate() const
    {
        return hasFlag(TEMPLATE);
    }
    inline bool isScalar() const
    {
        return hasFlag(SCALAR);
    }
    //@}

    void addTypedef(const DIDerivedType *diTypedef)
    {
        typedefs.insert(diTypedef);
    }

    const Set<const DIDerivedType *> &getTypedefs(void) const
    {
        return typedefs;
    }

    void setVTable(const GlobalValue *vtbl)
    {
        vtable = vtbl;
    }

    const GlobalValue *getVTable() const
    {
        return vtable;
    }

    /// Returns the vector of virtual function vectors.
    const std::vector<std::vector<const Function *>> &getVfnVectors(void) const
    {
        return vfnVectors;
    }

    /// Return the nth virtual function vector in the vtable.
    std::vector<const Function *> &getVfnVector(unsigned n)
    {
        if (vfnVectors.size() < n + 1)
        {
            vfnVectors.resize(n + 1);
        }

        return vfnVectors[n];
    }

private:
    /// Type of this node.
    const DIType *diType;
    /// Typedefs which map to this type.
    Set<const DIDerivedType *> typedefs;
    const GlobalValue* vtable;
    std::string typeName;
    size_t flags;
    /// The virtual functions which this class actually defines/overrides.
    std::vector<const Function *> primaryVTable;
    /// If a vtable is split into more than one vfn vector for multiple inheritance,
    /// 0 would be the primary base + this classes virtual functions, 1 would be
    /// the second parent, 2 would be third parent, etc.
    std::vector<std::vector<const Function*>> vfnVectors;
};

/// Dwarf based CHG.
class DCHGraph : public CommonCHGraph, public GenericGraph<DCHNode, DCHEdge>
{
public:
    /// Returns the DIType beneath the qualifiers. Does not strip away "DW_TAG_members".
    static const DIType *stripQualifiers(const DIType *);

    /// Returns the DIType beneath all qualifiers and arrays.
    static const DIType *stripArray(const DIType *);

    /// Returns true if t1 and t2 are equivalent, ignoring qualifiers.
    /// For equality...
    ///  Tags always need to be equal.
    ///   DIBasicType:      shallow pointer equality.
    ///   DIDerivedType:    base types (teq).
    ///   DICompositeType:  shallow pointer equality.
    ///   DISubroutineType: shallow pointer equality.
    static bool teq(const DIType *t1, const DIType *t2);

    /// Returns a human-readable version of the DIType.
    static std::string diTypeToStr(const DIType *);

    // Returns whether t is an array, a struct, a class, a union, or neither.
    static bool isAgg(const DIType *t);

public:
    DCHGraph(const SVFModule *svfMod)
        : svfModule(svfMod), numTypes(0)   // vfID(0), buildingCHGTime(0) {
    {
        this->kind = DI;
    }

    virtual ~DCHGraph() { };

    /// Builds the CHG from DWARF debug information. extend determines
    /// whether to extend the CHG with first field edges.
    virtual void buildCHG(bool extend);

    void dump(const std::string& filename);

    void print(void);

    virtual bool csHasVFnsBasedonCHA(CallSite cs) override
    {
        return csHasVtblsBasedonCHA(cs);
    }

    virtual const VFunSet &getCSVFsBasedonCHA(CallSite cs) override;

    virtual bool csHasVtblsBasedonCHA(CallSite cs) override
    {
        const DIType *type = getCanonicalType(getCSStaticType(cs));
        if (!hasNode(type))
        {
            return false;
        }

        return getNode(type)->getVTable() != nullptr;
    }

    virtual const VTableSet &getCSVtblsBasedonCHA(CallSite cs) override;
    virtual void getVFnsFromVtbls(CallSite cs, const VTableSet &vtbls, VFunSet &virtualFunctions) override;

    /// Returns true if a is a transitive base of b. firstField determines
    /// whether to consider first-field edges.
    virtual bool isBase(const DIType *a, const DIType *b, bool firstField);

    /// Returns true if f is a field of b (fields from getFieldTypes).
    virtual bool isFieldOf(const DIType *f, const DIType *b);

    static inline bool classof(const CommonCHGraph *chg)
    {
        return chg->getKind() == DI;
    }

    /// Returns the type representing all qualifier-variations of t.
    /// This should only matter in the case of DerivedTypes where
    /// qualifiers and have qualified base types cause a mess.
    const DIType *getCanonicalType(const DIType *t);

    /// Returns the type of field number idx (flattened) in base.
    const DIType *getFieldType(const DIType *base, unsigned idx)
    {
        base = getCanonicalType(base);
        if (base == nullptr)
        {
            // Conservative; the base object is untyped, sadly.
            return nullptr;
        }

        // For TBHC this is conservative because the union type is lower in the DCHG
        // than its fields. TODO: make more precise.
        if (base->getTag() == dwarf::DW_TAG_union_type)
        {
            return base;
        }

        if (base->getTag() == dwarf::DW_TAG_array_type)
        {
            const DICompositeType *cbase = SVFUtil::dyn_cast<DICompositeType>(base);
            assert(cbase && "DCHG: bad DIComposite case");
            return cbase->getBaseType();
        }

        if (!(base->getTag() == dwarf::DW_TAG_class_type
                || base->getTag() == dwarf::DW_TAG_structure_type))
        {
            return nullptr;
        }

        assert(fieldTypes.find(base) != fieldTypes.end() && "DCHG: base not flattened!");
        std::vector<const DIType *> &fields = fieldTypes[base];
        assert(fields.size() > idx && "DCHG: idx into struct larger than # fields!");
        return getCanonicalType(fields[idx]);
    }

    /// Returns a vector of the types of all fields in base.
    const std::vector<const DIType *> &getFieldTypes(const DIType *base)
    {
        base = getCanonicalType(base);
        assert(fieldTypes.find(base) != fieldTypes.end() && "DCHG: base not flattened!");
        return fieldTypes[base];
    }

    // Returns the number of fields in base (length of getFieldTypes).
    unsigned getNumFields(const DIType *base)
    {
        base = getCanonicalType(base);
        assert(fieldTypes.find(base) != fieldTypes.end() && "DCHG: base not flattened!");
        return fieldTypes[base].size();
    }

    /// Returns all the aggregates contained (transitively) in base.
    const Set<const DIType *> &getAggs(const DIType *base)
    {
        base = getCanonicalType(base);
        assert(containingAggs.find(base) != containingAggs.end() && "DCHG: aggregates not gathered for base!");
        return containingAggs[base];
    }

    bool isFirstField(const DIType *f, const DIType *b);

protected:
    /// SVF Module this CHG is built from.
    const SVFModule *svfModule;
    /// Whether this CHG is an extended CHG (first-field). Set by buildCHG.
    bool extended = false;
    /// Maps DITypes to their nodes.
    Map<const DIType *, DCHNode *> diTypeToNodeMap;
    /// Maps VTables to the DIType associated with them.
    Map<const GlobalValue *, const DIType *> vtblToTypeMap;
    /// Maps types to all children (i.e. CHA).
    Map<const DIType *, NodeBS> chaMap;
    /// Maps types to all children but also considering first field.
    Map<const DIType *, NodeBS> chaFFMap;
    /// Maps types to a set with their vtable and all their children's.
    Map<const DIType *, VTableSet> vtblCHAMap;
    /// Maps callsites to a set of potential virtual functions based on CHA.
    Map<CallSite, VFunSet> csCHAMap;
    /// Maps types to their canonical type (many-to-one).
    Map<const DIType *, const DIType *> canonicalTypeMap;
    /// Set of all possible canonical types (i.e. values of canonicalTypeMap).
    Set<const DIType *> canonicalTypes;
    /// Maps types to their flattened fields' types.
    Map<const DIType *, std::vector<const DIType *>> fieldTypes;
    /// Maps aggregate types to all the aggregate types it transitively contains.
    Map<const DIType *, Set<const DIType *>> containingAggs;

private:
    /// Construction helper to process DIBasicTypes.
    void handleDIBasicType(const DIBasicType *basicType);
    /// Construction helper to process DICompositeTypes.
    void handleDICompositeType(const DICompositeType *compositeType);
    /// Construction helper to process DIDerivedTypes.
    void handleDIDerivedType(const DIDerivedType *derivedType);
    /// Construction helper to process DISubroutineTypes.
    void handleDISubroutineType(const DISubroutineType *subroutineType);

    /// Finds all defined virtual functions and attaches them to nodes.
    void buildVTables(const Module &module);

    /// Returns a set of all children of type (CHA). Also gradually builds chaMap.
    const NodeBS &cha(const DIType *type, bool firstField);

    /// Attaches the typedef(s) to the base node.
    void handleTypedef(const DIType *typedefType);

    /// Populates fieldTypes for type and all its elements.
    void flatten(const DICompositeType *type);

    /// Populates containingAggs for type and all its elements.
    void gatherAggs(const DICompositeType *type);

    /// Creates a node from type, or returns it if it exists.
    DCHNode *getOrCreateNode(const DIType *type);

    /// Retrieves the metadata associated with a *virtual* callsite.
    const DIType *getCSStaticType(CallSite cs) const
    {
        MDNode *md = cs.getInstruction()->getMetadata(cppUtil::ctir::derefMDName);
        assert(md != nullptr && "Missing type metadata at virtual callsite");
        DIType *diType = SVFUtil::dyn_cast<DIType>(md);
        assert(diType != nullptr && "Incorrect metadata type at virtual callsite");
        return diType;
    }

    /// Checks if a node exists for type.
    bool hasNode(const DIType *type)
    {
        type = getCanonicalType(type);
        return diTypeToNodeMap.find(type) != diTypeToNodeMap.end();
    }

    /// Returns the node for type (nullptr if it doesn't exist).
    DCHNode *getNode(const DIType *type)
    {
        type = getCanonicalType(type);
        if (hasNode(type))
        {
            return diTypeToNodeMap.at(type);
        }

        return nullptr;
    }


    /// Creates an edge between from t1 to t2.
    DCHEdge *addEdge(const DIType *t1, const DIType *t2, DCHEdge::GEdgeKind et);
    /// Returns the edge between t1 and t2 if it exists, returns nullptr otherwise.
    DCHEdge *hasEdge(const DIType *t1, const DIType *t2, DCHEdge::GEdgeKind et);

    /// Number of types (nodes) in the graph.
    NodeID numTypes;
};

} // End namespace SVF

namespace llvm
{
/* !
 * GraphTraits specializations for generic graph algorithms.
 * Provide graph traits for traversing from a constraint node using standard graph traversals.
 */
template<> struct GraphTraits<SVF::DCHNode*> : public GraphTraits<SVF::GenericNode<SVF::DCHNode,SVF::DCHEdge>*  >
{
};

/// Inverse GraphTraits specializations for call graph node, it is used for inverse traversal.
template<>
struct GraphTraits<Inverse<SVF::DCHNode*> > : public GraphTraits<Inverse<SVF::GenericNode<SVF::DCHNode,SVF::DCHEdge>* > >
{
};

template<> struct GraphTraits<SVF::DCHGraph*> : public GraphTraits<SVF::GenericGraph<SVF::DCHNode,SVF::DCHEdge>* >
{
    typedef SVF::DCHNode *NodeRef;
};

} // End namespace llvm

#endif /* DCHG_H_ */
