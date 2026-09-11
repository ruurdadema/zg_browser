#pragma once

#include <juce_graphics/juce_graphics.h>

/** The app's colour palette, in one place so the components and the
  * ZGLookAndFeel can't drift apart.
  */
namespace zgb::theme
{

inline const juce::Colour background        {0xff1e1e22u};   ///< window / tree background
inline const juce::Colour contentBackground {0xff17171au};   ///< message pane, lists, text fields
inline const juce::Colour header            {0xff2a2a30u};   ///< the top bar
inline const juce::Colour border            {0xff3a3a42u};   ///< hairlines and control outlines

inline const juce::Colour control           {0xff33333cu};   ///< button face
inline const juce::Colour accent            {0xff2f5d8fu};   ///< selection, primary button
inline const juce::Colour overlay           {0xd0101013u};   ///< "disconnected" cover

inline const juce::Colour text              {0xfff0f0f4u};   ///< primary text
inline const juce::Colour textBody          {0xffd8d8dcu};   ///< monospaced dumps, editable text
inline const juce::Colour textDim           {0xff8e8e98u};   ///< secondary/annotation text

inline const juce::Colour connected         {0xff7bd88fu};
inline const juce::Colour disconnected      {0xffe0a04au};

}  // namespace zgb::theme
