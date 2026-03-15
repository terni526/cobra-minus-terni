#include "IDEFrame.h"

#include "CobraWidgetID.h"
#include "FindAndReplaceDialog.h"

#include <wx/wx.h>
#include <wx/log.h>

#include <wx/stc/stc.h>

#include <wx/tokenzr.h>

#include <wx/textfile.h> // Include header for wxTextFile
#include <wx/txtstrm.h>

#include <wx/msgdlg.h>   // For showing messages (like errors)
#include <wx/fontdlg.h>

#include <wx/sstream.h>

#include <wx/dir.h>
#include <wx/treectrl.h>

#include <wx/wfstream.h>

#include <iostream>
#include <fstream>

#include <filesystem>
#include <cstdio>

#include <cstdlib> // Required for system()
#include <string>  // Required for std::string

#include <sstream>
#include <vector>

#include <windows.h>

#include <regex>

#include "external/json.hpp"

using json = nlohmann::json;

constexpr auto COBRA_DARK_THEME = "Cobra - Dark Theme";
constexpr auto COBRA_LIGHT_THEME = "Cobra - Light Theme";

wxBEGIN_EVENT_TABLE(IDEFrame, wxFrame)
    EVT_MENU(wxID_NEW, IDEFrame::createNewFile)
    EVT_MENU(wxID_OPEN, IDEFrame::openNewFile)

    EVT_MENU(wxID_SAVE, IDEFrame::saveFile)
    EVT_MENU(wxID_SAVEAS, IDEFrame::saveAsFile)

    EVT_MENU(wxID_FIND, IDEFrame::findGivenText)
    EVT_MENU(STOP_FINDING_MENUITEM_ID, IDEFrame::stopFinding)

    EVT_MENU(wxID_REPLACE, IDEFrame::replaceFirstReferenceWithGivenText)
    EVT_MENU(wxID_REPLACE_ALL, IDEFrame::replaceAllWithGivenText)

    EVT_MENU(wxID_CLOSE, IDEFrame::closeApplication)

    EVT_MENU(wxID_CUT, IDEFrame::cutSelectedText)

    EVT_MENU(wxID_COPY, IDEFrame::copySelectedText)
    EVT_MENU(wxID_PASTE, IDEFrame::pasteText)

    EVT_MENU(wxID_DELETE, IDEFrame::deleteSelectedText)

    EVT_MENU(wxID_SELECTALL, IDEFrame::selectAllText)

    EVT_MENU(SET_DARK_THEME_MENUITEM_ID, IDEFrame::setDarkTheme)
    EVT_MENU(SET_LIGHT_THEME_MENUITEM_ID, IDEFrame::setLightTheme)

    EVT_MENU(SET_FONT_MENUITEM_ID, IDEFrame::setFont)

    EVT_MENU(RUN_CODE_MENUITEM_ID, IDEFrame::runPythonCode)

    EVT_TREE_ITEM_ACTIVATED(FILE_TREE_IDE_ID, IDEFrame::onFileTreeItemSelected)
wxEND_EVENT_TABLE();

void IDEFrame::fullyTerminateApplication()
{
    Destroy();
    wxTheApp->Exit();
}

void IDEFrame::confirmExit(wxCloseEvent& event)
{
    wxMessageDialog dialog(this,
        "Are you sure you want to exit?",
        "Confirm Exit",
        wxYES_NO | wxICON_QUESTION);

    if (dialog.ShowModal() == wxID_YES)
    {
        // User confirmed - allow the window to close
        fullyTerminateApplication();  // or Destroy() to force immediate closing
    }
    else
    {
        // User canceled - veto the close event
        event.Veto();
    }
}

void IDEFrame::createNewFile(wxCommandEvent& commandEvent)
{
    std::cout << "CREATING NEW FILE." << "\n";

    wxFileDialog openFileDialog(this, _("Create New File"), "", "",
        "Python Files (*.py)|*.py|All Files (*.*)|*.*",
        wxFD_OPEN | wxFD_OVERWRITE_PROMPT);

    if (openFileDialog.ShowModal() == wxID_CANCEL)
        return;

    wxString filePath = openFileDialog.GetPath();

    std::ofstream fileToCreate(filePath.ToStdString());

    if (fileToCreate.is_open())
    {
        fileToCreate << "";
        fileToCreate.close();
    }
}

void IDEFrame::openNewFile(wxCommandEvent& commandEvent)
{
    std::cout << "OPENING FILE." << "\n";

    wxFileDialog openFileDialog(this, _("Open File"), "", "", 
        "Python Files (*.py)|*.py|All Files (*.*)|*.*", 
        wxFD_OPEN | wxFD_OVERWRITE_PROMPT);

    if (openFileDialog.ShowModal() == wxID_CANCEL)
        return;

    wxString filePath = openFileDialog.GetPath();

    std::ifstream fileToOpen(filePath.ToStdString());

    if (fileToOpen.is_open())
    {
        std::string textToOutputToFileToOpen;

        std::string line;
        while (std::getline(fileToOpen, line)) 
        {
            textToOutputToFileToOpen += line + "\n";
        }

        codeBody->ClearAll();
        codeBody->SetText(textToOutputToFileToOpen);
    }
}

std::string IDEFrame::removeHalfChar(std::string str, char target) 
{
    // Count how many times target appears
    int count = 0;
    for (char c : str) {
        if (c == target) count++;
    }

    // Determine how many to remove (half of total count)
    int toRemove = count / 2;
    std::string result;

    for (char c : str) {
        if (c == target && toRemove > 0) {
            toRemove--; // skip this character
        }
        else {
            result += c; // keep the character
        }
    }

    return result;
}

std::string IDEFrame::replaceTabsWithSpaces(const std::string& input, int spacesPerTab) 
{
    std::string spaces(spacesPerTab, ' ');
    return std::regex_replace(input, std::regex("\t"), spaces);
}

std::string normalizeIndentationToTabs(const std::string& input, int tabWidth = 4) 
{
    std::istringstream iss(input);
    std::ostringstream oss;
    std::string line;

    while (std::getline(iss, line)) {
        size_t leadingSpaces = 0;
        while (leadingSpaces < line.size() && line[leadingSpaces] == ' ')
            ++leadingSpaces;

        size_t numTabs = leadingSpaces / tabWidth;
        size_t numSpaces = leadingSpaces % tabWidth;

        std::string newIndent(numTabs, '\t');
        newIndent.append(numSpaces, ' '); // Leftover spaces, if any

        line = newIndent + line.substr(leadingSpaces);
        oss << line << '\n';
    }

    return oss.str();
}

wxString getInstalledPythonModules()
{
    wxString output;
    wxArrayString result;

    long exitCode = wxExecute("python -c \"import pkgutil; print(' '.join([m.name for m in pkgutil.iter_modules()]))\"", result);

    if (exitCode == 0)
    {
        for (const auto& line : result)
        {
            output += line + " ";
        }
    }
    else
    {
        wxLogError("Failed to retrieve installed Python modules.");
    }

    return output;
}

void IDEFrame::saveFile(wxCommandEvent& commandEvent)
{
    std::cout << "SAVING FILE." << "\n";

    std::ofstream mainPythonFile(projectPath.ToStdString() + "\\main.py");

    if (mainPythonFile.is_open())
    {
        std::string text = codeBody->GetText().ToStdString();

        // Normalize line endings
        text = std::regex_replace(text, std::regex("\r\n|\r"), "\n");

        // Collapse multiple blank lines to max 1
        text = std::regex_replace(text, std::regex("\n{3,}"), "\n\n");

        // Normalize indentation to tabs
        text = normalizeIndentationToTabs(text, 4);

        mainPythonFile << text;
        mainPythonFile.close();

        fileToSaveHasBeenPicked = true;
        fileIsSaved = true;
    }
}

void IDEFrame::saveAsFile(wxCommandEvent& commandEvent)
{
    std::cout << "SAVING AS FILE." << "\n";

    wxFileDialog saveFileDialog(this, _("Save File"), "", "",
        "Python Files (*.py)|*.py|All Files (*.*)|*.*",
        wxFD_SAVE | wxFD_OVERWRITE_PROMPT);

    fileToSaveHasBeenPicked = true;

    if (saveFileDialog.ShowModal() == wxID_CANCEL)
        return; // User canceled the dialog

    // Get the selected file path
    wxString filePath = saveFileDialog.GetPath();

    // Save your data to the file
    wxFileOutputStream outputStream(filePath);
    if (!outputStream.IsOk()) {
        wxLogError("Cannot save file '%s'.", filePath);
        return;
    }

    // Write data to the file (example)
    wxTextOutputStream textStream(outputStream);
    textStream << codeBody->GetText().ToStdString();

    fileIsSaved = true;
}

void IDEFrame::closeApplication(wxCommandEvent& commandEvent)
{
    exit(0);
}

void IDEFrame::cutSelectedText(wxCommandEvent& commandEvent)
{
    codeBody->Cut();
}

void IDEFrame::copySelectedText(wxCommandEvent& commandEvent)
{
    codeBody->Copy();
}

void IDEFrame::pasteText(wxCommandEvent& commandEvent)
{
    codeBody->Paste();
}

void IDEFrame::deleteSelectedText(wxCommandEvent& commandEvent)
{
    codeBody->Clear();
}

void IDEFrame::selectAllText(wxCommandEvent& commandEvent)
{
    codeBody->SelectAll();
}

wxString IDEFrame::getInformationForFinding()
{
    return wxGetTextFromUser("Find: ", "Find");
}

void IDEFrame::findGivenText(wxCommandEvent& commandEvent)
{
    FindAndReplaceDialog* dialog = new FindAndReplaceDialog();

    int flags = 0;

    if (dialog->ShowModal() == wxID_OK)
    {
        if (dialog->getSearchText() == "")
        {
            wxMessageBox("Please type your word to search.", "Text Required", wxOK | wxICON_INFORMATION, this);
            return;
        }

        searchText = dialog->getSearchText();
         
        isFinding = true;

        int indicator = 0;

        codeBody->SetIndicatorCurrent(indicator);
        codeBody->IndicatorClearRange(0, codeBody->GetTextLength());

        // Set the indicator style (for highlighting, optional)
        codeBody->IndicatorSetStyle(indicator, wxSTC_INDIC_BOX);
        codeBody->IndicatorSetForeground(indicator, *wxYELLOW);

        if (dialog->hasMatchCase() && !dialog->hasMatchWholeWord())
        {
            flags = wxSTC_FIND_MATCHCASE;
        }

        if (!dialog->hasMatchCase() && dialog->hasMatchWholeWord())
        {
            flags = wxSTC_FIND_WHOLEWORD;
        }

        if (dialog->hasMatchCase() && dialog->hasMatchWholeWord())
        {
            flags = wxSTC_FIND_MATCHCASE | wxSTC_FIND_WHOLEWORD;
        }

        int start = 0;
        int end = codeBody->GetTextLength();

        while (start < end)
        {
            int pos = codeBody->FindText(start, end, searchText, flags);

            if (pos == -1)
                break; // No more matches

            // Optional: Highlight the match
            codeBody->IndicatorFillRange(pos, searchText.length());

            // Move start beyond the current match
            start = pos + searchText.length();
        }
    }

    dialog->Destroy();
}

wxString IDEFrame::getTextToReplaceReferences()
{
    OutputDebugStringA("GETTING TEXT FROM USER.");
    return wxGetTextFromUser("Replace with: ", "Replace");
}

void IDEFrame::replaceFirstReferenceWithGivenText(wxCommandEvent& commandEvent)
{
    if (!isFinding)
    {
        wxMessageBox("You must be finding text in order to replace the references.", "Not Finding", wxOK | wxICON_INFORMATION);
        return;
    }

    OutputDebugStringA("\nsearchText: " + searchText + "\n");

    wxString textToReplace = codeBody->GetText();
    wxString replacerText = getTextToReplaceReferences();

    bool dialogCancelled = replacerText == "";

    if (dialogCancelled)
    {
        return;
    }

    size_t pos = textToReplace.find(searchText);

    if (pos != std::string::npos)
    {
        textToReplace.replace(pos, searchText.length(), replacerText);
    }

    codeBody->SetText(textToReplace);

    isFinding = false;
}

void IDEFrame::replaceAllWithGivenText(wxCommandEvent& commandEvent)
{
    if (!isFinding)
    {
        wxMessageBox("You must be finding text in order to replace the references.", "Not Finding", wxOK | wxICON_INFORMATION);
        return;
    }

    wxString textToReplace = codeBody->GetText();
    wxString replacerText = getTextToReplaceReferences();

    bool dialogCancelled = replacerText == "";

    if (dialogCancelled)
    {
        return;
    }

    size_t start_pos = 0;

    while ((start_pos = textToReplace.find(searchText, start_pos)) != std::string::npos)
    {
        textToReplace.replace(start_pos, searchText.length(), replacerText);
        start_pos += replacerText.length();
    }

    codeBody->SetText(textToReplace);

    isFinding = false;
}

void IDEFrame::stopFinding(wxCommandEvent& commandEvent)
{
    // Unbind(wxEVT_TEXT, &IDEFrame::findAndReplace, this);

    int indicator = 0;

    codeBody->SetIndicatorCurrent(indicator);
    codeBody->IndicatorClearRange(0, codeBody->GetTextLength());

    isFinding = false;
}

void IDEFrame::populateProjectFileTreeWithFiles(wxTreeCtrl* tree, wxTreeItemId parent, const wxString& path)
{
    wxDir dir(path);
    if (!dir.IsOpened()) return;

    wxString filename;
    bool cont = dir.GetFirst(&filename);
    while (cont) {
        wxTreeItemId item = tree->AppendItem(parent, filename);
        wxString subPath = path + "/" + filename;
        if (wxDir::Exists(subPath)) { // Check if it's a directory
            populateProjectFileTreeWithFiles(tree, item, subPath); // Recursively populate
        }
        cont = dir.GetNext(&filename);
    }
}

void replaceLineInFile(const std::string& filename, const std::string& searchSubstring, const std::string& replacementLine) 
{
    std::ifstream settingsFile(filename);
    if (!settingsFile.is_open()) {
        std::cerr << "Failed to open " << filename << std::endl;
        return;
    }

    std::string tempFilename = filename + ".tmp";
    std::ofstream outputFile(tempFilename);
    if (!outputFile.is_open()) {
        std::cerr << "Failed to create temporary file." << std::endl;
        return;
    }

    std::string line;
    while (std::getline(settingsFile, line)) {
        if (line.find(searchSubstring) != std::string::npos) {
            outputFile << replacementLine << '\n';
        }
        else {
            outputFile << line << '\n';
        }
    }

    settingsFile.close();
    outputFile.close();

    // Replace the original file with the temporary file
    remove(filename.c_str());
    std::rename(tempFilename.c_str(), filename.c_str());
}

void IDEFrame::setDarkTheme(wxCommandEvent& commandEvent)
{
    applyTheme("Cobra - Dark Theme");

    // First read the existing settings
    std::ifstream inputSettingsFile(settingsFileInDotCobraPath.ToStdString());
    if (inputSettingsFile.is_open())
    {
        inputSettingsFile >> settings;
        inputSettingsFile.close();
    }

    // Update the theme setting
    settings["editor.theme"] = "Cobra - Dark Theme";

    // Then write the updated settings back to the file
    std::ofstream outputSettingsFile(settingsFileInDotCobraPath.ToStdString());
    if (outputSettingsFile.is_open())
    {
        outputSettingsFile << settings.dump(4);
        outputSettingsFile.close();
    }
}

void IDEFrame::setLightTheme(wxCommandEvent& commandEvent)
{
    applyTheme("Cobra - Light Theme");

    // First read the existing settings
    std::ifstream inputSettingsFile(settingsFileInDotCobraPath.ToStdString());
    if (inputSettingsFile.is_open())
    {
        inputSettingsFile >> settings;
        inputSettingsFile.close();
    }

    // Update the theme setting
    settings["editor.theme"] = "Cobra - Light Theme";

    // Then write the updated settings back to the file
    std::ofstream outputSettingsFile(settingsFileInDotCobraPath.ToStdString());
    if (outputSettingsFile.is_open())
    {
        outputSettingsFile << settings.dump(4);
        outputSettingsFile.close();
    }
}

void IDEFrame::applyTheme(std::string theme)
{
    if (theme == "Cobra - Dark Theme")
    {
        codeBody->StyleSetForeground(wxSTC_STYLE_DEFAULT, wxColour(191, 185, 185));
        codeBody->StyleSetBackground(wxSTC_STYLE_DEFAULT, wxColour(36, 33, 33));

        codeBody->StyleClearAll();  // Apply styles properly

        codeBody->SetCaretForeground(*wxWHITE);
        setSyntaxHighlighting();
    }

    if (theme == "Cobra - Light Theme")
    {
        codeBody->StyleSetForeground(wxSTC_STYLE_DEFAULT, wxColour(36, 33, 33));
        codeBody->StyleSetBackground(wxSTC_STYLE_DEFAULT, wxColour(246, 246, 246));

        codeBody->StyleClearAll();  // Apply styles properly

        codeBody->SetCaretForeground(*wxBLACK);
        setSyntaxHighlighting();
    }
}

void IDEFrame::setStyleFont(std::string fontFace, int fontSize)
{
    codeBody->StyleSetFaceName(wxSTC_STYLE_DEFAULT, fontFace);
    codeBody->StyleSetSize(wxSTC_STYLE_DEFAULT, fontSize);

    codeBody->StyleClearAll();

    setSyntaxHighlighting();
}

void IDEFrame::setFont(wxCommandEvent& commandEvent)
{
    wxFontData data;

    data.EnableEffects(false);
    data.SetInitialFont(codeBody->GetFont());

    wxFontDialog fontDialog(this, data);

    if (fontDialog.ShowModal() == wxID_OK)
    {
        wxFontData retData = fontDialog.GetFontData();
        wxFont font = retData.GetChosenFont();

        setStyleFont(font.GetFaceName().ToStdString(), font.GetPointSize());

        std::ofstream settingsFile(settingsFileInDotCobraPath.ToStdString(), std::ios::app);

        if (!settingsFile)
        {
            return;
        }
        // First read the existing settings
        std::ifstream inputSettingsFile(settingsFileInDotCobraPath.ToStdString());
        if (inputSettingsFile.is_open())
        {
            inputSettingsFile >> settings;
            inputSettingsFile.close();
        }

        // Update the theme setting
        settings["editor.fontFace"] = font.GetFaceName().ToStdString();
        settings["editor.fontSize"] = font.GetPointSize();

        // Then write the updated settings back to the file
        std::ofstream outputSettingsFile(settingsFileInDotCobraPath.ToStdString());
        if (outputSettingsFile.is_open())
        {
            outputSettingsFile << settings.dump(4);
            outputSettingsFile.close();
        }
    }
}

void IDEFrame::createDotCobraDirectory()
{
    dotCobraPath = projectPath + "\\.cobra";
    settingsFileInDotCobraPath = dotCobraPath + "\\settings.json";

    // Only create directory and settings file if they don't exist
    if (!wxDirExists(dotCobraPath))
    {
        if (wxFileName::Mkdir(dotCobraPath, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL))
        {
            wxLogStatus(".COBRA CREATED AS " + dotCobraPath);
        }
        else
        {
            wxLogStatus("FAILURE TO CREATE .COBRA AS " + dotCobraPath);
            return;
        }

        std::ofstream settingsFile(settingsFileInDotCobraPath.ToStdString());

        if (settingsFile.is_open())
        {
            settings["editor.theme"] = "Cobra - Dark Theme";
            settings["editor.fontFace"] = editorFontFace;
            settings["editor.fontSize"] = editorFontSize;

            settingsFile << settings.dump(4) << "\n";
            settingsFile.close();

            wxLogStatus(".cobra\\settings.json FILE CREATED SUCCESSFULLY.");
        }
        else
        {
            wxLogStatus(".cobra\\settings.json FILE FAILED TO CREATE.");
        }
    }
    else
    {
        // Directory exists, just load the settings
        std::ifstream inputSettingsFile(settingsFileInDotCobraPath.ToStdString());
        if (inputSettingsFile.is_open())
        {
            try {
                inputSettingsFile >> settings;
            }
            catch (const json::parse_error& e) {
                wxLogError("Failed to parse settings.json: %s", e.what());
            }
            inputSettingsFile.close();
        }
    }
}

void IDEFrame::runPythonCode(wxCommandEvent& commandEvent)
{
    if (!fileIsSaved)
    {
        wxMessageBox("Please save your file.", "Save Required", wxOK | wxICON_INFORMATION, this);
        return;
    }

    std::string commandToRunPythonFile = "cmd.exe /c start cmd.exe /k \"python " + projectPath.ToStdString() + "\\main.py\"";

    int exitCode = system(commandToRunPythonFile.c_str());
}

void IDEFrame::showMemberAutoComplete()
{
    int currentPos = codeBody->GetCurrentPos();
    int wordStart = codeBody->WordStartPosition(currentPos - 1, true);
    wxString moduleName = codeBody->GetTextRange(wordStart, currentPos - 1);

    if (moduleName.IsEmpty())
        return;

    // Query Python for members of this module
    wxString members = getModuleMembers(moduleName);

    if (!members.IsEmpty())
    {
        codeBody->AutoCompShow(0, members);
    }
}

wxString IDEFrame::getModuleMembers(const wxString& moduleName)
{
    wxString output;
    wxArrayString result;
    wxString command = wxString::Format(
        "python -c \"import %s; print(' '.join(dir(%s)))\"",
        moduleName, moduleName
    );

    long exitCode = wxExecute(command, result);
    
    if (exitCode == 0)
    {
        for (const auto& line : result)
        {
            output += line + " ";
        }
    }
    else
    {
        wxLogError("Failed to retrieve members for module: %s", moduleName);
    }

    return output;
}

void IDEFrame::onFileTreeItemSelected(wxTreeEvent& treeEvent)
{
    wxTreeItemId selectedItemId = treeEvent.GetItem();

    if (selectedItemId.IsOk())
    {
        wxString file = projectFileTree->GetItemText(selectedItemId);
        std::ifstream fileToOpen(projectPath.ToStdString() + "\\" + file.ToStdString());

        wxLogMessage("Tree Item Selected: %s", file);
        wxLogMessage("PATH: %s", projectPath.ToStdString() + "\\" + file.ToStdString()); 

        if (fileToOpen.is_open())
        {
            std::string textToOutputToFileToOpen;

            std::string line;
            while (std::getline(fileToOpen, line))
            {
                textToOutputToFileToOpen += line + "\n";
            }

            codeBody->ClearAll();
            codeBody->SetText(textToOutputToFileToOpen);
        }
        else
        {
            codeBody->SetText("");
        }
    }
}

void IDEFrame::OnCharAdded(wxStyledTextEvent& styledTextEvent)
{
    wxStyledTextCtrl* stc = static_cast<wxStyledTextCtrl*>(styledTextEvent.GetEventObject());

    char chr = (char) styledTextEvent.GetKey(); // The character that was just added

    // TODO Implement in OB.7.
    //if (chr == '.')
    //{
    //    // User typed a dot — trigger member autocomplete
    //    showMemberAutoComplete();
    //}

    int currentPos = stc->GetCurrentPos();
    int wordStartPos = stc->WordStartPosition(currentPos, true); // Find the start of the word right before the caret
    int lenEntered = currentPos - wordStartPos;

    // Minimum length to trigger autocomplete
    const int MinLengthForAutocomplete = 1; // Adjust as desired

    if (lenEntered >= MinLengthForAutocomplete)
    {
        wxString wordPart = stc->GetTextRange(wordStartPos, currentPos);

        // --- Simple Keyword Autocomplete ---
        // Prepare the list of keywords matching the entered part
        wxString suggestions;
        int keywordCount = 0; // Keep track of how many suggestions we found

        wxStringTokenizer tokenizer(pythonKeywordsForAutoComplete, ' ');

        while (tokenizer.HasMoreTokens())
        {
            wxString keyword = tokenizer.GetNextToken();

            if (!keyword.IsEmpty() && keyword.Lower().StartsWith(wordPart.Lower()))
            {
                if (keywordCount > 0)
                    suggestions << wxChar(stc->AutoCompGetSeparator());

                suggestions << keyword;
                keywordCount++;
            }
        }

        char ch = static_cast<char>(styledTextEvent.GetKey());

        if (isalpha(ch) || ch == '_')
        {
            wxString code = codeBody->GetText();
            std::string codeStd = std::string(code.mb_str());

            std::vector<std::string> symbols = extractSymbols(codeStd);

            for (const auto& sym : symbols) {
                if (!sym.empty()) {
                    if (!suggestions.IsEmpty())
                        suggestions << wxChar(stc->AutoCompGetSeparator());
                    suggestions << sym;
                    keywordCount++;
                }
            }
        }

        // Show the list if we have suggestions
        if (keywordCount > 0)
        {
            // Show the autocomplete list
            // Parameters:
            // 1. lenEntered: How many characters the user has already typed for this word.
            // 2. suggestions: The string containing suggestions separated by the separator char.
            stc->AutoCompShow(lenEntered, suggestions);
        }
        else
        {
            // If no suggestions match, ensure any existing list is cancelled
            if (stc->AutoCompActive()) {
                stc->AutoCompCancel();
            }
        }
    }
    else
    {
        // If the word is too short, or if the user deletes characters,
        // ensure any existing autocomplete list is cancelled.
        if (stc->AutoCompActive())
        {
            stc->AutoCompCancel();
        }
    }

    // Allow the event to propagate if needed (usually not necessary for CharAdded)
    // event.Skip();
}

void IDEFrame::handleKeyEvent(wxKeyEvent& keyEvent)
{
    int key = keyEvent.GetKeyCode();

    if (keyEvent.ShiftDown() && key == WXK_CAPITAL)
    {
        OutputDebugStringA("Shift + Caps Lock PRESSED, CALLING IDEFrame::capitalizeSelectedText().\n");

        int from = codeBody->GetSelectionStart();
        int to = codeBody->GetSelectionEnd();

        wxString selectedText;

        if (from != to)
        {
            selectedText = codeBody->GetTextRange(from, to);
        }

        wxString capitalizedText = selectedText;
        capitalizedText.MakeUpper();

        bool isCapitalized = selectedText == capitalizedText;

        OutputDebugStringA("COMPARING SELECTION WITH CAPITALIZATION.\n");
        OutputDebugStringA("SELECTION: " + selectedText + "\n");
        OutputDebugStringA("CAPTALIZATION: " + selectedText.MakeUpper() + "\n");
        
        if (!isCapitalized)
        {
            capitalizeSelectedText();
        }

        if (isCapitalized)
        {
            uncapitalizeSelectedText();
        }
    }

    //if (keyEvent.ControlDown() && keyEvent.ShiftDown() && key == WXK_CAPITAL)

    keyEvent.Skip();
}

void IDEFrame::capitalizeSelectedText()
{
    OutputDebugStringA("CAPITALIZING SELECTED TEXT.");

    int from = codeBody->GetSelectionStart();
    int to = codeBody->GetSelectionEnd();

    if (from != to) 
    {
        wxString selectedText = codeBody->GetTextRange(from, to);
        selectedText.MakeUpper(); // Capitalize

        codeBody->SetTargetStart(from);
        codeBody->SetTargetEnd(to);
        codeBody->ReplaceTarget(selectedText);
    }
}

void IDEFrame::uncapitalizeSelectedText()
{
    OutputDebugStringA("UNCAPITALIZING SELECTED TEXT.");

    int from = codeBody->GetSelectionStart();
    int to = codeBody->GetSelectionEnd();

    if (from != to)
    {
        wxString selectedText = codeBody->GetTextRange(from, to);
        selectedText.MakeLower(); // Capitalize

        codeBody->SetTargetStart(from);
        codeBody->SetTargetEnd(to);
        codeBody->ReplaceTarget(selectedText);
    }
}

std::vector<std::string> IDEFrame::extractSymbols(const std::string& code) 
{
    std::vector<std::string> symbols;

    std::regex funcRegex(R"(def\s+([a-zA-Z_]\w*)\s*\()");
    std::regex varRegex(R"((?:^|\s)([a-zA-Z_]\w*)\s*=\s*)");

    std::smatch match;

    std::string::const_iterator searchStart(code.cbegin());

    // Find functions
    while (std::regex_search(searchStart, code.cend(), match, funcRegex)) 
    {
        symbols.push_back(match[1]);
        searchStart = match.suffix().first;
    }

    searchStart = code.cbegin();

    // Find variables
    while (std::regex_search(searchStart, code.cend(), match, varRegex)) 
    {
        symbols.push_back(match[1]);
        searchStart = match.suffix().first;
    }

    return symbols;
}

void IDEFrame::addCodeBodyToIDE(const wxString& p_primaryKeywords, const wxString& p_secondaryKeywords)
{
    codeBody = new wxStyledTextCtrl(idePanel, CODEBODY_IDE_ID, wxPoint(0, 0), wxSize(1080, 720));

    codeBody->SetLexer(wxSTC_LEX_PYTHON);

    codeBody->StyleSetFaceName(wxSTC_STYLE_DEFAULT, editorFontFace);
    codeBody->StyleSetSize(wxSTC_STYLE_DEFAULT, editorFontSize);

    codeBody->StyleSetForeground(wxSTC_STYLE_DEFAULT, wxColour(191, 185, 185));
    codeBody->StyleSetBackground(wxSTC_STYLE_DEFAULT, wxColour(36, 33, 33));

    codeBody->StyleClearAll();  // Apply styles properly

    // Line number styling
    codeBody->StyleSetForeground(wxSTC_STYLE_LINENUMBER, wxColour(70, 70, 70));
    codeBody->StyleSetBackground(wxSTC_STYLE_LINENUMBER, wxColour(220, 220, 220));

    codeBody->SetCaretForeground(*wxWHITE);

    codeBody->SetKeyWords(0, p_primaryKeywords);
    codeBody->SetKeyWords(1, p_secondaryKeywords);

    setSyntaxHighlighting();

    // Enable line numbers
    codeBody->SetMarginType(0, wxSTC_MARGIN_NUMBER);
    codeBody->SetMarginMask(0, 0);
    codeBody->SetMarginWidth(0, 50);

    codeBody->SetUseTabs(true);
    codeBody->SetTabWidth(4);
    codeBody->SetIndent(4);

    // Autocomplete
    codeBody->AutoCompSetSeparator(' ');

    codeBody->AutoCompSetIgnoreCase(true);
    codeBody->AutoCompSetAutoHide(true);
    codeBody->AutoCompSetDropRestOfWord(true);

    codeBody->AutoCompSetCancelAtStart(true);

    codeBody->AutoCompSetChooseSingle(false);

    Bind(wxEVT_STC_CHARADDED, &IDEFrame::OnCharAdded, this);

    codeBody->Bind(wxEVT_KEY_DOWN, &IDEFrame::handleKeyEvent, this);
    codeBody->Bind(wxEVT_TEXT, &IDEFrame::ScanText, this);
}

void IDEFrame::ScanText(wxStyledTextEvent& styledTextEvent)
{
    wxString textToScanForIndents = codeBody->GetText();
}

void IDEFrame::addProjectFileTreeToIDE()
{
    projectFileTree = new wxTreeCtrl(this, FILE_TREE_IDE_ID, wxDefaultPosition, wxSize(300, 400),
        wxTR_HAS_BUTTONS | wxTR_LINES_AT_ROOT);

    std::string str = projectPath.ToStdString();
    std::istringstream ss(str);
    std::string word;
    std::vector<std::string> words;

    char delimeter = '\\';

    while (getline(ss, word, delimeter))
    {
        words.push_back(word);
    }

    wxTreeItemId root = projectFileTree->AddRoot(words.back());
    populateProjectFileTreeWithFiles(projectFileTree, root, projectPath);

    projectFileTree->Expand(root);
}

void IDEFrame::addMenuToIDE()
{
    wxMenuBar* ideMenuBar = new wxMenuBar();

    wxMenu* fileMenu = new wxMenu();

    wxMenu* editMenu = new wxMenu();
    wxMenu* findAndReplaceMenu = new wxMenu();

    wxMenu* settingsMenu = new wxMenu();
    wxMenu* themesMenu = new wxMenu();

    wxMenu* runMenu = new wxMenu();

    fileMenu->Append(wxID_NEW, "&New", "TOOLTIP: Create a new file.");
    fileMenu->Append(wxID_OPEN, "&Open", "TOOLTIP: Open a new project or file.");

    fileMenu->Append(wxID_SAVE, "&Save", "TOOLTIP: Save currently opened file.");
    fileMenu->Append(wxID_SAVEAS, "&Save As", "TOOLTIP: Save currently opened file as a different one.");

    fileMenu->Append(wxID_CLOSE, "&Close", "TOOLTIP: Close the application.");

    editMenu->Append(wxID_CUT, "&Cut", "TOOLTIP: Deletes the selected text, while still copying it to the clipboard.");

    editMenu->Append(wxID_COPY, "&Copy", "TOOLTIP: Copies the selected text to the clipboard.");
    editMenu->Append(wxID_PASTE, "&Paste", "TOOLTIP: Pastes the most recently copied text from the clipboard.");

    editMenu->Append(wxID_DELETE, "&Delete", "TOOLTIP: Deletes the selected text.");

    editMenu->Append(wxID_SELECTALL, "&Select All", "TOOLTIP: Selected all the text.");

    findAndReplaceMenu->Append(wxID_FIND, "&Find", "TOOLTIP: Find all references of the given word.");

    findAndReplaceMenu->Append(wxID_REPLACE, "&Replace", "TOOLTIP: Replace the first reference of the given word.");
    findAndReplaceMenu->Append(wxID_REPLACE_ALL, "&Replace All", "TOOLTIP: Replace all references of the given word.");

    findAndReplaceMenu->Append(STOP_FINDING_MENUITEM_ID, "&Stop", "TOOLTIP: Stop finding and remove the highlights.");

    themesMenu->Append(SET_DARK_THEME_MENUITEM_ID, "&Cobra - Dark Theme");
    themesMenu->Append(SET_LIGHT_THEME_MENUITEM_ID, "&Cobra - Light Theme");

    runMenu->Append(RUN_CODE_MENUITEM_ID, "&Run Code", "TOOLTIP: Run the code. Python must be installed.");

    editMenu->AppendSubMenu(findAndReplaceMenu, "&Find and Replace");

    settingsMenu->AppendSubMenu(themesMenu, "&Set Theme");
    settingsMenu->Append(SET_FONT_MENUITEM_ID, "&Set Font");

    ideMenuBar->Append(fileMenu, "&File");
    ideMenuBar->Append(editMenu, "&Edit");
    ideMenuBar->Append(settingsMenu, "&Settings");
    ideMenuBar->Append(runMenu, "&Run");

    SetMenuBar(ideMenuBar);
}

IDEFrame::IDEFrame(const wxString& title, wxString p_projectPath)
    : wxFrame(nullptr, wxID_ANY, title, wxDefaultPosition, wxSize(1080, 720))
{
    Bind(wxEVT_CLOSE_WINDOW, &IDEFrame::confirmExit, this);

    const wxString& primaryKeywords = "and as assert async await break class continue def del elif else except finally for from global if import in is lambda nonlocal not or pass raise return try while with yield";
    const wxString& secondaryKeywords = "True False None abs all any ascii bin bool breakpoint"
        "bytearray bytes callable chr classmethod compile complex "
        "delattr dict dir divmod enumerate eval exec filter float "
        "format frozenset getattr globals hasattr hash help hex id "
        "input int isinstance issubclass iter len list locals map "
        "max memoryview min next object oct open ord pow print "
        "property range repr reversed round set setattr slice "
        "sorted staticmethod str sum super tuple type vars zip "
        "__init__ __str__ __repr__ __len__ __getitem__ __setitem__ "
        "__delitem__ __iter__ __next__ __call__ __contains__ "
        "__eq__ __ne__ __lt__ __le__ __gt__ __ge__ __add__ __sub__ "
        "__mul__ __truediv__ __floordiv__ __mod__ __pow__ __and__ "
        "__or__ __xor__ __lshift__ __rshift__ __enter__ __exit__";
    
    pythonKeywordsForAutoComplete = primaryKeywords + " " + secondaryKeywords;

    wxString installedModules = getInstalledPythonModules();
    pythonKeywordsForAutoComplete += " " + installedModules;

    idePanel = new wxPanel(this);

    wxBoxSizer* boxLayout = new wxBoxSizer(wxHORIZONTAL);
    
    projectPath = p_projectPath;

    addMenuToIDE();

    addCodeBodyToIDE(primaryKeywords, secondaryKeywords);
    addProjectFileTreeToIDE();

    boxLayout->Add(codeBody, 2, wxEXPAND | wxALL, 5);
    boxLayout->Add(projectFileTree, 1, wxEXPAND | wxALL, 5);
    
    idePanel->SetSizerAndFit(boxLayout);  // FIXME Debug error detected here.
    boxLayout->Layout();

    CreateStatusBar();

    this->Maximize();

    createDotCobraDirectory();
}

void IDEFrame::setSyntaxHighlighting()
{
    codeBody->StyleSetForeground(wxSTC_P_COMMENTLINE, wxColour(0, 128, 0));
    codeBody->StyleSetForeground(wxSTC_P_NUMBER, wxColour(0, 135, 232));
    codeBody->StyleSetForeground(wxSTC_P_STRING, wxColour(255, 0, 255));
    codeBody->StyleSetForeground(wxSTC_P_CHARACTER, wxColour(254, 153, 0));
    codeBody->StyleSetForeground(wxSTC_P_WORD, wxColour(0, 0, 255));
    codeBody->StyleSetForeground(wxSTC_P_TRIPLE, wxColour(255, 0, 0));
    codeBody->StyleSetForeground(wxSTC_P_TRIPLEDOUBLE, wxColour(255, 0, 0));
    codeBody->StyleSetForeground(wxSTC_P_CLASSNAME, wxColour(47, 181, 38));
    codeBody->StyleSetForeground(wxSTC_P_DEFNAME, wxColour(253, 185, 0));
    codeBody->StyleSetForeground(wxSTC_P_OPERATOR, wxColour(133, 133, 133));
    codeBody->StyleSetForeground(wxSTC_P_IDENTIFIER, wxColour(1, 141, 255));
    codeBody->StyleSetForeground(wxSTC_P_WORD2, wxColour(255, 1, 170));
    codeBody->StyleSetForeground(wxSTC_P_DECORATOR, wxColour(255, 219, 15));
}
