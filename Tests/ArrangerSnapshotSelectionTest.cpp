#include "ArrangerSnapshotSelection.h"

#include <algorithm>
#include <iostream>
#include <limits>
#include <vector>

namespace
{
   struct Clip
   {
      int lane;
      double start;
      double end;
      int snapshot;
      bool available{ true };
   };

   int Resolve(const std::vector<Clip>& clips, int lane, double measure)
   {
      ArrangerSnapshotSelection selection(lane, measure);
      for (const auto& clip : clips)
         selection.Consider(clip.lane, clip.start, clip.end, clip.snapshot, clip.available);
      return selection.GetSnapshotIndex();
   }

   // Replay starts and ends in time order. A new winner applies its snapshot;
   // with no active clip, playback keeps the last applied snapshot.
   int PlayThrough(const std::vector<Clip>& clips, int lane, double measure)
   {
      if (measure < 0)
         return -1;

      std::vector<double> boundaries{ measure };
      for (const auto& clip : clips)
      {
         if (clip.lane != lane || !clip.available || clip.end <= clip.start)
            continue;
         if (clip.start <= measure)
            boundaries.push_back(clip.start);
         if (clip.end <= measure)
            boundaries.push_back(clip.end);
      }
      std::sort(boundaries.begin(), boundaries.end());
      boundaries.erase(std::unique(boundaries.begin(), boundaries.end()), boundaries.end());

      int selected = -1;
      for (double position : boundaries)
      {
         double latestStart = -std::numeric_limits<double>::infinity();
         int active = -1;
         for (const auto& clip : clips)
         {
            if (clip.lane == lane && clip.available && clip.start <= position && position < clip.end && clip.start >= latestStart)
            {
               latestStart = clip.start;
               active = clip.snapshot;
            }
         }
         if (active != -1)
            selected = active;
      }
      return selected;
   }

   bool Expect(int actual, int expected, const char* description)
   {
      if (actual == expected)
         return true;
      std::cerr << description << ": expected " << expected << ", got " << actual << '\n';
      return false;
   }

   bool ExpectReplay(const std::vector<Clip>& clips, int lane, double measure)
   {
      const int actual = Resolve(clips, lane, measure);
      const int expected = PlayThrough(clips, lane, measure);
      if (actual == expected)
         return true;
      std::cerr << "seek at measure " << measure << ": replay selected " << expected << ", resolver selected " << actual << '\n';
      return false;
   }
}

int main()
{
   const std::vector<Clip> clips{
      { 0, 4, 12, 1 },
      { 0, 8, 10, 2 },
      { 0, 8, 9, 3 }, // equal starts: later clip wins until its end
      { 1, 0, 16, 4 },
      { 0, 20, 20, 5 }, // zero-length clips are inert
   };

   bool ok = true;
   ok &= Expect(Resolve(clips, 0, -1), -1, "negative position");
   ok &= Expect(Resolve(clips, 0, 0), -1, "before first clip");
   ok &= Expect(Resolve(clips, 0, 4), 1, "start is inclusive");
   ok &= Expect(Resolve(clips, 0, 8.5), 3, "later equal-start clip wins overlap");
   ok &= Expect(Resolve(clips, 0, 9), 2, "shorter clip ends and next overlap resumes");
   ok &= Expect(Resolve(clips, 0, 10), 1, "remaining underlying clip resumes");
   ok &= Expect(Resolve(clips, 0, 12), 1, "end is exclusive; gap holds last active clip");
   ok &= Expect(Resolve(clips, 0, 20), 1, "zero-length clip does not replace held state");
   ok &= Expect(Resolve(clips, 1, 8), 4, "lanes are independent");
   ok &= Expect(Resolve(clips, 2, 8), -1, "unarranged lane");

   // Query out of playback order: a seek or loop must match sequential playback.
   for (double measure : { 20.0, 12.0, 8.5, 10.0, 9.0, 4.0, 0.0, 11.5, 8.0 })
      ok &= ExpectReplay(clips, 0, measure);

   const std::vector<Clip> sharedEnds{
      { 0, 0, 6, 10 },
      { 0, 2, 6, 11 },
      { 0, 7, 9, 12 },
      { 0, 7, 9, 13 }, // same start and end: later element wins
      { 0, 9, 11, 14 }, // adjacent start replaces held state at the boundary
   };
   ok &= Expect(Resolve(sharedEnds, 0, 6), 11, "simultaneous ends hold last active clip");
   ok &= Expect(Resolve(sharedEnds, 0, 6.5), 11, "gap after simultaneous ends");
   ok &= Expect(Resolve(sharedEnds, 0, 7), 13, "later element wins equal start");
   ok &= Expect(Resolve(sharedEnds, 0, 9), 14, "adjacent clip starts at end boundary");
   ok &= Expect(Resolve(sharedEnds, 0, 11), 14, "final gap holds adjacent clip");
   ok &= Expect(Resolve({ { 0, 7, 9, 12 }, { 0, 7, 9, 13 } }, 0, 9), 13, "equal start and end hold later element in gap");
   for (double measure : { 11.0, 6.5, 0.0, 9.0, 7.5, 2.0, 6.0 })
      ok &= ExpectReplay(sharedEnds, 0, measure);

   const std::vector<Clip> missing{
      { 0, 0, 10, 10 },
      { 0, 4, 8, 11, false },
      { 0, 6, 7, 12 },
   };
   ok &= Expect(Resolve(missing, 0, 5), 10, "missing overlapping snapshot is inert");
   ok &= Expect(Resolve(missing, 0, 6), 12, "valid clip over missing clip applies");
   ok &= Expect(Resolve(missing, 0, 7), 10, "underlying valid clip resumes");
   ok &= Expect(Resolve(missing, 0, 10), 10, "gap holds last valid snapshot");
   ok &= Expect(Resolve({ { 0, 4, 8, 11, false } }, 0, 9), -1, "only missing clip leaves patch unarranged");
   for (double measure : { 10.0, 5.0, 7.0, 6.0, 4.0 })
      ok &= ExpectReplay(missing, 0, measure);

   return ok ? 0 : 1;
}
