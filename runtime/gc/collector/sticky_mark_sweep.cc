/*
 * Copyright (C) 2012 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "gc/heap.h"
#include "gc/space/large_object_space.h"
#include "gc/space/space.h"
#include "sticky_mark_sweep.h"
#include "thread.h"

namespace art {
namespace gc {
namespace collector {

StickyMarkSweep::StickyMarkSweep(Heap* heap, bool is_concurrent, const std::string& name_prefix)
    : PartialMarkSweep(heap, is_concurrent,
                       name_prefix + (name_prefix.empty() ? "" : " ") + "sticky") {
  cumulative_timings_.SetName(GetName());
}

void StickyMarkSweep::BindBitmaps() {
  PartialMarkSweep::BindBitmaps();

  const std::vector<space::ContinuousSpace*>& spaces = GetHeap()->GetContinuousSpaces();
  WriterMutexLock mu(Thread::Current(), *Locks::heap_bitmap_lock_);
  // For sticky GC, we want to bind the bitmaps of all spaces as the allocation stack lets us
  // know what was allocated since the last GC. A side-effect of binding the allocation space mark
  // and live bitmap is that marking the objects will place them in the live bitmap.
  // TODO: C++0x
  typedef std::vector<space::ContinuousSpace*>::const_iterator It;
  for (It it = spaces.begin(), end = spaces.end(); it != end; ++it) {
    space::ContinuousSpace* space = *it;
    if (space->GetGcRetentionPolicy() == space::kGcRetentionPolicyAlwaysCollect) {
      BindLiveToMarkBitmap(space);
    }
  }

  GetHeap()->GetLargeObjectsSpace()->CopyLiveToMarked();
}

void StickyMarkSweep::MarkReachableObjects() {
  DisableFinger();
  RecursiveMarkDirtyObjects(accounting::CardTable::kCardDirty - 1);
}

void StickyMarkSweep::Sweep(bool swap_bitmaps) {
  timings_.NewSplit("SweepArray");
  accounting::ObjectStack* live_stack = GetHeap()->GetLiveStack();
  SweepArray(live_stack, false);
}

}  // namespace collector
}  // namespace gc
}  // namespace art