//
// Created by fordrl on 4/27/21.
//

#ifndef SVF_SIMPLEOPTIONS_H
#define SVF_SIMPLEOPTIONS_H
#include "llvm/Support/CommandLine.h"

namespace SVF {

    class SimpleOptions {
    public:
        // Options that affect dot file dumping.

        /// When dumping dot files, show all nodes, even those ordinarily hidden.
        static const llvm::cl::opt<bool> DotShowAll;

        /// Dump only the nodes in the largest connected subgraph
        static const llvm::cl::opt<bool> DotLargestSubgraph;

        /// Dump each connected subgraph in a separate dot file.
        static const llvm::cl::opt<bool> DotSeparateSubgraphs;

        // End of options affecting dot file dumping.

    };

}; // Namespace SVF

#endif //SVF_SIMPLEOPTIONS_H
