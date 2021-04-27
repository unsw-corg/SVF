//===- BasicTypes.h -- Basic types used in SVF-------------------------------//
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
 * BasicTypes.h
 *
 *  Created on: Apr 1, 2014
 *      Author: Yulei Sui
 */

#ifndef BASICTYPES_H_
#define BASICTYPES_H_

#include "Util/SVFBasicTypes.h"
#include "SVF-FE/GEPTypeBridgeIterator.h"
#include <llvm/Support/ToolOutputFile.h>
#include "Util/Casting.h"
#include <llvm/ADT/SmallVector.h>		// for small vector
#include <llvm/ADT/SparseBitVector.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/CallSite.h>
#include <llvm/IR/InstVisitor.h>	// for instruction visitor
#include <llvm/IR/InstIterator.h>	// for inst iteration
#include <llvm/IR/GetElementPtrTypeIterator.h>	//for gep iterator
#include <llvm/Analysis/ScalarEvolution.h>
#include <llvm/ADT/StringExtras.h>	// for utostr_32
#include <llvm/Analysis/AliasAnalysis.h>
#include <llvm/Analysis/CallGraph.h>	// call graph
#include <llvm/IR/GlobalVariable.h>	// for GlobalVariable

#include <llvm/Support/SourceMgr.h> // for SMDiagnostic
#include <llvm/Bitcode/BitcodeWriter.h>		// for WriteBitcodeToFile
#include <llvm/Bitcode/BitcodeReader.h>     /// for isBitcode
#include <llvm/IRReader/IRReader.h>	// IR reader for bit file
#include <llvm/Transforms/Utils/UnifyFunctionExitNodes.h>
#include <llvm/Analysis/DominanceFrontier.h>
#include <llvm/Analysis/PostDominators.h>
#include <llvm/Analysis/ScalarEvolutionExpressions.h>
#include <llvm/ADT/GraphTraits.h>		// for Graphtraits
#include <llvm/Support/GraphWriter.h>		// for graph write
#include <llvm/IR/IRBuilder.h>		// for instrument svf.main
#include <llvm/Transforms/Utils/Local.h>	// for FindDbgAddrUses
#include <llvm/IR/DebugInfo.h>

namespace SVF
{

class BddCond;


/// LLVM Basic classes
typedef llvm::SMDiagnostic SMDiagnostic;
typedef llvm::LLVMContext LLVMContext;
typedef llvm::Type Type;
typedef llvm::Function Function;
typedef llvm::BasicBlock BasicBlock;
typedef llvm::Value Value;
typedef llvm::Instruction Instruction;
typedef llvm::CallSite CallSite;
typedef llvm::GlobalObject GlobalObject;
typedef llvm::GlobalValue GlobalValue;
typedef llvm::GlobalVariable GlobalVariable;
typedef llvm::Module Module;
typedef llvm::CallGraph LLVMCallGraph;
typedef llvm::User User;
typedef llvm::Use Use;
typedef llvm::Loop Loop;
typedef llvm::LoopInfo LoopInfo;
typedef llvm::UnifyFunctionExitNodes UnifyFunctionExitNodes;
typedef llvm::ModulePass ModulePass;
typedef llvm::AnalysisUsage AnalysisUsage;

/// LLVM outputs
typedef llvm::raw_ostream raw_ostream;
typedef llvm::raw_string_ostream raw_string_ostream;
typedef llvm::raw_fd_ostream raw_fd_ostream;
typedef llvm::StringRef StringRef;
typedef llvm::ToolOutputFile ToolOutputFile;

/// LLVM types
typedef llvm::StructType StructType;
typedef llvm::ArrayType ArrayType;
typedef llvm::PointerType PointerType;
typedef llvm::FunctionType FunctionType;
typedef llvm::VectorType VectorType;
typedef llvm::MetadataAsValue MetadataAsValue;
typedef llvm::BlockAddress BlockAddress;

/// LLVM data layout
typedef llvm::DataLayout DataLayout;
typedef llvm::StructLayout StructLayout;
typedef llvm::SmallVector<BasicBlock*, 8> SmallBBVector;
typedef llvm::ConstantStruct ConstantStruct;
typedef llvm::MemoryLocation MemoryLocation;

/// LLVM Aliases and constants
typedef llvm::Argument Argument;
typedef llvm::Constant Constant;
typedef llvm::ConstantData ConstantData;
typedef llvm::ConstantExpr ConstantExpr;
typedef llvm::ConstantAggregate ConstantAggregate;
typedef llvm::ConstantPointerNull ConstantPointerNull;
typedef llvm::ConstantArray ConstantArray;
typedef llvm::GlobalAlias GlobalAlias;
typedef llvm::AliasResult AliasResult;
typedef llvm::ModRefInfo ModRefInfo;
typedef llvm::AnalysisID AnalysisID;
typedef llvm::ConstantDataArray ConstantDataArray;

/// LLVM metadata
typedef llvm::NamedMDNode NamedMDNode;
typedef llvm::MDString MDString;
typedef llvm::MDNode MDNode;


/// LLVM Instructions
typedef llvm::AllocaInst AllocaInst;
typedef llvm::CallInst CallInst;
typedef llvm::InvokeInst InvokeInst;
typedef llvm::CallBrInst CallBrInst;
typedef llvm::StoreInst StoreInst;
typedef llvm::LoadInst LoadInst;
typedef llvm::PHINode PHINode;
typedef llvm::GetElementPtrInst GetElementPtrInst;
typedef llvm::CastInst CastInst;
typedef llvm::BitCastInst BitCastInst;
typedef llvm::ReturnInst ReturnInst;
typedef llvm::ConstantInt ConstantInt;
typedef llvm::SelectInst SelectInst;
typedef llvm::IntToPtrInst IntToPtrInst;
typedef llvm::CmpInst CmpInst;
typedef llvm::BranchInst BranchInst;
typedef llvm::SwitchInst SwitchInst;
typedef llvm::ExtractValueInst  ExtractValueInst;
typedef llvm::InsertValueInst InsertValueInst;
typedef llvm::BinaryOperator BinaryOperator;
typedef llvm::UnaryOperator UnaryOperator;
typedef llvm::PtrToIntInst PtrToIntInst;
typedef llvm::VAArgInst VAArgInst;
typedef llvm::ExtractElementInst ExtractElementInst;
typedef llvm::InsertElementInst InsertElementInst;
typedef llvm::ShuffleVectorInst ShuffleVectorInst;
typedef llvm::LandingPadInst LandingPadInst;
typedef llvm::ResumeInst ResumeInst;
typedef llvm::UnreachableInst UnreachableInst;
typedef llvm::FenceInst FenceInst;
typedef llvm::AtomicCmpXchgInst AtomicCmpXchgInst;
typedef llvm::AtomicRMWInst AtomicRMWInst;
typedef llvm::UndefValue UndefValue;
#if (LLVM_VERSION_MAJOR >= 9)
typedef llvm::FunctionCallee FunctionCallee;
#endif

/// LLVM scalar evolution
typedef llvm::ScalarEvolutionWrapperPass ScalarEvolutionWrapperPass;
typedef llvm::ScalarEvolution ScalarEvolution;
typedef llvm::SCEVAddRecExpr SCEVAddRecExpr;
typedef llvm::SCEVConstant SCEVConstant;
typedef llvm::SCEV SCEV;

/// LLVM Dominators
typedef llvm::DominanceFrontier DominanceFrontier;
typedef llvm::DominatorTree DominatorTree;
typedef llvm::PostDominatorTree PostDominatorTree;
typedef llvm::DomTreeNode DomTreeNode;
typedef llvm::DominanceFrontierBase<BasicBlock, false> DominanceFrontierBase;
typedef llvm::PostDominatorTreeWrapperPass PostDominatorTreeWrapperPass;
typedef llvm::LoopInfoWrapperPass LoopInfoWrapperPass;

/// LLVM Iterators
typedef llvm::inst_iterator inst_iterator;
typedef llvm::succ_const_iterator succ_const_iterator;
typedef llvm::const_inst_iterator const_inst_iterator;
typedef llvm::const_pred_iterator const_pred_iterator;
typedef llvm::gep_type_iterator gep_type_iterator;
typedef llvm::bridge_gep_iterator bridge_gep_iterator;
typedef llvm::IRBuilder<> IRBuilder;
typedef llvm::IntegerType IntegerType;

/// LLVM debug information
typedef llvm::DebugInfoFinder DebugInfoFinder;
typedef llvm::DIType DIType;
typedef llvm::DIBasicType DIBasicType;
typedef llvm::DICompositeType DICompositeType;
typedef llvm::DIDerivedType DIDerivedType;
typedef llvm::DISubroutineType DISubroutineType;
typedef llvm::DISubprogram DISubprogram;
typedef llvm::DISubrange DISubrange;
typedef llvm::DINode DINode;
typedef llvm::DINodeArray DINodeArray;
typedef llvm::DITypeRefArray DITypeRefArray;
namespace dwarf = llvm::dwarf;

class SVFFunction : public SVFValue
{
private:
    bool isDecl;
    bool isIntri;
    Function* fun;
public:
    SVFFunction(const std::string& val): SVFValue(val,SVFValue::SVFFunc),
        isDecl(false), isIntri(false), fun(nullptr)
    {
    }

    SVFFunction(Function* f): SVFValue(f->getName(),SVFValue::SVFFunc),
        isDecl(f->isDeclaration()), isIntri(f->isIntrinsic()), fun(f)
    {
    }
    inline Function* getLLVMFun() const
    {
        assert(fun && "no LLVM Function found!");
        return fun;
    }

    inline bool isDeclaration() const
    {
        return isDecl;
    }

    inline bool isIntrinsic() const
    {
        return isIntri;
    }

    inline u32_t arg_size() const
    {
        return getLLVMFun()->arg_size();
    }

    inline bool isVarArg() const
    {
        return getLLVMFun()->isVarArg();
    }

    // Dump Control Flow Graph of llvm function, with instructions
    void viewCFG();

    // Dump Control Flow Graph of llvm function, without instructions
    void viewCFGOnly();

};

class SVFGlobal : public SVFValue
{

public:
    SVFGlobal(const std::string& val): SVFValue(val,SVFValue::SVFGlob)
    {
    }

};

class SVFBasicBlock : public SVFValue
{

public:
    SVFBasicBlock(const std::string& val): SVFValue(val,SVFValue::SVFBB)
    {
    }

};

class SVFInstruction : public SVFValue
{

public:
    SVFInstruction(const std::string& val): SVFValue(val,SVFValue::SVFInst)
    {
    }

};

template <typename F, typename S>
raw_ostream& operator<< (raw_ostream &o, const std::pair<F, S> &var)
{
    o << "<" << var.first << ", " << var.second << ">";
    return o;
}

} // End namespace SVF

/// Specialise hash for CallSites.
template <> struct std::hash<SVF::CallSite> {
    size_t operator()(const SVF::CallSite &cs) const {
        std::hash<SVF::Instruction *> h;
        return h(cs.getInstruction());
    }
};

/// Specialise hash for SparseBitVectors.
template <> struct std::hash<llvm::SparseBitVector<>>
{
    size_t operator()(const llvm::SparseBitVector<> &sbv) const {
        SVF::Hash<std::pair<std::pair<size_t, size_t>, size_t>> h;
        return h(std::make_pair(std::make_pair(sbv.count(), sbv.find_first()), sbv.find_last()));
    }
};

#endif /* BASICTYPES_H_ */
