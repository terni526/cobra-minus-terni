#pragma once

#include <wx/wx.h>
#include <wx/stc/stc.h>

#include <wx/treectrl.h>

#include <wx/dir.h>
#include <wx/filename.h>

#include <wx/fswatcher.h>

#include <unordered_map>

#include "external/json.hpp"

class IDEFrame : public wxFrame
{
	public:
		IDEFrame(const wxString& title, wxString p_projectPath);

		void setSyntaxHighlighting();

		void setDarkTheme(wxCommandEvent& commmandEvent);
		void setLightTheme(wxCommandEvent& commmandEvent);

		void applyTheme(std::string theme);
		void setStyleFont(std::string fontFace, int fontSize);

		wxStyledTextCtrl* codeBody;
		wxString projectPath;

	private:
		wxString pythonKeywordsForAutoComplete;

		wxTreeCtrl* projectFileTree;

		wxString editorFontFace = "Anonymous Pro";
		int editorFontSize = 13;

		void confirmExit(wxCloseEvent& event);

		void fullyTerminateApplication();

		wxPanel* idePanel;

		void addCodeBodyToIDE(const wxString& p_primaryKeywords, const wxString& p_secondaryKeywords);

		std::vector<std::string> extractSymbols(const std::string& code);

		void handleKeyEvent(wxKeyEvent& keyEvent);

		void capitalizeSelectedText();
		void uncapitalizeSelectedText();

		void addProjectFileTreeToIDE();
		void addMenuToIDE();

		void addFindAndReplaceBoxToIDE();
		wxTextCtrl* wordToFindOrReplace;

		bool isFinding = false;

		void showMemberAutoComplete();
		wxString getModuleMembers(const wxString& moduleName);

		void onFileTreeItemSelected(wxTreeEvent& treeEvent);

		void setFont(wxCommandEvent& commandEvent);

		void createDotCobraDirectory();

		wxString dotCobraPath;
		wxString settingsFileInDotCobraPath;

		nlohmann::json settings;

		bool fileToSaveHasBeenPicked = false;
		bool fileIsSaved = false;

		void createNewFile(wxCommandEvent& commmandEvent);
		void openNewFile(wxCommandEvent& commandEvent);
		
		std::string replaceTabsWithSpaces(const std::string& input, int spacesPerTab);

		int getLineNumber(const std::string& fileName, const std::string& setting);

		void saveFile(wxCommandEvent& commandEvent);
		void saveAsFile(wxCommandEvent& commandEvent);

		std::string removeHalfChar(std::string str, char target);

		void closeApplication(wxCommandEvent& commandEvent);

		void cutSelectedText(wxCommandEvent& commandEvent);

		void copySelectedText(wxCommandEvent& commandEvent);
		void pasteText(wxCommandEvent& commandEvent);

		void deleteSelectedText(wxCommandEvent& commandEvent);

		void selectAllText(wxCommandEvent& commandEvent);

		wxString getInformationForFinding();

		void findGivenText(wxCommandEvent& commandEvent);
		void stopFinding(wxCommandEvent& commandEvent);

		wxString searchText;

		wxString getTextToReplaceReferences();

		void replaceFirstReferenceWithGivenText(wxCommandEvent& commandEvent);
		void replaceAllWithGivenText(wxCommandEvent& commandEvent);

		const int FIND_AND_REPLACE_HIGHLIGHT_STYLE = 10;

		void runPythonCode(wxCommandEvent& commandEvent);

		void populateProjectFileTreeWithFiles(wxTreeCtrl* tree, wxTreeItemId parent, const wxString& path);

		void OnCharAdded(wxStyledTextEvent& styledTextEvent);
		void ScanText(wxStyledTextEvent& styledTextEvent);
		
		wxDECLARE_EVENT_TABLE();
};
