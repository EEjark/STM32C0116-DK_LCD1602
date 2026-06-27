Set fso = CreateObject("Scripting.FileSystemObject")
dir = fso.GetParentFolderName(WScript.ScriptFullName)
Set ws = CreateObject("Wscript.Shell")
ws.Run "cmd /c """"" & dir & "\启动PC监控.bat""""", 0, False
