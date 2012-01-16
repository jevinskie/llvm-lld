//===- Core/DefinedAtom.h - The Fundamental Unit of Linking ---------------===//
//
//                             The LLVM Linker
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLD_CORE_DEFINED_ATOM_H_
#define LLD_CORE_DEFINED_ATOM_H_

#include <assert.h>

#include "lld/Core/Atom.h"
#include "lld/Core/Reference.h"

namespace llvm {
  template <typename T>
  class ArrayRef;

  class StringRef;
}

namespace lld {

class File;

/// An atom is the fundamental unit of linking.  A C function or global variable
/// is an atom.  An atom has content and attributes. The content of a function
/// atom is the instructions that implement the function.  The content of a
/// global variable atom is its initial bytes.
///
/// Here are some example attribute sets for common atoms. If a particular
/// attribute is not listed, the default values are:  definition=regular,
/// sectionChoice=basedOnContent, scope=translationUnit, merge=no, 
/// internalName=false, deadStrip=normal, interposable=no
///
///  C function:  void foo() {} <br>
///    name=foo, type=code, perm=r_x, scope=global
///
///  C static function:  staic void func() {} <br>
///    name=func, type=code, perm=r_x
///
///  C global variable:  int count = 1; <br>
///    name=count, type=data, perm=rw_, scope=global
///
///  C tentative definition:  int bar; <br>
///    name=bar, type=zerofill, perm=rw_, scope=global, 
///    merge=asTentative, interposable=yesAndRuntimeWeak
///
///  Uninitialized C static variable:  static int stuff; <br>
///    name=stuff, type=zerofill, perm=rw_
///
///  Weak C function:  __attribute__((weak)) void foo() {} <br>
///    name=foo, type=code, perm=r_x, scope=global, merge=asWeak
///
///  Hidden C function:  __attribute__((visibility("hidden"))) void foo() {}<br>
///    name=foo, type=code, perm=r_x, scope=linkageUnit
///
///  No-dead-strip function:  __attribute__((used)) void foo() {} <br>
///    name=foo, type=code, perm=r_x, scope=global, deadStrip=never
///
///  Non-inlined C++ inline method:  inline void Foo::doit() {} <br>
///    name=_ZN3Foo4doitEv, type=code, perm=r_x, scope=global, 
///    mergeDupes=asWeak
///
///  Non-inlined C++ inline method whose address is taken:  
///     inline void Foo::doit() {} <br>
///    name=_ZN3Foo4doitEv, type=code, perm=r_x, scope=global, 
///    mergeDupes=asAddressedWeak
///
///  literal c-string:  "hello" <br>
///    name=L0, internalName=true, type=cstring, perm=r__, scope=linkageUnit
///
///  literal double:  1.234 <br>
///    name=L0, internalName=true, type=literal8, perm=r__, scope=linkageUnit
///
///  constant:  { 1,2,3 } <br>
///    name=L0, internalName=true, type=constant, perm=r__, scope=linkageUnit
///
///  Pointer to initializer function:  <br>
///    name=_init, internalName=true, type=initializer, perm=rw_l,
///    sectionChoice=customRequired
///
///  C function place in custom section:  __attribute__((section("__foo"))) 
///                                       void foo() {} <br>
///    name=foo, type=code, perm=r_x, scope=global, 
///    sectionChoice=customRequired, sectionName=__foo
///
class DefinedAtom : public Atom {
public:
  /// Whether this atom is defined or a proxy for an undefined symbol
  virtual Definition definition() const {
    return Atom::definitionRegular;
  }

  virtual const DefinedAtom* definedAtom() const { 
    return this; 
  }

  /// The scope in which this atom is acessible to other atoms.
  enum Scope {
    scopeTranslationUnit,  ///< Accessible only to atoms in the same translation
                           ///  unit (e.g. a C static).
    scopeLinkageUnit,      ///< Accessible to atoms being linked but not visible  
                           ///  to runtime loader (e.g. visibility=hidden).
    scopeGlobal            ///< Accessible to all atoms and visible to runtime
                           ///  loader (e.g. visibility=default).
  };

  enum Interposable {
    interposeNo,            // linker can directly bind uses of this atom
    interposeYes,           // linker must indirect (through GOT) uses
    interposeYesAndRuntimeWeak // must indirect and mark symbol weak in final 
                               // linked image
  };

  enum Merge {
    mergeNo,                // Another atom with same name is error
    mergeAsTentative,       // Is ANSI C tentative defintion, can be coalesced
    mergeAsWeak,            // is C++ inline definition that was not inlined,  
                            // but address was not taken, so atom can be hidden
                            // by linker
    mergeAsWeakAndAddressUsed // is C++ definition inline definition whose 
                              // address was taken.
  };

  enum ContentType {
    typeUnknown,            // for use with definitionUndefined
    typeCode,               // executable code
    typeResolver,           // function which returns address of target
    typeBranchIsland,       // linker created for large binaries
    typeBranchShim,         // linker created to switch thumb mode
    typeStub,               // linker created for calling external function
    typeStubHelper,         // linker created for initial stub binding
    typeConstant,           // a read-only constant
    typeCString,            // a zero terminated UTF8 C string
    typeUTF16String,        // a zero terminated UTF16 string
    typeCFI,                // a FDE or CIE from dwarf unwind info
    typeLSDA,               // extra unwinding info
    typeLiteral4,           // a four-btye read-only constant
    typeLiteral8,           // an eight-btye read-only constant
    typeLiteral16,          // a sixteen-btye read-only constant
    typeData,               // read-write data
    typeZeroFill,           // zero-fill data
    typeObjC1Class,         // ObjC1 class [Darwin]
    typeLazyPointer,        // pointer through which a stub jumps
    typeLazyDylibPointer,   // pointer through which a stub jumps [Darwin]
    typeCFString,           // NS/CFString object [Darwin]
    typeGOT,                // pointer to external symbol
    typeInitializerPtr,     // pointer to initializer function
    typeTerminatorPtr,      // pointer to terminator function
    typeCStringPtr,         // pointer to UTF8 C string [Darwin]
    typeObjCClassPtr,       // pointer to ObjC class [Darwin]
    typeObjC2CategoryList,  // pointers to ObjC category [Darwin]
    typeDTraceDOF,          // runtime data for Dtrace [Darwin]
    typeTempLTO,            // temporary atom for bitcode reader
    typeCompactUnwindInfo,  // runtime data for unwinder [Darwin]
    typeThunkTLV,           // thunk used to access a TLV [Darwin]
    typeTLVInitialData,     // initial data for a TLV [Darwin]
    typeTLVInitialZeroFill, // TLV initial zero fill data [Darwin]
    typeTLVInitializerPtr,  // pointer to thread local initializer [Darwin]
    typeFirstInSection,     // label for boundary of section [Darwin]
    typeLastInSection,      // label for boundary of section [Darwin]
  };

  enum ContentPermissions {
    perm___  = 0,           // mapped as unaccessible
    permR__  = 8,           // mapped read-only
    permR_X  = 8 + 2,       // mapped readable and executable
    permRW_  = 8 + 4,       // mapped readable and writable
    permRW_L = 8 + 4 + 1,   // initially mapped r/w, then made read-only
                            // loader writable
  };

  enum SectionChoice {
    sectionBasedOnContent,  // linker infers final section based on content
    sectionCustomPreferred, // linker may place in specific section
    sectionCustomRequired   // linker must place in specific section
  };

  enum DeadStripKind {
    deadStripNormal,        // linker may dead strip this atom
    deadStripNever,         // linker must never dead strip this atom
    deadStripAlways         // linker must remove this atom if unused
  };

  struct Alignment {
    Alignment(int p2, int m = 0)
      : powerOf2(p2)
      , modulus(m) {}

    uint16_t powerOf2;
    uint16_t modulus;
  };

  /// ordinal - returns a value for the order of this Atom within its file.
  /// This is used by the linker to order the layout of Atoms so that
  /// the resulting image is stable and reproducible.
  virtual uint64_t ordinal() const = 0;
    
  /// internalName - If the name is just a temporary label that should
  /// not show up in the final linked image.
  virtual bool internalName() const = 0;
  
  /// size - the number of bytes of space this atom's content will occupy
  /// in the final linked image.  For a function atom, it is the number
  /// of bytes of code in the function.
  virtual uint64_t size() const = 0;

  /// scope - The visibility of this atom to other atoms.  C static functions
  /// have scope scopeTranslationUnit.  Regular C functions have scope 
  /// scopeGlobal.  Functions compiled with visibility=hidden have scope
  /// scopeLinkageUnit so they can be see by other atoms being linked but not
  /// by the OS loader.
  virtual Scope scope() const = 0;
  
  /// interposable - Whether the linker should use direct or indirect
  /// access to this atom.
  virtual Interposable interposable() const = 0;
  
  /// merge - how the linker should handle if multiple atoms have 
  /// the same name.
  virtual Merge merge() const = 0;
  
  /// contentType - The type of this atom, such as code or data.
  virtual ContentType contentType() const = 0;
    
  /// alignment - The alignment constraints on how this atom must be laid out 
  /// in the final linked image (e.g. 16-byte aligned).
  virtual Alignment alignment() const = 0;
  
  /// sectionChoice - Whether this atom must be in a specially named section
  /// in the final linked image, or if the linker can infer the section 
  /// based on the contentType().
  virtual SectionChoice sectionChoice() const = 0;
    
  /// customSectionName - If sectionChoice() != sectionBasedOnContent, then
  /// this return the name of the section the atom should be placed into.
  virtual llvm::StringRef customSectionName() const = 0;
    
  /// deadStrip - constraints on whether the linker may dead strip away 
  /// this atom.
  virtual DeadStripKind deadStrip() const = 0;
    
  /// permissions - Returns the OS memory protections required for this atom's
  /// content at runtime.  A function atom is R_X, a global variable is RW_,
  /// and a read-only constant is R__.
  virtual ContentPermissions permissions() const = 0;
  
  /// isThumb - only applicable to ARM code. Tells the linker if the code
  /// uses thumb or arm instructions.  The linker needs to know this to
  /// set the low bit of pointers to thumb functions.
  virtual bool isThumb() const = 0;
    
  /// isAlias - means this is a zero size atom that exists to provide an
  /// alternate name for another atom.  Alias atoms must have a special
  /// Reference to the atom they alias which the layout engine recognizes
  /// and forces the alias atom to layout right before the target atom.
  virtual bool isAlias() const = 0; 
  
  /// rawContent - returns a reference to the raw (unrelocated) bytes of 
  /// this Atom's content.
  virtual llvm::ArrayRef<uint8_t> rawContent() const = 0;

  /// referencesBegin - used to start iterating this Atom's References
  virtual Reference::iterator referencesBegin() const = 0;

  /// referencesEnd - used to end iterating this Atom's References
  virtual Reference::iterator referencesEnd() const = 0;

protected:
  /// DefinedAtom is an abstract base class.  
  /// Only subclasses can access constructor.
  DefinedAtom() { }
  
  /// The memory for DefinedAtom objects is always managed by the owning File
  /// object.  Therefore, no one but the owning File object should call
  /// delete on an Atom.  In fact, some File objects may bulk allocate
  /// an array of Atoms, so they cannot be individually deleted by anyone.
  virtual ~DefinedAtom() {}
};

} // namespace lld

#endif // LLD_CORE_DEFINED_ATOM_H_