//===- GraphPrinter.h -- Print Generic Graph---------------------//
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
 * GraphPrinter.h
 *
 *  Created on: 19.Sep.,2018
 *      Author: Yulei
 */

#ifndef INCLUDE_UTIL_GRAPHPRINTER_H_
#define INCLUDE_UTIL_GRAPHPRINTER_H_

#include <system_error>
#include <llvm/Support/ToolOutputFile.h>
#include <llvm/Support/FileSystem.h>		// for file open flag
#include <llvm/Support/GraphWriter.h>
#include <Util/SimpleOptions.h>
#include <Graphs/GenericGraph.h>
#include <string>

namespace llvm
{

/*
 * Dump and print the graph for debugging
 */
class GraphPrinter
{

public:
    GraphPrinter()
    {
    }


    /*!
     *  Write selected parts of the graph into dot file for debugging purpose
     */
    template<class GraphType>
    static void SelectiveWriteGraphToFile(llvm::raw_ostream &O, ///< Stream for progress reporting
                                          const std::string &GraphName, ///< Name will be used to form .dot file names.
                                          const GraphType &GT, ///< The graph to dump.
                                          bool simple = false, ///< Whether to dump short names or full.)
                                          bool view = false) ///< View now
    {
        auto* genGraph = dynamic_cast<SVF::GenericGraphBase*>(GT);
        if (SVF::SimpleOptions::DotLargestSubgraph ||
            SVF::SimpleOptions::DotSeparateSubgraphs)
        {
            GT->createConnectedSubgraphs();
        }
        if (SVF::SimpleOptions::DotSeparateSubgraphs)
        {
            SVF::SubgraphIdTy numSubgraphs = genGraph->subgraphNum;
            for (int subgraph_id = 1; subgraph_id <= numSubgraphs; ++subgraph_id) {
                auto size = genGraph->subgraphSizeMap[subgraph_id];
                std::string name = GraphName + "_size" + std::to_string(size) + "_id" + std::to_string(subgraph_id);
                genGraph->currentSubgraphId = subgraph_id;
                std::string filename = GraphPrinter::WriteGraphToFile(O, name, GT, simple);
                if (!filename.empty() && view)
                {
                    llvm::DisplayGraph(filename, false);
                }
            }
        }
        else
        {
            std::string filename = GraphPrinter::WriteGraphToFile(O, GraphName, GT, simple);
            if (!filename.empty() && view)
            {
                llvm::DisplayGraph(filename, false);
            }
        }
    }

    /*!
     *  Write the graph into dot file for debugging purpose
     */
    template<class GraphType>
    static std::string WriteGraphToFile(llvm::raw_ostream &O,
                                 const std::string &GraphName, const GraphType &GT, bool simple = false)
    {
        // Filename of the output dot file
        std::string Filename = GraphName + ".dot";
        O << "Writing '" << Filename << "'...";
        std::error_code ErrInfo;
        llvm::ToolOutputFile F(Filename.c_str(), ErrInfo, llvm::sys::fs::F_None);

        if (!ErrInfo)
        {
            // dump the ValueFlowGraph here
            WriteGraph(F.os(), GT, simple);
            F.os().close();
            if (!F.os().has_error())
            {
                O << "\n";
                F.keep();
                return Filename;
            }
        }
        O << "  error opening file for writing!\n";
        F.os().clear_error();
        return "";
    }

    /*!
     * Print the graph to command line
     */
    template<class GraphType>
    static void PrintGraph(llvm::raw_ostream &O, const std::string &GraphName,
                           const GraphType &GT)
    {
        ///Define the GTraits and node iterator for printing
        typedef llvm::GraphTraits<GraphType> GTraits;

        typedef typename GTraits::NodeRef NodeRef;
        typedef typename GTraits::nodes_iterator node_iterator;
        typedef typename GTraits::ChildIteratorType child_iterator;

        O << "Printing VFG Graph" << "'...\n";
        // Print each node name and its edges
        node_iterator I = GTraits::nodes_begin(GT);
        node_iterator E = GTraits::nodes_end(GT);
        for (; I != E; ++I)
        {
            NodeRef *Node = *I;
            O << "node :" << Node << "'\n";
            child_iterator EI = GTraits::child_begin(Node);
            child_iterator EE = GTraits::child_end(Node);
            for (unsigned i = 0; EI != EE && i != 64; ++EI, ++i)
            {
                O << "child :" << *EI << "'\n";
            }
        }
    }
};

} // End namespace llvm

namespace SVF {
    typedef llvm::GraphPrinter GraphPrinter;
};

#endif /* INCLUDE_UTIL_GRAPHPRINTER_H_ */
