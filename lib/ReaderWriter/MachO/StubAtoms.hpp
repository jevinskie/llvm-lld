//===- lib/ReaderWriter/MachO/StubAtoms.hpp -------------------------------===//
//
//                             The LLVM Linker
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLD_READER_WRITER_MACHO_STUB_ATOM_H_
#define LLD_READER_WRITER_MACHO_STUB_ATOM_H_

#include "llvm/ADT/ArrayRef.h"

#include "lld/Core/DefinedAtom.h"
#include "lld/Core/SharedLibraryAtom.h"
#include "lld/Core/File.h"
#include "lld/Core/Reference.h"

#include "ReferenceKinds.h"
#include "SimpleAtoms.hpp"
#include "StubAtoms_x86_64.hpp"
#include "StubAtoms_x86.hpp"

namespace lld {
namespace mach_o {


//
// StubBinderAtom created by the stubs pass.
//
class StubBinderAtom : public SharedLibraryAtom {
public:
  StubBinderAtom(const File &f) : _file(f) { 
  }
          
  virtual const File& file() const {
    return _file;
  }

  virtual StringRef name() const {
    return StringRef("dyld_stub_binder");
  }

  virtual StringRef loadName() const {
    return StringRef("/usr/lib/libSystem.B.dylib");
  }
  
  virtual bool canBeNullAtRuntime() const {
    return false;
  }
  
private:
  const File  &_file;
};



} // namespace mach_o 
} // namespace lld 


#endif // LLD_READER_WRITER_MACHO_STUB_ATOM_H_
