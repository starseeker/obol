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
  \class SoAuditorList SoAuditorList.h Inventor/lists/SoAuditorList.h
  \brief The SoAuditorList class is used to keep track of auditors for certain object classes.

  \ingroup coin_general

  This class is mainly for internal use (from SoBase) and it should
  not be necessary to be familiar with it for "ordinary" Coin use.
*/


#include <Inventor/fields/SoField.h>
#include <Inventor/fields/SoFieldContainer.h>
#include <Inventor/sensors/SoDataSensor.h>
#if OBOL_DEBUG
#include <Inventor/errors/SoDebugError.h>
#endif // OBOL_DEBUG

#include "config.h"

#include "threads/recmutexp.h"

#include <vector>
#include <utility>

/*!
  Default constructor.
*/
SoAuditorList::SoAuditorList(void)
  : SbPList(8)
{
}

/*!
  Destructor.
*/
SoAuditorList::~SoAuditorList()
{
}

/*!
  Append an \a auditor of \a type to the list.
*/
void
SoAuditorList::append(void * const auditor, const SoNotRec::Type type)
{
  const cc_notify_lock_guard lock;
  SbPList::append(auditor);
  SbPList::append((void *)type);
}

/*!
  Set \a auditor pointer and auditor \a type in list at \a index.
*/
void
SoAuditorList::set(const int index,
                   void * const auditor, const SoNotRec::Type type)
{
  const cc_notify_lock_guard lock;
  assert(index >= 0 && index < this->getLength());

  SbPList::set(index * 2, auditor);
  SbPList::set(index * 2 + 1, (void *)type);
}

/*!
  Returns number of elements in list.
*/
int
SoAuditorList::getLength(void) const
{
  return SbPList::getLength() / 2;
}

/*!
  Find \a auditor of \a type in list and return index. Returns -1 if
  \a auditor is not in the list.
*/
int
SoAuditorList::find(void * const auditor, const SoNotRec::Type type) const
{
  const int num = this->getLength();
  for (int i = 0; i < num; i++) {
    if (this->getObject(i) == auditor && this->getType(i) == type)
      return i;
  }
  return -1;
}

/*!
  Returns auditor pointer at \a index.
*/
void *
SoAuditorList::getObject(const int index) const
{
  return SbPList::operator[](index * 2);
}

/*!
  Returns auditor type at \a index.
*/
SoNotRec::Type
SoAuditorList::getType(const int index) const
{
  const uintptr_t tmp = (uintptr_t)(SbPList::operator[](index*2+1));
  return (SoNotRec::Type)tmp;
}

/*!
  Remove auditor at \a index.
*/
void
SoAuditorList::remove(const int index)
{
  const cc_notify_lock_guard lock;
  assert(index >= 0 && index < this->getLength());
  SbPList::remove(index * 2); // ptr
  SbPList::remove(index * 2); // type
}

/*!
  Remove \a auditor of \a type from list.
*/
void
SoAuditorList::remove(void * const auditor, const SoNotRec::Type type)
{
  this->remove(this->find(auditor, type));
}

/*!
  Send notification to all our auditors.

  A true snapshot of the auditor list is taken under the notify lock
  before any callbacks are invoked.  This lets callbacks safely add or
  remove auditors mid-delivery without invalidating the iteration or
  triggering undefined behaviour.
*/
void
SoAuditorList::notify(SoNotList * l)
{
  std::vector<std::pair<void *, SoNotRec::Type>> snap;
  {
    const cc_notify_lock_guard lock;
    const int count = this->getLength();
    snap.reserve(static_cast<size_t>(count));
    for (int i = 0; i < count; i++)
      snap.emplace_back(this->getObject(i), this->getType(i));
  }
  const int num = static_cast<int>(snap.size());

  if (num == 1) {
    this->doNotify(l, snap[0].first, snap[0].second);
  }
  else if (num > 1) {
    // FIXME: should perhaps use a more general mechanism to detect when
    // to ignore notification? (In SoFieldContainer::notify() -- based
    // on SoNotList::getTimeStamp()?) 20000304 mortene.
    SbPList notified(num);
    for (auto & entry : snap) {
      if (notified.find(entry.first) == -1) {
        // use a copy of 'l', since the notification list might change
        // when auditors are notified
        SoNotList listcopy(l);
        this->doNotify(&listcopy, entry.first, entry.second);
        notified.append(entry.first);
      }
    }
  }
}

//
// Private method used to propagate 'l' to the 'auditor' of type 'type'
//
void
SoAuditorList::doNotify(SoNotList * l, const void * auditor, const SoNotRec::Type type)
{
  l->setLastType(type);

  switch (type) {
  case SoNotRec::CONTAINER:
  case SoNotRec::PARENT:
    {
      SoFieldContainer * obj = const_cast<SoFieldContainer*>(static_cast<const SoFieldContainer*>(auditor));
      obj->notify(l);
    }
    break;

  case SoNotRec::SENSOR:
    {
      SoDataSensor * obj = const_cast<SoDataSensor*>(static_cast<const SoDataSensor*>(auditor));
      // don't schedule the sensor here. The sensor instance will do
      // that in notify() (it might also choose _not_ to schedule),
      obj->notify(l);
    }
    break;

  case SoNotRec::FIELD:
  case SoNotRec::ENGINE:
    {
      // We used to check whether or not the fields was already
      // dirty before we transmitted the notification
      // message. This is _not_ correct (the dirty flag is
      // conceptually only relevant for whether or not to do
      // re-evaluation), so don't try to "optimize" the
      // notification mechanism by re-introducing that "feature".
      // :^/
      const_cast<SoField*>(static_cast<const SoField*>(auditor))->notify(l);
    }
    break;

  default:
    assert(0 && "Unknown auditor type");
  }
}
