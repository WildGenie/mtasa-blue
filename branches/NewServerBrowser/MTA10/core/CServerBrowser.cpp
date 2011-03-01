/*****************************************************************************
*
*  PROJECT:     Multi Theft Auto v1.0
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        core/CServerBrowser.cpp
*  PURPOSE:     In-game server browser user interface
*  DEVELOPERS:  Cecill Etheredge <ijsf@gmx.net>
*               Stanislav Bobrov <lil_toady@hotmail.com>
*               Alberto Alonso <rydencillo@gmail.com>
*               Florian Busse <flobu@gmx.net>
*               Sebas Lamers <sebasdevelopment@gmx.com>
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#include "StdInc.h"

using namespace std;

extern CCore* g_pCore;

template<> CServerBrowser * CSingleton < CServerBrowser >::m_pSingleton = NULL;


#define SB_SPAN             0.85f  //How much % of the screen the server browser should fill
#define SB_NAVBAR_SIZE_Y    40 // Navbar button size
#define SB_BUTTON_SIZE_X    26
#define SB_BUTTON_SIZE_Y    26
#define SB_SPACER           10 //Spacer between searchbar and navbar
#define SB_SMALL_SPACER     5
#define SB_SEARCHBAR_COMBOBOX_SIZE_X   45  // Mow much the search type combobox occupies of searchbar
#define SB_SEARCHBAR_COMBOBOX_SIZE_Y   22
#define SB_SEARCHBAR_SIZE_X     200  // Width of search bar
#define SB_PLAYERLIST_SIZE_X    200  // Width of players list [NB. adjusted for low resolutions in CServerBrowser::CreateTab]
#define SB_BACK_BUTTON_SIZE_Y   40  // Size of the back butt
#define COMBOBOX_ARROW_SIZE_X   23  //Fixed CEGUI size of the 'combobox' arrow
#define TAB_SIZE_Y              25  //Fixed CEGUI size of the Tab in a tab panel

#define PLAYER_LIST_PENDING_TEXT "  ..loading.."

CServerBrowser::CServerBrowser ( void )
{
    CGUI *pManager = g_pCore->GetGUI ();

    // Initialize
    m_ulLastUpdateTime = 0;
    m_firstTimeBrowseServer = true;
    m_bOptionsLoaded = false;
    m_PrevServerBrowserType = INTERNET;

    // Do some initial math
    CVector2D resolution = CCore::GetSingleton().GetGUI()->GetResolution();

	if ( resolution.fX <= 800 )  //Make our window bigger at small resolutions
    {
        m_WidgetSize = CVector2D( resolution.fX, resolution.fY );
    }
    else
        m_WidgetSize = CVector2D( resolution.fX*SB_SPAN, resolution.fY*SB_SPAN );

    // Create the serverlist tab panel and some tabs
    m_pPanel = reinterpret_cast < CGUITabPanel* > ( pManager->CreateTabPanel() );
    m_pPanel->SetPosition ( CVector2D ( (resolution.fX - m_WidgetSize.fX)/2, (resolution.fY - m_WidgetSize.fY)/2 ) );
    m_pPanel->SetSize ( m_WidgetSize );
    m_pPanel->SetAlwaysOnTop ( true );

    // Create locked icon
    m_pLockedIcon = reinterpret_cast < CGUIStaticImage* > ( pManager->CreateStaticImage () );
    m_pLockedIcon->SetVisible ( false );
    m_pLockedIcon->SetFrameEnabled ( false );
    m_pLockedIcon->LoadFromFile ( "cgui\\images\\serverbrowser\\locked.png" );

    // Create search filter types icon
    m_pSearchPlayersIcon = reinterpret_cast < CGUIStaticImage* > ( pManager->CreateStaticImage () );
    m_pSearchPlayersIcon->SetVisible ( false );
    m_pSearchPlayersIcon->SetFrameEnabled ( false );
    m_pSearchPlayersIcon->LoadFromFile ( "cgui\\images\\serverbrowser\\search-players.png" );

    m_pSearchServersIcon = reinterpret_cast < CGUIStaticImage* > ( pManager->CreateStaticImage () );
    m_pSearchServersIcon->SetVisible ( false );
    m_pSearchServersIcon->SetFrameEnabled ( false );
    m_pSearchServersIcon->LoadFromFile ( "cgui\\images\\serverbrowser\\search-servers.png" );


    // Create the tabs
    CreateTab ( ServerBrowserType::INTERNET, "Internet" );
    CreateTab ( ServerBrowserType::LAN, "Local" );
    CreateTab ( ServerBrowserType::FAVOURITES, "Favourites" );
    CreateTab ( ServerBrowserType::RECENTLY_PLAYED, "Recent" );

    // Login dialog
    m_pCommunityLogin.SetVisible ( false );
    m_pCommunityLogin.SetCallback ( &CServerBrowser::CompleteConnect );

    // Load options
    LoadOptions ( CCore::GetSingletonPtr ()->GetConfig ( )->FindSubNode ( CONFIG_NODE_SERVER_OPTIONS ) );

    // Save the active tab, needs to be done after at least one tab exists
    m_pPanel->SetSelectionHandler ( GUI_CALLBACK( &CServerBrowser::OnTabChanged, this ) );
}


CServerBrowser::~CServerBrowser ( void )
{
    // Delete the Tabs
    DeleteTab ( ServerBrowserType::INTERNET );
    DeleteTab ( ServerBrowserType::LAN );
    DeleteTab ( ServerBrowserType::FAVOURITES );
    DeleteTab ( ServerBrowserType::RECENTLY_PLAYED );

    // Unload the icon
    m_pLockedIcon->Clear();
    m_pSearchPlayersIcon->Clear();
    m_pSearchServersIcon->Clear();

    // Delete the GUI items
    delete m_pPanel;
}

void CServerBrowser::CreateTab ( ServerBrowserType type, const char* szName )
{
    CGUI *pManager = g_pCore->GetGUI ();

    m_pTab [ type ] = m_pPanel->CreateTab ( szName );

    float fPlayerListSizeX = SB_PLAYERLIST_SIZE_X;
    float fSearchBarSizeX = SB_SEARCHBAR_SIZE_X;

	//Make our playerlist smaller, if it's a small panel - either 15%, or a max of 200px in size.
    fPlayerListSizeX = min(m_WidgetSize.fX*0.15, SB_PLAYERLIST_SIZE_X);

    // Formulate our navigation bar

    float fX = SB_SMALL_SPACER;
    float fY = (SB_NAVBAR_SIZE_Y-SB_BUTTON_SIZE_X)/2;
    
    // Refresh button + icon
    m_pButtonRefresh [ type ] = reinterpret_cast < CGUIButton* > ( pManager->CreateButton ( m_pTab [ type ], "" ) );
    m_pButtonRefresh [ type ]->SetPosition ( CVector2D ( fX, fY ), false );
    m_pButtonRefresh [ type ]->SetSize ( CVector2D ( SB_BUTTON_SIZE_X, SB_BUTTON_SIZE_Y ), false );
    m_pButtonRefresh [ type ]->SetClickHandler ( GUI_CALLBACK ( &CServerBrowser::OnRefreshClick, this ) );
    m_pButtonRefreshIcon [ type ] = reinterpret_cast < CGUIStaticImage* > ( pManager->CreateStaticImage ( m_pButtonRefresh [ type ] ) );
    m_pButtonRefreshIcon [ type ]->SetSize ( CVector2D(1,1), true );
    m_pButtonRefreshIcon [ type ]->LoadFromFile ( "cgui\\images\\serverbrowser\\refresh.png" );
    m_pButtonRefreshIcon [ type ]->SetProperty ( "MousePassThroughEnabled","True" );

    // Address Bar + History Combo
	fX = fX + SB_BUTTON_SIZE_X + SB_SMALL_SPACER;
	    // Work out our size by calculating from the end - minus the searchbox, combobox, spacing, info button, play button
	float fWidth = m_WidgetSize.fX - SB_SMALL_SPACER - fSearchBarSizeX - SB_SPACER - SB_BUTTON_SIZE_X - SB_SMALL_SPACER - SB_BUTTON_SIZE_X - SB_SMALL_SPACER - COMBOBOX_ARROW_SIZE_X - fX;
    m_pEditAddress [ type ] = reinterpret_cast < CGUIEdit* > ( pManager->CreateEdit ( m_pTab [ type ], "" ) );
    m_pEditAddress [ type ]->SetPosition ( CVector2D ( fX, fY ), false );
    m_pEditAddress [ type ]->SetSize ( CVector2D ( fWidth, SB_BUTTON_SIZE_Y ), false);
    m_pEditAddress [ type ]->SetTextAcceptedHandler ( GUI_CALLBACK ( &CServerBrowser::OnConnectClick, this ) );
    m_pEditAddress [ type ]->SetTextChangedHandler ( GUI_CALLBACK ( &CServerBrowser::OnAddressChanged, this ) );

        // Favourite icon
    m_pAddressFavoriteIcon [ type ] = reinterpret_cast < CGUIStaticImage* > ( pManager->CreateStaticImage ( m_pEditAddress [ type ] ) );
    m_pAddressFavoriteIcon [ type ]->SetPosition ( CVector2D ( fWidth - 20 - 8, (SB_BUTTON_SIZE_Y-16)/2 ), false );
    m_pAddressFavoriteIcon [ type ]->SetSize ( CVector2D ( 16, 16 ), false );
    m_pAddressFavoriteIcon [ type ]->LoadFromFile ( "cgui\\images\\serverbrowser\\favorite.png" );
    m_pAddressFavoriteIcon [ type ]->SetAlpha ( 0.3f );
    m_pAddressFavoriteIcon [ type ]->SetClickHandler ( GUI_CALLBACK ( &CServerBrowser::OnFavouritesClick, this ) );

        // History Combo
	fWidth = fWidth + COMBOBOX_ARROW_SIZE_X;
    m_pComboAddressHistory [ type ] = reinterpret_cast < CGUIComboBox* > ( pManager->CreateComboBox ( m_pTab [ type ], "" ) );
    m_pComboAddressHistory [ type ]->SetPosition ( CVector2D ( fX-1, fY + (SB_BUTTON_SIZE_Y-SB_SEARCHBAR_COMBOBOX_SIZE_Y)/2 ), false );
    m_pComboAddressHistory [ type ]->SetSize ( CVector2D ( fWidth, 100 ), false );
    m_pComboAddressHistory [ type ]->AddItem ( "1231.124.124" );
    m_pComboAddressHistory [ type ]->AddItem ( "34.1515.123" );
    m_pComboAddressHistory [ type ]->SetReadOnly ( true );

    // Connect button + icon
	fX = fX + fWidth + SB_SMALL_SPACER;
    m_pButtonConnect [ type ] = reinterpret_cast < CGUIButton* > ( pManager->CreateButton ( m_pTab [ type ], "" ) );
    m_pButtonConnect [ type ]->SetPosition ( CVector2D ( fX, fY ), false );
    m_pButtonConnect [ type ]->SetSize ( CVector2D ( SB_BUTTON_SIZE_X, SB_BUTTON_SIZE_Y ), false );
    m_pButtonConnect [ type ]->SetClickHandler ( GUI_CALLBACK ( &CServerBrowser::OnConnectClick, this ) );
    m_pButtonConnectIcon [ type ] = reinterpret_cast < CGUIStaticImage* > ( pManager->CreateStaticImage ( m_pButtonConnect [ type ] ) );
    m_pButtonConnectIcon [ type ]->SetSize ( CVector2D(1,1), true );
    m_pButtonConnectIcon [ type ]->LoadFromFile ( "cgui\\images\\serverbrowser\\connect.png" );
    m_pButtonConnectIcon [ type ]->SetProperty ( "MousePassThroughEnabled","True" );

    // Info button + icon
    fX = fX + SB_BUTTON_SIZE_X + SB_SMALL_SPACER;
    m_pButtonInfo [ type ] = reinterpret_cast < CGUIButton* > ( pManager->CreateButton ( m_pTab [ type ], "" ) );
    m_pButtonInfo [ type ]->SetPosition ( CVector2D ( fX, fY ), false );
    m_pButtonInfo [ type ]->SetSize ( CVector2D ( SB_BUTTON_SIZE_X, SB_BUTTON_SIZE_Y ), false );
    m_pButtonInfoIcon [ type ] = reinterpret_cast < CGUIStaticImage* > ( pManager->CreateStaticImage ( m_pButtonInfo [ type ] ) );
    m_pButtonInfoIcon [ type ]->SetSize ( CVector2D(1,1), true );
    m_pButtonInfoIcon [ type ]->LoadFromFile ( "cgui\\images\\serverbrowser\\info.png" );
    m_pButtonInfoIcon [ type ]->SetProperty ( "MousePassThroughEnabled","True" );

    // Search bar + type combo
	fX = m_WidgetSize.fX - SB_SMALL_SPACER - fSearchBarSizeX;
    m_pComboSearchType [ type ] = reinterpret_cast < CGUIComboBox* > ( pManager->CreateComboBox ( m_pTab [ type ], "" ) );
    m_pComboSearchType [ type ]->SetPosition ( CVector2D ( fX, fY + (SB_BUTTON_SIZE_Y-SB_SEARCHBAR_COMBOBOX_SIZE_Y)/2 ), false );
    m_pComboSearchType [ type ]->SetSize ( CVector2D ( SB_SEARCHBAR_COMBOBOX_SIZE_X, 68 ), false );
    m_pComboSearchType [ type ]->AddItem ( "" );
    m_pComboSearchType [ type ]->AddItem ( "" );
    m_pComboSearchType [ type ]->SetItemImage ( 0, m_pSearchServersIcon );
    m_pComboSearchType [ type ]->SetItemImage ( 1, m_pSearchPlayersIcon );
    m_pComboSearchType [ type ]->SetReadOnly ( true );

    fWidth = fSearchBarSizeX-SB_SEARCHBAR_COMBOBOX_SIZE_X;
    m_pEditSearch [ type ] = reinterpret_cast < CGUIEdit* > ( pManager->CreateEdit ( m_pTab [ type ], "" ) );
    m_pEditSearch [ type ]->SetPosition ( CVector2D ( fX+SB_SEARCHBAR_COMBOBOX_SIZE_X, fY + (SB_BUTTON_SIZE_Y-SB_SEARCHBAR_COMBOBOX_SIZE_Y)/2 ), false );
    m_pEditSearch [ type ]->SetSize ( CVector2D ( fWidth, SB_SEARCHBAR_COMBOBOX_SIZE_Y ), false );

    // Server search icon
    m_pServerSearchIcon [ type ] = reinterpret_cast < CGUIStaticImage* > ( pManager->CreateStaticImage ( m_pEditSearch [ type ] ) );
    m_pServerSearchIcon [ type ]->SetPosition ( CVector2D ( fWidth - 18, (SB_SEARCHBAR_COMBOBOX_SIZE_Y-16)/2 ), false );
    m_pServerSearchIcon [ type ]->SetSize ( CVector2D ( 16, 16 ), false );
    m_pServerSearchIcon [ type ]->LoadFromFile ( "cgui\\images\\serverbrowser\\search.png" );

    // Create the serverlist
	fX = 5;
	fY = fY + SB_BUTTON_SIZE_Y + (SB_NAVBAR_SIZE_Y-SB_BUTTON_SIZE_X)/2;
    fWidth = m_WidgetSize.fX - SB_SMALL_SPACER - fPlayerListSizeX - fX;
    float fHeight = m_WidgetSize.fY - SB_BACK_BUTTON_SIZE_Y - fY - TAB_SIZE_Y;
    m_pServerList [ type ] = reinterpret_cast < CGUIGridList* > ( pManager->CreateGridList ( m_pTab [ type ] ) );
    m_pServerList [ type ]->SetPosition ( CVector2D ( fX, fY ), false );
    m_pServerList [ type ]->SetSize ( CVector2D ( fWidth, fHeight ), false );
    m_pServerList [ type ]->SetIgnoreTextSpacer ( true );
    m_pServerListRevision [ type ] = 0;

    // Server List Columns
    m_hVersion [ type ] = m_pServerList [ type ]->AddColumn ( "", 0.2f );
    m_hLocked [ type ] = m_pServerList [ type ]->AddColumn ( "", 0.2f );
    m_hName [ type ] = m_pServerList [ type ]->AddColumn ( "Name", 0.2f );
    m_hPlayers [ type ] = m_pServerList [ type ]->AddColumn ( "Players", 0.2f );
    m_hPing [ type ] = m_pServerList [ type ]->AddColumn ( "Ping", 0.2f );
    m_hGame [ type ] = m_pServerList [ type ]->AddColumn ( "Gamemode", 0.2f );

    // NB. SetColumnWidth seems to start from 0
    m_pServerList [ type ]->SetColumnWidth ( m_hVersion [ type ]-1, 25, false );
    m_pServerList [ type ]->SetColumnWidth ( m_hLocked [ type ]-1, 16, false );
    m_pServerList [ type ]->SetColumnWidth ( m_hPlayers [ type ]-1, 70, false );
    m_pServerList [ type ]->SetColumnWidth ( m_hPing [ type ]-1, 35, false );

    // We give Name and Gamemode 65% and 35% of the remaining length respectively
    float fRemainingWidth = fWidth - 25 - 16 - 70 - 35 - 50; // All the fixed sizes plus 50 for the scrollbar
    
    m_pServerList [ type ]->SetColumnWidth ( m_hGame [ type ]-1, fRemainingWidth*0.35, false );
    m_pServerList [ type ]->SetColumnWidth ( m_hName [ type ]-1, fRemainingWidth*0.65, false );

    // Server player list;
	fX = fX + fWidth;
	fHeight = m_WidgetSize.fY - SB_BACK_BUTTON_SIZE_Y - fY - TAB_SIZE_Y;
    m_pServerPlayerList [ type ] = reinterpret_cast < CGUIGridList* > ( pManager->CreateGridList ( m_pTab [ type ] ) );
    m_pServerPlayerList [ type ]->SetPosition ( CVector2D ( fX, fY ), false );
    m_pServerPlayerList [ type ]->SetSize ( CVector2D ( fPlayerListSizeX, fHeight ), false );
    m_pServerPlayerList [ type ]->SetIgnoreTextSpacer ( true );
    // Player List Columns
    m_hPlayerName [ type ] = m_pServerPlayerList [ type ]->AddColumn ( "Player list", 0.75f );

    // Filters
    float fLineHeight = SB_BACK_BUTTON_SIZE_Y/2;
	fX = SB_SMALL_SPACER;
    fY = m_WidgetSize.fY - SB_BACK_BUTTON_SIZE_Y - TAB_SIZE_Y;

    // Include label
    m_pLabelInclude [ type ] = reinterpret_cast < CGUILabel* > ( pManager->CreateLabel ( m_pTab [ type ], "Include:" ) );
    m_pLabelInclude [ type ]->SetPosition ( CVector2D ( fX, fY ), false ); 
    m_pLabelInclude [ type ]->AutoSize ( m_pLabelInclude [ type ]->GetText ().c_str () );

    // Include checkboxes
	fX = fX + m_pLabelInclude [ type ]->GetTextExtent() + SB_SPACER;
    m_pIncludeEmpty [ type ] = reinterpret_cast < CGUICheckBox* > ( pManager->CreateCheckBox ( m_pTab [ type ], "Empty", true ) );
    m_pIncludeEmpty [ type ]->SetPosition ( CVector2D ( fX, fY ), false );
    m_pIncludeEmpty [ type ]->SetSize ( CVector2D ( 53, 17 ) );
    m_pIncludeEmpty [ type ]->SetClickHandler ( GUI_CALLBACK ( &CServerBrowser::OnFilterChanged, this ) );

    fX = fX + 53 + SB_SPACER;
    m_pIncludeFull [ type ] = reinterpret_cast < CGUICheckBox* > ( pManager->CreateCheckBox ( m_pTab [ type ], "Full", true ) );
    m_pIncludeFull [ type ]->SetPosition ( CVector2D ( fX, fY ), false );
    m_pIncludeFull [ type ]->SetSize ( CVector2D ( 35, 17 ) );
    m_pIncludeFull [ type ]->SetClickHandler ( GUI_CALLBACK ( &CServerBrowser::OnFilterChanged, this ) );

    fX = fX + 35 + SB_SPACER;
    m_pIncludeLocked [ type ] = reinterpret_cast < CGUICheckBox* > ( pManager->CreateCheckBox ( m_pTab [ type ], "Locked", true ) );
    m_pIncludeLocked [ type ]->SetPosition ( CVector2D ( fX, fY ), false );
    m_pIncludeLocked [ type ]->SetSize ( CVector2D ( 57, 17 ) );
    m_pIncludeLocked [ type ]->SetClickHandler ( GUI_CALLBACK ( &CServerBrowser::OnFilterChanged, this ) );

#if MTA_DEBUG
    if ( type != ServerBrowserType::LAN )
#else
    if ( type != ServerBrowserType::INTERNET && type != ServerBrowserType::LAN )
#endif
    {
        fX = fX + 57 + SB_SPACER;
        m_pIncludeOffline [ type ] = reinterpret_cast < CGUICheckBox* > ( pManager->CreateCheckBox ( m_pTab [ type ], "Offline", true ) );
        m_pIncludeOffline [ type ]->SetPosition ( CVector2D ( fX, fY ), false );
        m_pIncludeOffline [ type ]->SetSize ( CVector2D ( 53, 17 ) );
        m_pIncludeOffline [ type ]->SetClickHandler ( GUI_CALLBACK ( &CServerBrowser::OnFilterChanged, this ) );
    }
    else
    {
        m_pIncludeOffline [ type ] = NULL;
    }

    fX = fX + 60 + SB_SPACER*2;
    m_pIncludeOtherVersions [ type ] = reinterpret_cast < CGUICheckBox* > ( pManager->CreateCheckBox ( m_pTab [ type ], "Other Versions", false ) );
    m_pIncludeOtherVersions [ type ]->SetPosition ( CVector2D ( fX, fY ), false );
    m_pIncludeOtherVersions [ type ]->SetSize ( CVector2D ( 99, 17 ) );
    m_pIncludeOtherVersions [ type ]->SetClickHandler ( GUI_CALLBACK ( &CServerBrowser::OnFilterChanged, this ) );
    m_pIncludeOtherVersions [ type ]->SetVisible ( false );

    // Status bar
	fX = 5;
	fY = fY + fLineHeight;

    m_pServerListStatus [ type ] = reinterpret_cast < CGUILabel* > ( pManager->CreateLabel ( m_pTab [ type ], "Loading..." ) );
    m_pServerListStatus [ type ]->SetPosition ( CVector2D ( fX, fY ) );
    m_pServerListStatus [ type ]->SetSize ( CVector2D ( 100, fLineHeight ), true );

    // Back button
    fX = m_WidgetSize.fX - fPlayerListSizeX - SB_SMALL_SPACER;
    fY = m_WidgetSize.fY - SB_BACK_BUTTON_SIZE_Y - TAB_SIZE_Y;
    m_pButtonBack [ type ] = reinterpret_cast < CGUIButton* > ( pManager->CreateButton ( m_pTab [ type ], "Back" ) );
    m_pButtonBack [ type ]->SetPosition ( CVector2D ( fX, fY ), false );
    m_pButtonBack [ type ]->SetSize ( CVector2D ( fPlayerListSizeX, SB_BACK_BUTTON_SIZE_Y ), false );
    m_pButtonBack [ type ]->SetClickHandler ( GUI_CALLBACK ( &CServerBrowser::OnBackClick, this ) );

    // Disable resizing of the first and second columns (Version & Locked)
    m_pServerList [ type ]->SetColumnSegmentSizingEnabled(0, false);
    m_pServerList [ type ]->SetColumnSegmentSizingEnabled(1, false);

    // Bring our combo box to front so it doesnt get stuck behind server list
	m_pServerList [ type ]->MoveToBack();
    m_pComboAddressHistory [ type ]->BringToFront();
    m_pEditAddress [ type ]->BringToFront();
    m_pComboAddressHistory [ type ]->SetZOrderingEnabled ( false );
    m_pServerList [ type ]->SetZOrderingEnabled ( false );
}

void CServerBrowser::DeleteTab ( ServerBrowserType type )
{
    delete m_pButtonConnect [ type ];
    delete m_pButtonRefresh [ type ];

    delete m_pEditAddress [ type ];
    delete m_pComboSearchType [ type ];
    delete m_pButtonInfoIcon [ type ];
    delete m_pButtonConnectIcon [ type ];
    delete m_pComboAddressHistory [ type ];
    delete m_pAddressFavoriteIcon [ type ];
    delete m_pEditSearch [ type ];
    delete m_pButtonRefreshIcon [ type ];
    
    delete m_pLabelInclude [ type ];
    delete m_pIncludeEmpty [ type ];
    delete m_pIncludeFull [ type ];
    delete m_pIncludeLocked [ type ];
    delete m_pIncludeOtherVersions [ type ];

    if ( m_pIncludeOffline [ type ] )
    {
        delete m_pIncludeOffline [ type ];
    }

    delete m_pServerPlayerList [ type ];
    
    delete m_pServerListStatus [ type ];

    delete m_pServerList [ type ];
    delete m_pTab [ type ];
}

CServerBrowser::ServerBrowserType CServerBrowser::GetCurrentServerBrowserType ( void )
{ 
    ServerBrowserType currentServerBrowserType;

    if ( m_pPanel->IsTabSelected ( m_pTab [ ServerBrowserType::FAVOURITES ] ) )
    {
        currentServerBrowserType = ServerBrowserType::FAVOURITES;
    }
    else if ( m_pPanel->IsTabSelected ( m_pTab [ ServerBrowserType::RECENTLY_PLAYED ] ) )
    {
        currentServerBrowserType = ServerBrowserType::RECENTLY_PLAYED;
    }
    else if ( m_pPanel->IsTabSelected ( m_pTab [ ServerBrowserType::LAN ] ) )
    {
        currentServerBrowserType = ServerBrowserType::LAN;
    }
    else
    {
        currentServerBrowserType = ServerBrowserType::INTERNET;
    }

    return currentServerBrowserType;
}

void CServerBrowser::Update ( void )
{
    ServerBrowserType Type = GetCurrentServerBrowserType ();
    CServerList *pList = GetServerList ( Type );

    // Update the current server list class
    if ( IsVisible () )
        pList->Pulse ();

    // If an update is needed, the serverbrowser is visible and it has gone some time since last update
    if ( ( pList->IsUpdated () || m_PrevServerBrowserType != Type ) && m_ulLastUpdateTime < CClientTime::GetTime () - SERVER_BROWSER_UPDATE_INTERVAL )
    {
        // Update the GUI
        UpdateServerList ( Type , Type == RECENTLY_PLAYED );

        // Set the status string
        SetStatusText ( pList->GetStatus () );

        // Update last time updated
        m_ulLastUpdateTime = CClientTime::GetTime ();

        // Update last viewed tab
        m_PrevServerBrowserType = Type;

        UpdateSelectedServerPlayerList ( Type );
    }

}

void CServerBrowser::SetVisible ( bool bVisible )
{
    m_pPanel->SetVisible ( bVisible );
    m_pPanel->BringToFront ();

    // Are we making this window visible?
    if ( bVisible )
    {
        if ( m_firstTimeBrowseServer )
        {
            // Start loading all servers
            for ( unsigned int i = 0; i < SERVER_BROWSER_TYPE_COUNT; i++ )
            {
                //m_pEditPassword [ i ]->SetText( "" );
                m_iSelectedServer [ i ] = -1;
                GetServerList ( (ServerBrowserType)i )->Refresh ();
            }
            m_firstTimeBrowseServer = false;
        }
    }
}


bool CServerBrowser::IsVisible ( void )
{
    return m_pPanel->IsVisible ();
}


void CServerBrowser::UpdateServerList ( ServerBrowserType Type, bool bClearServerList )
{
    GetVersionUpdater ()->GetBlockedVersionMap ( m_blockedVersionMap );

#if MTA_DEBUG
    // Selecting 'Nightly update' in debug build will show other versions
    SString strUpdateBuildType;
    CVARS_GET ( "update_build_type", strUpdateBuildType );
    if ( strUpdateBuildType == "2" )
        m_blockedVersionMap.clear ();
#endif

    // Setting this in coreconfig will show other versions
    bool bForceBrowseOtherVersions = false;
    CVARS_GET ( "force_browse_other_versions", bForceBrowseOtherVersions );
    if ( bForceBrowseOtherVersions )
        m_blockedVersionMap.clear ();

    // Save sort info
    uint uiSortColumn;
    CGUIGridList::SortDirection sortDirection;
    m_pServerList [ Type ]->GetSort( uiSortColumn, sortDirection );

    // Disable sorting
    m_pServerList [ Type ]->Sort( uiSortColumn, CGUIGridList::SortDirection::None );

    // Get the appropriate server list
    CServerList* pList = GetServerList ( Type );

    if ( pList->GetRevision () != m_pServerListRevision [ Type ] || bClearServerList )
    {
        m_pServerListRevision [ Type ] = pList->GetRevision ();

        // Clear current list
        m_pServerList [ Type ]->Clear ();

        // Clear the player lists
        m_pServerPlayerList [ Type ]->Clear ();
    }

    bool bIncludeOffline = m_pIncludeOffline [ Type ] && m_pIncludeOffline [ Type ]->GetSelected ();

    // Loop the server list
    for ( CServerListIterator it = pList->IteratorBegin () ; it != pList->IteratorEnd (); it++ )
    {
        CServerListItem * pServer = *it;

        // Add/update/remove the item to the list
        if ( pServer->revisionInList[ Type ] != pServer->uiRevision || bClearServerList )
        {
            pServer->revisionInList[ Type ] = pServer->uiRevision;
            AddServerToList ( pServer, Type );
        }
    }
    bool bIncludeOtherVersions = m_pIncludeOtherVersions [ Type ]->IsVisible () && m_pIncludeOtherVersions [ Type ]->GetSelected ();
    ServerBrowserType type = Type;

    if ( bIncludeOtherVersions )
    {
        m_pServerList [ type ]->SetColumnWidth ( 0, 34, false );
    }
    else
    {
        m_pServerList [ type ]->SetColumnWidth ( 0, 0.03f, true );
    }

    // Re-enable sorting
    m_pServerList [ Type ]->Sort( uiSortColumn, sortDirection );

    m_pServerList [ Type ]->ForceUpdate ();
    pList->SetUpdated ( false );
}


void CServerBrowser::AddServerToList ( const CServerListItem * pServer, const ServerBrowserType Type )
{
    bool bIncludeEmpty  = m_pIncludeEmpty [ Type ]->GetSelected ();
    bool bIncludeFull   = m_pIncludeFull [ Type ]->GetSelected ();
    bool bIncludeLocked = m_pIncludeLocked [ Type ]->GetSelected ();
    bool bIncludeOffline = m_pIncludeOffline [ Type ] && m_pIncludeOffline [ Type ]->GetSelected ();
    bool bIncludeOtherVersions = m_pIncludeOtherVersions [ Type ]->IsVisible () && m_pIncludeOtherVersions [ Type ]->GetSelected ();
    bool bServerSearchFound = true;
    bool bPlayerSearchFound = true;

    /*std::string strServerSearchText = m_pEditServerSearch [ Type ]->GetText ();
    std::string strPlayerSearchText = m_pEditPlayerSearch [ Type ]->GetText ();

    if ( !strServerSearchText.empty() )
    {
        // Search for the search text in the servername
        SString strServerName = pServer->strName;
        bServerSearchFound = strServerName.ContainsI ( strServerSearchText );
    }

    if ( !strPlayerSearchText.empty() )
    {
        bPlayerSearchFound = false;

        if ( pServer->nPlayers > 0 )
        {
            // Search for the search text in the names of the players in the server
            for ( unsigned int i = 0; i < pServer->vecPlayers.size (); i++ ) 
            {
                SString strPlayerName = pServer->vecPlayers[i];

                if ( strPlayerName.ContainsI ( strPlayerSearchText ) )
                {
                    bPlayerSearchFound = true;
                    int k = m_pServerPlayerList [ Type ]->AddRow ( true );
                    m_pServerPlayerList [ Type ]->SetItemText ( k, m_hPlayerName [ Type ], strPlayerName.c_str (), false, false, true );
                }
            }
        }
    }
    */

    //
    // Add or remove ?
    //

    bool bAddServer;

    bool bIsOtherVersion    = ( !pServer->strVersion.empty () ) && ( pServer->strVersion != MTA_DM_ASE_VERSION );
    bool bIsOffline         = ( pServer->bSkipped ) || ( pServer->MaybeWontRespond () );
    bool bWasGoodNowFailing = ( !bIsOffline ) && ( pServer->bMaybeOffline );
    bool bLowQuality        = ( pServer->GetDataQuality () <= SERVER_INFO_ASE_0 );
    bool bIsEmpty           = ( pServer->nPlayers == 0 ) && ( pServer->nMaxPlayers != 0 );
    bool bIsFull            = ( pServer->nPlayers >= pServer->nMaxPlayers ) && ( pServer->nMaxPlayers != 0 );
    bool bIsLocked          = pServer->bPassworded;
    bool bIsBlockedVersion  = bIsOtherVersion && MapContains ( m_blockedVersionMap, pServer->strVersion );
    bool bIsBlockedServer   = ( pServer->uiMasterServerSaysRestrictions & ASE_FLAG_RESTRICTIONS ) != false;

    // Maybe switch on 'Other version' checkbox
    if ( bIsOtherVersion && !bIsBlockedVersion )
    {
        if ( !m_pIncludeOtherVersions [ Type ]->IsVisible () )
        {
            m_pIncludeOtherVersions [ Type ]->SetSelected ( true );
            m_pIncludeOtherVersions [ Type ]->SetVisible ( true );
        }
    }

    if (
        ( !pServer->strVersion.empty () || bIsOffline ) &&
        ( !bLowQuality || bIsOffline ) &&
        ( !bIsEmpty || bIncludeEmpty ) &&
        ( !bIsFull || bIncludeFull ) &&
        ( !bIsLocked || bIncludeLocked ) &&
        ( !bIsOffline || bIncludeOffline || bWasGoodNowFailing ) &&
        ( !bIsOtherVersion || bIncludeOtherVersions ) &&
        ( !bIsBlockedVersion ) &&
        ( !bIsBlockedServer ) &&
        ( bServerSearchFound ) &&
        ( bPlayerSearchFound )
       )
    {
        bAddServer = true;
    }
    else
    {
        bAddServer = false;
    }


    if ( !bAddServer )
    {
        //
        // Remove server from list
        //

        int iIndex = FindRowFromServer ( Type, pServer );
        if ( iIndex != -1 )
        {
            m_pServerList [ Type ]->RemoveRow ( iIndex );
        }
    }
    else
    {
        //
        // Add/update server in list
        //

        // Get existing row or create a new row if not found
        int iIndex = FindRowFromServer ( Type, pServer );
        if ( iIndex == - 1 )
            iIndex = m_pServerList [ Type ]->AddRow ( true );

        const SString strVersion          = !bIncludeOtherVersions ? "" : pServer->strVersion;
        const SString strVersionSortKey   = pServer->strVersionSortKey + pServer->strTieBreakSortKey;

        const SString strPlayers          = pServer->nMaxPlayers == 0 ? "" : SString ( "%d / %d", pServer->nPlayers, pServer->nMaxPlayers );
        const SString strPlayersSortKey   = SString ( "%04d-", pServer->nMaxPlayers ? pServer->nPlayers + 1 : 0 ) + pServer->strTieBreakSortKey;

        const SString strPing             = pServer->nPing == 9999 ? "" : SString ( "%d", pServer->nPing );
        const SString strPingSortKey      = SString ( "%04d-", pServer->nPing ) + pServer->strTieBreakSortKey;

        // The row index could change at any point here if list sorting is enabled
        iIndex = m_pServerList [ Type ]->SetItemText ( iIndex, m_hVersion [ Type ], strVersion, false, false, true, strVersionSortKey );
        iIndex = m_pServerList [ Type ]->SetItemText ( iIndex, m_hName [ Type ],    pServer->strName, false, false, true, pServer->strNameSortKey );
        iIndex = m_pServerList [ Type ]->SetItemText ( iIndex, m_hGame [ Type ],    pServer->strGameMode, false, false, true );
        iIndex = m_pServerList [ Type ]->SetItemText ( iIndex, m_hPlayers [ Type ], strPlayers, false, false, true, strPlayersSortKey );
        iIndex = m_pServerList [ Type ]->SetItemText ( iIndex, m_hPing [ Type ],    strPing, false, false, true, strPingSortKey );

        // Locked icon
        m_pServerList [ Type ]->SetItemImage ( iIndex, m_hLocked [ Type ], pServer->bPassworded ? m_pLockedIcon : NULL );

        // Data for later use
        m_pServerList [ Type ]->SetItemData ( iIndex, DATA_PSERVER, (void*)pServer );

        // Colours
        SColor color = SColorRGBA ( 255,255,255,255 );

#if MTA_DEBUG
        if ( pServer->uiCacheNoReplyCount )                     color.R /= 2;
        if ( pServer->bMasterServerSaysNoResponse )             color.G /= 2;
        if ( pServer->uiMasterServerSaysRestrictions )          color.B /= 4;
#endif
        if ( bIsOtherVersion )                                  color.B /= 2;
        if ( pServer->bMaybeOffline )                           color.A = color.A / 3 * 2;
        if ( pServer->bSkipped )                                color.A = color.A / 4 * 3;

        m_pServerList [ Type ]->SetItemColor ( iIndex, m_hVersion [ Type ], color.R, color.G, color.B, color.A );
        m_pServerList [ Type ]->SetItemColor ( iIndex, m_hLocked [ Type ],  color.R, color.G, color.B, color.A );
        m_pServerList [ Type ]->SetItemColor ( iIndex, m_hName [ Type ],    color.R, color.G, color.B, color.A );
        m_pServerList [ Type ]->SetItemColor ( iIndex, m_hPlayers [ Type ], color.R, color.G, color.B, color.A );
        m_pServerList [ Type ]->SetItemColor ( iIndex, m_hPing [ Type ],    color.R, color.G, color.B, color.A );
        m_pServerList [ Type ]->SetItemColor ( iIndex, m_hGame [ Type ],    color.R, color.G, color.B, color.A );
    }
}


CServerList * CServerBrowser::GetServerList ( ServerBrowserType Type )
{
    if ( Type == ServerBrowserType::FAVOURITES )
        return &m_ServersFavourites;
    else if ( Type == ServerBrowserType::INTERNET )
        return &m_ServersInternet;
    else if ( Type == ServerBrowserType::LAN )
        return &m_ServersLAN;
    else if ( Type == ServerBrowserType::RECENTLY_PLAYED)
        return &m_ServersRecent;

    assert ( false );
    return NULL;
}


bool CServerBrowser::OnClick ( CGUIElement* pElement )
{
    ServerBrowserType Type = GetCurrentServerBrowserType ();

    if ( pElement == m_pServerPlayerList [ Type ] && m_pServerPlayerList [ Type ]->GetSelectedCount () >= 1 )
    {
        // Get the selected row of the player gridlist
        int iSelectedIndex = m_pServerPlayerList [ Type ]->GetSelectedItemRow ();
        std::string strSelectedPlayerName = m_pServerPlayerList [ Type ]->GetItemText ( iSelectedIndex, m_hPlayerName [ Type ] );

        // Walk the server list looking for the player on a server 
        CServerList * pList = GetServerList ( Type );
        CServerListIterator i, i_b = pList->IteratorBegin (), i_e = pList->IteratorEnd ();
        for ( i = i_b; i != i_e; i++ ) 
        {
            CServerListItem * pServer = *i;

            for ( unsigned int j = 0; j < pServer->vecPlayers.size (); j++ )
            {
                std::string strPlayerName = pServer->vecPlayers[j].c_str ();
                if ( strPlayerName.compare ( strSelectedPlayerName ) == 0 )
                {
                    // We found the server on which the player is
                    // Walk the server gridlist looking for the server host to get the row index
                    for ( int k = 0; k < m_pServerList [ Type ]->GetRowCount (); k++ )
                    {
                        if ( pServer == ((CServerListItem *)m_pServerList [ Type ]->GetItemData ( k, DATA_PSERVER )) )
                        {
                            // We found the index, select it
                            m_pServerList [ Type ]->SetSelectedItem ( k, DATA_PSERVER, true );

                            // save the selected server
                            m_iSelectedServer [ Type ] = iSelectedIndex;

                            return true;
                        }
                    }
                }
            }
        }
    }

    // If there is one item selected
    if ( m_pServerList [ Type ]->GetSelectedCount () >= 1 )
    {
        // Clear the player list
        m_pServerPlayerList [ Type ]->Clear ();

        // Get the selected row
        int iSelectedIndex = m_pServerList [ Type ]->GetSelectedItemRow ();

        CServerListItem * pServer = FindSelectedServer ( Type );
        if ( pServer )
        {
            // We found the server, add all the players
            for ( unsigned int j = 0; j < pServer->vecPlayers.size (); j++ ) 
            {
                int k = m_pServerPlayerList [ Type ]->AddRow ();
                m_pServerPlayerList [ Type ]->SetItemText ( k, m_hPlayerName [ Type ], pServer->vecPlayers[j].c_str () );
            }

            if ( pServer->nPlayers && !pServer->vecPlayers.size () )
            {
                int k = m_pServerPlayerList [ Type ]->AddRow ();
                m_pServerPlayerList [ Type ]->SetItemText ( k, m_hPlayerName [ Type ], PLAYER_LIST_PENDING_TEXT );
            }

            SetAddressBarText ( "mtasa://" + pServer->strEndpoint );

        }

        // save the selected server
        m_iSelectedServer [ Type ] = iSelectedIndex;
    }
    return true;
}


bool CServerBrowser::OnDoubleClick ( CGUIElement* pElement )
{
    return ConnectToSelectedServer ();
}

bool CServerBrowser::OnConnectClick ( CGUIElement* pElement )
{   
    unsigned short usPort;
    std::string strHost, strNick, strPassword;
    std::string strURI = m_pEditAddress [ GetCurrentServerBrowserType() ]->GetText();
    g_pCore->GetConnectParametersFromURI(strURI.c_str(), strHost, usPort, strNick, strPassword );

    // Valid nick?
    if ( !CCore::GetSingleton ().IsValidNick ( strNick.c_str () ) )
    {
        CCore::GetSingleton ().ShowMessageBox ( "Error", "Invalid nickname! Please go to Settings and set a new!", MB_BUTTON_OK | MB_ICON_INFO );
        return true;
    }

    // Start the connect
    CCore::GetSingleton ().GetConnectManager ()->Connect ( strHost.c_str (), usPort, strNick.c_str (), strPassword.c_str() );
    return true;
}

void CServerBrowser::CompleteConnect ( void )
{
    CServerBrowser *pBrowser = CServerBrowser::GetSingletonPtr ();
    pBrowser->ConnectToSelectedServer();
}

bool CServerBrowser::ConnectToSelectedServer ( void )
{
    ServerBrowserType Type = GetCurrentServerBrowserType ();
    m_pServerPlayerList [ Type ]->Clear ();

    // If there is one item selected
    if ( CServerListItem * pServer = FindSelectedServer ( Type ) )
    {
        if ( ( pServer->bSerials ) && ( !g_pCore->GetCommunity()->IsLoggedIn() ) )
        {
            m_pCommunityLogin.SetVisible ( true );
            return true;
        }
        // Get the nick from the config
        std::string strNick;
        CVARS_GET ( "nick", strNick );

        // Valid nick?
        if ( !CCore::GetSingleton ().IsValidNick ( strNick.c_str () ) )
        {
            CCore::GetSingleton ().ShowMessageBox ( "Error", "Invalid nickname! Please go to Settings and set a new!", MB_BUTTON_OK | MB_ICON_INFO );
            return true;
        }

        // Password buffer
        SString strPassword = "";

        // Start the connect
        CCore::GetSingleton ().GetConnectManager ()->Connect ( pServer->strHost.c_str (), pServer->usGamePort, strNick.c_str (), strPassword );
    }
    else
    {
        CCore::GetSingleton ().ShowMessageBox ( "Information", "You have to select a server to connect to.", MB_BUTTON_OK | MB_ICON_INFO );
    }
    return false;
}



bool CServerBrowser::OnRefreshClick ( CGUIElement* pElement )
{
    ServerBrowserType Type = GetCurrentServerBrowserType ();

    GetServerList ( Type )->Refresh ();

    return true;
}

bool CServerBrowser::OnFavouritesClick ( CGUIElement* pElement )
{
    unsigned short usPort;
    std::string strHost, strNick, strPassword;
    std::string strURI = m_pEditAddress [ GetCurrentServerBrowserType() ]->GetText();
    g_pCore->GetConnectParametersFromURI(strURI.c_str(), strHost, usPort, strNick, strPassword );

    // If there are more than 0 items selected in the browser
    if ( strHost.size() > 0 && usPort )
    {
        usPort += SERVER_LIST_QUERY_PORT_OFFSET;

        in_addr Address;

        CServerListItem::Parse ( strHost.c_str(), Address );

        // Do we have this entry already?  If so, remove it
        if ( m_ServersFavourites.Remove ( Address, usPort ) )
        {
            SaveFavouritesList();
            UpdateServerList ( ServerBrowserType::FAVOURITES, true );
            for ( unsigned int i = 0; i < SERVER_BROWSER_TYPE_COUNT; i++ )
            {
                m_pAddressFavoriteIcon[i]->SetAlpha ( 0.3f );
            }    
            return true;
        }

        if ( m_ServersFavourites.AddUnique ( Address, usPort ) )
        {
            SaveFavouritesList();
            UpdateServerList ( ServerBrowserType::FAVOURITES, true );
            for ( unsigned int i = 0; i < SERVER_BROWSER_TYPE_COUNT; i++ )
            {
                m_pAddressFavoriteIcon[i]->SetAlpha ( 1.0f );
            }    
        }
    }
    return true;
}

bool CServerBrowser::OnAddressChanged ( CGUIElement* pElement )
{
    unsigned short usPort;
    std::string strHost, strNick, strPassword;
    ServerBrowserType Type = GetCurrentServerBrowserType();
    std::string strURI = m_pEditAddress [ Type ]->GetText();
    g_pCore->GetConnectParametersFromURI(strURI.c_str(), strHost, usPort, strNick, strPassword );
    
    // If this address exists in favourites, change our favourites icon
    CServerListIterator i, i_b = m_ServersFavourites.IteratorBegin (), i_e = m_ServersFavourites.IteratorEnd ();
    for ( CServerListIterator i = i_b; i != i_e; i++ )
    {
        CServerListItem * pServer = *i;
        if ( pServer->strEndpoint == ( strHost + ":" + SString("%u",usPort) ) )
        {
            for ( unsigned int i = 0; i < SERVER_BROWSER_TYPE_COUNT; i++ )
            {
                m_pAddressFavoriteIcon[i]->SetAlpha ( 1.0f );                  
            }    
            return true;
        }
    }
    for ( unsigned int i = 0; i < SERVER_BROWSER_TYPE_COUNT; i++ )
    {
        m_pAddressFavoriteIcon[i]->SetAlpha ( 0.3f );
    }    
    return true;
}


bool CServerBrowser::OnBackClick ( CGUIElement* pElement )
{
    CMainMenu *pMainMenu = CLocalGUI::GetSingleton ().GetMainMenu ();

    m_pPanel->SetVisible ( false );
    pMainMenu->m_bIsInSubWindow = false;

    return true;
}


bool CServerBrowser::OnMouseClick ( CGUIMouseEventArgs Args )
{
    ServerBrowserType Type = GetCurrentServerBrowserType ();

    if ( Args.pWindow == m_pServerList [ ServerBrowserType::INTERNET ] )
    {
        OnClick ( m_pServerList [ ServerBrowserType::INTERNET ] );
        return true;
    }
    else if ( Args.pWindow == m_pServerList [ ServerBrowserType::LAN ] )
    {
        OnClick ( m_pServerList [ ServerBrowserType::LAN ] );
        return true;
    }
    else if ( Args.pWindow == m_pServerList [ ServerBrowserType::FAVOURITES ] )
    {
        OnClick ( m_pServerList [ ServerBrowserType::FAVOURITES ] );
        return true;
    }
    else if ( Args.pWindow == m_pServerList [ ServerBrowserType::RECENTLY_PLAYED ] )
    {
        OnClick ( m_pServerList [ ServerBrowserType::RECENTLY_PLAYED ] );
        return true;
    }
    else if ( Args.pWindow == m_pServerPlayerList [ Type ] /*&& !m_pEditPlayerSearch [ Type ]->GetText ().empty()*/ )
    {
        OnClick ( m_pServerPlayerList [ Type ] );
        return true;
    }

    return false;
}


bool CServerBrowser::OnMouseDoubleClick ( CGUIMouseEventArgs Args )
{
    if ( Args.pWindow == m_pServerList [ ServerBrowserType::INTERNET ] )
    {
        OnDoubleClick ( m_pServerList [ ServerBrowserType::INTERNET ] );
        return true;
    }
    if ( Args.pWindow == m_pServerList [ ServerBrowserType::LAN ] )
    {
        OnDoubleClick ( m_pServerList [ ServerBrowserType::LAN ] );
        return true;
    }
    else if ( Args.pWindow == m_pServerList [ ServerBrowserType::FAVOURITES ] )
    {
        OnDoubleClick ( m_pServerList [ ServerBrowserType::FAVOURITES ] );
        return true;
    }
    else if ( Args.pWindow == m_pServerList [ ServerBrowserType::RECENTLY_PLAYED ] )
    {
        OnDoubleClick ( m_pServerList [ ServerBrowserType::RECENTLY_PLAYED ] );
        return true;
    }

    return false;
}

bool CServerBrowser::OnFilterChanged ( CGUIElement* pElement )
{
    UpdateServerList ( GetCurrentServerBrowserType (), true );
    //SaveOptions ( );  Slow

    return true;
}

bool CServerBrowser::OnTabChanged ( CGUIElement* pElement )
{
    SaveOptions ( );

    return true;
}


bool CServerBrowser::LoadServerList ( CXMLNode* pNode, const std::string& strTagName, CServerList *pList )
{
    CXMLNode* pSubNode = NULL;
    in_addr Address;
    int iPort;

    if ( !pNode )
        return false;

    // Loop through all subnodes looking for relevant nodes
    unsigned int uiCount = pNode->GetSubNodeCount ();
    for ( unsigned int i = 0; i < uiCount; i++ )
    {
        pSubNode = pNode->GetSubNode ( i );
        if ( pSubNode && pSubNode->GetTagName ().compare ( strTagName ) == 0 )
        {
            // This node is relevant, so get the attributes we need and add it to the list
            CXMLAttribute* pHostAttribute = pSubNode->GetAttributes ().Find ( "host" );
            CXMLAttribute* pPortAttribute = pSubNode->GetAttributes ().Find ( "port" );
            if ( pHostAttribute && pPortAttribute ) {
                if ( CServerListItem::Parse ( pHostAttribute->GetValue ().c_str (), Address ) )
                {
                    iPort = atoi ( pPortAttribute->GetValue ().c_str () ) + SERVER_LIST_QUERY_PORT_OFFSET;
                    if ( iPort > 0 )
                        pList->AddUnique ( Address, iPort );
                }
            }
        }
    }
    pList->SetUpdated ( true );
    return true;
}


void CServerBrowser::SaveRecentlyPlayedList()
{
    CXMLNode* pConfig = CCore::GetSingletonPtr ()->GetConfig ();
    CXMLNode* pRecent = pConfig->FindSubNode ( CONFIG_NODE_SERVER_REC );
    if ( !pRecent )
        pRecent = pConfig->CreateSubNode ( CONFIG_NODE_SERVER_REC );
    SaveServerList ( pRecent, CONFIG_RECENT_LIST_TAG, GetRecentList () );
}


void CServerBrowser::SaveFavouritesList()
{
    CXMLNode* pConfig = CCore::GetSingletonPtr ()->GetConfig ();
    CXMLNode* pFavourites = pConfig->FindSubNode ( CONFIG_NODE_SERVER_FAV );
    if ( !pFavourites )
        pFavourites = pConfig->CreateSubNode ( CONFIG_NODE_SERVER_FAV );
    SaveServerList ( pFavourites, CONFIG_FAVOURITE_LIST_TAG, GetFavouritesList () );
}


bool CServerBrowser::SaveServerList ( CXMLNode* pNode, const std::string& strTagName, CServerList *pList )
{
    if ( !pNode )
        return false;

    // Start by clearing out all previous nodes
    pNode->DeleteAllSubNodes ();

    // Iterate through the list, adding any items to our node
    CServerListIterator i, i_b = pList->IteratorBegin (), i_e = pList->IteratorEnd ();
    for ( CServerListIterator i = i_b; i != i_e; i++ )
    {
        CServerListItem * pServer = *i;

        // Add the item to the node
        CXMLNode * pSubNode = pNode->CreateSubNode ( strTagName.c_str () );
        if ( pSubNode )
        {
            CXMLAttribute* pHostAttribute = pSubNode->GetAttributes ().Create ( "host" );
            std::string strHost = pServer->strHost;
            if ( !pServer->strHostName.empty () )
                strHost = pServer->strHostName;
            pHostAttribute->SetValue ( strHost.c_str () );
            
            CXMLAttribute* pPortAttribute = pSubNode->GetAttributes ().Create ( "port" );
            pPortAttribute->SetValue ( pServer->usGamePort );
        }
    }
    return true;
}

void CServerBrowser::LoadOptions ( CXMLNode* pNode )
{
    if ( !pNode ) {
        //Node does not exist so allow saving
        m_bOptionsLoaded = true;
        return;
    }

    // loop through all subnodes
    unsigned int uiCount = pNode->GetSubNodeCount ( ); 
    for ( unsigned int ui = 0; ui < uiCount; ui ++ )
    {
        CXMLNode * pSubNode = pNode->GetSubNode ( ui );
        if ( pSubNode && pSubNode->GetTagName ( ).compare ( "list" ) == 0 )
        {
            CXMLAttribute* pListID = pSubNode->GetAttributes ( ).Find ( "id" );
            if ( pListID )
            {
                // Check for a valid list ID
                int i = atoi ( pListID->GetValue ().c_str () );
                if ( i >= 0 && i < SERVER_BROWSER_TYPE_COUNT )
                {
                    // load all checkbox options
                    CXMLAttribute* pIncludeEmpty = pSubNode->GetAttributes ( ).Find ( "include_empty" );
                    if ( pIncludeEmpty )
                        m_pIncludeEmpty[ i ]->SetSelected ( pIncludeEmpty->GetValue ( ).compare ( "1" ) == 0 );

                    CXMLAttribute* pIncludeFull = pSubNode->GetAttributes ( ).Find ( "include_full" );
                    if ( pIncludeFull )
                        m_pIncludeFull[ i ]->SetSelected ( pIncludeFull->GetValue ( ).compare ( "1" ) == 0 );

                    CXMLAttribute* pIncludeLocked = pSubNode->GetAttributes ( ).Find ( "include_locked" );
                    if ( pIncludeLocked )
                        m_pIncludeLocked[ i ]->SetSelected ( pIncludeLocked->GetValue ( ).compare ( "1" ) == 0 );

                    //CXMLAttribute* pIncludeOtherVersions = pSubNode->GetAttributes ( ).Find ( "include_other_versions" );
                    //if ( pIncludeOtherVersions )
                    //    m_pIncludeOtherVersions[ i ]->SetSelected ( pIncludeOtherVersions->GetValue ( ).compare ( "1" ) == 0 );

                    // load 'include offline' if the checkbox exists
                    if ( m_pIncludeOffline[ i ] )
                    {
                        CXMLAttribute* pIncludeOffline = pSubNode->GetAttributes ( ).Find ( "include_offline" );
                        if ( pIncludeOffline )
                            m_pIncludeOffline[ i ]->SetSelected ( pIncludeOffline->GetValue ( ).compare ( "1" ) == 0 );
                    }

                    // restore the active tab
                    CXMLAttribute* pActiveTab = pSubNode->GetAttributes ( ).Find ( "active" );
                    if ( pActiveTab && pActiveTab->GetValue ( ).compare ( "1" ) == 0 )
                        m_pPanel->SetSelectedTab ( m_pTab [ i ] );

                    // restore the search field contents
                    std::string strSearch = pSubNode->GetTagContent ( );
                    //if ( strSearch.length ( ) > 0 )
                        //m_pEditServerSearch [ i ]->SetText ( strSearch.c_str ( ) );
                }
            }
        }
    }
    m_bOptionsLoaded = true;
}

void CServerBrowser::SaveOptions ( )
{
    // Check to make sure if the options were loaded yet, if not the 'changed' events might screw up
    if ( !m_bOptionsLoaded )
        return;

    CXMLNode* pConfig = CCore::GetSingletonPtr ( )->GetConfig ( );
    CXMLNode* pOptions = pConfig->FindSubNode ( CONFIG_NODE_SERVER_OPTIONS );
    if ( !pOptions )
    {
        pOptions = pConfig->CreateSubNode ( CONFIG_NODE_SERVER_OPTIONS );
    }
    else
    {
        // start with a clean node
        pOptions->DeleteAllSubNodes ( );
    }

    int iCurrentType = GetCurrentServerBrowserType ( );

    // Save the options for all four lists
    for ( unsigned int ui = 0; ui < SERVER_BROWSER_TYPE_COUNT; ui++ )
    {
        CXMLNode * pSubNode = pOptions->CreateSubNode ( "list" );
        if ( pSubNode ) 
        {
            // ID of the list to save
            CXMLAttribute* pListID = pSubNode->GetAttributes ( ).Create ( "id" );
            pListID->SetValue ( ui );

            // Checkboxes
            CXMLAttribute* pIncludeEmpty = pSubNode->GetAttributes ( ).Create ( "include_empty" );
            pIncludeEmpty->SetValue ( m_pIncludeEmpty [ ui ]->GetSelected ( ) );

            CXMLAttribute* pIncludeFull = pSubNode->GetAttributes ( ).Create ( "include_full" );
            pIncludeFull->SetValue ( m_pIncludeFull [ ui ]->GetSelected ( ) );

            CXMLAttribute* pIncludeLocked = pSubNode->GetAttributes ( ).Create ( "include_locked" );
            pIncludeLocked->SetValue ( m_pIncludeLocked [ ui ]->GetSelected ( ) );

            //CXMLAttribute* pIncludeOtherVersions = pSubNode->GetAttributes ( ).Create ( "include_other_versions" );
            //pIncludeOtherVersions->SetValue ( m_pIncludeOtherVersions [ ui ]->GetSelected ( ) );

            // Only recently played & favorites have 'Include offline'
            if ( m_pIncludeOffline [ ui ] )
            {
                CXMLAttribute* pIncludeOffline = pSubNode->GetAttributes ( ).Create ( "include_offline" );
                pIncludeOffline->SetValue ( m_pIncludeOffline [ ui ]->GetSelected ( ) );
            }

            // Save the active Tab
            if ( iCurrentType == ui )
            {
                CXMLAttribute* pActive = pSubNode->GetAttributes ( ).Create ( "active" );
                pActive->SetValue ( 1 );
            }

            // save the search box content
            /*std::string strSearch = m_pEditServerSearch [ ui ]->GetText ( );
            if ( strSearch.length ( ) > 0 )
            {
                pSubNode->SetTagContent ( strSearch.c_str ( ) );
            }*/
        }
    }
    g_pCore->SaveConfig ( );
}

void CServerBrowser::SetServerPassword ( const std::string& strHost, const std::string& strPassword )
{
    CXMLNode* pConfig = CCore::GetSingletonPtr ()->GetConfig ();
    CXMLNode* pServerPasswords = pConfig->FindSubNode ( CONFIG_NODE_SERVER_SAVED );
    if ( !pServerPasswords )
    {
        pServerPasswords = pConfig ->CreateSubNode ( CONFIG_NODE_SERVER_SAVED );
    }
    //Check if the server password already exists
    for ( unsigned int i = 0 ; i < pServerPasswords->GetSubNodeCount() ; i++ )
    {    
        CXMLAttributes* pAttributes = &(pServerPasswords->GetSubNode(i)->GetAttributes());
        if ( pAttributes->Find( "host" ) )
        {
            if ( CXMLAttribute* pHost = pAttributes->Find ( "host" ) )
            {
                const std::string& strXMLHost = pHost->GetValue();
                if ( strXMLHost == strHost )
                {
                    CXMLAttribute* pPassword = pAttributes->Create( "password" );
                    pPassword->SetValue(strPassword.c_str());
                    return;
                }
            }
        }
    }

    // Otherwise create the node from scratch
    CXMLNode* pNode = pServerPasswords->CreateSubNode( "server" );
    CXMLAttribute* pHostAttribute = pNode->GetAttributes().Create ( "host" );
    pHostAttribute->SetValue(strHost.c_str());
    CXMLAttribute* pPasswordAttribute = pNode->GetAttributes().Create ( "password" );
    pPasswordAttribute->SetValue(strPassword.c_str());
}


std::string CServerBrowser::GetServerPassword ( const std::string& strHost )
{
    CXMLNode* pConfig = CCore::GetSingletonPtr ()->GetConfig ();
    CXMLNode* pServerPasswords = pConfig->FindSubNode ( CONFIG_NODE_SERVER_SAVED );
    if ( !pServerPasswords )
    {
        pServerPasswords = pConfig ->CreateSubNode ( CONFIG_NODE_SERVER_SAVED );
    }
    //Check if the server password already exists
    for ( unsigned int i = 0 ; i < pServerPasswords->GetSubNodeCount() ; i++ )
    {    
        CXMLAttributes* pAttributes = &(pServerPasswords->GetSubNode(i)->GetAttributes());
        if ( pAttributes->Find( "host" ) )
        {
            if ( CXMLAttribute* pHost = pAttributes->Find ( "host" ) )
            {
                const std::string& strXMLHost = pHost->GetValue();
                if ( pHost->GetValue() == strHost )
                {
                    CXMLAttribute* pPassword = pAttributes->Create( "password" );
                    const std::string& strPassword = pPassword->GetValue();
                    return strPassword;
                }
            }
        }
        
    }
    return "";
}


void CServerBrowser::ClearServerPasswords ()
{
    CXMLNode* pConfig = CCore::GetSingletonPtr ()->GetConfig ();
    CXMLNode* pServerPasswords = pConfig->FindSubNode ( CONFIG_NODE_SERVER_SAVED );
    if ( pServerPasswords )
    {
        pServerPasswords->DeleteAllSubNodes();
        pConfig->DeleteSubNode ( pServerPasswords );
    }
}


/////////////////////////////////////////////////////////////////
//
// CServerBrowser::FindSelectedServer
//
//
//
/////////////////////////////////////////////////////////////////
CServerListItem* CServerBrowser::FindSelectedServer ( ServerBrowserType Type )
{
    if ( m_pServerList [ Type ]->GetSelectedCount () >= 1 )
    {
        return FindServerFromRow ( Type, m_pServerList [ Type ]->GetSelectedItemRow () );
    }
    return NULL;
}


/////////////////////////////////////////////////////////////////
//
// CServerBrowser::FindServerFromRow
//
//
//
/////////////////////////////////////////////////////////////////
CServerListItem* CServerBrowser::FindServerFromRow ( ServerBrowserType Type, int iRow )
{
    CServerList * pList = GetServerList ( Type );
    CServerListIterator i, i_b = pList->IteratorBegin (), i_e = pList->IteratorEnd ();
    CServerListItem * pSelectedServer = (CServerListItem *)m_pServerList [ Type ]->GetItemData ( iRow, DATA_PSERVER );
    if ( pSelectedServer )
        return pSelectedServer;
    return NULL;
}


/////////////////////////////////////////////////////////////////
//
// CServerBrowser::FindRowFromServer
//
//
//
/////////////////////////////////////////////////////////////////
int CServerBrowser::FindRowFromServer ( ServerBrowserType Type, const CServerListItem * pServer )
{
    ulong ulIp = pServer->Address.s_addr;
    ushort usPort = pServer->usGamePort;

    CGUIGridList* pServerList = m_pServerList [ Type ];
    int iRowCount = pServerList->GetRowCount ();
    for ( int i = 0 ; i < iRowCount ; i++ )
    {
        if ( pServer == (CServerListItem *)pServerList->GetItemData ( i, DATA_PSERVER ) )
        {
            return i;
        }
    }
    return -1;
}


/////////////////////////////////////////////////////////////////
//
// CServerBrowser::UpdateSelectedServerPlayerList
//
// Update GUI player list if select server gets query response
//
/////////////////////////////////////////////////////////////////
void CServerBrowser::UpdateSelectedServerPlayerList ( ServerBrowserType Type )
{
    // If there is one item selected
    if ( m_pServerList [ Type ]->GetSelectedCount () >= 1 )
    {
        // Get the selected row
        int iSelectedIndex = m_pServerList [ Type ]->GetSelectedItemRow ();

        // Get number of players as defined in the gridlist
        int iNumPlayers = atoi ( m_pServerList [ Type ]->GetItemText ( iSelectedIndex, m_hPlayers [ Type ] ) );

        // Get number of rows in the gui player list
        int iNumPlayerRows = m_pServerPlayerList [ Type ]->GetRowCount ();

        // If number of rows in player list is less than number of players in server item,
        if ( iNumPlayers > iNumPlayerRows || iNumPlayerRows == 1 && iNumPlayers == 1 )
        {
            // find server item
            CServerListItem * pServer = FindSelectedServer ( Type );

            if ( pServer && pServer->vecPlayers.size () > 0 )
            {
                bool bUpdatePlayerList = false;
                if ( iNumPlayerRows == 1 && pServer->vecPlayers.size () == 1 )
                {
                    SString strPlayerName = m_pServerPlayerList [ Type ]->GetItemText ( 0, m_hPlayerName [ Type ] );
                    if ( strPlayerName == PLAYER_LIST_PENDING_TEXT )
                        bUpdatePlayerList = true;
                }
                if ( (int)pServer->vecPlayers.size () > iNumPlayerRows || bUpdatePlayerList )
                {
                    m_pServerPlayerList [ Type ]->Clear ();

                    // Add all the players
                    for ( unsigned int j = 0; j < pServer->vecPlayers.size (); j++ ) 
                    {
                        int k = m_pServerPlayerList [ Type ]->AddRow ();
                        m_pServerPlayerList [ Type ]->SetItemText ( k, m_hPlayerName [ Type ], pServer->vecPlayers[j].c_str () );
                    }
                }
            }
        }
    }
}


/////////////////////////////////////////////////////////////////
//
// CServerBrowser::GetVisibleEndPointList
//
// Get list servers that are visible in the GUI
//
/////////////////////////////////////////////////////////////////
void CServerBrowser::GetVisibleEndPointList ( std::vector < SAddressPort >& outEndpointList )
{
    ServerBrowserType Type = GetCurrentServerBrowserType ();

    int iFirst, iLast;
    m_pServerList [ Type ]->GetVisibleRowRange ( iFirst, iLast );
    for ( int i = iFirst; i >= 0 && i <= iLast ; i++ )
    {
        ulong ulIp = (ulong)m_pServerList [ Type ]->GetItemData ( i, DATA_PSERVER );
        ushort usPort = (ushort)m_pServerList [ Type ]->GetItemData ( i, m_hMap [ Type ] );
        outEndpointList.push_back ( SAddressPort ( (in_addr&)ulIp, usPort ) );
    }
}

void CServerBrowser::SetStatusText ( std::string strStatus )
{
    for ( unsigned int i = 0; i < SERVER_BROWSER_TYPE_COUNT; i++ )
    {
        m_pServerListStatus[i]->SetText ( strStatus.c_str() );
    }
}

void CServerBrowser::SetAddressBarText ( std::string strText )
{
    for ( unsigned int i = 0; i < SERVER_BROWSER_TYPE_COUNT; i++ )
    {
        m_pEditAddress[i]->SetText ( strText.c_str() );
    }    
}