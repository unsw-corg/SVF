echo "Setting up environment for SVF"


#########
# export LLVM_DIR and Z3_DIR
# Please change LLVM_DIR and Z3_DIR if they are different 
########

export SVFHOME=`pwd`
if [ -z "$LLVM_DIR" ]; then
  export LLVM_DIR=$SVFHOME/llvm-10.0.0.obj
fi

echo "LLVM_DIR =" $LLVM_DIR

if [ -z "$Z3_DIR" ]; then
  export Z3_DIR=$SVFHOME/z3.obj
fi

echo "Z3_DIR =" $Z3_DIR

export PATH=$LLVM_DIR/bin:$PATH


#########
#export PATH FOR SVF and LLVM executables
#########
if [ "$1" = "debug" ]; then
  PTAOBJTY='Debug'
else
  PTAOBJTY='Release'
fi
Build=$PTAOBJTY'-build'

export PATH=$LLVM_DIR/bin:$PATH
export PTABIN=$SVFHOME/$Build/bin
export PATH=$PTABIN:$PATH
