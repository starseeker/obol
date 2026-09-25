/**************************************************************************\
 * Copyright (c) Kongsberg Oil & Gas Technologies AS
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * 
 * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 
 * Neither the name of the copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
\**************************************************************************/

/*!
  \class SoChildList SoChildList.h Inventor/misc/SoChildList.h
  \brief The SoChildList class is a container for node children.

  \ingroup coin_general

  This class does automatic notification on the parent nodes upon
  adding or removing children.

  Methods for action traversal of the children are also provided.
*/

#include <Inventor/misc/SoChildList.h>
#include <Inventor/SoPath.h>
#include <algorithm>
#include <exception>
#include <functional>
#include <stdexcept>
#include <utility>
#include <limits>
#include <unordered_map>
#include <Inventor/actions/SoAction.h>
#include <Inventor/nodes/SoNode.h>
#include <Inventor/SbName.h>

#if OBOL_DEBUG
#include <Inventor/errors/SoDebugError.h>
#endif // OBOL_DEBUG



/*!
  Default constructor, sets parent container and initializes a minimal
  list.
*/
SoChildList::SoChildList(SoNode * const parentptr)
  : SoNodeList()
{
  this->parent = parentptr;
}

/*!
  Constructor with hint about list size.

  \sa SoNodeList::SoNodeList(const int)
*/
SoChildList::SoChildList(SoNode * const parentptr, const int size)
  : SoNodeList(size)
{
  this->parent = parentptr;
}

/*!
  Copy constructor.

  \sa SoNodeList::SoNodeList(const SoNodeList &)
*/
SoChildList::SoChildList(SoNode * const parentptr, const SoChildList & cl)
  : SoNodeList()
{
  this->parent = parentptr;
  this->copy(cl);
}

/*!
  Destructor.
*/
SoChildList::~SoChildList()
{
  // Retire the graph links without dispatching unrelated immediate callbacks
  // from a destructor. Children still have list references while paths shrink.
  if (this->parent) {
    for (int i = 0; i < this->getLength(); ++i)
      if ((*this)[i]) (*this)[i]->removeAuditor(this->parent, SoNotRec::PARENT);
    for (int i = 0; i < this->auditors.getLength(); ++i) {
      SoPath * path = this->auditors[i];
      const int position = path->findNode(this->parent) + 1;
      if (position > 0 && position < path->getFullLength())
        path->truncate(position, FALSE);
    }
  }
  this->SoNodeList::truncate(0);
}

/*!
  Append a new \a node instance as a child of our parent container.

  Automatically notifies parent node and any SoPath instances auditing
  paths with nodes from this list.

  Overloaded from parent to accept an SoNode pointer argument.

  \sa SbPList::insert()
*/
void
SoChildList::append(SoNode * const node)
{
  // Complete storage preparation before installing the parent auditor. Once
  // connected, publishing the list slot and its reference cannot allocate.
  this->reserve(this->getLength() + 1);
  if (this->parent) {
    node->addAuditor(this->parent, SoNotRec::PARENT);
  }
  SoNodeList::append(node);

  if (this->parent) {
    this->parent->startNotify();
  }
  // Doesn't need to notify SoPath auditors, as adding a new node at
  // _the end_ won't affect any path "passing through" this childlist.
}

/*!
  Insert a new \a node instance as a child of our parent container at
  position \a addbefore.

  Automatically notifies parent node and any SoPath instances auditing
  paths with nodes from this list.

  Overloaded from parent to accept an SoNode pointer argument.

  \sa SbPList::insert()
*/
void
SoChildList::insert(SoNode * const node, const int addbefore)
{
  assert(addbefore <= this->getLength());
  if (this->parent) {
    node->addAuditor(this->parent, SoNotRec::PARENT);
  }
  SoNodeList::insert(node, addbefore);

  // FIXME: shouldn't we move this startNotify() call to the end of
  // the function?  pederb, 2002-10-02
  if (this->parent) {
    this->parent->startNotify();
    for (int i=0; i < this->auditors.getLength(); i++) {
      this->auditors[i]->insertIndex(this->parent, addbefore);
    }
  }
}

/*!
  \copydetails SbPList::remove(const int index)

  Automatically notifies parent node and any SoPath instances auditing
  paths with nodes from this list.

  Overloaded from parent to handle notification.

  \sa SbPList::remove(const int index)
*/
void
SoChildList::remove(const int index)
{
  assert(index >= 0 && index < this->getLength());
  if (this->parent) {
    SoNodeList::operator[](index)->removeAuditor(this->parent, SoNotRec::PARENT);
  }
  // FIXME: we experienced memory corruption if the
  // SoNodeList::remove(index) statement was placed here (before
  // updating paths). It seems to be working ok now, but we should
  // figure out exactly why we can't remove the node before updating
  // the paths.  pederb, 2002-10-02
  if (this->parent) {
    for (int i=0; i < this->auditors.getLength(); i++) {
      this->auditors[i]->removeIndex(this->parent, index);
    }
    /* notify before removal, so that the notification source gets the
     * chance to operate on the child to be removed. 20100426 tamer. */
    this->parent->startNotify();
  }
  SoNodeList::remove(index);
}

// Preparation owns only removal indices, affected paths and removed node
// references. The child graph and path chains are never copied.
namespace {
void notify_child_edit(SoBase *object, std::exception_ptr &failure)
{
  try { object->startNotify(); }
  catch (...) { if (!failure) failure = std::current_exception(); }
}

void release_prepared_parent(SoNode *parent, bool hadOwner)
{
  if (!parent) return;
  if (hadOwner) parent->unref();
  else parent->unrefNoDelete();
}
}

class SoChildList::PathChanges {
public:
  template <typename Remap>
  PathChanges(SoChildList &children, Remap remap)
  {
    this->changes.reserve(size_t(children.auditors.getLength()));
    for (int i = 0; i < children.auditors.getLength(); ++i) {
      SoPath *path = children.auditors[i];
      const int parentPosition = path->findNode(children.parent);
      if (parentPosition < 0)
        throw std::logic_error("child path auditor does not contain its parent");
      const int position = parentPosition + 1;
      if (position == path->getFullLength()) continue;
      const int oldIndex = path->getIndex(position);
      const int nextIndex = remap(oldIndex);
      if (nextIndex != oldIndex)
        this->changes.push_back({path, position, nextIndex});
    }
    for (const Change &change : this->changes) change.path->ref();
  }
  ~PathChanges()
  {
    for (const Change &change : this->changes) change.path->unref();
  }
  void commit()
  {
    for (const Change &change : this->changes) {
      if (change.index < 0) change.path->truncate(change.position, FALSE);
      else change.path->indices[change.position] = change.index;
    }
  }
  void notify(std::exception_ptr &failure)
  {
    for (const Change &change : this->changes)
      if (change.index < 0) notify_child_edit(change.path, failure);
  }
private:
  struct Change {
    SoPath *path;
    int position;
    int index;
  };
  std::vector<Change> changes;
};

class SoChildList::Removal::Impl {
public:
  Impl(SoChildList & target, std::vector<int> removedIndices)
    : children(target), indices(std::move(removedIndices)),
      originalLength(target.getLength())
  {
    int previous = -1;
    for (int index : this->indices) {
      if (index <= previous || index >= this->originalLength)
        throw std::invalid_argument("child removal indices must be sorted, unique and in range");
      previous = index;
    }
    this->nodes.reserve(this->indices.size());
    for (int index : this->indices) this->nodes.push_back(target[index]);

    // One parent auditor covers every occurrence of a shared child. Disconnect
    // it only if the removal leaves no occurrence of that child in this list.
    this->unparented = this->nodes;
    const std::less<SoNode *> ordered;
    std::sort(this->unparented.begin(), this->unparented.end(), ordered);
    this->unparented.erase(std::unique(this->unparented.begin(), this->unparented.end()),
                          this->unparented.end());
    std::vector<bool> retained(this->unparented.size(), false);
    size_t removed = 0;
    for (int i = 0; i < this->originalLength; ++i) {
      if (removed < this->indices.size() && this->indices[removed] == i) {
        ++removed;
        continue;
      }
      const auto found = std::lower_bound(this->unparented.begin(),
                                         this->unparented.end(), target[i], ordered);
      if (found != this->unparented.end() && *found == target[i])
        retained[size_t(found - this->unparented.begin())] = true;
    }
    size_t detached = 0;
    for (size_t i = 0; i < this->unparented.size(); ++i)
      if (!retained[i]) this->unparented[detached++] = this->unparented[i];
    this->unparented.resize(detached);

    this->paths = std::make_unique<PathChanges>(target, [this](int oldIndex) {
      const auto found = std::lower_bound(this->indices.begin(), this->indices.end(), oldIndex);
      return found != this->indices.end() && *found == oldIndex ? -1 :
        oldIndex - int(found - this->indices.begin());
    });
    // Everything which can allocate is prepared before acquiring references.
    // No retained node can die while its path and parent slots are compacted.
    if (target.parent) {
      this->parentHadOwner = target.parent->getRefCount() > 0;
      target.parent->ref();
    }
    for (SoNode * node : this->nodes) if (node) node->ref();
  }

  ~Impl()
  {
    this->paths.reset();
    for (SoNode * node : this->nodes) if (node) node->unref();
    release_prepared_parent(this->children.parent, this->parentHadOwner);
  }

  void commit()
  {
    if (this->committed) return;
    assert(this->children.getLength() == this->originalLength);
    if (this->children.parent)
      for (SoNode * node : this->unparented)
        if (node) node->removeAuditor(this->children.parent, SoNotRec::PARENT);
    this->paths->commit();
    // Compact once, preserving order. Removed nodes and every kept node still
    // have references while overwritten slots and the old tail are released.
    int written = 0;
    size_t removed = 0;
    for (int i = 0; i < this->originalLength; ++i) {
      if (removed < this->indices.size() && this->indices[removed] == i) {
        ++removed;
        continue;
      }
      if (written != i) this->children.SoNodeList::set(written, this->children[i]);
      ++written;
    }
    this->children.SoNodeList::truncate(written);
    this->committed = true;
  }

  void notify()
  {
    if (!this->committed || this->notified) return;
    this->notified = true;
    std::exception_ptr failure;
    this->paths->notify(failure);
    if (this->children.parent) notify_child_edit(this->children.parent, failure);
    if (failure) std::rethrow_exception(failure);
  }

private:
  SoChildList & children;
  std::vector<int> indices;
  int originalLength;
  std::vector<SoNode *> nodes;
  std::vector<SoNode *> unparented;
  std::unique_ptr<PathChanges> paths;
  bool parentHadOwner = false;
  bool committed = false;
  bool notified = false;
};

SoChildList::Removal::Removal(std::unique_ptr<Impl> state)
  : impl(std::move(state)) { }
SoChildList::Removal::~Removal() = default;
void SoChildList::Removal::commit() { this->impl->commit(); }
void SoChildList::Removal::notify() { this->impl->notify(); }

std::unique_ptr<SoChildList::Removal>
SoChildList::prepareRemoval(std::vector<int> indices)
{
  if (indices.empty()) return nullptr;
  auto state = std::make_unique<Removal::Impl>(*this, std::move(indices));
  return std::unique_ptr<Removal>(new Removal(std::move(state)));
}

class SoChildList::Replacement::Impl {
public:
  Impl(SoChildList &target, const std::vector<SoNode *> &next)
    : children(target), originalLength(target.getLength())
  {
    if (next.size() > size_t(std::numeric_limits<int>::max()))
      throw std::length_error("child replacement exceeds list capacity");
    struct Positions {
      std::vector<int> indices;
      size_t matched = 0;
    };
    std::unordered_map<SoNode *, Positions> positions;
    this->before.reserve(this->originalLength);
    this->after.reserve(int(next.size()));
    for (int i = 0; i < this->originalLength; ++i)
      this->before.append(target[i]);
    for (size_t i = 0; i < next.size(); ++i) {
      if (!next[i]) throw std::invalid_argument("null replacement child");
      this->after.append(next[i]);
      positions[next[i]].indices.push_back(int(i));
    }
    std::vector<int> nextIndices(size_t(this->originalLength), -1);
    this->unparented.reserve(size_t(this->originalLength));
    for (int i = 0; i < this->originalLength; ++i) {
      const auto found = positions.find(target[i]);
      if (found == positions.end()) {
        this->unparented.push_back(target[i]);
      } else {
        Positions &where = found->second;
        if (where.matched < where.indices.size())
          nextIndices[size_t(i)] = where.indices[where.matched];
        ++where.matched;
      }
    }
    std::sort(this->unparented.begin(), this->unparented.end(), std::less<SoNode *>());
    this->unparented.erase(std::unique(this->unparented.begin(), this->unparented.end()),
                          this->unparented.end());
    this->paths = std::make_unique<PathChanges>(target,
      [&nextIndices](int index) { return nextIndices.at(size_t(index)); });
    target.reserve(int(next.size()));
    this->attached.reserve(positions.size());
    // Only unpublished links are added here. The caller holds the scene thread
    // and keeps these nodes and audited paths unchanged through commit.
    try {
      if (target.parent) {
        for (const auto &entry : positions) {
          if (entry.second.matched) continue;
          entry.first->addAuditor(target.parent, SoNotRec::PARENT);
          this->attached.push_back(entry.first);
        }
      }
    } catch (...) {
      this->detachPreparedLinks();
      throw;
    }
    if (target.parent) {
      this->parentHadOwner = target.parent->getRefCount() > 0;
      target.parent->ref();
    }
  }
  ~Impl()
  {
    if (!this->committed) this->detachPreparedLinks();
    this->paths.reset();
    this->before.truncate(0);
    this->after.truncate(0);
    release_prepared_parent(this->children.parent, this->parentHadOwner);
  }
  void commit()
  {
    if (this->committed) return;
    assert(this->children.getLength() == this->originalLength);
    if (this->children.parent)
      for (SoNode *node : this->unparented)
        node->removeAuditor(this->children.parent, SoNotRec::PARENT);
    this->paths->commit();
    // Both lists retain their nodes until publication and notification finish.
    this->children.SoNodeList::truncate(0);
    for (int i = 0; i < this->after.getLength(); ++i)
      this->children.SoNodeList::append(this->after[i]);
    this->committed = true;
  }
  void notify()
  {
    if (!this->committed || this->notified) return;
    this->notified = true;
    std::exception_ptr failure;
    this->paths->notify(failure);
    if (this->children.parent) notify_child_edit(this->children.parent, failure);
    if (failure) std::rethrow_exception(failure);
  }
private:
  void detachPreparedLinks()
  {
    for (SoNode *node : this->attached)
      node->removeAuditor(this->children.parent, SoNotRec::PARENT);
  }
  SoChildList &children;
  int originalLength;
  SoNodeList before;
  SoNodeList after;
  std::vector<SoNode *> unparented;
  std::vector<SoNode *> attached;
  std::unique_ptr<PathChanges> paths;
  bool parentHadOwner = false;
  bool committed = false;
  bool notified = false;
};

SoChildList::Replacement::Replacement(std::unique_ptr<Impl> state)
  : impl(std::move(state)) { }
SoChildList::Replacement::~Replacement() = default;
void SoChildList::Replacement::commit() { this->impl->commit(); }
void SoChildList::Replacement::notify() { this->impl->notify(); }

std::unique_ptr<SoChildList::Replacement>
SoChildList::prepareReplacement(const std::vector<SoNode *> &children)
{
  auto state = std::make_unique<Replacement::Impl>(*this, children);
  return std::unique_ptr<Replacement>(new Replacement(std::move(state)));
}

/*!
  \copydetails SbPList::truncate(const int length, const int fit)

  Overloaded from parent to handle notification.

  \sa SbPList::truncate()
*/ 
void
SoChildList::truncate(const int length)
{
  const int n = this->getLength();
  assert(length >= 0 && length <= n);

  if (length != n) {
    if (this->parent) {
      for (int i = length; i < n; i++) {
        SoNodeList::operator[](i)->removeAuditor(this->parent, SoNotRec::PARENT);
      }
      /* FIXME: shouldn't we move this startNotify() call to the end of
         the function?  pederb, 2002-10-02 */
      /* notify before truncation, so that the notification source gets
         the chance to operate on the child to be removed. 20100426
         tamer. */
      this->parent->startNotify();
      for (int k=0; k < this->auditors.getLength(); k++) {
        for (int j=n-1; j >= length; --j) {
          this->auditors[k]->removeIndex(this->parent, j);
        }
      }
    }
    SoNodeList::truncate(length);
  }
}

/*!
  Copy contents of \a cl into this list.

  Overloaded from parent to handle notification.

  \sa SbPList::copy()
*/
void
SoChildList::copy(const SoChildList & cl)
{
  if (this == &cl) return;

  // Call truncate() explicitly here to get the path notification.
  this->truncate(0);
  SoBaseList::copy(cl);

  // it's important to add parent as auditor for all nodes (this is
  // usually done in SoChildList::append/insert)
  if (this->parent) {
    for (int i = 0; i < this->getLength(); i++) {
      (*this)[i]->addAuditor(this->parent, SoNotRec::PARENT);
    }
    this->parent->startNotify();
  }
}

/*!
  \copydetails SbPList::set(const int index, void * item)

  Overloaded from parent to handle notification.

  \sa SbPList::set()
*/
void
SoChildList::set(const int index, SoNode * const node)
{
  // Overridden from superclass to handle notification.

#if OBOL_DEBUG && 0 // debug
  SoDebugError::postInfo("SoChildList::set",
                         "(%p) index=%d, node=%p, oldnode=%p",
                         this, index, node, (*this)[index]);
#endif // debug

  assert(index >= 0 && index < this->getLength());
  if (this->parent) {
    SoNodeList::operator[](index)->removeAuditor(this->parent, SoNotRec::PARENT);
    node->addAuditor(this->parent, SoNotRec::PARENT);
  }

  /* keep the node that is to be replaced around until after the
   * notifications have been sent */
  SoNode * prevchild = (SoNode *)this->get(index);
  prevchild->ref();

  SoBaseList::set(index, (SoBase *)node);

  // FIXME: shouldn't we move this startNotify() call to the end of
  // the function?  pederb, 2002-10-02
  /* notify before truncation, so that the notification source gets
     the chance to operate on the child to be removed. 20100426
     tamer. */
  if (this->parent) {
    this->parent->startNotify();
    for (int i=0; i < this->auditors.getLength(); i++) {
      this->auditors[i]->replaceIndex(this->parent, index, node);
    }
  }

  prevchild->unref();
}

/*!
  Optimized IN_PATH traversal method.

  This method is an extension versus the Open Inventor API.
*/
void
SoChildList::traverseInPath(SoAction * const action,
                            const int numindices,
                            const int * indices)
{
  assert(action->getCurPathCode() == SoAction::IN_PATH);

  // only traverse nodes in path list, and nodes off path that
  // affects state.
  int childidx = 0;

  for (int i = 0; i < numindices && !action->hasTerminated(); i++) {
    int stop = indices[i];
    for (; childidx < stop && !action->hasTerminated(); childidx++) {
      // we are off path. Check if node affects state before traversing
      SoNode * node = (*this)[childidx];
      if (node->affectsState()) {
        action->pushCurPath(childidx, node);
        action->traverse(node);
        action->popCurPath(SoAction::IN_PATH);
      }
    }

    if (!action->hasTerminated()) {
      // here we are in path. Always traverse
      SoNode * node = (*this)[childidx];
      action->pushCurPath(childidx, node);
      action->traverse(node);
      action->popCurPath(SoAction::IN_PATH);
      childidx++;
    }
  }
}

/*!
  Traverse child nodes in the list from index \a first up to and
  including index \a last, or until the SoAction::hasTerminated() flag
  of \a action has been set.
*/
void
SoChildList::traverse(SoAction * const action, const int first, const int last)
{
  int i;
  SoNode * node = NULL;

  assert((first >= 0) && (first < this->getLength()) && "index out of bounds");
  assert((last >= 0) && (last < this->getLength()) && "index out of bounds");
  assert((last >= first) && "erroneous indices");

#if OBOL_DEBUG
  // Calculate a checksum over the children node pointers, to later
  // catch attempts at changing the scene graph layout mid-traversal
  // with an assert. (chksum reversed to initial value and controlled
  // at the bottom end of this function.)
  //
  // Note: we might find this to be overly strict, because there are
  // cases where this will stop an unharmful attempt at changing the
  // current group node's set of children. But that's only if the
  // application programmer _really_, _really_ know what he is doing,
  // and it's still a slippery slope.. so "better safe than sorry" and
  // all that.
  //
  // mortene.
  uintptr_t chksum = 0xdeadbeef;
  for (i = first; i <= last; i++) { chksum ^= (uintptr_t)(*this)[i]; }
  SbBool changedetected = FALSE;
#endif // OBOL_DEBUG

  SoAction::PathCode pathcode = action->getCurPathCode();

  switch (pathcode) {
  case SoAction::NO_PATH:
  case SoAction::BELOW_PATH:
    // always traverse all nodes.
    action->pushCurPath();
    for (i = first; (i <= last) && !action->hasTerminated(); i++) {
#if OBOL_DEBUG
      if (i >= this->getLength()) {
        changedetected = TRUE;
        break;
      }
#endif // OBOL_DEBUG
      node = (*this)[i];
      action->popPushCurPath(i, node);
      action->traverse(node);
    }
    action->popCurPath();
    break;
  case SoAction::OFF_PATH:
    for (i = first; (i <= last) && !action->hasTerminated(); i++) {      
#if OBOL_DEBUG
      if (i >= this->getLength()) {
        changedetected = TRUE;
        break;
      }
#endif // OBOL_DEBUG
      node = (*this)[i];
      // only traverse nodes that affects state
      if (node->affectsState()) {
        action->pushCurPath(i, node);
        action->traverse(node);
        action->popCurPath(pathcode);
      }
    }
    break;
  case SoAction::IN_PATH:
    for (i = first; (i <= last) && !action->hasTerminated(); i++) {
#if OBOL_DEBUG
      if (i >= this->getLength()) {
        changedetected = TRUE;
        break;
      }
#endif // OBOL_DEBUG
      node = (*this)[i];
      action->pushCurPath(i, node);
      // if we're OFF_PATH after pushing, we only traverse if the node
      // affects the state.
      if ((action->getCurPathCode() != SoAction::OFF_PATH) ||
          node->affectsState()) {
        action->traverse(node);
      }
      action->popCurPath(pathcode);
    }
    break;
  default:
    assert(0 && "unknown path code.");
    break;
  }

#if OBOL_DEBUG
  if (!changedetected) {
    for (i = last; i >= first; i--) { chksum ^= (uintptr_t)(*this)[i]; }
    if (chksum != 0xdeadbeef) changedetected = TRUE;
  }
  if (changedetected) {
    SoDebugError::postWarning("SoChildList::traverse",
                              "Detected modification of scene graph layout "
                              "during action traversal. This is considered to "
                              "be hazardous and error prone, and we "
                              "strongly advice you to change your code "
                              "and/or reorganize your scene graph so that "
                              "this is not necessary.");
  }
#endif // OBOL_DEBUG
}

/*!
  Traverse all nodes in the list, invoking their methods for the given
  \a action.
*/
void
SoChildList::traverse(SoAction * const action)
{
  if (this->getLength() == 0) return;
  this->traverse(action, 0, this->getLength() - 1);
}

/*!
  Traverse the node at \a index (and possibly its children, if it is a
  group node), applying the node's method for the given \a action.
*/
void
SoChildList::traverse(SoAction * const action, const int index)
{
  assert((index >= 0) && (index < this->getLength()) && "index out of bounds");
  this->traverse(action, index, index);
}

/*!
  Traverse the \a node (and possibly its children, if it is a group
  node), applying the nodes method for the given \a action.
*/
void
SoChildList::traverse(SoAction * const action, SoNode * node)
{
  int idx = this->find(node);
  assert(idx != -1);
  this->traverse(action, idx);
}

/*!
  Notify \a path whenever this list of node children changes.
*/
void
SoChildList::addPathAuditor(SoPath * const path)
{
#if OBOL_DEBUG && 0 // debug
  SoDebugError::postInfo("SoChildList::addPathAuditor",
                         "add SoPath auditor %p to list %p", path, this);
#endif // debug

  this->auditors.append(path);
}

/*!
  Remove \a path as an auditor for our list of node children.
*/
void
SoChildList::removePathAuditor(SoPath * const path)
{
#if OBOL_DEBUG && 0 // debug
  SoDebugError::postInfo("SoChildList::removePathAuditor",
                         "remove SoPath auditor %p from list %p", path, this);
#endif // debug

  const int index = this->auditors.find(path);
#if OBOL_DEBUG
  if (index == -1) {
    SoDebugError::post("SoChildList::removePathAuditor",
                       "no SoPath %p is auditing list %p! (of parent %p (%s))",
                       static_cast<void *>(path),
                       static_cast<void *>(this),
                       static_cast<void *>(this->parent),
                       this->parent ? this->parent->getTypeId().getName().getString() : "<no type>");
    return;
  }
#endif // OBOL_DEBUG
  this->auditors.remove(index);
}
