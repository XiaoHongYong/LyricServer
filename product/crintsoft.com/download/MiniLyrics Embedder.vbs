Option Explicit
'v1.4b
Dim MI, Panel, Tmr, EmbedTmr
Const EmbedTimerTimeout = 500
Const EmbedTriesMax = 20
Const RePoSiTimerTimeout = 1000

Dim EmbedTriesCount : EmbedTriesCount = 0
Dim IsEmbedded : IsEmbedded = False

Sub OnStartup
    On Error Resume Next
    Script.RegisterEvent SDB, "OnShutdown", "SDB_OnShutdown"
    
    Set MI = SDB.UI.AddMenuItem(SDB.UI.Menu_View, 1, -3)
    MI.Caption = "MiniLyrics Panel"
    MI.IconIndex = 51
    MI.ShortCut = "Ctrl+Shift+L"
    MI.Hint = "Show the panel for the embedded MiniLyrics program"
    Script.RegisterEvent MI, "OnClick", "MI_OnClick"
	
    Set Panel = SDB.UI.NewDockablePersistentPanel("MiniLyricsEmbedder_Panel")
    If Panel.IsNew Then 
        Panel.DockedTo = 3
        Panel.Common.Width = 300
        Panel.Common.Height = 125
    End If 
    Panel.Caption = "Embedded MiniLyrics"
    Set SDB.Objects("MiniLyricsEmbedder_Panel") = Panel        'Storing the Panel in MM's Objects array is only needed to keep it alive the whole time
    Script.RegisterEvent Panel, "OnClose", "Panel_OnClose"
    Script.RegisterEvent Panel.Common, "OnResize", "Panel_OnResize"

    Panel.Common.Visible = True
    MI.Checked = True
    
End Sub

Sub EmbedTmr_OnTimer(EmbedTmr)

    On Error Resume Next
    
    IsEmbedded = True

    If SDB.IniFile.StringValue("MiniLyricsEmbedder", "PanelVisible") <> "True" Then
        Panel.Common.Visible = False
        MI.Checked = False
    End If
End Sub

Sub MI_OnClick(MI)

    On Error Resume Next
	MI.Checked = Not MI.Checked
	Panel.Common.Visible = MI.Checked
	SDB.IniFile.StringValue("MiniLyricsEmbedder", "PanelVisible") = MI.Checked
End Sub

Sub SDB_OnShutdown
    On Error Resume Next
    Script.UnRegisterAllEvents
    Set MI = Nothing
    Set Panel = Nothing
    Set Tmr = Nothing
    Set EmbedTmr = Nothing
End Sub

Sub Panel_OnClose(Panel)
    On Error Resume Next
    MI.Checked = False
    SDB.IniFile.StringValue("MiniLyricsEmbedder", "PanelVisible") = "False"
    Script.UnRegisterEvents Tmr
    Set Tmr = Nothing
End Sub

Sub Panel_OnResize(Panel)
	If Not IsEmbedded Then Exit Sub
    On Error Resume Next
End Sub

Sub RepositionAndResize(Tmr)
    On Error Resume Next
End Sub
