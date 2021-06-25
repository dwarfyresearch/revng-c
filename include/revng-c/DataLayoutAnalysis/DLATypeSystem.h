#pragma once

//
// Copyright (c) rev.ng Srls. See LICENSE.md for details.
//

#include <compare>
#include <cstddef>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <utility>

#include "llvm/ADT/GraphTraits.h"
#include "llvm/ADT/IntEqClasses.h"
#include "llvm/ADT/SmallSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/Allocator.h"
#include "llvm/Support/raw_ostream.h"

#include "revng/ADT/FilteredGraphTraits.h"
#include "revng/Support/Assert.h"

#include "revng-c/DataLayoutAnalysis/DLALayouts.h"

namespace dla {

/// Class used to mark InstanceLinkTags between LayoutTypes
struct OffsetExpression {
  int64_t Offset;
  llvm::SmallVector<int64_t, 4> Strides;
  llvm::SmallVector<std::optional<int64_t>, 4> TripCounts;

  explicit OffsetExpression() : OffsetExpression(0LL){};
  explicit OffsetExpression(int64_t Off) :
    Offset(Off), Strides(), TripCounts() {}

  std::strong_ordering
  operator<=>(const OffsetExpression &Other) const = default;

  void print(llvm::raw_ostream &OS) const;
}; // end class OffsetExpression

class TypeLinkTag {
public:
  enum LinkKind {
    LK_Inheritance,
    LK_Equality,
    LK_Instance,
    LK_All,
  };

  static const char *toString(enum LinkKind K) {
    switch (K) {
    case LK_Inheritance:
      return "Inheritance";
    case LK_Equality:
      return "Equality";
    case LK_Instance:
      return "Instance";
    case LK_All:
      return "None";
    }
    revng_unreachable();
  }

protected:
  OffsetExpression OE;
  const LinkKind Kind;

  explicit TypeLinkTag(LinkKind K, OffsetExpression &&O) : OE(O), Kind(K) {}

  // TODO: potentially we are interested in marking TypeLinkTags with some info
  // that allows us to track which step on the type system has created them.
  // However, this is not necessary now, so I'll leave it for when we have
  // identified more clearly if we really need it and why.

public:
  TypeLinkTag() = delete;

  LinkKind getKind() const { return Kind; }

  const OffsetExpression &getOffsetExpr() const {
    revng_assert(getKind() == LK_Instance);
    return OE;
  }

  static TypeLinkTag equalityTag() {
    return TypeLinkTag(LK_Equality, OffsetExpression{});
  }

  static TypeLinkTag inheritanceTag() {
    return TypeLinkTag(LK_Inheritance, OffsetExpression{});
  }

  // This method is templated just to enable perfect forwarding.
  template<typename OffsetExpressionT>
  static TypeLinkTag instanceTag(OffsetExpressionT &&O) {
    return TypeLinkTag(LK_Instance, std::forward<OffsetExpressionT>(O));
  }

  std::strong_ordering operator<=>(const TypeLinkTag &Other) const = default;
}; // end class TypeLinkTag

class LayoutTypeSystem;

enum InterferingChildrenInfo {
  Unknown = 0,
  AllChildrenAreInterfering,
  AllChildrenAreNonInterfering,
};

struct LayoutTypeSystemNode {
  const uint64_t ID = 0ULL;
  using Link = std::pair<LayoutTypeSystemNode *, const TypeLinkTag *>;
  using NeighborsSet = std::set<Link>;
  NeighborsSet Successors{};
  NeighborsSet Predecessors{};
  uint64_t Size{};
  InterferingChildrenInfo InterferingInfo{ Unknown };
  LayoutTypeSystemNode(uint64_t I) : ID(I) {}

public:
  // This method should never be called, but it's necessary to be able to use
  // some llvm::GraphTraits algorithms, otherwise they wouldn't compile.
  LayoutTypeSystem *getParent() {
    revng_unreachable();
    return nullptr;
  }

  void print(llvm::raw_ostream &OS) const;

  void printAsOperand(llvm::raw_ostream &OS, bool /* unused */) const {
    print(OS);
  }
};

///\brief This class handles equivalence classes between indexes of vectors
class VectEqClasses : public llvm::IntEqClasses {
private:
  // ID of the first removed ID
  std::optional<unsigned> RemovedID = {};
  unsigned NElems = 0;

private:
  ///\brief Used internally, operator[] is removed for this class
  unsigned lookupEqClass(unsigned ID) const {
    return llvm::IntEqClasses::operator[](ID);
  }

public:
  ///\brief Add 1 element with its own equivalence class
  unsigned growBy1();

  ///\brief Remove the whole equivalence class of \a ID
  void remove(const unsigned ID);

  ///\brief Check if the element has been removed
  bool isRemoved(const unsigned ID) const;

  ///\brief Get the total number of elements added
  unsigned getNumElements() const { return NElems; }

public:
  ///\brief You can't access the Eq Classes directly, some might be deleted
  unsigned operator[](unsigned) const = delete;

  ///\brief Get the Equivalence class ID of an element (must be compressed)
  ///\return empty if the element is out-of-bounds or has been removed
  std::optional<unsigned> getEqClassID(const unsigned ID) const;

  ///\brief Get all the elements that are in the same equivalence class of \a ID
  ///\note Expensive: performs a linear scan of all the elements
  std::vector<unsigned> getEqClass(const unsigned ID) const;

  ///\brief Check if \a ID1 and \a ID2 have the same equivalence class
  bool haveSameEqClass(unsigned ID1, unsigned ID2) const;
};

///\brief This class is used to print debug information about the TypeSystem
///
/// Override this to obtain implementation-specific debug prints.
struct TSDebugPrinter {
  virtual void printNodeContent(const LayoutTypeSystem &TS,
                                const LayoutTypeSystemNode *N,
                                llvm::raw_fd_ostream &File) const;

  virtual ~TSDebugPrinter() {}
};

class LayoutTypeSystem {
public:
  using Node = LayoutTypeSystemNode;
  using NodePtr = LayoutTypeSystemNode *;
  using NodeUniquePtr = std::unique_ptr<LayoutTypeSystemNode>;

  static dla::LayoutTypeSystem::NodePtr
  getNodePtr(const dla::LayoutTypeSystem::NodeUniquePtr &P) {
    return P.get();
  }

  LayoutTypeSystem() : DebugPrinter(new TSDebugPrinter) {}

  ~LayoutTypeSystem() {
    for (auto *Layout : Layouts) {
      Layout->~LayoutTypeSystemNode();
      NodeAllocator.Deallocate(Layout);
    }
    Layouts.clear();
  }

public:
  LayoutTypeSystemNode *createArtificialLayoutType();

protected:
  // This method is templated only to enable perfect forwarding.
  template<typename TagT>
  std::pair<const TypeLinkTag *, bool>
  addLink(LayoutTypeSystemNode *Src, LayoutTypeSystemNode *Tgt, TagT &&Tag) {
    if (Src == nullptr or Tgt == nullptr or Src == Tgt)
      return std::make_pair(nullptr, false);
    revng_assert(Layouts.count(Src));
    revng_assert(Layouts.count(Tgt));
    auto It = LinkTags.insert(std::forward<TagT>(Tag)).first;
    revng_assert(It != LinkTags.end());
    const TypeLinkTag *T = &*It;
    bool New = Src->Successors.insert(std::make_pair(Tgt, T)).second;
    New |= Tgt->Predecessors.insert(std::make_pair(Src, T)).second;
    return std::make_pair(T, New);
  }

public:
  std::pair<const TypeLinkTag *, bool>
  addEqualityLink(LayoutTypeSystemNode *Src, LayoutTypeSystemNode *Tgt) {
    auto ForwardLinkTag = addLink(Src, Tgt, dla::TypeLinkTag::equalityTag());
    auto BackwardLinkTag = addLink(Tgt, Src, dla::TypeLinkTag::equalityTag());
    revng_assert(ForwardLinkTag == BackwardLinkTag);
    return ForwardLinkTag;
  }

  std::pair<const TypeLinkTag *, bool>
  addInheritanceLink(LayoutTypeSystemNode *Src, LayoutTypeSystemNode *Tgt) {
    return addLink(Src, Tgt, dla::TypeLinkTag::inheritanceTag());
  }

  // This method is templated just to enable perfect forwarding.
  template<typename OffsetExpressionT>
  std::pair<const TypeLinkTag *, bool>
  addInstanceLink(LayoutTypeSystemNode *Src,
                  LayoutTypeSystemNode *Tgt,
                  OffsetExpressionT &&OE) {
    using OET = OffsetExpressionT;
    return addLink(Src,
                   Tgt,
                   dla::TypeLinkTag::instanceTag(std::forward<OET>(OE)));
  }

  void dumpDotOnFile(const char *FName) const;

  void dumpDotOnFile(const std::string &FName) const {
    dumpDotOnFile(FName.c_str());
  }

  auto getNumLayouts() const { return Layouts.size(); }

  auto getLayoutsRange() const {
    return llvm::make_range(Layouts.begin(), Layouts.end());
  }

public:
  void mergeNodes(const std::vector<LayoutTypeSystemNode *> &ToMerge);

  void removeNode(LayoutTypeSystemNode *N);

  void moveEdges(LayoutTypeSystemNode *OldSrc,
                 LayoutTypeSystemNode *NewSrc,
                 LayoutTypeSystemNode *Tgt,
                 int64_t OffsetToSum);

private:
  uint64_t NID = 0ULL;

  // Holds all the LayoutTypeSystemNode
  llvm::BumpPtrAllocator NodeAllocator = {};
  std::set<LayoutTypeSystemNode *> Layouts = {};

  // Holds the link tags, so that they can be deduplicated and referred to using
  // TypeLinkTag * in the links inside LayoutTypeSystemNode
  std::set<TypeLinkTag> LinkTags = {};

public:
  // Checks that is valid, and returns true if it is, false otherwise
  bool verifyConsistency() const;
  // Checks that is valid and a DAG, and returns true if it is, false otherwise
  bool verifyDAG() const;
  // Checks that is valid and a DAG, and returns true if it is, false otherwise
  bool verifyInheritanceDAG() const;
  // Checks that is valid and a DAG, and returns true if it is, false otherwise
  bool verifyInstanceDAG() const;
  // Checks that the type system, filtered looking only at inheritance edges, is
  // a tree, meaning that a give LayoutTypeSystemNode cannot inherit from two
  // different LayoutTypeSystemNodes.
  bool verifyInheritanceTree() const;
  // Checks that there are no leaf nodes without valid layout information
  bool verifyLeafs() const;
  // Checks that there are no equality edges.
  bool verifyNoEquality() const;

private:
  // Equivalence classes between nodes. Each node is identified by an ID.
  VectEqClasses EqClasses;
  // Object that defines how the content of each node should be printed
  std::unique_ptr<TSDebugPrinter> DebugPrinter;

public:
  unsigned getNID() const { return NID; }

  VectEqClasses &getEqClasses() { return EqClasses; }
  const VectEqClasses &getEqClasses() const { return EqClasses; }

  void setDebugPrinter(std::unique_ptr<TSDebugPrinter> &&Printer) {
    DebugPrinter = std::move(Printer);
  }
}; // end class LayoutTypeSystem

} // end namespace dla

template<>
struct llvm::GraphTraits<dla::LayoutTypeSystemNode *> {
protected:
  using NodeT = dla::LayoutTypeSystemNode;

public:
  using NodeRef = NodeT *;
  using EdgeRef = const NodeT::NeighborsSet::value_type;

  static NodeRef edge_dest(EdgeRef E) { return E.first; }
  using EdgeDestT = NodeRef (*)(EdgeRef);

  using ChildEdgeIteratorType = NodeT::NeighborsSet::iterator;
  using ChildIteratorType = llvm::mapped_iterator<ChildEdgeIteratorType,
                                                  EdgeDestT>;

  static NodeRef getEntryNode(const NodeRef &N) { return N; }

  static ChildIteratorType child_begin(NodeRef N) {
    return llvm::map_iterator(N->Successors.begin(), edge_dest);
  }
  static ChildIteratorType child_end(NodeRef N) {
    return llvm::map_iterator(N->Successors.end(), edge_dest);
  }

  static ChildEdgeIteratorType child_edge_begin(NodeRef N) {
    return N->Successors.begin();
  }
  static ChildEdgeIteratorType child_edge_end(NodeRef N) {
    return N->Successors.end();
  }
}; // end struct llvm::GraphTraits<dla::LayoutTypeSystemNode *>

template<>
struct llvm::GraphTraits<const dla::LayoutTypeSystemNode *> {
protected:
  using NodeT = const dla::LayoutTypeSystemNode;

public:
  using NodeRef = NodeT *;
  using EdgeRef = const NodeT::NeighborsSet::value_type;

  static NodeRef edge_dest(EdgeRef E) { return E.first; }
  using EdgeDestT = NodeRef (*)(EdgeRef);

  using ChildEdgeIteratorType = NodeT::NeighborsSet::iterator;
  using ChildIteratorType = llvm::mapped_iterator<ChildEdgeIteratorType,
                                                  EdgeDestT>;

  static NodeRef getEntryNode(const NodeRef &N) { return N; }

  static ChildIteratorType child_begin(NodeRef N) {
    return llvm::map_iterator(N->Successors.begin(), edge_dest);
  }
  static ChildIteratorType child_end(NodeRef N) {
    return llvm::map_iterator(N->Successors.end(), edge_dest);
  }

  static ChildEdgeIteratorType child_edge_begin(NodeRef N) {
    return N->Successors.begin();
  }
  static ChildEdgeIteratorType child_edge_end(NodeRef N) {
    return N->Successors.end();
  }
}; // end struct llvm::GraphTraits<dla::LayoutTypeSystemNode *>

template<>
struct llvm::GraphTraits<llvm::Inverse<dla::LayoutTypeSystemNode *>> {
protected:
  using NodeT = dla::LayoutTypeSystemNode;

public:
  using NodeRef = NodeT *;
  using EdgeRef = const NodeT::NeighborsSet::value_type;

  static NodeRef edge_dest(EdgeRef E) { return E.first; }
  using EdgeDestT = NodeRef (*)(EdgeRef);

  using ChildEdgeIteratorType = NodeT::NeighborsSet::iterator;
  using ChildIteratorType = llvm::mapped_iterator<ChildEdgeIteratorType,
                                                  EdgeDestT>;

  static NodeRef getEntryNode(const NodeRef &N) { return N; }

  static ChildIteratorType child_begin(NodeRef N) {
    return llvm::map_iterator(N->Predecessors.begin(), edge_dest);
  }
  static ChildIteratorType child_end(NodeRef N) {
    return llvm::map_iterator(N->Predecessors.end(), edge_dest);
  }

  static ChildEdgeIteratorType child_edge_begin(NodeRef N) {
    return N->Predecessors.begin();
  }
  static ChildEdgeIteratorType child_edge_end(NodeRef N) {
    return N->Predecessors.end();
  }
}; // end struct llvm::GraphTraits<dla::LayoutTypeSystemNode *>

template<>
struct llvm::GraphTraits<llvm::Inverse<const dla::LayoutTypeSystemNode *>> {
protected:
  using NodeT = const dla::LayoutTypeSystemNode;

public:
  using NodeRef = NodeT *;
  using EdgeRef = const NodeT::NeighborsSet::value_type;

  static NodeRef edge_dest(EdgeRef E) { return E.first; }
  using EdgeDestT = NodeRef (*)(EdgeRef);

  using ChildEdgeIteratorType = NodeT::NeighborsSet::iterator;
  using ChildIteratorType = llvm::mapped_iterator<ChildEdgeIteratorType,
                                                  EdgeDestT>;

  static NodeRef getEntryNode(const NodeRef &N) { return N; }

  static ChildIteratorType child_begin(NodeRef N) {
    return llvm::map_iterator(N->Predecessors.begin(), edge_dest);
  }
  static ChildIteratorType child_end(NodeRef N) {
    return llvm::map_iterator(N->Predecessors.end(), edge_dest);
  }

  static ChildEdgeIteratorType child_edge_begin(NodeRef N) {
    return N->Predecessors.begin();
  }
  static ChildEdgeIteratorType child_edge_end(NodeRef N) {
    return N->Predecessors.end();
  }
}; // end struct llvm::GraphTraits<dla::LayoutTypeSystemNode *>

template<>
struct llvm::GraphTraits<const dla::LayoutTypeSystem *>
  : public llvm::GraphTraits<const dla::LayoutTypeSystemNode *> {

public:
  using nodes_iterator = std::set<dla::LayoutTypeSystemNode *>::iterator;

  static NodeRef getEntryNode(const dla::LayoutTypeSystem *) { return nullptr; }

  static nodes_iterator nodes_begin(const dla::LayoutTypeSystem *G) {
    return G->getLayoutsRange().begin();
  }

  static nodes_iterator nodes_end(const dla::LayoutTypeSystem *G) {
    return G->getLayoutsRange().end();
  }

  static unsigned size(const dla::LayoutTypeSystem *G) {
    return G->getNumLayouts();
  }
}; // struct llvm::GraphTraits<dla::LayoutTypeSystem>

template<>
struct llvm::GraphTraits<dla::LayoutTypeSystem *>
  : public llvm::GraphTraits<dla::LayoutTypeSystemNode *> {

public:
  using nodes_iterator = std::set<dla::LayoutTypeSystemNode *>::iterator;

  static NodeRef getEntryNode(const dla::LayoutTypeSystem *) { return nullptr; }

  static nodes_iterator nodes_begin(const dla::LayoutTypeSystem *G) {
    return G->getLayoutsRange().begin();
  }

  static nodes_iterator nodes_end(const dla::LayoutTypeSystem *G) {
    return G->getLayoutsRange().end();
  }

  static unsigned size(dla::LayoutTypeSystem *G) { return G->getNumLayouts(); }
}; // struct llvm::GraphTraits<dla::LayoutTypeSystem>

namespace dla {

template<dla::TypeLinkTag::LinkKind K>
inline bool hasLinkKind(const dla::LayoutTypeSystemNode::Link &L) {
  if constexpr (K == dla::TypeLinkTag::LinkKind::LK_All)
    return true;
  else
    return L.second->getKind() == K;
}

inline bool
isEqualityEdge(const llvm::GraphTraits<LayoutTypeSystemNode *>::EdgeRef &E) {
  return hasLinkKind<TypeLinkTag::LinkKind::LK_Equality>(E);
}

inline bool
isInheritanceEdge(const llvm::GraphTraits<LayoutTypeSystemNode *>::EdgeRef &E) {
  return hasLinkKind<TypeLinkTag::LinkKind::LK_Inheritance>(E);
}

inline bool
isInstanceEdge(const llvm::GraphTraits<LayoutTypeSystemNode *>::EdgeRef &E) {
  return hasLinkKind<TypeLinkTag::LinkKind::LK_Instance>(E);
}

template<dla::TypeLinkTag::LinkKind K = dla::TypeLinkTag::LinkKind::LK_All>
inline bool isLeaf(const LayoutTypeSystemNode *N) {
  using LTSN = const LayoutTypeSystemNode;
  using GraphNodeT = LTSN *;
  using FilteredNodeT = EdgeFilteredGraph<GraphNodeT, hasLinkKind<K>>;
  using GT = llvm::GraphTraits<FilteredNodeT>;
  return GT::child_begin(N) == GT::child_end(N);
}

inline bool isInheritanceLeaf(const LayoutTypeSystemNode *N) {
  return isLeaf<dla::TypeLinkTag::LinkKind::LK_Inheritance>(N);
}

inline bool isInstanceLeaf(const LayoutTypeSystemNode *N) {
  return isLeaf<dla::TypeLinkTag::LinkKind::LK_Instance>(N);
}

template<dla::TypeLinkTag::LinkKind K = dla::TypeLinkTag::LinkKind::LK_All>
inline bool isRoot(const LayoutTypeSystemNode *N) {
  using LTSN = const LayoutTypeSystemNode;
  using GraphNodeT = LTSN *;
  using FilteredNodeT = EdgeFilteredGraph<GraphNodeT, hasLinkKind<K>>;
  using IGT = llvm::GraphTraits<llvm::Inverse<FilteredNodeT>>;
  return IGT::child_begin(N) == IGT::child_end(N);
}

inline bool isInheritanceRoot(const LayoutTypeSystemNode *N) {
  return isRoot<dla::TypeLinkTag::LinkKind::LK_Inheritance>(N);
}

inline bool isInstanceRoot(const LayoutTypeSystemNode *N) {
  return isRoot<dla::TypeLinkTag::LinkKind::LK_Instance>(N);
}
} // end namespace dla
