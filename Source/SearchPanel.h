#pragma once

#include <functional>
#include <vector>

#include <juce_gui_basics/juce_gui_basics.h>

#include "MuscleJuce.h"

#include "message/Message.h"

/** Far-right pane:  searches the server's database for nodes whose path matches
  * a wildcard pattern, and lists what came back.
  *
  * The results are self-contained -- each row keeps its own payload -- so a hit
  * can be shown in the Message pane even when its node isn't in the visible
  * tree (the tree only holds the part of the database that is open on screen).
  */
class SearchPanel final : public juce::Component,
                          public juce::ListBoxModel
{
public:
   SearchPanel();

   /** Called with the text the user wants to search for. */
   std::function<void(const juce::String &)> onSearchRequested;

   /** Called when a result row is clicked (show it in the Message pane). */
   std::function<void(const muscle::String &, const muscle::ConstMessageRef &)> onResultClicked;

   /** Called when a result row is double-clicked (reveal it in the tree). */
   std::function<void(const muscle::String &)> onResultDoubleClicked;

   /** Called when the user drops the result selection -- by clicking the empty
     * part of the list, or by clearing the search while a result was selected.
     */
   std::function<void()> onResultDeselected;

   /** Called when the user clears the search (any search still in flight should be forgotten). */
   std::function<void()> onSearchCleared;

   /** Replaces the result list.  Paths are session-relative. */
   void setResults(const std::vector<std::pair<muscle::String, muscle::ConstMessageRef> > & results);

   /** Drops the results and any in-progress state (eg on disconnect). */
   void clear();

   /** Drops the row highlight, without touching the results themselves.
     * Called when the selection moves to the tree, so the highlight stops
     * claiming that the Message pane is showing this row.
     */
   void deselectAll();

   /** Greys out the controls while we're not connected to anything. */
   void setConnected(bool isConnected);

   void resized() override;
   void paint(juce::Graphics & g) override;
   void paintOverChildren(juce::Graphics & g) override;

   // ListBoxModel
   int getNumRows() override;
   void paintListBoxItem(int rowNumber, juce::Graphics & g, int width, int height, bool rowIsSelected) override;
   void listBoxItemClicked(int rowNumber, const juce::MouseEvent & e) override;
   void listBoxItemDoubleClicked(int rowNumber, const juce::MouseEvent & e) override;
   void backgroundClicked(const juce::MouseEvent & e) override;

private:
   void startSearch();
   void clearSearch();

   struct ResultRow
   {
      muscle::String _path;
      muscle::ConstMessageRef _payload;
   };

   std::vector<ResultRow> _rows;
   bool _searchPending = false;
   juce::String _emptyMessage {"Type a pattern and press Search."};

   juce::Label _titleLabel;
   juce::TextEditor _searchText;
   juce::TextButton _searchButton {"Search"};
   juce::TextButton _clearButton {juce::String::fromUTF8("\xc3\x97")};   // U+00D7 MULTIPLICATION SIGN, spelled out so no compiler can mis-decode it
   juce::Label _hintLabel;
   juce::ListBox _listBox {"results", this};

   JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SearchPanel)
};
