#include "SearchPanel.h"

#include "Theme.h"

using namespace muscle;

SearchPanel :: SearchPanel()
{
   _titleLabel.setText("Search", juce::dontSendNotification);
   _titleLabel.setFont(juce::Font(juce::FontOptions(15.0f, juce::Font::bold)));
   addAndMakeVisible(_titleLabel);

   _searchText.setFont(juce::Font(juce::FontOptions(13.5f)));
   _searchText.setIndents(8, 5);
   _searchText.setTextToShowWhenEmpty("node name (wildcards allowed, eg fo*)", zgb::theme::textDim.darker(0.3f));
   _searchText.onReturnKey = [this] {startSearch();};
   _searchText.onEscapeKey = [this] {clearSearch();};
   addAndMakeVisible(_searchText);

   _searchButton.setColour(juce::TextButton::buttonColourId, zgb::theme::accent);
   _searchButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
   _searchButton.onClick = [this] {startSearch();};
   addAndMakeVisible(_searchButton);

   _clearButton.setTooltip("Clear the search (Esc)");
   _clearButton.onClick = [this] {clearSearch();};
   addAndMakeVisible(_clearButton);

   // Path wildcards have no case-insensitive form (that only exists for payload
   // filters), so say so rather than let the user wonder why "Foo" missed "foo".
   _hintLabel.setText("Matches paths, case-sensitively.", juce::dontSendNotification);
   _hintLabel.setFont(juce::Font(juce::FontOptions(11.5f)));
   _hintLabel.setColour(juce::Label::textColourId, zgb::theme::textDim);
   addAndMakeVisible(_hintLabel);

   _listBox.setRowHeight(20);
   addAndMakeVisible(_listBox);
}

void SearchPanel :: startSearch()
{
   const juce::String text = _searchText.getText().trim();
   if (text.isEmpty()) return;

   _rows.clear();
   _listBox.updateContent();

   _searchPending = true;
   _emptyMessage = "Searching...";
   repaint();

   if (onSearchRequested) onSearchRequested(text);
}

void SearchPanel :: clearSearch()
{
   const bool resultWasShown = (_listBox.getNumSelectedRows() > 0);

   _searchText.clear();
   clear();

   if (onSearchCleared) onSearchCleared();
   if ((resultWasShown)&&(onResultDeselected)) onResultDeselected();  // the Message pane was showing a result we just dropped
}

void SearchPanel :: setResults(const std::vector<std::pair<String, ConstMessageRef> > & results)
{
   _searchPending = false;

   _rows.clear();
   _rows.reserve(results.size());
   for (const auto & r : results) _rows.push_back(ResultRow{r.first, r.second});

   _emptyMessage = "No matching nodes.";
   _listBox.updateContent();
   _listBox.deselectAllRows();
   repaint();
}

void SearchPanel :: clear()
{
   _searchPending = false;
   _rows.clear();
   _emptyMessage = "Type a pattern and press Search.";
   _listBox.updateContent();
   _listBox.deselectAllRows();
   repaint();
}

void SearchPanel :: deselectAll()
{
   _listBox.deselectAllRows();
}

void SearchPanel :: setConnected(bool isConnected)
{
   _searchText.setEnabled(isConnected);
   _searchButton.setEnabled(isConnected);
   _clearButton.setEnabled(isConnected);
}

int SearchPanel :: getNumRows()
{
   return (int) _rows.size();
}

void SearchPanel :: paintListBoxItem(int rowNumber, juce::Graphics & g, int width, int height, bool rowIsSelected)
{
   if ((rowNumber < 0)||(rowNumber >= (int) _rows.size())) return;
   const ResultRow & row = _rows[(size_t) rowNumber];

   if (rowIsSelected) g.fillAll(zgb::theme::accent);

   g.setColour(rowIsSelected ? juce::Colours::white : zgb::theme::textBody);
   g.setFont(juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 12.0f, juce::Font::plain)));

   // Right-justify the trim so a long path keeps its leaf name visible -- that's
   // the part the user was searching for.
   g.drawText(zgb::toJuce(row._path.WithPrepend("/")), 8, 0, width-16, height, juce::Justification::centredLeft, false);
}

// Note this is listBoxItemClicked() rather than selectedRowsChanged():  the
// latter only fires when the selection actually *changes*, so clicking the
// already-highlighted row -- to bring its Message back after the tree stole the
// pane -- would do nothing at all.
void SearchPanel :: listBoxItemClicked(int rowNumber, const juce::MouseEvent &)
{
   if ((rowNumber < 0)||(rowNumber >= (int) _rows.size())) return;

   _listBox.selectRow(rowNumber, true, true);

   const ResultRow & row = _rows[(size_t) rowNumber];
   if (onResultClicked) onResultClicked(row._path, row._payload);
}

void SearchPanel :: listBoxItemDoubleClicked(int rowNumber, const juce::MouseEvent &)
{
   if ((rowNumber < 0)||(rowNumber >= (int) _rows.size())) return;

   const ResultRow & row = _rows[(size_t) rowNumber];
   if (onResultDoubleClicked) onResultDoubleClicked(row._path);
}

// Only called for clicks below the last row:  the rows handle their own clicks.
void SearchPanel :: backgroundClicked(const juce::MouseEvent &)
{
   // With no row highlighted the Message pane isn't ours (it's the tree's, or
   // empty), so there's nothing to deselect and nothing we should clear.
   if (_listBox.getNumSelectedRows() == 0) return;

   _listBox.deselectAllRows();
   if (onResultDeselected) onResultDeselected();
}

void SearchPanel :: paint(juce::Graphics & g)
{
   g.fillAll(zgb::theme::background);
}

// The status message ("Searching...", "No matching nodes.") has to be drawn
// over our children:  underneath, the list box's opaque background hides it.
void SearchPanel :: paintOverChildren(juce::Graphics & g)
{
   if (_rows.empty())
   {
      g.setColour(zgb::theme::textDim);
      g.setFont(juce::Font(juce::FontOptions(13.0f)));
      g.drawFittedText(_emptyMessage, _listBox.getBounds().reduced(10), juce::Justification::centred, 3);
   }
}

void SearchPanel :: resized()
{
   auto r = getLocalBounds().reduced(8, 6);

   _titleLabel.setBounds(r.removeFromTop(22));
   r.removeFromTop(4);

   auto searchRow = r.removeFromTop(26);
   _searchButton.setBounds(searchRow.removeFromRight(74));
   searchRow.removeFromRight(4);
   _clearButton.setBounds(searchRow.removeFromRight(26));
   searchRow.removeFromRight(6);
   _searchText.setBounds(searchRow);

   _hintLabel.setBounds(r.removeFromTop(18));
   r.removeFromTop(4);

   _listBox.setBounds(r);
}
