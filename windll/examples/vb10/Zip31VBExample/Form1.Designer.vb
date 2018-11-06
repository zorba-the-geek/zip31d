<Global.Microsoft.VisualBasic.CompilerServices.DesignerGenerated()> _
Partial Class ZipVBForm
  Inherits System.Windows.Forms.Form

  'Form overrides dispose to clean up the component list.
  <System.Diagnostics.DebuggerNonUserCode()> _
  Protected Overrides Sub Dispose(ByVal disposing As Boolean)
    Try
      If disposing AndAlso components IsNot Nothing Then
        components.Dispose()
      End If
    Finally
      MyBase.Dispose(disposing)
    End Try
  End Sub

  'Required by the Windows Form Designer
  Private components As System.ComponentModel.IContainer

  'NOTE: The following procedure is required by the Windows Form Designer
  'It can be modified using the Windows Form Designer.  
  'Do not modify it using the code editor.
  <System.Diagnostics.DebuggerStepThrough()> _
  Private Sub InitializeComponent()
    Dim resources As System.ComponentModel.ComponentResourceManager = New System.ComponentModel.ComponentResourceManager(GetType(ZipVBForm))
    Me.StatusWindow = New System.Windows.Forms.RichTextBox()
    Me.ZipCommandLine = New System.Windows.Forms.TextBox()
    Me.Label1 = New System.Windows.Forms.Label()
    Me.Label2 = New System.Windows.Forms.Label()
    Me.DoIt_Button = New System.Windows.Forms.Button()
    Me.Label3 = New System.Windows.Forms.Label()
    Me.ZpVersion_Button = New System.Windows.Forms.Button()
    Me.ZpTestComParse_Button = New System.Windows.Forms.Button()
    Me.ZpTestCallback_Button = New System.Windows.Forms.Button()
    Me.ZpTestZip_Button = New System.Windows.Forms.Button()
    Me.ZpTestStructFunc_Button = New System.Windows.Forms.Button()
    Me.PercentEntry_Box = New System.Windows.Forms.TextBox()
    Me.Label4 = New System.Windows.Forms.Label()
    Me.Label5 = New System.Windows.Forms.Label()
    Me.PercentTotal_Box = New System.Windows.Forms.TextBox()
    Me.Label6 = New System.Windows.Forms.Label()
    Me.RootDir_Box = New System.Windows.Forms.TextBox()
    Me.Abort_Button = New System.Windows.Forms.Button()
    Me.EntryPath_Box = New System.Windows.Forms.TextBox()
    Me.Label8 = New System.Windows.Forms.Label()
    Me.FileSize_Box = New System.Windows.Forms.TextBox()
    Me.SeeZipOutput_CheckBox = New System.Windows.Forms.CheckBox()
    Me.ActionString_Box = New System.Windows.Forms.TextBox()
    Me.Label7 = New System.Windows.Forms.Label()
    Me.Label9 = New System.Windows.Forms.Label()
    Me.SeeServiceOutput_Checkbox = New System.Windows.Forms.CheckBox()
    Me.UnicodeZipOutput_CheckBox = New System.Windows.Forms.CheckBox()
    Me.SuspendLayout()
    '
    'StatusWindow
    '
    resources.ApplyResources(Me.StatusWindow, "StatusWindow")
    Me.StatusWindow.DetectUrls = False
    Me.StatusWindow.Name = "StatusWindow"
    Me.StatusWindow.ReadOnly = True
    '
    'ZipCommandLine
    '
    resources.ApplyResources(Me.ZipCommandLine, "ZipCommandLine")
    Me.ZipCommandLine.Name = "ZipCommandLine"
    '
    'Label1
    '
    resources.ApplyResources(Me.Label1, "Label1")
    Me.Label1.Name = "Label1"
    '
    'Label2
    '
    resources.ApplyResources(Me.Label2, "Label2")
    Me.Label2.Name = "Label2"
    '
    'DoIt_Button
    '
    resources.ApplyResources(Me.DoIt_Button, "DoIt_Button")
    Me.DoIt_Button.Name = "DoIt_Button"
    Me.DoIt_Button.UseVisualStyleBackColor = True
    '
    'Label3
    '
    resources.ApplyResources(Me.Label3, "Label3")
    Me.Label3.Name = "Label3"
    '
    'ZpVersion_Button
    '
    resources.ApplyResources(Me.ZpVersion_Button, "ZpVersion_Button")
    Me.ZpVersion_Button.Name = "ZpVersion_Button"
    Me.ZpVersion_Button.UseVisualStyleBackColor = True
    '
    'ZpTestComParse_Button
    '
    resources.ApplyResources(Me.ZpTestComParse_Button, "ZpTestComParse_Button")
    Me.ZpTestComParse_Button.Name = "ZpTestComParse_Button"
    Me.ZpTestComParse_Button.UseVisualStyleBackColor = True
    '
    'ZpTestCallback_Button
    '
    resources.ApplyResources(Me.ZpTestCallback_Button, "ZpTestCallback_Button")
    Me.ZpTestCallback_Button.Name = "ZpTestCallback_Button"
    Me.ZpTestCallback_Button.UseVisualStyleBackColor = True
    '
    'ZpTestZip_Button
    '
    resources.ApplyResources(Me.ZpTestZip_Button, "ZpTestZip_Button")
    Me.ZpTestZip_Button.Name = "ZpTestZip_Button"
    Me.ZpTestZip_Button.UseVisualStyleBackColor = True
    '
    'ZpTestStructFunc_Button
    '
    resources.ApplyResources(Me.ZpTestStructFunc_Button, "ZpTestStructFunc_Button")
    Me.ZpTestStructFunc_Button.Name = "ZpTestStructFunc_Button"
    Me.ZpTestStructFunc_Button.UseVisualStyleBackColor = True
    '
    'PercentEntry_Box
    '
    resources.ApplyResources(Me.PercentEntry_Box, "PercentEntry_Box")
    Me.PercentEntry_Box.Name = "PercentEntry_Box"
    Me.PercentEntry_Box.ReadOnly = True
    '
    'Label4
    '
    resources.ApplyResources(Me.Label4, "Label4")
    Me.Label4.Name = "Label4"
    '
    'Label5
    '
    resources.ApplyResources(Me.Label5, "Label5")
    Me.Label5.Name = "Label5"
    '
    'PercentTotal_Box
    '
    resources.ApplyResources(Me.PercentTotal_Box, "PercentTotal_Box")
    Me.PercentTotal_Box.Name = "PercentTotal_Box"
    Me.PercentTotal_Box.ReadOnly = True
    '
    'Label6
    '
    resources.ApplyResources(Me.Label6, "Label6")
    Me.Label6.Name = "Label6"
    '
    'RootDir_Box
    '
    resources.ApplyResources(Me.RootDir_Box, "RootDir_Box")
    Me.RootDir_Box.Name = "RootDir_Box"
    '
    'Abort_Button
    '
    resources.ApplyResources(Me.Abort_Button, "Abort_Button")
    Me.Abort_Button.Name = "Abort_Button"
    Me.Abort_Button.UseVisualStyleBackColor = True
    '
    'EntryPath_Box
    '
    resources.ApplyResources(Me.EntryPath_Box, "EntryPath_Box")
    Me.EntryPath_Box.Name = "EntryPath_Box"
    Me.EntryPath_Box.ReadOnly = True
    '
    'Label8
    '
    resources.ApplyResources(Me.Label8, "Label8")
    Me.Label8.Name = "Label8"
    '
    'FileSize_Box
    '
    resources.ApplyResources(Me.FileSize_Box, "FileSize_Box")
    Me.FileSize_Box.Name = "FileSize_Box"
    Me.FileSize_Box.ReadOnly = True
    '
    'SeeZipOutput_CheckBox
    '
    resources.ApplyResources(Me.SeeZipOutput_CheckBox, "SeeZipOutput_CheckBox")
    Me.SeeZipOutput_CheckBox.Checked = True
    Me.SeeZipOutput_CheckBox.CheckState = System.Windows.Forms.CheckState.Checked
    Me.SeeZipOutput_CheckBox.Name = "SeeZipOutput_CheckBox"
    Me.SeeZipOutput_CheckBox.UseVisualStyleBackColor = True
    '
    'ActionString_Box
    '
    resources.ApplyResources(Me.ActionString_Box, "ActionString_Box")
    Me.ActionString_Box.Name = "ActionString_Box"
    Me.ActionString_Box.ReadOnly = True
    '
    'Label7
    '
    resources.ApplyResources(Me.Label7, "Label7")
    Me.Label7.Name = "Label7"
    '
    'Label9
    '
    resources.ApplyResources(Me.Label9, "Label9")
    Me.Label9.Name = "Label9"
    '
    'SeeServiceOutput_Checkbox
    '
    resources.ApplyResources(Me.SeeServiceOutput_Checkbox, "SeeServiceOutput_Checkbox")
    Me.SeeServiceOutput_Checkbox.Checked = True
    Me.SeeServiceOutput_Checkbox.CheckState = System.Windows.Forms.CheckState.Checked
    Me.SeeServiceOutput_Checkbox.Name = "SeeServiceOutput_Checkbox"
    Me.SeeServiceOutput_Checkbox.UseVisualStyleBackColor = True
    '
    'UnicodeZipOutput_CheckBox
    '
    resources.ApplyResources(Me.UnicodeZipOutput_CheckBox, "UnicodeZipOutput_CheckBox")
    Me.UnicodeZipOutput_CheckBox.Checked = True
    Me.UnicodeZipOutput_CheckBox.CheckState = System.Windows.Forms.CheckState.Checked
    Me.UnicodeZipOutput_CheckBox.Name = "UnicodeZipOutput_CheckBox"
    Me.UnicodeZipOutput_CheckBox.UseVisualStyleBackColor = True
    '
    'ZipVBForm
    '
    resources.ApplyResources(Me, "$this")
    Me.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font
    Me.Controls.Add(Me.UnicodeZipOutput_CheckBox)
    Me.Controls.Add(Me.SeeServiceOutput_Checkbox)
    Me.Controls.Add(Me.Label9)
    Me.Controls.Add(Me.Label7)
    Me.Controls.Add(Me.ActionString_Box)
    Me.Controls.Add(Me.SeeZipOutput_CheckBox)
    Me.Controls.Add(Me.Label8)
    Me.Controls.Add(Me.FileSize_Box)
    Me.Controls.Add(Me.EntryPath_Box)
    Me.Controls.Add(Me.Abort_Button)
    Me.Controls.Add(Me.RootDir_Box)
    Me.Controls.Add(Me.Label6)
    Me.Controls.Add(Me.PercentTotal_Box)
    Me.Controls.Add(Me.Label5)
    Me.Controls.Add(Me.Label4)
    Me.Controls.Add(Me.PercentEntry_Box)
    Me.Controls.Add(Me.ZpTestStructFunc_Button)
    Me.Controls.Add(Me.ZpTestZip_Button)
    Me.Controls.Add(Me.ZpTestCallback_Button)
    Me.Controls.Add(Me.ZpTestComParse_Button)
    Me.Controls.Add(Me.ZpVersion_Button)
    Me.Controls.Add(Me.Label3)
    Me.Controls.Add(Me.DoIt_Button)
    Me.Controls.Add(Me.Label2)
    Me.Controls.Add(Me.Label1)
    Me.Controls.Add(Me.ZipCommandLine)
    Me.Controls.Add(Me.StatusWindow)
    Me.Name = "ZipVBForm"
    Me.ResumeLayout(False)
    Me.PerformLayout()

  End Sub
  Friend WithEvents ZipCommandLine As System.Windows.Forms.TextBox
  Friend WithEvents Label1 As System.Windows.Forms.Label
  Friend WithEvents Label2 As System.Windows.Forms.Label
  Friend WithEvents DoIt_Button As System.Windows.Forms.Button
  Friend WithEvents Label3 As System.Windows.Forms.Label
  Public WithEvents StatusWindow As System.Windows.Forms.RichTextBox
  Friend WithEvents ZpVersion_Button As System.Windows.Forms.Button
  Friend WithEvents ZpTestComParse_Button As System.Windows.Forms.Button
  Friend WithEvents ZpTestCallback_Button As System.Windows.Forms.Button
  Friend WithEvents ZpTestZip_Button As System.Windows.Forms.Button
  Friend WithEvents ZpTestStructFunc_Button As System.Windows.Forms.Button
  Friend WithEvents PercentEntry_Box As System.Windows.Forms.TextBox
  Friend WithEvents Label4 As System.Windows.Forms.Label
  Friend WithEvents Label5 As System.Windows.Forms.Label
  Friend WithEvents PercentTotal_Box As System.Windows.Forms.TextBox
  Friend WithEvents Label6 As System.Windows.Forms.Label
  Friend WithEvents RootDir_Box As System.Windows.Forms.TextBox
  Friend WithEvents Abort_Button As System.Windows.Forms.Button
  Friend WithEvents EntryPath_Box As System.Windows.Forms.TextBox
  Friend WithEvents Label8 As System.Windows.Forms.Label
  Friend WithEvents FileSize_Box As System.Windows.Forms.TextBox
  Friend WithEvents SeeZipOutput_CheckBox As System.Windows.Forms.CheckBox
  Friend WithEvents ActionString_Box As System.Windows.Forms.TextBox
  Friend WithEvents Label7 As System.Windows.Forms.Label
  Friend WithEvents Label9 As System.Windows.Forms.Label
  Friend WithEvents SeeServiceOutput_Checkbox As System.Windows.Forms.CheckBox
  Friend WithEvents UnicodeZipOutput_CheckBox As System.Windows.Forms.CheckBox

End Class
