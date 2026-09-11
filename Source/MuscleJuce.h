#pragma once

#include <juce_core/juce_core.h>

#include "util/String.h"

/** Small helpers for living in both the muscle:: and juce:: worlds at once.
  * Note that this project never says "using namespace juce" -- muscle and JUCE
  * both define String, Message, Thread, ... so juce names are always qualified.
  */
namespace zgb
{

inline juce::String toJuce(const muscle::String & s)
{
   return juce::String::fromUTF8(s(), (int) s.Length());
}

inline muscle::String toMuscle(const juce::String & s)
{
   return muscle::String(s.toRawUTF8());
}

/** Returns the parent path of a session-relative node path ("a/b/c" -> "a/b", "a" -> ""). */
inline muscle::String parentPathOf(const muscle::String & nodePath)
{
   const int lastSlash = nodePath.LastIndexOf('/');
   return (lastSlash >= 0) ? nodePath.Substring(0, (uint32) lastSlash) : muscle::String();
}

/** Returns the final component of a session-relative node path ("a/b/c" -> "c"). */
inline muscle::String leafNameOf(const muscle::String & nodePath)
{
   const int lastSlash = nodePath.LastIndexOf('/');
   return (lastSlash >= 0) ? nodePath.Substring((uint32) lastSlash + 1) : nodePath;
}

/** Returns the subscription-string that asks for the direct children of (nodePath).
  * The root of the tree is the empty path, whose children are matched by "*".
  */
inline muscle::String childrenSubscriptionString(const muscle::String & nodePath)
{
   return nodePath.IsEmpty() ? muscle::String("*") : (nodePath + "/*");
}

}  // namespace zgb
