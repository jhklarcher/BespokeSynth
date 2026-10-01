#include "ArrangerSnapshotSelection.h"

#include <iostream>
#include <vector>

namespace
{
   struct Clip
   {
      int lane;
      double start;
      double end;
      int snapshot;
   };

   int Resolve(const std::vector<Clip>& clips, int lane, double measure)
   {
      ArrangerSnapshotSelection selection(lane, measure);
      for (const auto& clip : clips)
         selection.Consider(clip.lane, clip.start, clip.end, clip.snapshot);
      return selection.GetSnapshotIndex();
   }

   bool Expect(int actual, int expected, const char* description)
   {
      if (actual == expected)
         return true;
      std::cerr << description << ": expected " << expected << ", got " << actual << '\n';
      return false;
   }
}

int main()
{
   const std::vector<Clip> clips{
      { 0, 4, 12, 1 },
      { 0, 8, 10, 2 },
      { 0, 8, 9, 3 }, // equal starts: later clip wins while both are active
      { 1, 0, 16, 4 },
      { 0, 20, 20, 5 }, // zero-length clips do not select anything
   };

   bool ok = true;
   ok &= Expect(Resolve(clips, 0, -1), -1, "negative position");
   ok &= Expect(Resolve(clips, 0, 0), -1, "before first clip");
   ok &= Expect(Resolve(clips, 0, 4), 1, "start boundary");
   ok &= Expect(Resolve(clips, 0, 8.5), 3, "overlap with equal starts");
   ok &= Expect(Resolve(clips, 0, 9), 2, "shorter overlap ended");
   ok &= Expect(Resolve(clips, 0, 10), 1, "latest active clip wins");
   ok &= Expect(Resolve(clips, 0, 12), 3, "gap holds latest started clip");
   ok &= Expect(Resolve(clips, 0, 20), 3, "zero-length clip ignored");
   ok &= Expect(Resolve(clips, 1, 8), 4, "lanes are independent");
   ok &= Expect(Resolve(clips, 2, 8), -1, "unarranged lane");
   ok &= Expect(Resolve(clips, 0, 4), 1, "backward seek or loop resolves from position");

   return ok ? 0 : 1;
}
