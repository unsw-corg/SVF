//
// Created by fordrl on 4/27/21.
//

#include <llvm/Support/CommandLine.h>
#include "Util/SimpleOptions.h"

namespace SVF {

    /// When dumping dot files, show all nodes, even those ordinarily hidden.
    const llvm::cl::opt<bool> SimpleOptions::DotShowAll(
            "dot-show-all",
            llvm::cl::init(false),
            llvm::cl::desc("In .dot file, show all nodes, even hidden."));

    /// Dump only the nodes in the largest connected subgraph
    const llvm::cl::opt<bool> SimpleOptions::DotLargestSubgraph(
            "dot-largest-subgraph",
            llvm::cl::init(false),
            llvm::cl::desc("Dump only nodes in largest connected subgraph"));

    /// Dump each connected subgraph in a separate dot file.
    const llvm::cl::opt<bool> SimpleOptions::DotSeparateSubgraphs(
            "dot-separate-subgraphs",
            llvm::cl::init(false),
            llvm::cl::desc("Dump each connected subgraph in a separate .dot file"));

};
