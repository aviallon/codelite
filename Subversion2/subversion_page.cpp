#include <wx/app.h>
#include "subversion2_ui.h"
#include <wx/settings.h>
#include <wx/filedlg.h>
#include <wx/textdlg.h>
#include "plugin.h"
#include "procutils.h"
#include "svn_login_dialog.h"
#include "svn_command_handlers.h"
#include "svn_copy_dialog.h"
#include "svninfohandler.h"
#include "commit_dialog.h"
#include "svn_default_command_handler.h"
#include <wx/menu.h>
#include <wx/dirdlg.h>
#include "fileextmanager.h"
#include "svnsettingsdata.h"
#include "svnstatushandler.h"
#include <wx/wupdlock.h>
#include "subversion_strings.h"
#include "subversion_page.h"
#include <wx/xrc/xmlres.h>
#include "svntreedata.h"
#include <wx/imaglist.h>
#include "imanager.h"
#include "workspace.h"
#include "subversion2.h"
#include "svn_console.h"
#include "globals.h"

BEGIN_EVENT_TABLE(SubversionView, SubversionPageBase)
	EVT_UPDATE_UI(XRCID("svn_stop"),         SubversionView::OnStopUI)
	EVT_UPDATE_UI(XRCID("clear_svn_output"), SubversionView::OnClearOuptutUI)
END_EVENT_TABLE()

SubversionView::SubversionView( wxWindow* parent, Subversion2 *plugin )
		: SubversionPageBase( parent )
		, m_plugin          ( plugin )
{
	CreatGUIControls();
	m_plugin->GetManager()->GetTheApp()->Connect(wxEVT_WORKSPACE_LOADED, wxCommandEventHandler(SubversionView::OnWorkspaceLoaded), NULL, this);
	m_plugin->GetManager()->GetTheApp()->Connect(wxEVT_WORKSPACE_CLOSED, wxCommandEventHandler(SubversionView::OnWorkspaceClosed), NULL, this);
	m_plugin->GetManager()->GetTheApp()->Connect(wxEVT_FILE_SAVED,       wxCommandEventHandler(SubversionView::OnRefreshView),     NULL, this);
	m_plugin->GetManager()->GetTheApp()->Connect(wxEVT_PROJ_FILE_ADDED,  wxCommandEventHandler(SubversionView::OnRefreshView),     NULL, this);
	m_plugin->GetManager()->GetTheApp()->Connect(wxEVT_FILE_RENAMED,     wxCommandEventHandler(SubversionView::OnFileRenamed),     NULL, this);
	Connect(XRCID("svn_commit2"), wxCommandEventHandler(SubversionView::OnCommitWithLogin), NULL, this);
}

SubversionView::~SubversionView()
{
	m_plugin->GetManager()->GetTheApp()->Disconnect(wxEVT_WORKSPACE_LOADED, wxCommandEventHandler(SubversionView::OnWorkspaceLoaded), NULL, this);
	m_plugin->GetManager()->GetTheApp()->Disconnect(wxEVT_WORKSPACE_CLOSED, wxCommandEventHandler(SubversionView::OnWorkspaceClosed), NULL, this);
	m_plugin->GetManager()->GetTheApp()->Disconnect(wxEVT_FILE_SAVED,       wxCommandEventHandler(SubversionView::OnRefreshView),     NULL, this);
	m_plugin->GetManager()->GetTheApp()->Disconnect(wxEVT_PROJ_FILE_ADDED,  wxCommandEventHandler(SubversionView::OnRefreshView),     NULL, this);
	m_plugin->GetManager()->GetTheApp()->Disconnect(wxEVT_FILE_RENAMED,     wxCommandEventHandler(SubversionView::OnFileRenamed),     NULL, this);
	Disconnect(XRCID("svn_commit2"), wxCommandEventHandler(SubversionView::OnCommitWithLogin), NULL, this);
}

void SubversionView::OnChangeRootDir( wxCommandEvent& event )
{
	wxUnusedVar(event);
	wxString path(m_textCtrlRootDir->GetValue());
	wxString new_path = wxDirSelector(wxT("Select working directory:"), path, wxDD_DEFAULT_STYLE, wxDefaultPosition, this);
	if (new_path.IsEmpty() == false) {
		m_textCtrlRootDir->SetValue(new_path);
		BuildTree();
	}
}

void SubversionView::OnTreeMenu( wxTreeEvent& event )
{
	// Popup the menu
	wxArrayTreeItemIds items;
	size_t count = m_treeCtrl->GetSelections(items);
	if (count) {
		SvnTreeData::SvnNodeType type = DoGetSelectionType(items);
		if (type == SvnTreeData::SvnNodeTypeInvalid)
			// Mix or an invalid selection
			return;

		wxMenu menu;
		switch (type) {
		case SvnTreeData::SvnNodeTypeFile:
			CreateFileMenu( &menu );
			break;

		case SvnTreeData::SvnNodeTypeRoot:
			CreateRootMenu( &menu );
			break;

		case SvnTreeData::SvnNodeTypeAddedRoot:
		case SvnTreeData::SvnNodeTypeDeletedRoot:
		case SvnTreeData::SvnNodeTypeModifiedRoot:
			CreateSecondRootMenu( &menu );
			break;

		default:
			return;
		}

		PopupMenu( &menu );
	}
}

void SubversionView::CreatGUIControls()
{
	// Assign the image list
	wxImageList *imageList = new wxImageList(16, 16, true);

	imageList->Add(wxXmlResource::Get()->LoadBitmap(wxT("folder")));
	imageList->Add(wxXmlResource::Get()->LoadBitmap(wxT("error")));
	imageList->Add(wxXmlResource::Get()->LoadBitmap(wxT("edit_add")));
	imageList->Add(wxXmlResource::Get()->LoadBitmap(wxT("edit_delete")));
	imageList->Add(wxXmlResource::Get()->LoadBitmap(wxT("project_conflict")));
	imageList->Add(wxXmlResource::Get()->LoadBitmap(wxT("files_unversioned")));

	// File icons 6-13
	imageList->Add( wxXmlResource::Get()->LoadBitmap(wxT("project" ) ) ); // 6
	imageList->Add( wxXmlResource::Get()->LoadBitmap(wxT("folder" ) ) );  // 7
	imageList->Add( wxXmlResource::Get()->LoadBitmap(wxT("page_white_c" ) ) );//8
	imageList->Add( wxXmlResource::Get()->LoadBitmap(wxT("page_white_cplusplus" ) ) );//9
	imageList->Add( wxXmlResource::Get()->LoadBitmap(wxT("page_white_h" ) ) );//10
	imageList->Add( wxXmlResource::Get()->LoadBitmap(wxT("page_white_text" ) ) );//11
	imageList->Add( wxXmlResource::Get()->LoadBitmap(wxT("workspace" ) ) );//12
	imageList->Add( wxXmlResource::Get()->LoadBitmap(wxT("formbuilder" ) ) );//13

	m_treeCtrl->AssignImageList( imageList );

	// Add toolbar
	//Create the toolbar
	wxToolBar *tb = new wxToolBar(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTB_FLAT|wxTB_HORIZONTAL|wxTB_NODIVIDER);

	tb->AddTool(XRCID("clear_svn_output"), wxT("Clear Svn Output Tab"), wxXmlResource::Get()->LoadBitmap(wxT("document_delete")), wxT("Clear Svn Output Tab"), wxITEM_NORMAL);
	tb->AddTool(XRCID("svn_refresh"),      wxT("Refresh View"), wxXmlResource::Get()->LoadBitmap ( wxT ( "svn_refresh" ) ), wxT ( "Refresh View" ) );
	tb->AddSeparator();

	tb->AddTool(XRCID("svn_stop"),         wxT("Stop current svn process"), wxXmlResource::Get()->LoadBitmap ( wxT ( "stop_build16" ) ), wxT ( "Stop current svn process" ) );
	tb->AddTool(XRCID("svn_cleanup"),      wxT("Svn Cleanup"), wxXmlResource::Get()->LoadBitmap ( wxT ( "eraser" ) ), wxT ( "Svn Cleanup" ) );

	tb->AddSeparator();
	tb->AddTool(XRCID("svn_checkout"),         wxT("Svn Checkout"), wxXmlResource::Get()->LoadBitmap ( wxT ( "svn_checkout" ) ), wxT ( "Svn Checkout" ) );

	tb->AddSeparator();
	tb->AddTool(XRCID("svn_info"),         wxT("Svn Info"), wxXmlResource::Get()->LoadBitmap ( wxT ( "svn_info" ) ), wxT ( "Svn Info" ) );

	tb->Connect(XRCID("clear_svn_output"), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(SubversionView::OnClearOuptut), NULL, this);
	tb->Connect(XRCID("svn_stop"),         wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(SubversionView::OnStop),        NULL, this);
	tb->Connect(XRCID("svn_cleanup"),      wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(SubversionView::OnCleanup),     NULL, this);
	tb->Connect(XRCID("svn_info"),         wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(SubversionView::OnShowSvnInfo), NULL, this);
	tb->Connect(XRCID("svn_refresh"),      wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(SubversionView::OnRefreshView), NULL, this);
	tb->Connect(XRCID("svn_checkout"),     wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(SubversionView::OnCheckout),    NULL, this);

	wxSizer *sz = GetSizer();
	sz->Insert(0, tb, 0, wxEXPAND);
	tb->Realize();

	if (m_plugin->GetManager()->IsWorkspaceOpen()) {
		m_textCtrlRootDir->SetValue(m_plugin->GetManager()->GetWorkspace()->GetWorkspaceFileName().GetPath());
	}
	BuildTree();
}

void SubversionView::BuildTree()
{
	wxString rootDir = m_textCtrlRootDir->GetValue();

	if (rootDir.IsEmpty()) {
		return;
	}

	wxString command;
	command << m_plugin->GetSvnExeName(true) << wxT("--xml -q status");
	m_simpleCommand.Execute(command, rootDir, new SvnStatusHandler(m_plugin));
}

void SubversionView::OnWorkspaceLoaded(wxCommandEvent& event)
{
	event.Skip();
	Workspace *workspace = m_plugin->GetManager()->GetWorkspace();
	if(m_plugin->GetManager()->IsWorkspaceOpen() && workspace) {
		m_textCtrlRootDir->SetValue(workspace->GetWorkspaceFileName().GetPath());
		BuildTree();
	}
}

void SubversionView::OnWorkspaceClosed(wxCommandEvent& event)
{
	event.Skip();
	m_textCtrlRootDir->SetValue(wxT(""));
	ClearAll();
	m_plugin->GetShell()->Clear();
}

void SubversionView::ClearAll()
{
	m_treeCtrl->DeleteAllItems();
}

void SubversionView::UpdateTree(const wxArrayString& modifiedFiles, const wxArrayString& conflictedFiles, const wxArrayString& unversionedFiles, const wxArrayString& newFiles, const wxArrayString& deletedFiles)
{

	wxWindowUpdateLocker locker( m_treeCtrl );
	ClearAll();

	// Add root node
	wxString rootDir = m_textCtrlRootDir->GetValue();
	wxTreeItemId root = m_treeCtrl->AddRoot(rootDir, 0, 0, new SvnTreeData(SvnTreeData::SvnNodeTypeRoot, rootDir));

	if(root.IsOk() == false)
		return;

	DoAddNode(svnMODIFIED_FILES,    1, SvnTreeData::SvnNodeTypeModifiedRoot,    modifiedFiles);
	DoAddNode(svnADDED_FILES,       2, SvnTreeData::SvnNodeTypeAddedRoot,       newFiles);
	DoAddNode(svnDELETED_FILES,     3, SvnTreeData::SvnNodeTypeDeletedRoot,     deletedFiles);
	DoAddNode(svnCONFLICTED_FILES,  4, SvnTreeData::SvnNodeTypeConflictRoot,    conflictedFiles);
	DoAddNode(svnUNVERSIONED_FILES, 5, SvnTreeData::SvnNodeTypeUnversionedRoot, unversionedFiles);

	if (m_treeCtrl->ItemHasChildren(root)) {
		m_treeCtrl->Expand(root);
	}
}

void SubversionView::DoAddNode(const wxString& title, int imgId, SvnTreeData::SvnNodeType nodeType, const wxArrayString& files)
{
	wxTreeItemId root = m_treeCtrl->GetRootItem();
	wxString basePath = m_textCtrlRootDir->GetValue();

	// Add the basic four root items
	if (files.IsEmpty() == false) {

		wxTreeItemId parent = m_treeCtrl->AppendItem(root, title, imgId, imgId, new SvnTreeData(nodeType, wxT("")));

		// Set the parent node with bold font
		wxFont font = wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT);
		font.SetWeight(wxBOLD);
		m_treeCtrl->SetItemFont(parent, font);

		// Add all children items
		for (size_t i=0; i<files.GetCount(); i++) {
			wxString filename(files.Item(i));
			m_treeCtrl->AppendItem(parent, files.Item(i), DoGetIconIndex(filename), DoGetIconIndex(filename), new SvnTreeData(SvnTreeData::SvnNodeTypeFile, files.Item(i)));
		}

		if ( nodeType != SvnTreeData::SvnNodeTypeUnversionedRoot) {
			m_treeCtrl->Expand(parent);
		}
	}
}

int SubversionView::DoGetIconIndex(const wxString& filename)
{
	FileExtManager::Init();
	FileExtManager::FileType type = FileExtManager::GetType(filename);

	int iconIndex( 11 );

	switch ( type ) {
	case FileExtManager::TypeHeader:
		iconIndex = 10;
		break;

	case FileExtManager::TypeSourceC:
		iconIndex = 8;
		break;

	case FileExtManager::TypeSourceCpp:
		iconIndex = 9;
		break;

	case FileExtManager::TypeProject:
		iconIndex = 6;
		break;

	case FileExtManager::TypeWorkspace:
		iconIndex = 12;
		break;

	case FileExtManager::TypeFormbuilder:
		iconIndex = 13;
		break;
	default:
		iconIndex = 11;
		break;
	}
	return iconIndex;
}

SvnTreeData::SvnNodeType SubversionView::DoGetSelectionType(const wxArrayTreeItemIds& items)
{
	m_selectionInfo.Clear();
	SvnTreeData::SvnNodeType type(SvnTreeData::SvnNodeTypeInvalid);
	for (size_t i=0; i<items.GetCount(); i++) {
		if (items.Item(i).IsOk() == false) {
			m_selectionInfo.Clear();
			return m_selectionInfo.m_selectionType; // Invalid
		}

		SvnTreeData *data = (SvnTreeData *)m_treeCtrl->GetItemData(items.Item(i));
		if ( !data ) {
			m_selectionInfo.Clear();
			return m_selectionInfo.m_selectionType; // Invalid
		}

		if (data->GetType() == SvnTreeData::SvnNodeTypeRoot && items.GetCount() == 1) {
			// populate the list of paths with all the added paths
			DoGetPaths( items.Item(i), m_selectionInfo.m_paths );
			m_selectionInfo.m_selectionType = SvnTreeData::SvnNodeTypeRoot;
			return m_selectionInfo.m_selectionType;
		}

		if (data->GetType() == SvnTreeData::SvnNodeTypeAddedRoot && items.GetCount() == 1) {
			// populate the list of paths with all the added paths
			DoGetPaths( items.Item(i), m_selectionInfo.m_paths );
			m_selectionInfo.m_selectionType = SvnTreeData::SvnNodeTypeAddedRoot;
			return m_selectionInfo.m_selectionType;
		}

		if (data->GetType() == SvnTreeData::SvnNodeTypeDeletedRoot && items.GetCount() == 1) {
			// populate the list of paths with all the deleted paths
			DoGetPaths( items.Item(i), m_selectionInfo.m_paths );
			m_selectionInfo.m_selectionType = SvnTreeData::SvnNodeTypeDeletedRoot;
			return m_selectionInfo.m_selectionType;
		}

		if (data->GetType() == SvnTreeData::SvnNodeTypeConflictRoot && items.GetCount() == 1) {
			// populate the list of paths with all the conflicted paths
			DoGetPaths( items.Item(i), m_selectionInfo.m_paths );
			m_selectionInfo.m_selectionType = SvnTreeData::SvnNodeTypeConflictRoot;
			return m_selectionInfo.m_selectionType;
		}

		if (data->GetType() == SvnTreeData::SvnNodeTypeModifiedRoot && items.GetCount() == 1) {
			// populate the list of paths with all the conflicted paths
			DoGetPaths( items.Item(i), m_selectionInfo.m_paths );
			m_selectionInfo.m_selectionType = SvnTreeData::SvnNodeTypeModifiedRoot;
			return m_selectionInfo.m_selectionType;
		}

		if (type == SvnTreeData::SvnNodeTypeInvalid &&
		        (data->GetType() == SvnTreeData::SvnNodeTypeFile || data->GetType() == SvnTreeData::SvnNodeTypeRoot)) {
			type = data->GetType();
			m_selectionInfo.m_selectionType = type;
			m_selectionInfo.m_paths.Add(data->GetFilepath());

		} else if ( type == SvnTreeData::SvnNodeTypeInvalid ) {
			type = data->GetType();

		} else if (data->GetType() != type) {
			m_selectionInfo.m_paths.Clear();
			return SvnTreeData::SvnNodeTypeInvalid;

		} else {
			// Same type, just add the path
			m_selectionInfo.m_paths.Add(data->GetFilepath());
		}
	}
	return type;
}

void SubversionView::CreateSecondRootMenu(wxMenu* menu)
{
	menu->Append(XRCID("svn_commit"),  wxT("Commit"));
	menu->Append(XRCID("svn_update"),  wxT("Update"));
	menu->AppendSeparator();
	menu->Append(XRCID("svn_revert"),  wxT("Revert"));
	menu->AppendSeparator();
	menu->Append(XRCID("svn_diff"),    wxT("Create Diff..."));

	menu->Connect(XRCID("svn_commit"), wxEVT_COMMAND_MENU_SELECTED,  wxCommandEventHandler(SubversionView::OnCommit), NULL, this);
	menu->Connect(XRCID("svn_update"), wxEVT_COMMAND_MENU_SELECTED,  wxCommandEventHandler(SubversionView::OnUpdate), NULL, this);
	menu->Connect(XRCID("svn_revert"), wxEVT_COMMAND_MENU_SELECTED,  wxCommandEventHandler(SubversionView::OnRevert), NULL, this);
	menu->Connect(XRCID("svn_diff"),   wxEVT_COMMAND_MENU_SELECTED,  wxCommandEventHandler(SubversionView::OnDiff),   NULL, this);
}

void SubversionView::CreateFileMenu(wxMenu* menu)
{
	menu->Append(XRCID("svn_commit"),  wxT("Commit"));
	menu->Append(XRCID("svn_update"),  wxT("Update"));
	menu->AppendSeparator();
	menu->Append(XRCID("svn_revert"),  wxT("Revert"));
	menu->AppendSeparator();
	menu->Append(XRCID("svn_add"),     wxT("Add"));
	menu->Append(XRCID("svn_delete"),  wxT("Delete"));
	menu->AppendSeparator();
	menu->Append(XRCID("svn_resolve"), wxT("Resolve"));
	menu->AppendSeparator();
	menu->Append(XRCID("svn_diff"),    wxT("Create Diff..."));

	menu->Connect(XRCID("svn_commit"),  wxEVT_COMMAND_MENU_SELECTED,  wxCommandEventHandler(SubversionView::OnCommit),  NULL, this);
	menu->Connect(XRCID("svn_update"),  wxEVT_COMMAND_MENU_SELECTED,  wxCommandEventHandler(SubversionView::OnUpdate),  NULL, this);
	menu->Connect(XRCID("svn_add"),     wxEVT_COMMAND_MENU_SELECTED,  wxCommandEventHandler(SubversionView::OnAdd),     NULL, this);
	menu->Connect(XRCID("svn_revert"),  wxEVT_COMMAND_MENU_SELECTED,  wxCommandEventHandler(SubversionView::OnRevert),  NULL, this);
	menu->Connect(XRCID("svn_resolve"), wxEVT_COMMAND_MENU_SELECTED,  wxCommandEventHandler(SubversionView::OnResolve), NULL, this);
	menu->Connect(XRCID("svn_delete"),  wxEVT_COMMAND_MENU_SELECTED,  wxCommandEventHandler(SubversionView::OnDelete),  NULL, this);
	menu->Connect(XRCID("svn_diff"),    wxEVT_COMMAND_MENU_SELECTED,  wxCommandEventHandler(SubversionView::OnDiff),    NULL, this);
}

void SubversionView::CreateRootMenu(wxMenu* menu)
{
	menu->Append(XRCID("svn_commit"),        wxT("Commit"));
	menu->Append(XRCID("svn_update"),        wxT("Update"));
	menu->AppendSeparator();

	menu->Append(XRCID("svn_revert"),        wxT("Revert"));
	menu->AppendSeparator();

	menu->Append(XRCID("svn_tag"),           wxT("Create Tag"));
	menu->Append(XRCID("svn_branch"),        wxT("Create Branch"));
	menu->AppendSeparator();

	menu->Append(XRCID("svn_diff"),          wxT("Create Diff..."));
	menu->Append(XRCID("svn_patch"),         wxT("Apply Patch..."));
	menu->Append(XRCID("svn_patch_dry_run"), wxT("Apply Patch - Dry Run..."));

	menu->Connect(XRCID("svn_commit"),        wxEVT_COMMAND_MENU_SELECTED,  wxCommandEventHandler(SubversionView::OnCommit),      NULL, this);
	menu->Connect(XRCID("svn_update"),        wxEVT_COMMAND_MENU_SELECTED,  wxCommandEventHandler(SubversionView::OnUpdate),      NULL, this);
	menu->Connect(XRCID("svn_revert"),        wxEVT_COMMAND_MENU_SELECTED,  wxCommandEventHandler(SubversionView::OnRevert),      NULL, this);
	menu->Connect(XRCID("svn_tag"),           wxEVT_COMMAND_MENU_SELECTED,  wxCommandEventHandler(SubversionView::OnTag),         NULL, this);
	menu->Connect(XRCID("svn_branch"),        wxEVT_COMMAND_MENU_SELECTED,  wxCommandEventHandler(SubversionView::OnBranch),      NULL, this);
	menu->Connect(XRCID("svn_diff"),          wxEVT_COMMAND_MENU_SELECTED,  wxCommandEventHandler(SubversionView::OnDiff),        NULL, this);
	menu->Connect(XRCID("svn_patch"),         wxEVT_COMMAND_MENU_SELECTED,  wxCommandEventHandler(SubversionView::OnPatch),       NULL, this);
	menu->Connect(XRCID("svn_patch_dry_run"), wxEVT_COMMAND_MENU_SELECTED,  wxCommandEventHandler(SubversionView::OnPatchDryRun), NULL, this);
}

void SubversionView::DoGetPaths(const wxTreeItemId& parent, wxArrayString& paths)
{
	if ( m_treeCtrl->ItemHasChildren(parent) == false ) {
		return;
	}

	wxTreeItemIdValue cookie;
	wxTreeItemId item = m_treeCtrl->GetFirstChild(parent, cookie);
	while ( item.IsOk() ) {
		SvnTreeData *data = (SvnTreeData *)m_treeCtrl->GetItemData(item);
		if (data) {
			if (data->GetFilepath().IsEmpty() == false) {
				paths.Add( data->GetFilepath() );
			}

			if (( data->GetType() == SvnTreeData::SvnNodeTypeAddedRoot    ||
			        data->GetType() == SvnTreeData::SvnNodeTypeModifiedRoot ||
			        data->GetType() == SvnTreeData::SvnNodeTypeDeletedRoot) &&

			        m_treeCtrl->ItemHasChildren(item)) {

				DoGetPaths(item, paths);
			}
		}
		item = m_treeCtrl->GetNextChild(parent, cookie);
	}
}

////////////////////////////////////////////
// Source control command handlers
////////////////////////////////////////////

void SubversionView::OnUpdate(wxCommandEvent& event)
{
	wxString command;
	command << m_plugin->GetSvnExeName(false) << wxT(" update ");

	if (m_selectionInfo.m_selectionType != SvnTreeData::SvnNodeTypeRoot) {
		// Concatenate list of files to be updated
		for (size_t i=0; i<m_selectionInfo.m_paths.GetCount(); i++) {
			command << wxT("\"") << m_selectionInfo.m_paths.Item(i) << wxT("\" ");
		}
	}

	m_plugin->GetShell()->Execute(command, m_textCtrlRootDir->GetValue(), new SvnUpdateHandler(m_plugin));
}

void SubversionView::OnCommit(wxCommandEvent& event)
{
	wxString command;

	// Pope the "Commit Dialog" dialog

	CommitDialog dlg(m_plugin->GetManager()->GetTheApp()->GetTopWindow(), m_selectionInfo.m_paths, m_plugin->GetManager());
	if (dlg.ShowModal() == wxID_OK) {
		m_selectionInfo.m_paths = dlg.GetPaths();
		if (m_selectionInfo.m_paths.IsEmpty())
			return;

		command << m_plugin->GetSvnExeName(true) << wxT(" commit ");

		for (size_t i=0; i<m_selectionInfo.m_paths.GetCount(); i++) {
			command << wxT("\"") << m_selectionInfo.m_paths.Item(i) << wxT("\" ");
		}

		command << wxT(" -m \"");
		command << dlg.GetMesasge();
		command << wxT("\"");
		m_plugin->GetShell()->Execute(command, m_textCtrlRootDir->GetValue(), new SvnCommitHandler(m_plugin, this));
	}
}


void SubversionView::OnAdd(wxCommandEvent& event)
{
	wxString command;
	command << m_plugin->GetSvnExeName(true) << wxT(" add ");

	// Concatenate list of files to be updated
	for (size_t i=0; i<m_selectionInfo.m_paths.GetCount(); i++) {
		command << wxT("\"") << m_selectionInfo.m_paths.Item(i) << wxT("\" ");
	}

	m_plugin->GetShell()->Execute(command, m_textCtrlRootDir->GetValue(), new SvnDefaultCommandHandler(m_plugin));
}

void SubversionView::OnRevert(wxCommandEvent& event)
{
	wxString command;
	command << m_plugin->GetSvnExeName(false) << wxT(" revert --recursive ");

	if (m_selectionInfo.m_selectionType != SvnTreeData::SvnNodeTypeRoot) {
		// Concatenate list of files to be updated
		for (size_t i=0; i<m_selectionInfo.m_paths.GetCount(); i++) {
			command << wxT("\"") << m_selectionInfo.m_paths.Item(i) << wxT("\" ");
		}
	} else {
		command << wxT(".");
	}
	m_plugin->GetShell()->Execute(command, m_textCtrlRootDir->GetValue(), new SvnDefaultCommandHandler(m_plugin));
}

void SubversionView::OnBranch(wxCommandEvent& event)
{
	wxString command;
	command << m_plugin->GetSvnExeName(false) << wxT("info --xml ");

	m_simpleCommand.Execute(command, m_textCtrlRootDir->GetValue(), new SvnInfoHandler(m_plugin, SvnInfo_Branch));
}

void SubversionView::OnTag(wxCommandEvent& event)
{
	wxString command;
	command << m_plugin->GetSvnExeName(false) << wxT("info --xml ");

	m_simpleCommand.Execute(command, m_textCtrlRootDir->GetValue(), new SvnInfoHandler(m_plugin, SvnInfo_Tag));
}

void SubversionView::OnDelete(wxCommandEvent& event)
{
	wxString command;
	command << m_plugin->GetSvnExeName(false) << wxT(" --force delete ");

	// Concatenate list of files to be updated
	for (size_t i=0; i<m_selectionInfo.m_paths.GetCount(); i++) {
		command << wxT("\"") << m_selectionInfo.m_paths.Item(i) << wxT("\" ");
	}

	m_plugin->GetShell()->Execute(command, m_textCtrlRootDir->GetValue(), new SvnDefaultCommandHandler(m_plugin));
}

void SubversionView::OnResolve(wxCommandEvent& event)
{
	wxString command;
	command << m_plugin->GetSvnExeName(false) << wxT(" resolved ");

	// Concatenate list of files to be updated
	for (size_t i=0; i<m_selectionInfo.m_paths.GetCount(); i++) {
		command << wxT("\"") << m_selectionInfo.m_paths.Item(i) << wxT("\" ");
	}

	m_plugin->GetShell()->Execute(command, m_textCtrlRootDir->GetValue(), new SvnDefaultCommandHandler(m_plugin));
}

void SubversionView::OnDiff(wxCommandEvent& event)
{
	wxString diffAgainst(wxT("BASE"));
	diffAgainst = wxGetTextFromUser(wxT("Insert base revision to diff against:"), wxT("Diff against"), wxT("BASE"));
	if (diffAgainst.empty()) {
		// user clicked 'Cancel'
		diffAgainst = wxT("BASE");
	}

	bool     useExtDiff = m_plugin->GetSettings().GetFlags() & SvnUseExternalDiff;
	wxString extDiff    = m_plugin->GetSettings().GetExternalDiffViewer();
	extDiff.Trim().Trim(false);

	// Only use external diff viewer when the selected file is equal to 1 and the selection is a file!
	if ( useExtDiff && extDiff.IsEmpty() == false && m_selectionInfo.m_paths.GetCount() == 1 && m_selectionInfo.m_selectionType == SvnTreeData::SvnNodeTypeFile ) {
		wxString extDiffCmd = m_plugin->GetSettings().GetExternalDiffViewerCommand();

		// export BASE revision of file to tmp file
		const wxString& base = wxFileName::CreateTempFileName( wxT("svnExport"), (wxFile*)NULL );
		::wxRemoveFile( base ); // just want the name, not the file.
		wxString exportCmd;
		exportCmd << m_plugin->GetSvnExeName(false);
		exportCmd << wxT("export -r ") << diffAgainst << wxT(" \"") << m_selectionInfo.m_paths.Item(0) << wxT("\" ") << base;

		// Launch export command
		wxArrayString output;
		ProcUtils::ExecuteCommand(exportCmd, output);

		// We now got 2 files:
		// m_selectionInfo.m_paths.Item(0) and 'base'
		extDiffCmd.Replace(wxT("$(MyFile)"),       wxString::Format( wxT("\"%s\""), m_selectionInfo.m_paths.Item(0).c_str()));
		extDiffCmd.Replace(wxT("$(OriginalFile)"), wxString::Format( wxT("\"%s\""), base.c_str()));

		wxString command;
		command << wxT("\"") << extDiff << wxT("\" ") << extDiffCmd;

		// Launch the external diff
		m_plugin->GetShell()->AppendText(command + wxT("\n"));
		m_diffCommand.Execute(command, m_textCtrlRootDir->GetValue(), NULL);
	} else {
		// Simple diff
		wxString command;
		command << m_plugin->GetSvnExeName(false) << wxT("diff -r") << diffAgainst << wxT(" ");
		for (size_t i=0; i<m_selectionInfo.m_paths.GetCount(); i++) {
			command << wxT("\"") << m_selectionInfo.m_paths.Item(i) << wxT("\" ");
		}
		m_plugin->GetShell()->Execute(command, m_textCtrlRootDir->GetValue(), new SvnDiffHandler(m_plugin));
	}
}

void SubversionView::OnPatch(wxCommandEvent& event)
{
	wxUnusedVar(event);
	DoPatch(false);
}

void SubversionView::OnPatchDryRun(wxCommandEvent& event)
{
	wxUnusedVar(event);
	DoPatch(true);
}

void SubversionView::OnSvnInfo(const SvnInfo& svnInfo, int reason)
{
	if (reason == SvnInfo_Info ) {
		SvnInfoDialog dlg(m_plugin->GetManager()->GetTheApp()->GetTopWindow());
		dlg.m_textCtrlAuthor->SetValue( svnInfo.m_author );
		dlg.m_textCtrlDate->SetValue( svnInfo.m_date );
		dlg.m_textCtrlRevision->SetValue( svnInfo.m_revision );
		dlg.m_textCtrlRootURL->SetValue( svnInfo.m_url );
		dlg.m_textCtrlURL->SetValue( svnInfo.m_sourceUrl );
		dlg.ShowModal();

	} else {

		SvnCopyDialog dlg(m_plugin->GetManager()->GetTheApp()->GetTopWindow());

		wxString baseUrlNoTrunk (svnInfo.m_sourceUrl);
		svnInfo.m_sourceUrl.EndsWith(wxT("trunk"), &baseUrlNoTrunk);

		switch (reason) {

		default:
		case SvnInfo_Tag:
			dlg.SetTitle(wxT("Create Tag"));
			dlg.SetSourceURL(svnInfo.m_sourceUrl);
			dlg.SetTargetURL(baseUrlNoTrunk + wxT("tags/"));
			break;

		case SvnInfo_Branch:
			dlg.SetTitle(wxT("Create Branch"));
			dlg.SetSourceURL(svnInfo.m_sourceUrl);
			dlg.SetTargetURL(baseUrlNoTrunk + wxT("branches/"));
			break;

		}
		if (dlg.ShowModal() == wxID_OK) {
			wxString command;
			command << m_plugin->GetSvnExeName(false)
			<< wxT("copy ")
			<< dlg.GetSourceURL()
			<< wxT(" ")
			<< dlg.GetTargetURL()
			<< wxT(" -m \"")
			<< dlg.GetMessage()
			<< wxT("\"");
			m_plugin->GetShell()->Execute(command, m_textCtrlRootDir->GetValue(), new SvnDefaultCommandHandler(m_plugin));
		}
	}
}

void SubversionView::OnCommitWithLogin(wxCommandEvent& event)
{
	SvnLoginDialog dlg(m_plugin->GetManager()->GetTheApp()->GetTopWindow());
	if (dlg.ShowModal() == wxID_OK) {
		wxString command;
		command << m_plugin->GetSvnExeName(true) << wxT(" commit --username ") << dlg.GetUsername() << wxT(" --password ") << dlg.GetPassword() << wxT(" ");

		// Pop the commit dialog message now
		CommitDialog commitdlg(m_plugin->GetManager()->GetTheApp()->GetTopWindow(), m_selectionInfo.m_paths, m_plugin->GetManager());
		if (commitdlg.ShowModal() == wxID_OK) {
			m_selectionInfo.m_paths = commitdlg.GetPaths();

			if (m_selectionInfo.m_paths.IsEmpty())
				return;

			for (size_t i=0; i<m_selectionInfo.m_paths.GetCount(); i++) {
				command << wxT("\"") << m_selectionInfo.m_paths.Item(i) << wxT("\" ");
			}

			command << wxT(" -m \"");
			command << commitdlg.GetMesasge();
			command << wxT("\"");
			m_plugin->GetShell()->Execute(command, m_textCtrlRootDir->GetValue(), new SvnCommitHandler(m_plugin, this), false);
		}
	}
}

void SubversionView::OnCleanup(wxCommandEvent& event)
{
	wxUnusedVar(event);
	wxString command;
	command << m_plugin->GetSvnExeName(false) << wxT(" cleanup ");
	m_plugin->GetShell()->Execute(command, m_textCtrlRootDir->GetValue(), NULL);
}

void SubversionView::OnStop(wxCommandEvent& event)
{
	wxUnusedVar(event);
	m_plugin->GetShell()->Stop();
}

void SubversionView::OnClearOuptut(wxCommandEvent& event)
{
	wxUnusedVar(event);
	m_plugin->GetShell()->Clear();
}

void SubversionView::DoPatch(bool dryRun)
{
	// open a file selector to select the patch file
	const wxString ALL(	wxT("Patch files (*.patch;*.diff)|*.patch;*.diff|")
	                    wxT("All Files (*)|*"));


	wxString patchFile = wxFileSelector(wxT("Select Patch File:"), NULL, NULL, NULL, ALL, 0, m_plugin->GetManager()->GetTheApp()->GetTopWindow());
	if (patchFile.IsEmpty() == false) {

		// try to load and convert the file into the platform line ending
		wxString fileContent;

		if (!ReadFileWithConversion(patchFile, fileContent)) {
			m_plugin->GetShell()->AppendText(wxString::Format(wxT("Failed to read patch file '%s'\n"), patchFile.c_str()));
			return;
		}

		// Make sure the file has \n line ending
		fileContent.Replace(wxT("\r\n"), wxT("\n"));

		wxString tmpFileName(patchFile + wxT(".tmp"));
		if (!WriteFileWithBackup(tmpFileName, fileContent, false)) {
			// failed to write the temporary file
			m_plugin->GetShell()->AppendText(wxString::Format(wxT("Failed to convert patch file EOL mode '%s'\n"), tmpFileName.c_str()));
			return;
		}

		// execute the command
		wxString command;
		command << wxT("patch -p0 ");
		if(dryRun)
			command << wxT(" --dry-run ");
		command << wxT(" -i \"") << tmpFileName << wxT("\"");

		SvnCommandHandler *handler(NULL);
		if(dryRun) {
			handler = new SvnPatchDryRunHandler(m_plugin);
		} else {
			handler = new SvnPatchHandler(m_plugin);
		}
		m_simpleCommand.Execute(command, m_textCtrlRootDir->GetValue(), handler);
	}
}

void SubversionView::OnRefreshView(wxCommandEvent& event)
{
	wxUnusedVar(event);
	BuildTree();
}

void SubversionView::OnFileRenamed(wxCommandEvent& event)
{
	wxArrayString *files = (wxArrayString*)event.GetClientData();
	if(files) {
		wxString oldName = files->Item(0);
		wxString newName = files->Item(1);
		wxString command;
		command << m_plugin->GetSvnExeName(false) << wxT(" rename \"") << oldName << wxT("\" \"") << newName << wxT("\"");
		m_plugin->GetShell()->Execute(command, m_textCtrlRootDir->GetValue(), new SvnStatusHandler(m_plugin));
	}

	event.Skip();
}

void SubversionView::OnShowSvnInfo(wxCommandEvent& event)
{
	wxString command;
	command << m_plugin->GetSvnExeName(false) << wxT("info --xml ");

	m_simpleCommand.Execute(command, m_textCtrlRootDir->GetValue(), new SvnInfoHandler(m_plugin, SvnInfo_Info));
}

void SubversionView::OnItemActivated(wxTreeEvent& event)
{
	wxArrayTreeItemIds items;
	wxArrayString      paths;
	size_t count = m_treeCtrl->GetSelections(items);
	for(size_t i=0; i<count; i++) {
		wxTreeItemId item = items.Item(i);

		if(item.IsOk() == false)
			continue;

		SvnTreeData *data = (SvnTreeData *)m_treeCtrl->GetItemData(item);
		if (data && data->GetType() == SvnTreeData::SvnNodeTypeFile) {
			paths.Add(m_textCtrlRootDir->GetValue() + wxFileName::GetPathSeparator() + data->GetFilepath());
		}
	}

	for(size_t i=0; i<paths.GetCount(); i++) {

		if(wxFileName(paths.Item(i)).IsDir() == false)
			m_plugin->GetManager()->OpenFile(paths.Item(i));

	}
}

void SubversionView::OnStopUI(wxUpdateUIEvent& event)
{
	event.Enable(m_plugin->GetShell()->IsRunning());
}

void SubversionView::OnClearOuptutUI(wxUpdateUIEvent& event)
{
	event.Enable(m_plugin->GetShell()->IsEmpty() == false);
}

void SubversionView::OnCheckout(wxCommandEvent& event)
{
}
