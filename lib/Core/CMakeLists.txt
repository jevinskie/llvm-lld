set(LLVM_LINK_COMPONENTS support)

add_lld_library(lldCore
  DefinedAtom.cpp
  Error.cpp
  File.cpp
  InputFiles.cpp
  Resolver.cpp
  SymbolTable.cpp
  )
