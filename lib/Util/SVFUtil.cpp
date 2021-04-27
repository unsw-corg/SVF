//===- AnalysisUtil.cpp -- Helper functions for pointer analysis--------------//
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
 * AnalysisUtil.cpp
 *
 *  Created on: Apr 11, 2013
 *      Author: Yulei Sui
 */

#include "Util/Options.h"
#include "Util/SimpleOptions.h"
#include "Util/SVFUtil.h"
#include "SVF-FE/LLVMUtil.h"

#include "Util/Conditions.h"
#include <sys/resource.h>		/// increase stack size

using namespace SVF;

/// Color for output format
#define KNRM  "\x1B[1;0m"
#define KRED  "\x1B[1;31m"
#define KGRN  "\x1B[1;32m"
#define KYEL  "\x1B[1;33m"
#define KBLU  "\x1B[1;34m"
#define KPUR  "\x1B[1;35m"
#define KCYA  "\x1B[1;36m"
#define KWHT  "\x1B[1;37m"



/*!
 * print successful message by converting a string into green string output
 */
std::string SVFUtil::sucMsg(std::string msg)
{
    return KGRN + msg + KNRM;
}

/*!
 * print warning message by converting a string into yellow string output
 */
std::string SVFUtil::wrnMsg(std::string msg)
{
    return KYEL + msg + KNRM;
}

void SVFUtil::writeWrnMsg(std::string msg)
{
    if(Options::DisableWarn) return;
    outs() << wrnMsg(msg) << "\n";
}

/*!
 * print error message by converting a string into red string output
 */
std::string SVFUtil::errMsg(std::string msg)
{
    return KRED + msg + KNRM;
}

std::string SVFUtil::bugMsg1(std::string msg)
{
    return KYEL + msg + KNRM;
}

std::string SVFUtil::bugMsg2(std::string msg)
{
    return KPUR + msg + KNRM;
}

std::string SVFUtil::bugMsg3(std::string msg)
{
    return KCYA + msg + KNRM;
}

/*!
 * print each pass/phase message by converting a string into blue string output
 */
std::string SVFUtil::pasMsg(std::string msg)
{
    return KBLU + msg + KNRM;
}

/*!
 * Dump points-to set
 */
void SVFUtil::dumpPointsToSet(unsigned node, NodeBS bs)
{
    outs() << "node " << node << " points-to: {";
    dumpSet(bs);
    outs() << "}\n";
}

// For use from the debugger.
void SVFUtil::dumpSparseSet(const NodeBS& bs)
{
    outs() << "{";
    dumpSet(bs);
    outs() << "}\n";
}

void SVFUtil::dumpPointsToList(const PointsToList& ptl)
{
    outs() << "{";
    for (PointsToList::const_iterator ii = ptl.begin(), ie = ptl.end();
         ii != ie; ii++)
    {
        auto bs = *ii;
        dumpSparseSet(bs);
    }
    outs() << "}\n";
}

/*!
 * Dump alias set
 */
void SVFUtil::dumpAliasSet(unsigned node, NodeBS bs)
{
    outs() << "node " << node << " alias set: {";
    dumpSet(bs);
    outs() << "}\n";
}

/*!
 * Dump bit vector set
 */
void SVFUtil::dumpSet(NodeBS bs, raw_ostream & O)
{
    for (NodeBS::iterator ii = bs.begin(), ie = bs.end();
            ii != ie; ii++)
    {
        O << " " << *ii << " ";
    }
}

/*!
 * Print memory usage
 */
void SVFUtil::reportMemoryUsageKB(const std::string& infor, raw_ostream & O)
{
    u32_t vmrss, vmsize;
    if (getMemoryUsageKB(&vmrss, &vmsize))
        O << infor << "\tVmRSS: " << vmrss << "\tVmSize: " << vmsize << "\n";
}

/*!
 * Get memory usage
 */
bool SVFUtil::getMemoryUsageKB(u32_t* vmrss_kb, u32_t* vmsize_kb)
{
    /* Get the the current process' status file from the proc filesystem */
    char buffer[8192];
    FILE* procfile = fopen("/proc/self/status", "r");
    if(procfile)
    {
        u32_t result = fread(buffer, sizeof(char), 8192, procfile);
        if (result == 0)
        {
            fputs ("Reading error\n",stderr);
        }
    }
    else
    {
        fputs ("/proc/self/status file not exit\n",stderr);
        return false;
    }
    fclose(procfile);

    /* Look through proc status contents line by line */
    char delims[] = "\n";
    char* line = strtok(buffer, delims);

    bool found_vmrss = false;
    bool found_vmsize = false;

    while (line != nullptr && (found_vmrss == false || found_vmsize == false))
    {
        if (strstr(line, "VmRSS:") != nullptr)
        {
            sscanf(line, "%*s %u", vmrss_kb);
            found_vmrss = true;
        }

        if (strstr(line, "VmSize:") != nullptr)
        {
            sscanf(line, "%*s %u", vmsize_kb);
            found_vmsize = true;
        }

        line = strtok(nullptr, delims);
    }

    return (found_vmrss && found_vmsize);
}

/*!
 * Increase stack size
 */
void SVFUtil::increaseStackSize()
{
    const rlim_t kStackSize = 256L * 1024L * 1024L;   // min stack size = 256 Mb
    struct rlimit rl;
    int result = getrlimit(RLIMIT_STACK, &rl);
    if (result == 0)
    {
        if (rl.rlim_cur < kStackSize)
        {
            rl.rlim_cur = kStackSize;
            result = setrlimit(RLIMIT_STACK, &rl);
            if (result != 0)
            	writeWrnMsg("setrlimit returned result !=0 \n");
        }
    }
}


/*!
 * Get source code line number of a function according to debug info
 */
std::string SVFUtil::getSourceLocOfFunction(const Function *F)
{
    std::string str;
    raw_string_ostream rawstr(str);
    /*
     * https://reviews.llvm.org/D18074?id=50385
     * looks like the relevant
     */
    if (llvm::DISubprogram *SP =  F->getSubprogram())
    {
        if (SP->describes(F))
            rawstr << "in line: " << SP->getLine() << " file: " << SP->getFilename();
    }
    return rawstr.str();
}

/*!
 * Get the meta data (line number and file name) info of a LLVM value
 */
std::string SVFUtil::getSourceLoc(const Value* val)
{
    if(val==nullptr)  return "{ empty val }";

    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "{ ";

    if (const Instruction *inst = SVFUtil::dyn_cast<Instruction>(val))
    {
        if (SVFUtil::isa<AllocaInst>(inst))
        {
            for (llvm::DbgInfoIntrinsic *DII : FindDbgAddrUses(const_cast<Instruction*>(inst)))
            {
                if (llvm::DbgDeclareInst *DDI = SVFUtil::dyn_cast<llvm::DbgDeclareInst>(DII))
                {
                    llvm::DIVariable *DIVar = SVFUtil::cast<llvm::DIVariable>(DDI->getVariable());
                    rawstr << "ln: " << DIVar->getLine() << " fl: " << DIVar->getFilename();
                    break;
                }
            }
        }
        else if (MDNode *N = inst->getMetadata("dbg"))   // Here I is an LLVM instruction
        {
            llvm::DILocation* Loc = SVFUtil::cast<llvm::DILocation>(N);                   // DILocation is in DebugInfo.h
            unsigned Line = Loc->getLine();
            unsigned Column = Loc->getColumn();
            StringRef File = Loc->getFilename();
            //StringRef Dir = Loc.getDirectory();
            if(File.str().empty() || Line == 0) {
                auto inlineLoc = Loc->getInlinedAt();
                if(inlineLoc) {
                    Line = inlineLoc->getLine();
                    Column = inlineLoc->getColumn();
                    File = inlineLoc->getFilename();
                }   
            }
            rawstr << "ln: " << Line << "  cl: " << Column << "  fl: " << File;
        }
    }
    else if (const Argument* argument = SVFUtil::dyn_cast<Argument>(val))
    {
        if (argument->getArgNo()%10 == 1)
            rawstr << argument->getArgNo() << "st";
        else if (argument->getArgNo()%10 == 2)
            rawstr << argument->getArgNo() << "nd";
        else if (argument->getArgNo()%10 == 3)
            rawstr << argument->getArgNo() << "rd";
        else
            rawstr << argument->getArgNo() << "th";
        rawstr << " arg " << argument->getParent()->getName() << " "
               << getSourceLocOfFunction(argument->getParent());
    }
    else if (const GlobalVariable* gvar = SVFUtil::dyn_cast<GlobalVariable>(val))
    {
        rawstr << "Glob ";
        NamedMDNode* CU_Nodes = gvar->getParent()->getNamedMetadata("llvm.dbg.cu");
        if(CU_Nodes)
        {
            for (unsigned i = 0, e = CU_Nodes->getNumOperands(); i != e; ++i)
            {
                llvm::DICompileUnit *CUNode = SVFUtil::cast<llvm::DICompileUnit>(CU_Nodes->getOperand(i));
                for (llvm::DIGlobalVariableExpression *GV : CUNode->getGlobalVariables())
                {
                    llvm::DIGlobalVariable * DGV = GV->getVariable();

                    if(DGV->getName() == gvar->getName())
                    {
                        rawstr << "ln: " << DGV->getLine() << " fl: " << DGV->getFilename();
                    }

                }
            }
        }
    }
    else if (const Function* func = SVFUtil::dyn_cast<Function>(val))
    {
        rawstr << getSourceLocOfFunction(func);
    }
    else if (const BasicBlock* bb = SVFUtil::dyn_cast<BasicBlock>(val))
    {
        rawstr << "basic block: " << bb->getName() << " " << getSourceLoc(bb->getFirstNonPHI());
    }
    else if(SVFUtil::isConstantData(val))
    {
        rawstr << "constant data";
    }
    else
    {
        rawstr << "Can only get source location for instruction, argument, global var, function or constant data.";
    }
    rawstr << " }";

    return rawstr.str();
}


/*!
 * return string of an LLVM Value
 */
const std::string SVFUtil::value2String(const Value* value) {
    std::string str;
    raw_string_ostream rawstr(str);
    if(value){
        if(const SVF::Function* fun = SVFUtil::dyn_cast<Function>(value))
            rawstr << " " << fun->getName() << " ";
        else
            rawstr << " " << *value << " ";
        rawstr << getSourceLoc(value);
    }
    return rawstr.str();
}

void SVFFunction::viewCFG() {
    if (fun != nullptr) {
        fun->viewCFG();
    }
}

void SVFFunction::viewCFGOnly() {
    if (fun != nullptr) {
        fun->viewCFGOnly();
    }
}

/// Decide whether to hide a node in a dot graph.
/// Based on command line options, decide whether a node
/// should be hidden. This is used to implement dumping of
/// connected subgraphs by ignoring nodes not in the selected
/// connected subgraph. The maybe_hide parameter tells if this
/// node is of a sort that is not important and need not be
/// displayed ordinarily. However, if --dot-show-all is
/// specified then even these are shown.
bool GenericNodeBase::reallyHideNode(bool maybe_hide)
{
    assert(graph && "Node not in graph");
    SubgraphIdTy current_subgraph = graph->currentSubgraphId;
    if (SimpleOptions::DotLargestSubgraph || SimpleOptions::DotSeparateSubgraphs)
    {
        // In these cases, the graph's current_subgraph will have been
        // set to the desired subgraph, and nodes not in that
        // subgraph should be hidden.
        if (subgraphID != current_subgraph)
        {
            return true;
        }
    }
    if (SimpleOptions::DotShowAll)
    {
        // If this option, we want to see all of the nodes of the
        // graph (or the selected subgraph).
        return false;
    }
    // Otherwise, use the judgement of the caller whether to hide.
    return maybe_hide;
}
